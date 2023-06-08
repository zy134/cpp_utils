#include "Log.h"
#include "format.h"
#include "utils.h"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <ratio>
#include <string>
#include <string_view>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <array>
#include <exception>
#include <future>
#include <tuple>
#include <unistd.h>
#include <fcntl.h>
#include <chrono>
#include <vector>

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

    void write(std::string_view fmt);
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
    mLogFd = ::open(filePath.data(), O_CREAT | O_RDWR, 0666);
    if (mLogFd < 0) {
        std::string errMsg = "Can't open path:";
        errMsg.append(filePath);
        THROW_SYS_ERR(errMsg);
    }
    mStopThread = false;
    mpCurrentBuffer = std::make_unique<LogBuffer>();
    mFlushThread = std::thread([&] {
        std::vector<std::unique_ptr<LogBuffer>> needFlushBuffers;
        while (!mStopThread) {
            // Get pending buffers.
            {
                std::unique_lock lock { mMutex };
                auto res = mCond.wait_for(lock, milliseconds(1000));
                if (res == std::cv_status::timeout && mpCurrentBuffer->flushEnable()) {
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
            {
                std::unique_lock lock { mMutex };
                for (auto& buffer: needFlushBuffers) {
                    mvAvailbleBuffers.emplace_back(std::move(buffer));
                }
                needFlushBuffers.clear();
            }
        }
    });
}

LogServer::~LogServer() {
    std::lock_guard lock { mMutex };
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

void LogServer::write(std::string_view fmt) {
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
        mCond.notify_all();
    }
}

} // namespace utils::detail

namespace utils {

void write_log_line(std::string_view fmt) {
    static auto gLogServer = detail::LogServer::getInstance();
    gLogServer->write(fmt);
}

} // namespace utils
