#ifndef LMONITOR_TCP_CONNECTION_H
#define LMONITOR_TCP_CONNECTION_H

#include "reactor/Channel.h"
#include "reactor/EventLoop.h"

#include <functional>
#include <string>


class TcpConnection {
public:
    using MessageCallback =
        std::function<void(
            const std::string& clientName,
            const std::string& message
        )>;

    using CloseCallback =
        std::function<void(int fd)>;


    TcpConnection(
        EventLoop& eventLoop,
        int fd,
        std::string clientName
    );

    ~TcpConnection();


    TcpConnection(
        const TcpConnection&
    ) = delete;

    TcpConnection& operator=(
        const TcpConnection&
    ) = delete;


    int fd() const;

    const std::string& clientName() const;


    void setMessageCallback(
        MessageCallback callback
    );

    void setCloseCallback(
        CloseCallback callback
    );


private:
    void handleRead();

    void handleClose();

    void handleError();

    void processPendingData();

    void requestClose();


private:
    EventLoop& eventLoop_;

    int fd_ = -1;

    std::string clientName_;

    Channel channel_;

    std::string pendingData_;

    MessageCallback messageCallback_;

    CloseCallback closeCallback_;

    bool closing_ = false;
};

#endif
