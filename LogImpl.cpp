#include "Log.h"
#include "utils.h"
#include "Error.h"
#include "Backtrace.h"

#include <algorithm>
#include <cstddef>
#include <exception>
#include <iostream>
#include <memory>
#include <string_view>
#include <sstream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <array>
#include <future>
#include <chrono>
#include <vector>
#include <fstream>
#include <filesystem>

#ifdef TAG
#undef TAG
#endif
static constexpr std::string_view TAG = "LOG";

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

namespace utils::detail {

// TODO: equals to page size
constexpr size_t LOG_BUFFER_SIZE = 4096;

using namespace std::chrono;
using namespace std::chrono_literals;

// LogBuffer is a simple memory buffer wrapper.
// Not thread-safety!
class LogBuffer {
    DISABLE_COPY(LogBuffer);
    DISABLE_MOVE(LogBuffer);

public:
    LogBuffer() {
        mUsedSize = 0;
    }

    ~LogBuffer() {}

    [[nodiscard]]
    bool writable(size_t size) const {
        return (mRawBuffer.size() - mUsedSize) >= size;
    }

    void write(const char* srcData, size_t size) {
        // std::cout << "start write size:" << size << std::endl;
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
    LogServer();

    ~LogServer();

    // Call by LOG_FATAL, force flush all log files and terminate process.
    // Thread-safety.
    void forceDestroy() noexcept;

    // Call by LOG_ERR, force flush current log buffer.
    // Thread-safety.
    void forceFlush() noexcept;

    // Client would call this funtion to format log line and write it to LogBuffer.
    // Thread-safety.
    void write(LogLevel level, std::string_view fmt, std::string_view tag);

private:
    auto createLogFileStream() -> std::fstream;

    void doFlushAsync();

    // Default interval time which LogServer would flush all current buffer to log file.
    static constexpr auto DEFAULT_FLUSH_INTERVAL = 2000ms;

    std::fstream            mLogFileStream;
    uint32_t                mLogAlreadyWritenBytes;

    std::mutex              mMutex;
    std::condition_variable mCond;
    std::thread             mFlushThread;
    bool                    mStopThread;
    bool                    mNeedFlushNow;
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
    mNeedFlushNow = false;
    mpCurrentBuffer = std::make_unique<LogBuffer>();
    mFlushThread = std::thread([this] {
        doFlushAsync();
    });
}

LogServer::~LogServer() {
    forceDestroy();
}

void LogServer::forceDestroy() noexcept {
    try {
        // std::lock_guard lock { mMutex };
        // Notify flush thread to syncronize log buffer and close log file.
        mStopThread = true;
        mCond.notify_one();
        // Destroy flush thread.
        if (mFlushThread.joinable()) {
            mFlushThread.join();
        }
    } catch (...) {
        // ignore exception
    }
}

void LogServer::forceFlush() noexcept {
    try {
        // std::lock_guard lock { mMutex };
        mNeedFlushNow = true;
        mCond.notify_one();
    } catch (...) {
        // ignore exception.
    }
}

void LogServer::write(LogLevel level, std::string_view fmt, std::string_view tag) {
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
            case LogLevel::Fatal:
                return "Fatal";
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
    // TODO: Deal with this exception.
    if (localtime_r(&t, &now) == nullptr) {
        throw SystemException("Can't get current time.");
    }

    // Get milliseconds suffix
    auto timeSinceEpoch = std::chrono::system_clock::now().time_since_epoch().count();
    auto milliSeconds = static_cast<int>(timeSinceEpoch % (1000 * 1000 * 1000)) / 1000;

    // Format log line.
    int logLineLength = snprintf(logLine.data(), logLine.size(), "%04d-%02d-%02d %02d.%02d.%02d.%06d %5d %5d [%s][%s] %s\n"
            , now.tm_year + 1900, now.tm_mon + 1, now.tm_mday
            , now.tm_hour, now.tm_min, now.tm_sec
            , milliSeconds
            , pid, tid
            , log_level_to_string(level)
            , tag.data()
            , fmt.data());
    // TODO: Deal with this exception.
    if (logLineLength < 0) {
        throw SystemException("Error happen when format string.");
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

void LogServer::doFlushAsync() {
    std::vector<std::unique_ptr<LogBuffer>> needFlushBuffers;
    while (!mStopThread) {
        // Get pending buffers.
        {
            std::unique_lock lock { mMutex };
            // Wait for availble pending buffers.
            auto timeout = !mCond.wait_for(lock, DEFAULT_FLUSH_INTERVAL, [&] {
                return mStopThread || !mvPendingBuffers.empty() || mNeedFlushNow;
            });
            // Flush thread is exited, so flush all buffers, and then close log file.
            if (mStopThread) {
                for (auto& buffer: mvPendingBuffers) {
                    buffer->flush(mLogFileStream);
                }
                if (mpCurrentBuffer->flushEnable()) {
                    mpCurrentBuffer->flush(mLogFileStream);
                }
                mLogFileStream.flush();
                mLogFileStream.close();
                return ;
            }
            // Condition wait time out, there has no pending buffers, so flush current buffer to log file.
            // mNeedFlushNow == true, so need to flush current buffer immediately.
            if (timeout || mNeedFlushNow) {
                mvPendingBuffers.emplace_back(std::move(mpCurrentBuffer));
                if (mvAvailbleBuffers.empty()) {
                    mpCurrentBuffer = std::make_unique<LogBuffer>();
                } else {
                    mpCurrentBuffer = std::move(mvAvailbleBuffers.back());
                    mvAvailbleBuffers.pop_back();
                }
                // set mNeedFlushNow as false.
                [[unlikely]]
                if (mNeedFlushNow) {
                    mNeedFlushNow = false;
                }
            }
            needFlushBuffers.swap(mvPendingBuffers);
        }
        // Start flush buffer to log file.
        // This operation may take long time, so don't lock mutex now.
        for (auto& buffer: needFlushBuffers) {
            if (buffer->size() + mLogAlreadyWritenBytes >= LOG_MAX_FILE_SIZE) {
                // std::cout << __FUNCTION__ << ": Log file is full, create new log file." << std::endl;
                mLogFileStream.flush();
                mLogFileStream.close();
                mLogFileStream = createLogFileStream();
                mLogAlreadyWritenBytes = 0;
            }
            mLogAlreadyWritenBytes += buffer->size();
            buffer->flush(mLogFileStream);
        }
        // Return availble buffers.
        // std::cout << __FUNCTION__ << ": Return buffers" << std::endl;
        {
            std::lock_guard lock { mMutex };
            for (auto& buffer: needFlushBuffers) {
                mvAvailbleBuffers.emplace_back(std::move(buffer));
            }
            needFlushBuffers.clear();
        }
    }

}

std::fstream LogServer::createLogFileStream() {
    // Create log path and change its permissions to 0777.
    // TODO: Permission error may be happen, how to deal with that case?
    try {
        // create_directory() return false because the directory is existed, ignored it.
        // We just focus on the case which create_directory() or permissions() throw a exception
        std::filesystem::create_directory(DEFAULT_LOG_PATH);
        std::filesystem::permissions(DEFAULT_LOG_PATH, std::filesystem::perms::all);
    } catch (...) {
        throw SystemException("Can't create log filePath because:");
    }

    // Format the name of log file.
    std::array<char, 128> filePath = {};
    time_t t = time(nullptr);
    struct tm now = {};
    // TODO: Deal with this exception.
    if (localtime_r(&t, &now) == nullptr) {
        throw SystemException("Can't get current time.");
    }
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
        throw SystemException("Can't create log file because:");
    }
}

void format_log_line(LogLevel level, std::string_view fmt, std::string_view tag) noexcept {
    // Every thread can hold only one instance of LogServer
    // Besides, every instance of LogServer would create a new thread as asynchronously-flush thread.
    // TODO: Reduce the use of threads.
    // thread_local static LogServer gLogServer {};
    static LogServer gLogServer {};
    try {
        gLogServer.write(level, fmt, tag);
        // For fatal case, global dtor would not be invoked, so call the destructor manually to flush log file.
        [[unlikely]]
        if (level == LogLevel::Fatal) {
            auto backtraces = getBacktrace();
            for (size_t i = 1; i < backtraces.size(); ++i) {
                gLogServer.write(level, backtraces[i].c_str(), "Backtrace");
            }
            gLogServer.forceDestroy();
            std::terminate();
        }
        // For error case, need to flush buffer to log file immediately.
        [[unlikely]]
        if (level == LogLevel::Error) {
            gLogServer.forceFlush();
        }
    } catch(const std::exception& e) {
        // Focus on three type of exception:
        // 1. Memeoy out of use: We can't handle this exception, make process abort to notify kernel watchdog!
        // 2. Insufficient disk capacity: We can't handle this exception, make process abort to notify kernel watchdog!
        // 3. Permission error: We can't handle this exception, give user more infomation and then let the process abort!
        // For other exception, see it as bug and need to fix it.
        std::cerr << e.what() << std::endl;
        std::terminate();
    }
}

} // namespace utils::detail

namespace utils {

void assertTrue(bool cond, std::string_view msg) {
//#ifdef DEBUG_BUILD
if (!cond) {
    printBacktrace();
    LOG_FATAL("[ASSERT] assert error: %s\n", msg.data());
}
//#endif
}

void printBacktrace() {
    auto backtraces = getBacktrace();
    LOG_WARN("================================================================================");
    LOG_WARN("============================== Start print backtrace ===========================");
    for (size_t i = 1; i < backtraces.size(); ++i) {
        LOG_WARN(backtraces[i].c_str());
    }
    LOG_WARN("=============================== End print backtrace  ===========================");
    // Use LOG_ERR to flush current buffer to log file in last log line.
    LOG_ERR("================================================================================");
}

} // namespace utils
