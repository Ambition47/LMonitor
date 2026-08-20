#include "timer/TimerFd.h"

#include <cerrno>
#include <cstring>
#include <stdexcept>


#include <sys/timerfd.h>

#include <unistd.h>


// ============================================================
// Constructor
// ============================================================

TimerFd::TimerFd()
    :
    fd_(
        timerfd_create(
            CLOCK_MONOTONIC,
            TFD_NONBLOCK |
            TFD_CLOEXEC
        )
    )
{

    if (fd_ < 0) {

        throw std::runtime_error(
            "Failed to create timerfd: "
            +
            std::string(
                std::strerror(
                    errno
                )
            )
        );
    }
}



// ============================================================
// Destructor
// ============================================================

TimerFd::~TimerFd()
{

    if (fd_ >= 0) {

        close(
            fd_
        );
    }
}



// ============================================================
// File descriptor
// ============================================================

int TimerFd::fd() const noexcept
{
    return fd_;
}



// ============================================================
// Start periodic timer
// ============================================================

void TimerFd::start(
    uint32_t intervalSeconds
)
{

    if (intervalSeconds == 0) {

        throw std::invalid_argument(
            "Timer interval cannot be zero"
        );
    }


    itimerspec timerSpec {};



    timerSpec.it_value.tv_sec =
        intervalSeconds;



    timerSpec.it_interval.tv_sec =
        intervalSeconds;



    if (timerfd_settime(
            fd_,
            0,
            &timerSpec,
            nullptr
        ) < 0) {


        throw std::runtime_error(
            "Failed to start timerfd: "
            +
            std::string(
                std::strerror(
                    errno
                )
            )
        );
    }
}



// ============================================================
// Stop timer
// ============================================================

void TimerFd::stop()
{

    itimerspec timerSpec {};



    timerfd_settime(
        fd_,
        0,
        &timerSpec,
        nullptr
    );
}



// ============================================================
// Read expiration count
// ============================================================

uint64_t TimerFd::readExpirationCount()
{

    uint64_t expirationCount =
        0;



    const ssize_t result =
        read(
            fd_,
            &expirationCount,
            sizeof(expirationCount)
        );



    if (result < 0) {


        if (errno == EAGAIN ||
            errno == EWOULDBLOCK) {

            return 0;
        }



        throw std::runtime_error(
            "Failed to read timerfd: "
            +
            std::string(
                std::strerror(
                    errno
                )
            )
        );
    }



    return expirationCount;
}
