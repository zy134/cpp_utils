#include "Log.h"
#include "format.h"
#include "utils.h"

#include <algorithm>
#include <asm-generic/errno-base.h>
#include <bits/types/struct_tm.h>
#include <cstdint>
#include <memory>
#include <sched.h>
#include <string>
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

extern "C" {
#include <unistd.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
}

#define THROW_SYS_ERR(msg) throw std::system_error { errno, std::system_category(), msg };
#define THROW_RUNTIME_ERR(msg) throw std::runtime_error { msg };
#define THROW_NET_ERR(errNum, msg) throw std::runtime_error { std::string(msg) + gai_strerror(errNum) };

namespace utils::detail {

constexpr size_t LOG_BUFFER_SIZE = 4096;

using namespace std::chrono;
using namespace std::chrono_literals;

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

    void flush(int fd) {
        std::cout << "start flush" << std::endl;
        ::write(fd, mRawBuffer.data(), mUsedSize);
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

class LogServer {
    DISABLE_COPY(LogServer);
    DISABLE_MOVE(LogServer);
public:
    static std::shared_ptr<LogServer> getInstance();
    LogServer();
    ~LogServer();

    void write(LogLevel level, std::string_view fmt);
private:
    int createLogFile();

    int                     mLogFd;
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
    mLogFd = createLogFile();
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
                auto hasPendingBuf = mCond.wait_for(lock, 1000ms, [&] {
                    return mStopThread || !mvPendingBuffers.empty();
                });
                if (mStopThread) {
                    return ;
                }
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
                    mLogFd = createLogFile();
                    mLogAlreadyWritenBytes = 0;
                }
                mLogAlreadyWritenBytes += buffer->size();
                buffer->flush(mLogFd);
            }
            // Return availble buffers.
            std::cout << __FUNCTION__ << ": Return buffers" << std::endl;
            {
                // std::lock_guard lock { mMutex };
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
        buffer->flush(mLogFd);
    }
    if (mpCurrentBuffer->flushEnable())
        mpCurrentBuffer->flush(mLogFd);
    close(mLogFd);
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
    thread_local pid_t pid = getpid();
    thread_local pid_t tid = gettid();
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
    timeval tv = {};
    gettimeofday(&tv, nullptr);

    // Format log line.
    int logLineLength = snprintf(logLine.data(), logLine.size(), "%04d-%02d-%02d %02d.%02d.%02d.%03d %5d %5d [%s] %s\n"
            , now.tm_year + 1900, now.tm_mon + 1, now.tm_mday
            , now.tm_hour, now.tm_min, now.tm_sec
            , static_cast<int>(tv.tv_usec)
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

// Thread safety function, unlock.
int LogServer::createLogFile() {
    int fd = -1;
    // Create log path. If log path is already exist, ignore errno.
    // Don't use "check and make", just call mkdir directly to avoid race condition.
    auto res = mkdir(DEFAULT_PATH.data(), 0777);
    if (res != 0 && errno != EEXIST) {
        std::string errMsg = "Can't create path:";
        errMsg.append(DEFAULT_PATH);
        THROW_SYS_ERR(errMsg);
    }

    // Create log file.
    std::array<char, 128> filePath = {};
    time_t t = time(nullptr);
    struct tm now = {};
    if (localtime_r(&t, &now) == nullptr)
        THROW_SYS_ERR("Can't get current time.");
    snprintf(filePath.data(), filePath.size(), "%s/%04d-%02d-%02d_%02d-%02d-%02d.log"
            , DEFAULT_PATH.data()
            , now.tm_year + 1900, now.tm_mon + 1, now.tm_mday
            , now.tm_hour, now.tm_min, now.tm_sec
    );
    fd = ::open(filePath.data(), O_CREAT | O_RDWR | O_TRUNC, 0666);
    if (fd < 0) {
        std::string errMsg = "Can't create log file:";
        errMsg.append(filePath.data());
        THROW_SYS_ERR(errMsg);
    }
    return fd;
}

} // namespace utils::detail

namespace utils {

void format_log_line(LogLevel level, std::string_view fmt) {
    // Every thread can hold a reference to LogServer
    thread_local static auto gLogServer = detail::LogServer::getInstance();
    gLogServer->write(level, fmt);
}

void printf_log_line(LogLevel level, const char *format, ...) {
    THROW_RUNTIME_ERR("Not support for printf_log_line now!");
}

} // namespace utils
