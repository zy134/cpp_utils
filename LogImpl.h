#ifndef LOGIMPL_H
#define LOGIMPL_H
#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <string_view>
#include <mutex>
#include <condition_variable>
#include <array>
#include <chrono>
#include <fstream>

#define DISABLE_COPY(class_name) class_name(const class_name &) = delete; \
    class_name & operator=(const class_name &) = delete;

#define DISABLE_MOVE(class_name) class_name(class_name &&) = delete; \
    class_name & operator=(class_name &&) = delete;

constexpr auto DEFAULT_PAGE_SIZE = 4096;
constexpr auto DEFAULT_FLUSH_INTERVAL = std::chrono::milliseconds(10000);

// a cross-platform file block holder
// why dont' use std::fstream? because we don't need any internal cache for I/O operation of files
class FileHolder {
public:
    void flush();
};

// just a wrapper of raw buffer, don't offer any thread safety
class LogBuffer {
public:
    LogBuffer();
    ~LogBuffer();
    LogBuffer(LogBuffer&&) noexcept;
    LogBuffer& operator=(LogBuffer&&) noexcept;
    LogBuffer& swap(LogBuffer&&) noexcept;

    auto empty() noexcept { return mSize == 0; }
    auto size() noexcept { return mSize; }

    // allcate memory for buffer
    void allocate();
    // just write string to a pre-allocated buffer, this method never throw exception
    void write(std::string_view) noexcept;
    // flush raw buffer and make it empty
    void flush();

    DISABLE_COPY(LogBuffer);
private:
    using RawBufferType = std::array<char, DEFAULT_PAGE_SIZE>;

    std::unique_ptr<RawBufferType>  mRawBuffer;
    size_t                          mSize;
};

class LogImpl {
public:
    LogImpl(std::string_view path);
    ~LogImpl();

    void start();
    void format(std::string_view fmt);
    void stop();

    DISABLE_COPY(LogImpl);
    DISABLE_MOVE(LogImpl);
private:
    std::string_view        mLogPath;
    bool                    mStop;
    std::mutex              mMutex;
    std::condition_variable mCond;
    std::thread             mAutoFlushThread;
    std::vector<LogBuffer>  mFullBuffers;
    std::vector<LogBuffer>  mFreeBuffers;
    LogBuffer               mCurrentBuffer;
    LogBuffer               mHighPriorityBuffer;
};

#endif // LOGIMPL_H
