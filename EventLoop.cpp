#include "EventLoop.h"
#include "Channel.h"
#include "utils.h"
#include "Log.h"
#include "Backtrace.h"

#include <array>
#include <bits/types/struct_itimerspec.h>
#include <cstdint>
#include <ctime>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

extern "C" {
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <sys/epoll.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
}

using namespace utils;
using namespace std;

// static check
static_assert(EPOLLIN == POLLIN,        "epoll uses same flag values as poll");
static_assert(EPOLLPRI == POLLPRI,      "epoll uses same flag values as poll");
static_assert(EPOLLOUT == POLLOUT,      "epoll uses same flag values as poll");
static_assert(EPOLLRDHUP == POLLRDHUP,  "epoll uses same flag values as poll");
static_assert(EPOLLERR == POLLERR,      "epoll uses same flag values as poll");
static_assert(EPOLLHUP == POLLHUP,      "epoll uses same flag values as poll");

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
/********************** EventLoop ***************************/
/************************************************************/
thread_local int EventLoop::tCurrentLoopTid = -1;

EventLoop::EventLoop() {
    assertTrue(tCurrentLoopTid == -1, "Every thread can hold only one event loop!");

    tCurrentLoopTid = ::gettid();

    mEpollFd = ::epoll_create(10);
    if (mEpollFd < 0) {
        THROW_SYS_ERR("Can't create epoll fd!");
    }

    mWakeupFd = ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (mWakeupFd < 0) {
        THROW_SYS_ERR("Can't create event fd!");
    }
    epoll_event event = {};
    event.data.fd = mWakeupFd;
    event.events = EPOLLIN;
    if (auto res = ::epoll_ctl(mEpollFd, EPOLL_CTL_ADD, mWakeupFd, &event); res != 0) {
        THROW_SYS_ERR("Can't add event fd to epoll list!");
    }

    mIsLoopRunning = false;
    LOG_INFO("[EventLoop] %s", __FUNCTION__);
}

EventLoop::~EventLoop() {
    ::epoll_ctl(mEpollFd, EPOLL_CTL_DEL, mWakeupFd, nullptr);
    ::close(mWakeupFd);
    ::close(mEpollFd);
    tCurrentLoopTid = -1;
    LOG_INFO("[EventLoop] %s", __FUNCTION__);
}

void EventLoop::registerChannel(Channel *channel) {
    LOG_INFO("[EventLoop] %s: channel %p, fd %d", __FUNCTION__, channel, channel->getFd());

    assertInLoopThread();
    assertTrue(mChannelSet.count(channel) == 0, "This channel has been registered!");
    assertTrue(mFdChannelMap.count(channel->getFd()) == 0, "This channel has been registered!");

    epoll_event event {};
    event.data.fd = channel->getFd();
    auto res = ::epoll_ctl(mEpollFd, EPOLL_CTL_ADD, channel->getFd(), &event);
    if (res < 0)
        THROW_SYS_ERR("Failed to add new fd to epoll list.");
    mChannelSet.insert(channel);
    mFdChannelMap.insert({ channel->getFd(), channel });
}

void EventLoop::removeChannel(Channel *channel) {
    LOG_INFO("[EventLoop] %s: channel %p, fd %d", __FUNCTION__, channel, channel->getFd());

    assertInLoopThread();
    assertTrue(mChannelSet.count(channel) != 0, "This channel is not registered! Can't remove it.");
    assertTrue(mFdChannelMap.count(channel->getFd()) != 0, "This channel is not registered! Can't remove it.");

    mFdChannelMap.erase(channel->getFd());
    mChannelSet.erase(channel);
    auto res = ::epoll_ctl(mEpollFd, EPOLL_CTL_DEL, channel->getFd(), nullptr);
    if (res < 0) {
        THROW_SYS_ERR("Failed to remove fd "s + std::to_string(channel->getFd()) + " from epoll list"s);
    }
}

void EventLoop::updateChannel(Channel *channel) {
    LOG_INFO("[EventLoop] %s: channel %p, fd %d", __FUNCTION__, channel, channel->getFd());

    assertInLoopThread();
    assertTrue(mChannelSet.count(channel) != 0, "This channel is not registered! Can't update it.");
    assertTrue(mFdChannelMap.count(channel->getFd()) != 0, "This channel is not registered! Can't update it.");

    epoll_event event {};
    event.data.fd = channel->getFd();
    event.events = channel->getEvent();
    auto res = ::epoll_ctl(mEpollFd, EPOLL_CTL_MOD, channel->getFd(), &event);
    if (res < 0)
        THROW_SYS_ERR("Failed to add new fd to epoll list.");
}

void EventLoop::startLoop() {
    LOG_INFO("[EventLoop] %s +", __FUNCTION__);
    mIsLoopRunning = true;
    std::array<epoll_event, 256> revents;
    while (mIsLoopRunning) {
        auto nfds = ::epoll_wait(mEpollFd, revents.data(), revents.size(), 5000);
        if (nfds > 0) {
            for (auto i = 0; i != nfds; ++i) {
                if (revents[i].data.fd == mWakeupFd) {
                    LOG_INFO("[EventLoop] %s: event loop has been wakeup", __FUNCTION__);
                    handleWakeup();
                    std::vector<std::function<void ()>> pendingTasks;
                    {
                        std::lock_guard lock { mMutex };
                        std::swap(pendingTasks, mPendingTasks);
                    }
                    for (auto&& task : pendingTasks)
                        task();
                } else if (mFdChannelMap.count(revents[i].data.fd) != 0) {
                    LOG_INFO("[EventLoop] %s: channel %d is active now", __FUNCTION__, revents[i].events);
                    auto channel = mFdChannelMap.find(revents[i].data.fd)->second;
                    channel->handleEvent(revents[i].events);
                } else {
                    LOG_ERR("[EventLoop] Someting error happen! Fd %d has not been registered!", revents[i].data.fd);
                }
            }
        } else if (nfds == 0) {
            LOG_INFO("[EventLoop] %s: wait time out!", __FUNCTION__);
        } else {
            THROW_SYS_ERR("EventLoop wait error!");
        }
    }
    LOG_INFO("[EventLoop] %s -", __FUNCTION__);
}

void EventLoop::quitLoop() {
    LOG_INFO("[EventLoop] %s", __FUNCTION__);
    assertInLoopThread();
    mIsLoopRunning = false;
    if (!isInLoopThread())
        wakeup();
}

void EventLoop::queueInLoop(std::function<void ()>&& functor) {
    if (isInLoopThread()) {
        mPendingTasks.push_back(std::move(functor));
    } else {
        std::lock_guard lock { mMutex };
        mPendingTasks.push_back(std::move(functor));
        wakeup();
    }
}

void EventLoop::runAfter(std::function<void ()>&& functor, uint32_t millis) {
    auto runAfterUnlock = [&] {
        auto newTimerFd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
        if (newTimerFd < 0) {
            LOG_ERR("[EventLoop] %s: Can't create timerfd because %s!", __FUNCTION__, strerror(newTimerFd));
            return ;
        }
        itimerspec newTimer;
        newTimer.it_interval.tv_nsec = millis;
        if (auto res = ::timerfd_settime(newTimerFd, 0, &newTimer, nullptr); res != 0) {
            LOG_ERR("[EventLoop] %s: Can't set timerfd because %s!", __FUNCTION__, strerror(res));
            close(newTimerFd);
            return ;
        }
        auto channel = Channel::createChannel(newTimerFd, this);
        channel->setReadCallback([functor = std::move(functor)] (int fd) {
            functor();
        });
        LOG_INFO("[EventLoop] Add new timerfd %d", newTimerFd);
    };
    queueInLoop(std::move(runAfterUnlock));
}

bool EventLoop::isInLoopThread() {
    thread_local static int tCachedCurrentTid = -1;
    if (tCachedCurrentTid < 0) {
        tCachedCurrentTid = ::gettid();
    }
    return (tCachedCurrentTid == tCurrentLoopTid);
}

void EventLoop::assertInLoopThread() {
    if (!isInLoopThread()) {
#ifdef DEBUG_BUILD
        PrintBacktrace();
#endif
        THROW_RUNTIME_ERR("AssertInLoopThread failed!");
    }
}

void EventLoop::wakeup() {
    LOG_INFO("[EventLoop] %s", __FUNCTION__);
    uint64_t one = 1;
    ssize_t n = ::write(mWakeupFd, &one, sizeof one);
    if (n != sizeof one) {
        LOG_ERR("[EventLoop] wake uperror!");
    }
}

void EventLoop::handleWakeup() {
    LOG_INFO("[EventLoop] %s", __FUNCTION__);
    uint64_t one = 1;
    ssize_t n = ::read(mWakeupFd, &one, sizeof one);
    if (n != sizeof one) {
        LOG_ERR("[EventLoop] handle wakeup error!");
    }
}

