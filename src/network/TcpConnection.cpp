#include "network/TcpConnection.h"
#include "protocol/FrameCodec.h"

#include <cerrno>
#include <exception>
#include <iostream>
#include <utility>

#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>


namespace {

constexpr std::size_t BUFFER_SIZE =
    4096;


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

        std::string message;


        try {

            // 尝试从 TCP 字节流缓存中
            // 解析出一条完整 length-prefixed frame。
            //
            // false:
            //     当前数据还不够一整帧
            //
            // true:
            //     成功解析出一整帧
            if (!FrameCodec::tryDecode(
                    pendingData_,
                    message
                )) {

                return;
            }

        } catch (
            const std::exception& e
        ) {

            // 非法长度等协议错误。
            //
            // 例如客户端声称下一帧有 100MB，
            // 超过 FrameCodec::MAX_FRAME_SIZE。
            std::cerr
                << "Invalid frame from "
                << clientName_
                << ": "
                << e.what()
                << '\n';


            requestClose();

            return;
        }


        // ----------------------------------------------------
        // One complete payload has been decoded.
        // ----------------------------------------------------

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
