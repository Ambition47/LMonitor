#ifndef LMONITOR_EVENT_LOOP_H
#define LMONITOR_EVENT_LOOP_H

#include <functional>
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


    // 进入完整 Reactor 循环
    void loop();


    // 请求退出 Reactor
    void quit();


    // 执行一轮 epoll_wait + 事件分发
    void loopOnce(
        int timeoutMilliseconds = -1
    );


    // 将任务加入 EventLoop，
    // 在当前这一轮事件分发结束后执行
    void queueInLoop(
        Functor functor
    );


private:
    // 执行所有延迟任务
    void doPendingFunctors();


private:
    int epollFd_ = -1;

    std::vector<epoll_event> events_;

    bool looping_ = false;

    bool quit_ = false;


    // 等待本轮事件处理结束后执行的任务
    std::vector<Functor>
        pendingFunctors_;
};

#endif
