#pragma once

#include "utils.h"
#include <functional>
#include <memory>

namespace utils {

using ChannelCbType = std::function<void (int)>;

// The lifetime of EventLoop must be longer than Channel.
class EventLoop;

// The Channl not hold the file descriptor, it just record it. So you need manage the lifetime
// of file descriptor yourself!
class Channel {
public:
    DISABLE_COPY(Channel);
    DISABLE_MOVE(Channel);

    static auto createChannel(const int fd, EventLoop* loop) {
        return std::unique_ptr<Channel>(new Channel(fd, loop));
    }

    ~Channel();

    void handleEvent(int revents);

    void setWriteCallback(ChannelCbType&& cb);

    void setReadCallback(ChannelCbType&& cb);

    void setErrorCallback(ChannelCbType&& cb);

    void setCloseCallback(ChannelCbType&& cb);

    [[nodiscard]]
    bool needWriteCallback() const;

    [[nodiscard]]
    bool needReadCallback() const;

    [[nodiscard]]
    bool needErrorCallback() const;

    [[nodiscard]]
    bool needCloseCallback() const { return mNeedCloseCb; }

    [[nodiscard]]
    int getFd() const { return mFd; }

    [[nodiscard]]
    uint32_t getEvent() const { return mEvent; }

private:
    Channel(const int fd, EventLoop* loop);

    ChannelCbType   mReadCb;
    ChannelCbType   mWriteCb;
    ChannelCbType   mErrorCb;
    ChannelCbType   mCloseCb;

    EventLoop*      mpEventLoop;
    bool            mNeedCloseCb;
    uint32_t        mEvent;
    int             mFd;

};

}
