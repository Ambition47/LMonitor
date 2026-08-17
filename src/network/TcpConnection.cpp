#include "network/TcpConnection.h"

#include <cerrno>
#include <iostream>
#include <utility>

#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>


namespace {

constexpr std::size_t BUFFER_SIZE =
    4096;

const std::string MESSAGE_DELIMITER =
    "END\n";

}  // namespace


TcpConnection::TcpConnection(
    EventLoop& eventLoop,
    int fd,
    std::string clientName
)
    : eventLoop_(eventLoop),
      fd_(fd),
      clientName_(
          std::move(clientName)
      ),
      channel_(fd) {

    // 关注：
    // 1. 可读事件
    // 2. 对端关闭连接
    channel_.setEvents(
        EPOLLIN |
        EPOLLRDHUP
    );


    channel_.setReadCallback(
        [this]() {
            handleRead();
        }
    );


    channel_.setCloseCallback(
        [this]() {
            handleClose();
        }
    );


    channel_.setErrorCallback(
        [this]() {
            handleError();
        }
    );


    // 注册到 Reactor EventLoop
    eventLoop_.addChannel(
        &channel_
    );
}


TcpConnection::~TcpConnection() {
    // 析构函数不能让异常继续传播
    try {
        eventLoop_.removeChannel(
            &channel_
        );
    } catch (...) {
        // Destructor must not throw.
    }


    if (fd_ >= 0) {
        close(
            fd_
        );

        fd_ = -1;
    }
}


int TcpConnection::fd() const {
    return fd_;
}


const std::string&
TcpConnection::clientName() const {
    return clientName_;
}


void TcpConnection::setMessageCallback(
    MessageCallback callback
) {
    messageCallback_ =
        std::move(callback);
}


void TcpConnection::setCloseCallback(
    CloseCallback callback
) {
    closeCallback_ =
        std::move(callback);
}


// ============================================================
// Read all currently available socket data
// ============================================================

void TcpConnection::handleRead() {
    if (closing_) {
        return;
    }


    char buffer[
        BUFFER_SIZE
    ];


    while (true) {

        const ssize_t bytesReceived =
            recv(
                fd_,
                buffer,
                sizeof(buffer),
                0
            );


        if (bytesReceived > 0) {

            pendingData_.append(
                buffer,
                static_cast<std::size_t>(
                    bytesReceived
                )
            );


            processPendingData();

            continue;
        }


        if (bytesReceived == 0) {

            // 对端正常关闭 TCP 连接
            requestClose();

            return;
        }


        // recv < 0

        if (errno == EINTR) {
            continue;
        }


        if (errno == EAGAIN ||
            errno == EWOULDBLOCK) {

            // 非阻塞 Socket 当前已经没有更多数据
            return;
        }


        requestClose();

        return;
    }
}


// ============================================================
// Channel close event
// ============================================================

void TcpConnection::handleClose() {
    requestClose();
}


// ============================================================
// Channel error event
// ============================================================

void TcpConnection::handleError() {
    if (closing_) {
        return;
    }


    std::cerr
        << "Socket error: "
        << clientName_
        << " fd="
        << fd_
        << '\n';


    requestClose();
}


// ============================================================
// Parse complete LMONITOR messages
// ============================================================

void TcpConnection::processPendingData() {
    while (true) {

        const std::size_t delimiterPosition =
            pendingData_.find(
                MESSAGE_DELIMITER
            );


        if (delimiterPosition ==
            std::string::npos) {

            // 还没有收到完整消息
            return;
        }


        const std::size_t messageLength =
            delimiterPosition +
            MESSAGE_DELIMITER.size();


        const std::string message =
            pendingData_.substr(
                0,
                messageLength
            );


        pendingData_.erase(
            0,
            messageLength
        );


        if (messageCallback_) {

            messageCallback_(
                clientName_,
                message
            );
        }
    }
}


// ============================================================
// Deferred close request
// ============================================================

void TcpConnection::requestClose() {
    if (closing_) {
        return;
    }


    closing_ =
        true;


    if (closeCallback_) {

        closeCallback_(
            fd_
        );
    }
}
