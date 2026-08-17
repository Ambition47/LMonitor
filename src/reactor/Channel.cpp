#include "reactor/Channel.h"

#include <stdexcept>
#include <utility>

#include <sys/epoll.h>


Channel::Channel(
    int fd
)
    : fd_(fd) {

    if (fd_ < 0) {
        throw std::invalid_argument(
            "Channel fd must be valid"
        );
    }
}


int Channel::fd() const {
    return fd_;
}


uint32_t Channel::events() const {
    return events_;
}


void Channel::setEvents(
    uint32_t events
) {
    events_ =
        events;
}


void Channel::setReadCallback(
    Callback callback
) {
    readCallback_ =
        std::move(callback);
}


void Channel::setCloseCallback(
    Callback callback
) {
    closeCallback_ =
        std::move(callback);
}


void Channel::setErrorCallback(
    Callback callback
) {
    errorCallback_ =
        std::move(callback);
}


void Channel::handleEvent(
    uint32_t revents
) {
    // Socket error
    if (revents &
        EPOLLERR) {

        if (errorCallback_) {
            errorCallback_();
        }
    }


    // Socket has readable data
    if (revents &
        EPOLLIN) {

        if (readCallback_) {
            readCallback_();
        }
    }


    // Peer closed connection
    if (revents &
        (
            EPOLLHUP |
            EPOLLRDHUP
        )) {

        if (closeCallback_) {
            closeCallback_();
        }
    }
}
