#ifndef LMONITOR_CHANNEL_H
#define LMONITOR_CHANNEL_H

#include <cstdint>
#include <functional>


class Channel {
public:
    using Callback =
        std::function<void()>;


    explicit Channel(
        int fd
    );


    Channel(
        const Channel&
    ) = delete;

    Channel& operator=(
        const Channel&
    ) = delete;


    int fd() const;


    uint32_t events() const;


    void setEvents(
        uint32_t events
    );


    void setReadCallback(
        Callback callback
    );


    void setCloseCallback(
        Callback callback
    );


    void setErrorCallback(
        Callback callback
    );


    void handleEvent(
        uint32_t revents
    );


private:
    int fd_ = -1;

    uint32_t events_ = 0;


    Callback readCallback_;

    Callback closeCallback_;

    Callback errorCallback_;
};

#endif
