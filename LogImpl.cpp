#include "Log.h"
#include "format.h"
#include "utils.h"

#include <algorithm>
#include <ios>
#include <memory>
#include <string_view>
#include <sstream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <array>
#include <exception>
#include <future>
#include <chrono>
#include <vector>
#include <fstream>
#include <filesystem>

extern "C" {
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#if defined (__linux__) || defined (__unix__) || defined (__ANDROID__)
    #include <unistd.h>
#elif defined (__WIN32) || defined(__WIN64) || defined(WIN32)
    #include <windows.h>
#else
    #error "Not support platform!"
#endif

}

#define THROW_SYS_ERR(msg) throw std::system_error { errno, std::system_category(), msg };
#define THROW_RUNTIME_ERR(msg) throw std::runtime_error { msg };
#define THROW_NET_ERR(errNum, msg) throw std::runtime_error { std::string(msg) + gai_strerror(errNum) };

namespace utils::detail {

constexpr size_t LOG_BUFFER_SIZE = 4096;

using namespace std::chrono;
using namespace std::chrono_literals;

// LogBuffer is a simple memory buffer wrapper.
class LogBuffer {
    DISABLE_COPY(LogBuffer);
    DISABLE_MOVE(LogBuffer);

public:
    LogBuffer() {
        mUsedSize = 0;
    }

    ~LogBuffer() = default;

    [[nodiscard]]
    bool writable(size_t size) const {
        return (mRawBuffer.size() - mUsedSize) >= size;
    }

    void write(const char* srcData, size_t size) {
        std::cout << "start write size:" << size << std::endl;
        if (!writable(size))
            THROW_RUNTIME_ERR("Write failed because data is too big");
        ::memcpy(mRawBuffer.data() + mUsedSize, srcData, size);
        mUsedSize += size;
    }

    [[nodiscard]]
    bool flushEnable() const {
        return mUsedSize > 0;
    }

    void flush(std::fstream &fileStream) {
        fileStream.write(mRawBuffer.data(), mUsedSize);
        fileStream.flush();
        mUsedSize = 0;
    }

    [[nodiscard]]
    int size() const {
        return mUsedSize;
    }

private:
    std::array<char, LOG_BUFFER_SIZE>   mRawBuffer;
    size_t                              mUsedSize;
};

// LogServer is the backend server, which manage multiple memory buffers and
// flush these buffers to Log file asynchronously in appropriate time.
class LogServer {
    DISABLE_COPY(LogServer);
    DISABLE_MOVE(LogServer);
public:
    static auto getInstance() -> std::shared_ptr<LogServer>;
    LogServer();
    ~LogServer();

    // Client would call this funtion to format log line and write it to LogBuffer.
    void write(LogLevel level, std::string_view fmt);
private:
    auto createLogFileStream() -> std::fstream;

    // Default interval time which LogServer would flush all current buffer to log file.
    static constexpr auto DEFAULT_FLUSH_INTERVAL = 2000ms;

    std::fstream            mLogFileStream;
    uint32_t                mLogAlreadyWritenBytes;

    std::mutex              mMutex;
    std::condition_variable mCond;
    std::thread             mFlushThread;
    bool                    mStopThread;
    std::unique_ptr<LogBuffer>
                            mpCurrentBuffer;
    std::vector<std::unique_ptr<LogBuffer>>
                            mvAvailbleBuffers;
    std::vector<std::unique_ptr<LogBuffer>>
                            mvPendingBuffers;
};

LogServer::LogServer() {
    // Create log file.
    mLogFileStream = createLogFileStream();
    mLogAlreadyWritenBytes = 0;

    // Create LogServer thread.
    mStopThread = false;
    mpCurrentBuffer = std::make_unique<LogBuffer>();
    mFlushThread = std::thread([this] {
        std::vector<std::unique_ptr<LogBuffer>> needFlushBuffers;
        while (!mStopThread) {
            // Get pending buffers.
            {
                std::unique_lock lock { mMutex };
                // Wait for availble pending buffers.
                auto hasPendingBuf = mCond.wait_for(lock, DEFAULT_FLUSH_INTERVAL, [&] {
                    return mStopThread || !mvPendingBuffers.empty();
                });
                if (mStopThread) {
                    return ;
                }
                // Condition wait time out, there has no pending buffers, so flush current buffer to log file.
                if (!hasPendingBuf) {
                    mvPendingBuffers.emplace_back(std::move(mpCurrentBuffer));
                    if (mvAvailbleBuffers.empty()) {
                        mpCurrentBuffer = std::make_unique<LogBuffer>();
                    } else {
                        mpCurrentBuffer = std::move(mvAvailbleBuffers.back());
                        mvAvailbleBuffers.pop_back();
                    }
                }
                needFlushBuffers.swap(mvPendingBuffers);
            }
            // Start flush buffer to log file.
            for (auto& buffer: needFlushBuffers) {
                if (buffer->size() + mLogAlreadyWritenBytes >= LOG_MAX_FILE_SIZE) {
                    std::cout << __FUNCTION__ << ": Log file is full, create new log file." << std::endl;
                    mLogFileStream.flush();
                    mLogFileStream.close();
                    mLogFileStream = createLogFileStream();
                    mLogAlreadyWritenBytes = 0;
                }
                mLogAlreadyWritenBytes += buffer->size();
                buffer->flush(mLogFileStream);
            }
            // Return availble buffers.
            std::cout << __FUNCTION__ << ": Return buffers" << std::endl;
            {
                std::lock_guard lock { mMutex };
                for (auto& buffer: needFlushBuffers) {
                    mvAvailbleBuffers.emplace_back(std::move(buffer));
                }
                needFlushBuffers.clear();
            }
        }
    });
}

LogServer::~LogServer() {
    mStopThread = true;
    mCond.notify_one();
    if (mFlushThread.joinable())
        mFlushThread.join();
    // Flush reserve buffers.
    for (auto& buffer: mvPendingBuffers) {
        buffer->flush(mLogFileStream);
    }
    if (mpCurrentBuffer->flushEnable())
        mpCurrentBuffer->flush(mLogFileStream);
    mLogFileStream.flush();
    mLogFileStream.close();
}

auto LogServer::getInstance() -> std::shared_ptr<LogServer> {
    static std::mutex gInstanceMutex;
    static std::weak_ptr<LogServer> gInstanceWp;

    std::lock_guard lock { gInstanceMutex };
    auto instance = gInstanceWp.lock();
    if (instance == nullptr) {
        instance = std::make_shared<LogServer>();
        gInstanceWp = instance;
    }
    return instance;
}

void LogServer::write(LogLevel level, std::string_view fmt) {
#if defined (__linux__) || defined (__unix__) || defined (__ANDROID__)
    thread_local int pid = getpid();
    thread_local int tid = gettid();
#elif defined (__WIN32) || defined(__WIN64) || defined(WIN32)
    thread_local int pid = GetCurrentProcessId();
    thread_local int tid = GetCurrentThreadId();
#else
    #error "Not support platform!"
#endif
    constexpr auto log_level_to_string= [] (LogLevel level) {
        switch (level) {
            case LogLevel::Version:
                return "Ver  ";
            case LogLevel::Debug:
                return "Debug";
            case LogLevel::Info:
                return "Info ";
            case LogLevel::Warning:
                return "Warn ";
            case LogLevel::Error:
                return "Error";
            default:
                return " ";
        }
    };

    // We need lock at first time to make sure that the time sequence of input is right.
    std::lock_guard lock { mMutex };

    // Prepare log line.
    std::array<char, LOG_MAX_LINE_SIZE> logLine = {};
    time_t t = time(nullptr);
    struct tm now = {};
    // Get local time, it's not a system call.
    if (localtime_r(&t, &now) == nullptr)
        THROW_SYS_ERR("Can't get current time.");

    // Get milliseconds suffix
    auto timeSinceEpoch = std::chrono::system_clock::now().time_since_epoch().count();
    auto milliSeconds = static_cast<int>(timeSinceEpoch % (1000 * 1000 * 1000)) / 1000;

    // Format log line.
    int logLineLength = snprintf(logLine.data(), logLine.size(), "%04d-%02d-%02d %02d.%02d.%02d.%06d %5d %5d [%s] %s\n"
            , now.tm_year + 1900, now.tm_mon + 1, now.tm_mday
            , now.tm_hour, now.tm_min, now.tm_sec
            , milliSeconds
            , pid, tid
            , log_level_to_string(level)
            , fmt.data());
    if (logLineLength < 0) {
        THROW_RUNTIME_ERR("Error happen when format string.");
    }

    // Write log line to memory buffer.
    if (mpCurrentBuffer->writable(logLineLength)) {
        mpCurrentBuffer->write(logLine.data(), logLineLength);
    } else {
        // Current buffer is full, need to flush.
        mvPendingBuffers.emplace_back(std::move(mpCurrentBuffer));
        // And get new availble buffer.
        if (mvAvailbleBuffers.empty()) {
            mpCurrentBuffer = std::make_unique<LogBuffer>();
        } else {
            mpCurrentBuffer = std::move(mvAvailbleBuffers.back());
            mvAvailbleBuffers.pop_back();
        }
        mpCurrentBuffer->write(logLine.data(), logLineLength);
        // Notify backend server to flush pending buffers.
        mCond.notify_one();
    }
}

std::fstream LogServer::createLogFileStream() {
    // Create log path and change its permissions to 0777.
    try {
        // create_directory() return false because the directory is existed, ignored it.
        // We just focus on the case which create_directory() or permissions() throw a exception
        std::filesystem::create_directory(DEFAULT_LOG_PATH);
        std::filesystem::permissions(DEFAULT_LOG_PATH, std::filesystem::perms::all);
    } catch (...) {
        THROW_SYS_ERR("Can't create log filePath because:");
    }

    // Format the name of log file.
    std::array<char, 128> filePath = {};
    time_t t = time(nullptr);
    struct tm now = {};
    if (localtime_r(&t, &now) == nullptr)
        THROW_SYS_ERR("Can't get current time.");
    snprintf(filePath.data(), filePath.size(), "%s/%04d-%02d-%02d_%02d-%02d-%02d.log"
            , DEFAULT_LOG_PATH
            , now.tm_year + 1900, now.tm_mon + 1, now.tm_mday
            , now.tm_hour, now.tm_min, now.tm_sec
    );

    // Create log file.
    try {
        auto fileStream = std::fstream(filePath.data(), std::ios::in | std::ios::out | std::ios::trunc);
        return fileStream;
    } catch (...) {
        THROW_SYS_ERR("Can't create log file because:");
    }
}

void format_log_line(LogLevel level, std::string_view fmt) {
    // Every thread can hold a reference to LogServer
    thread_local static auto gLogServer = detail::LogServer::getInstance();
    gLogServer->write(level, fmt);
}

} // namespace utils::detail
