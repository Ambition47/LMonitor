#ifndef LMONITOR_EVENT_LOOP_H
#define LMONITOR_EVENT_LOOP_H

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include <sys/epoll.h>


class Channel;


class EventLoop {
public:
    using Functor =
        std::function<void()>;


    explicit EventLoop(
        int maxEvents = 64
    );

    ~EventLoop();


    EventLoop(
        const EventLoop&
    ) = delete;

    EventLoop& operator=(
        const EventLoop&
    ) = delete;


    void addChannel(
        Channel* channel
    );


    void updateChannel(
        Channel* channel
    );


    void removeChannel(
        Channel* channel
    );


    // Run complete Reactor loop.
    void loop();


    // Request Reactor shutdown.
    // This can wake epoll_wait().
    void quit();


    // Run one epoll iteration.
    void loopOnce(
        int timeoutMilliseconds = -1
    );


    // Queue a task for EventLoop.
    //
    // This function is now protected by a mutex
    // and wakes the EventLoop through eventfd.
    void queueInLoop(
        Functor functor
    );


private:
    void doPendingFunctors();

    void wakeup();

    void handleWakeup();


private:
    int epollFd_ = -1;

    std::vector<epoll_event> events_;


    // ========================================================
    // EventLoop lifecycle
    // ========================================================

    bool looping_ = false;

    std::atomic<bool> quit_ {
        false
    };


    // ========================================================
    // Pending functors
    // ========================================================

    std::mutex pendingFunctorsMutex_;

    std::vector<Functor>
        pendingFunctors_;


    // ========================================================
    // eventfd wakeup mechanism
    // ========================================================

    int wakeupFd_ = -1;

    std::unique_ptr<Channel>
        wakeupChannel_;
};

#endif
