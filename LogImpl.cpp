#include "LogImpl.h"
#include <algorithm>
#include <iostream>

LogBuffer::LogBuffer() = default;

LogBuffer::~LogBuffer() = default;

LogBuffer::LogBuffer(LogBuffer&& rhs) noexcept {
    using std::swap;
    swap(*this, rhs);
}

LogBuffer& LogBuffer::operator=(LogBuffer&& rhs) noexcept {
    using std::swap;
    swap(*this, rhs);
    return *this;
}

LogBuffer& LogBuffer::swap(LogBuffer&& rhs) noexcept {
    using std::swap;
    swap(this->mSize, rhs.mSize);
    swap(this->mRawBuffer, rhs.mRawBuffer);
    return *this;
}

void LogBuffer::allocate() {
    try {
        mRawBuffer = std::make_unique<RawBufferType>();
    }  catch (...) {
        // print error...
        throw;
    }
}

void LogBuffer::write(std::string_view fmt) noexcept {
    auto raw_ptr = mRawBuffer.get()->data();
    int unused_size = DEFAULT_PAGE_SIZE - size();
    if (unused_size > 0 && unused_size >= fmt.size()) {
        // write string to buffer, it must success
        std::uninitialized_copy(fmt.begin(), fmt.end(), raw_ptr + mSize);
        mSize -= fmt.size();
    }
}

void LogBuffer::flush() {

}

LogImpl::LogImpl(std::string_view path) {
    mLogPath = path;
    mStop = false;
    mCurrentBuffer = LogBuffer();
    mHighPriorityBuffer = LogBuffer();
}

LogImpl::~LogImpl() {
    if (mAutoFlushThread.joinable())
        mAutoFlushThread.join();
}

void LogImpl::start() {
    // initialize with one normal free buffer, one high provity buffer
    // mFreeBuffers = { 2, LogBuffer() };
    mCurrentBuffer.allocate();
    mHighPriorityBuffer.allocate();
    mAutoFlushThread = std::thread ([&] {
        for ( ; ; ) {
            std::unique_lock lock { mMutex };
            mCond.wait_for(lock, DEFAULT_FLUSH_INTERVAL, [&] {
                return mFullBuffers.size() != 0 || !mHighPriorityBuffer.empty() || mStop;
            });
            if (mStop)
                return;
            // first, flush high priority buffer
            if (!mHighPriorityBuffer.empty()) {
                mHighPriorityBuffer.flush();
            }
            for (auto it = mFullBuffers.begin(); it != mFullBuffers.end(); it = mFullBuffers.erase(it)) {
                it->flush();
                mFreeBuffers.emplace_back(std::move(*it));
            }
        };
    });
}

void LogImpl::format(std::string_view fmt) {
    std::lock_guard lock { mMutex };
    if (mStop)
        return ;
    auto unused_size = DEFAULT_PAGE_SIZE - mCurrentBuffer.size();
    if (unused_size >= fmt.size()) {
        mFullBuffers.emplace_back(std::move(mCurrentBuffer));
        if (mFreeBuffers.empty()) {
            mFreeBuffers.emplace_back(LogBuffer());
        }

        mCurrentBuffer = std::move(mFreeBuffers.back());
        mFreeBuffers.pop_back();
        mCond.notify_one();
    }
    mCurrentBuffer.write(fmt);
}

void LogImpl::stop() {
    std::lock_guard lock { mMutex };
    mStop = true;
    mCond.notify_all();
}
