#include "Log.h"
#include "format.h"
#include "utils.h"
#include <chrono>
#include <bits/types/struct_timeval.h>
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
constexpr std::string_view DEFAULT_PATH = "/home/zy134/test/log.txt";

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

private:
    std::array<char, LOG_BUFFER_SIZE>   mRawBuffer;
    size_t                              mUsedSize;
};

class LogServer {
    DISABLE_COPY(LogServer);
    DISABLE_MOVE(LogServer);
public:
    static std::shared_ptr<LogServer> getInstance();
    LogServer(std::string_view filePath);
    ~LogServer();

    void write(const std::string& fmt);
private:

    int                     mLogFd;
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

LogServer::LogServer(std::string_view filePath) {
    mLogFd = ::open(filePath.data(), O_CREAT | O_RDWR | O_TRUNC, 0666);
    if (mLogFd < 0) {
        std::string errMsg = "Can't open path:";
        errMsg.append(filePath);
        THROW_SYS_ERR(errMsg);
    }
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
        instance = std::make_shared<LogServer>(DEFAULT_PATH);
        gInstanceWp = instance;
    }
    return instance;
}

void LogServer::write(const std::string& fmt) {
    std::lock_guard lock { mMutex };
    if (mpCurrentBuffer->writable(fmt.size())) {
        mpCurrentBuffer->write(fmt.data(), fmt.size());
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
        mpCurrentBuffer->write(fmt.data(), fmt.size());
        // Notify backend server to flush pending buffers.
        mCond.notify_one();
    }
}

} // namespace utils::detail

namespace utils {

constexpr auto log_level_to_string(LogLevel level) {
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
}

constexpr size_t LOG_MAX_LINE_SIZE = 256;
constexpr size_t LOG_MAX_FILE_SIZE = 1024 * 1024 * 1024;  // 1GB

void format_log_line(LogLevel level, const std::string& fmt) {
    // Every thread can hold a reference to LogServer
    thread_local static auto gLogServer = detail::LogServer::getInstance();
    thread_local pid_t pid = getpid();
    thread_local pid_t tid = gettid();
#if 0
// It's too complexed, so I deprecated it...
//#if __cplusplus >= 201907L
    const std::chrono::zoned_time cur_time{ std::chrono::current_zone(), std::chrono::system_clock::now() };
    std::stringstream ss {};
    ss << cur_time << " " << log_level_to_string(level) << " "
        << getpid() << " " << gettid() << " " << fmt << "\n\0";
    gLogServer->write(ss.str());
#else
    std::array<char, LOG_MAX_LINE_SIZE> buffer = {};
    time_t t = time(nullptr);
    struct tm now = {};
    // Get local time, it's not a system call.
    auto res = localtime_r(&t, &now);
    if (res == nullptr)
        THROW_SYS_ERR("Can't get current time.");

    // Get milliseconds suffix
    timeval tv = {};
    gettimeofday(&tv, nullptr);

    // Format log line.
    snprintf(buffer.data(), buffer.size(), "%04d-%02d-%02d %02d.%02d.%02d.%03d %5d %5d [%s] %s\n"
            , now.tm_year + 1900, now.tm_mon + 1, now.tm_mday
            , now.tm_hour, now.tm_min, now.tm_sec
            , static_cast<int>(tv.tv_usec)
            , pid, tid
            , log_level_to_string(level)
            , fmt.c_str());

    gLogServer->write(buffer.data());
#endif
}

void printf_log_line(LogLevel level, const char *format, ...) {
    THROW_RUNTIME_ERR("Not support for printf_log_line now!");
}

} // namespace utils
