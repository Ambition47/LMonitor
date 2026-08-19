#include "reactor/EventLoop.h"

#include "log/Logger.h"
#include "reactor/Channel.h"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

#include <sys/eventfd.h>
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


    // --------------------------------------------------------
    // Create epoll instance
    // --------------------------------------------------------

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


    // --------------------------------------------------------
    // Create eventfd used to wake epoll_wait()
    // --------------------------------------------------------

    wakeupFd_ =
        eventfd(
            0,
            EFD_NONBLOCK |
            EFD_CLOEXEC
        );


    if (wakeupFd_ < 0) {

        close(
            epollFd_
        );

        epollFd_ = -1;


        throw std::runtime_error(
            "Failed to create EventLoop eventfd"
        );
    }


    try {

        wakeupChannel_ =
            std::make_unique<Channel>(
                wakeupFd_
            );


        wakeupChannel_->setEvents(
            EPOLLIN
        );


        wakeupChannel_->setReadCallback(
            [this]() {
                handleWakeup();
            }
        );


        addChannel(
            wakeupChannel_.get()
        );

    } catch (...) {

        wakeupChannel_.reset();


        close(
            wakeupFd_
        );

        wakeupFd_ = -1;


        close(
            epollFd_
        );

        epollFd_ = -1;


        throw;
    }
}


// ============================================================
// Destructor
// ============================================================

EventLoop::~EventLoop() {

    if (wakeupChannel_) {

        try {

            removeChannel(
                wakeupChannel_.get()
            );

        } catch (...) {
            // Destructor must not throw.
        }


        wakeupChannel_.reset();
    }


    if (wakeupFd_ >= 0) {

        close(
            wakeupFd_
        );

        wakeupFd_ = -1;
    }


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


    quit_.store(
        false
    );


    try {

        while (!quit_.load()) {

            loopOnce();
        }

    } catch (...) {

        looping_ =
            false;

        throw;
    }


    looping_ =
        false;
}


// ============================================================
// Request shutdown
// ============================================================

void EventLoop::quit() {

    quit_.store(
        true
    );


    wakeup();
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


    {
        std::lock_guard<std::mutex>
            lock(
                pendingFunctorsMutex_
            );


        pendingFunctors_.push_back(
            std::move(
                functor
            )
        );
    }


    wakeup();
}


// ============================================================
// Wake EventLoop through eventfd
// ============================================================

void EventLoop::wakeup() {

    const uint64_t value =
        1;


    while (true) {

        const ssize_t bytesWritten =
            write(
                wakeupFd_,
                &value,
                sizeof(value)
            );


        if (bytesWritten ==
            static_cast<ssize_t>(
                sizeof(value)
            )) {

            return;
        }


        if (bytesWritten < 0 &&
            errno == EINTR) {

            continue;
        }


        if (bytesWritten < 0 &&
            errno == EAGAIN) {

            return;
        }


        Logger::instance().error(
            "Failed to wake EventLoop through eventfd"
        );

        return;
    }
}


// ============================================================
// Consume eventfd notification
// ============================================================

void EventLoop::handleWakeup() {

    uint64_t value =
        0;


    while (true) {

        const ssize_t bytesRead =
            read(
                wakeupFd_,
                &value,
                sizeof(value)
            );


        if (bytesRead ==
            static_cast<ssize_t>(
                sizeof(value)
            )) {

            return;
        }


        if (bytesRead < 0 &&
            errno == EINTR) {

            continue;
        }


        if (bytesRead < 0 &&
            errno == EAGAIN) {

            return;
        }


        Logger::instance().error(
            "Failed to read EventLoop eventfd"
        );

        return;
    }
}


// ============================================================
// Execute deferred tasks
// ============================================================

void EventLoop::doPendingFunctors() {

    std::vector<Functor>
        functors;


    {
        std::lock_guard<std::mutex>
            lock(
                pendingFunctorsMutex_
            );


        functors.swap(
            pendingFunctors_
        );
    }


    for (auto& functor :
         functors) {

        if (functor) {

            try {

                functor();

            } catch (
                const std::exception& e
            ) {

                Logger::instance().error(
                    "EventLoop pending functor exception: " +
                    std::string(
                        e.what()
                    )
                );

            } catch (...) {

                Logger::instance().error(
                    "EventLoop pending functor threw unknown exception"
                );
            }
        }
    }
}


// ============================================================
// Run one epoll iteration
// ============================================================

void EventLoop::loopOnce(
    int timeoutMilliseconds
) {
    int readyCount =
        0;


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
    // Dispatch all ready Channels
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

            Logger::instance().warning(
                "EventLoop received an event with null Channel"
            );

            continue;
        }


        channel->handleEvent(
            event.events
        );
    }


    doPendingFunctors();
}
