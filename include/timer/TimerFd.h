#ifndef LMONITOR_TIMER_FD_H
#define LMONITOR_TIMER_FD_H

#include <cstdint>


class TimerFd {
public:

    TimerFd();


    ~TimerFd();


    TimerFd(
        const TimerFd&
    ) = delete;


    TimerFd& operator=(
        const TimerFd&
    ) = delete;



    int fd() const noexcept;



    void start(
        uint32_t intervalSeconds
    );


    void stop();



    uint64_t readExpirationCount();



private:

    int fd_;
};

#endif
