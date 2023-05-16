#include "Log.h"
#include "format.h"
#include "utils.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <array>
#include <exception>
#include <future>
#include <unistd.h>
#include <fcntl.h>

#define THROW_SYS_ERR(msg) throw std::system_error { errno, std::system_category(), msg };
#define THROW_RUNTIME_ERR(msg) throw std::runtime_error { msg };
#define THROW_NET_ERR(errNum, msg) throw std::runtime_error { std::string(msg) + gai_strerror(errNum) };

namespace utils::detail {

class LogBuffer {
    DISABLE_COPY(LogBuffer);

public:
    LogBuffer() {
        mpRawBuffer = std::make_unique<std::array<char, 4096>>();
        mUsedSize = 0;
    }

    LogBuffer(LogBuffer&& rhs) noexcept {
        mpRawBuffer = std::move(rhs.mpRawBuffer);
        mUsedSize = rhs.mUsedSize;
    }

    ~LogBuffer() = default;

    bool writable(size_t size) {
        return (mpRawBuffer->size() - mUsedSize) >= size;
    }

    void write(const char* srcData, size_t size) {
        if (!writable(size))
            THROW_RUNTIME_ERR("Write failed because data is too big");
        ::memcpy(mpRawBuffer->data() + mUsedSize, srcData, size);
        mUsedSize += size;
    }

    void flushAsync(int fd) {
        mFlushResult = std::async(std::launch::async, [&] {
            int res = mUsedSize;
            while (res != 0) {
                res = ::write(fd, mpRawBuffer->data(), mUsedSize);
                if (res < 0) {
                    THROW_SYS_ERR("Write error");
                }
            }
            res = ::fsync(fd);
            mUsedSize = 0;
            return res;
        });
    }

    void waitFlush() {
        if (mFlushResult.valid())
            mFlushResult.wait();
    }

private:
    std::unique_ptr<std::array<char, 4096>>  mpRawBuffer;
    size_t              mUsedSize;
    std::future<int>    mFlushResult;
};

class LogServer {
    DISABLE_COPY(LogServer);
    DISABLE_MOVE(LogServer);
public:
    std::shared_ptr<LogServer> getInstance();
    LogServer(std::string_view filePath);
    ~LogServer();

    void write_info(std::string_view fmt);
    void write_err(std::string_view fmt);
private:
    void flush_normal_buffer(LogBuffer& buffer);
    void flush_hi_buffer(LogBuffer& buffer);

    int mLogFd;
};

LogServer::LogServer(std::string_view filePath) {
    mLogFd = ::open(filePath.data(), O_CREAT | O_RDWR);
    if (mLogFd < 0) {
        std::string errMsg = "Can't open path:";
        errMsg.append(filePath);
        THROW_SYS_ERR(errMsg);
    }
}

LogServer::~LogServer() {
    close(mLogFd);
}

auto LogServer::getInstance() -> std::shared_ptr<LogServer> {
    static std::mutex gInstanceMutex;
    static std::weak_ptr<LogServer> gInstanceWp;
    constexpr std::string_view DEFAULT_PATH = "/home/zy134/test/log.txt";

    std::lock_guard lock { gInstanceMutex };
    auto instance = gInstanceWp.lock();
    if (instance == nullptr) {
        try {
            instance = std::make_shared<LogServer>(DEFAULT_PATH);
        } catch(...) {
            throw;
        }
        gInstanceWp = instance;
    }
    return instance;
}

} // namespace utils::detail
