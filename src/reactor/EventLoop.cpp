#include "reactor/EventLoop.h"

#include "reactor/Channel.h"

#include <cerrno>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

#include <unistd.h>


// ============================================================
// Constructor
// ============================================================

EventLoop::EventLoop(
    int maxEvents
) {
    if (maxEvents <= 0) {
        throw std::invalid_argument(
            "maxEvents must be greater than 0"
        );
    }


    epollFd_ =
        epoll_create1(
            EPOLL_CLOEXEC
        );


    if (epollFd_ < 0) {
        throw std::runtime_error(
            "Failed to create epoll instance"
        );
    }


    events_.resize(
        static_cast<std::size_t>(
            maxEvents
        )
    );
}


// ============================================================
// Destructor
// ============================================================

EventLoop::~EventLoop() {
    if (epollFd_ >= 0) {

        close(
            epollFd_
        );

        epollFd_ = -1;
    }
}


// ============================================================
// Add Channel
// ============================================================

void EventLoop::addChannel(
    Channel* channel
) {
    if (channel == nullptr) {
        throw std::invalid_argument(
            "Channel must not be null"
        );
    }


    epoll_event event {};

    event.events =
        channel->events();

    event.data.ptr =
        channel;


    if (epoll_ctl(
            epollFd_,
            EPOLL_CTL_ADD,
            channel->fd(),
            &event
        ) < 0) {

        throw std::runtime_error(
            "Failed to add Channel to epoll"
        );
    }
}


// ============================================================
// Update Channel
// ============================================================

void EventLoop::updateChannel(
    Channel* channel
) {
    if (channel == nullptr) {
        throw std::invalid_argument(
            "Channel must not be null"
        );
    }


    epoll_event event {};

    event.events =
        channel->events();

    event.data.ptr =
        channel;


    if (epoll_ctl(
            epollFd_,
            EPOLL_CTL_MOD,
            channel->fd(),
            &event
        ) < 0) {

        throw std::runtime_error(
            "Failed to update Channel in epoll"
        );
    }
}


// ============================================================
// Remove Channel
// ============================================================

void EventLoop::removeChannel(
    Channel* channel
) {
    if (channel == nullptr) {
        return;
    }


    if (epoll_ctl(
            epollFd_,
            EPOLL_CTL_DEL,
            channel->fd(),
            nullptr
        ) < 0) {

        if (errno == ENOENT) {
            return;
        }


        throw std::runtime_error(
            "Failed to remove Channel from epoll"
        );
    }
}


// ============================================================
// Main Reactor loop
// ============================================================

void EventLoop::loop() {
    if (looping_) {
        throw std::runtime_error(
            "EventLoop is already running"
        );
    }


    looping_ =
        true;

    quit_ =
        false;


    while (!quit_) {

        loopOnce();
    }


    looping_ =
        false;
}


// ============================================================
// Request shutdown
// ============================================================

void EventLoop::quit() {
    quit_ =
        true;
}


// ============================================================
// Queue deferred task
// ============================================================

void EventLoop::queueInLoop(
    Functor functor
) {
    if (!functor) {
        return;
    }


    pendingFunctors_.push_back(
        std::move(
            functor
        )
    );
}


// ============================================================
// Execute deferred tasks
// ============================================================

void EventLoop::doPendingFunctors() {

    // 非常重要：
    //
    // 先把当前待执行任务移动到临时 vector。
    //
    // 因为某个 functor 在执行过程中，
    // 可能再次调用 queueInLoop()。
    //
    // 新加入的任务应该留到下一轮执行，
    // 而不是修改当前正在遍历的 vector。

    std::vector<Functor>
        functors;


    functors.swap(
        pendingFunctors_
    );


    for (auto& functor :
         functors) {

        if (functor) {
            functor();
        }
    }
}


// ============================================================
// Run one epoll iteration
// ============================================================

void EventLoop::loopOnce(
    int timeoutMilliseconds
) {
    int readyCount = 0;


    while (true) {

        readyCount =
            epoll_wait(
                epollFd_,
                events_.data(),
                static_cast<int>(
                    events_.size()
                ),
                timeoutMilliseconds
            );


        if (readyCount >= 0) {
            break;
        }


        if (errno == EINTR) {
            continue;
        }


        throw std::runtime_error(
            "epoll_wait failed"
        );
    }


    // ========================================================
    // Dispatch events
    // ========================================================

    for (int i = 0;
         i < readyCount;
         ++i) {

        epoll_event& event =
            events_[
                static_cast<std::size_t>(
                    i
                )
            ];


        Channel* channel =
            static_cast<Channel*>(
                event.data.ptr
            );


        if (channel == nullptr) {
            continue;
        }


        channel->handleEvent(
            event.events
        );
    }


    // ========================================================
    // Important:
    //
    // 所有 Channel callback 全部处理完以后，
    // 才执行延迟任务。
    //
    // 因此这里可以安全进行类似：
    //
    // connections.erase(fd)
    //
    // 的操作。
    // ========================================================

    doPendingFunctors();
}
