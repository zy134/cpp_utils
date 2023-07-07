#pragma once
#include "utils.h"

#include <functional>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <vector>

namespace utils {

class Channel;

class EventLoop {
public:
    DISABLE_COPY(EventLoop);
    DISABLE_MOVE(EventLoop);

    static EventLoop& getEventLoop() {
        thread_local static EventLoop tEventLoop {};
        return tEventLoop;
    }

    ~EventLoop();

    void registerChannel(Channel* channel);

    void removeChannel(Channel* channel);

    void updateChannel(Channel* channel);

    void startLoop();

    void quitLoop();

    void queueInLoop(std::function<void()>&&);

    void runAfter(std::function<void()>&&, uint32_t);

private:
    EventLoop();

    int                                             mEpollFd;
    int                                             mWakeupFd;
    bool                                            mIsLoopRunning;
    std::mutex                                      mMutex;
    std::unordered_set<Channel *>                   mChannelSet;
    std::unordered_map<int, Channel *>              mFdChannelMap;
    std::vector<std::function<void ()>>             mPendingTasks;
    std::unordered_map<int, std::function<void ()>> mTimerTasks;
    
    thread_local static int tCurrentLoopTid;

private:
    bool isInLoopThread();
    void assertInLoopThread();

    void wakeup();
    void handleWakeup();
};

};
