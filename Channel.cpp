#include "Channel.h"
#include "EventLoop.h"
#include "Log.h"

#include <array>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

extern "C" {
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
}

using namespace utils;
using namespace std;

static void assertTrue(bool cond, std::string_view msg) {
    if (!cond) {
        std::string errMsg = "[ASSERT] ";
        errMsg.append(msg);
        LOG_ERR("[ASSERT] %s", errMsg.c_str());
#ifdef DEBUG_BUILD
        PrintBacktrace();
#endif
        THROW_RUNTIME_ERR(errMsg);
    }
}

/************************************************************/
/*********************** Channel ****************************/
/************************************************************/

Channel::Channel(const int fd, EventLoop* loop) {
    LOG_INFO("[EventLoop] %s", __FUNCTION__);
    assertTrue(fd >= 0, "File descriptor must be valid!");

    mFd = fd;
    if (auto res = fcntl(mFd, F_SETFL, O_NONBLOCK); res != 0) {
        THROW_SYS_ERR("File descriptor must be non-blocking!");
    }

    mEvent = 0;
    mpEventLoop = loop;

    loop->registerChannel(this);
}

Channel::~Channel() {
    LOG_INFO("[EventLoop] %s", __FUNCTION__);
    mpEventLoop->removeChannel(this);
}

void Channel::handleEvent(int revents) {
    if ((revents & EPOLLHUP) && !(revents & EPOLLIN)) {
        if (needCloseCallback())
            mCloseCb(mFd);
        return ;
    }

    if (revents & EPOLLERR) {
        if (needErrorCallback())
            mErrorCb(mFd);
        return ;
    }

    if (revents & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)) {
        if (needReadCallback())
            mReadCb(mFd);
    }

    if (revents & EPOLLOUT) {
        if (needWriteCallback())
            mWriteCb(mFd);
    }
}

void Channel::setReadCallback(ChannelCbType &&cb) {
    mReadCb = std::move(cb);
    auto newEvent = mEvent | EPOLLIN;
    if (newEvent != mEvent) {
        mEvent = newEvent;
        mpEventLoop->updateChannel(this);
    }
}

void Channel::setWriteCallback(ChannelCbType &&cb) {
    mWriteCb = std::move(cb);
    auto newEvent = mEvent | EPOLLOUT;
    if (newEvent != mEvent) {
        mEvent = newEvent;
        mpEventLoop->updateChannel(this);
    }
}

void Channel::setErrorCallback(ChannelCbType &&cb) {
    mErrorCb = std::move(cb);
    auto newEvent = mEvent | EPOLLERR;
    if (newEvent != mEvent) {
        mEvent = newEvent;
        mpEventLoop->updateChannel(this);
    }
}

void Channel::setCloseCallback(ChannelCbType &&cb) {
    mCloseCb = std::move(cb);
    mpEventLoop->updateChannel(this);
}

bool Channel::needWriteCallback() const { return mEvent & EPOLLOUT; }

bool Channel::needReadCallback() const { return mEvent & EPOLLIN; }

bool Channel::needErrorCallback() const { return mEvent & EPOLLERR; }


