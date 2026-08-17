#ifndef LMONITOR_ACCEPTOR_H
#define LMONITOR_ACCEPTOR_H

#include "reactor/Channel.h"

#include <cstdint>
#include <functional>
#include <string>


class EventLoop;


class Acceptor {
public:
    using NewConnectionCallback =
        std::function<void(
            int clientFd,
            const std::string& clientName
        )>;


    Acceptor(
        EventLoop& eventLoop,
        uint16_t port
    );

    ~Acceptor();


    Acceptor(
        const Acceptor&
    ) = delete;

    Acceptor& operator=(
        const Acceptor&
    ) = delete;


    void setNewConnectionCallback(
        NewConnectionCallback callback
    );


private:
    void handleAccept();

    static int createListeningSocket(
        uint16_t port
    );


private:
    EventLoop& eventLoop_;

    int listenFd_ = -1;

    Channel channel_;

    NewConnectionCallback
        newConnectionCallback_;
};

#endif
