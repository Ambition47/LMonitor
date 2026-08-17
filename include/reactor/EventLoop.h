#ifndef LMONITOR_EVENT_LOOP_H
#define LMONITOR_EVENT_LOOP_H

#include <vector>

#include <sys/epoll.h>


class Channel;


class EventLoop {
public:
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


    void loopOnce(
        int timeoutMilliseconds = -1
    );


private:
    int epollFd_ = -1;

    std::vector<epoll_event> events_;
};

#endif
