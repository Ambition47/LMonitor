#include "reactor/EventLoop.h"

#include "reactor/Channel.h"

#include <cerrno>
#include <cstddef>
#include <stdexcept>

#include <unistd.h>


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


    // 这里不再保存 fd，
    // 而是直接保存 Channel 指针。
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

void EventLoop::loop() {
    if (looping_) {
        throw std::runtime_error(
            "EventLoop is already running"
        );
    }

    looping_ = true;
    quit_ = false;

    while (!quit_) {
        loopOnce();
    }

    looping_ = false;
}


void EventLoop::quit() {
    quit_ = true;
}





// ============================================================
// Wait once and dispatch events
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
    // Dispatch ready events directly to Channel
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
}
