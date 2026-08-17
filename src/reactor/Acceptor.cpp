#include "reactor/Acceptor.h"

#include "reactor/EventLoop.h"

#include <cerrno>
#include <iostream>
#include <stdexcept>
#include <utility>

#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>


namespace {

constexpr int LISTEN_BACKLOG =
    SOMAXCONN;

}  // namespace


// ============================================================
// Create listening socket
// ============================================================

int Acceptor::createListeningSocket(
    uint16_t port
) {
    const int listenFd =
        socket(
            AF_INET,
            SOCK_STREAM |
            SOCK_NONBLOCK |
            SOCK_CLOEXEC,
            0
        );


    if (listenFd < 0) {
        throw std::runtime_error(
            "Failed to create listening socket"
        );
    }


    // --------------------------------------------------------
    // Allow quick port reuse
    // --------------------------------------------------------

    int reuse = 1;


    if (setsockopt(
            listenFd,
            SOL_SOCKET,
            SO_REUSEADDR,
            &reuse,
            sizeof(reuse)
        ) < 0) {

        close(
            listenFd
        );

        throw std::runtime_error(
            "Failed to set SO_REUSEADDR"
        );
    }


    // --------------------------------------------------------
    // Configure address
    // --------------------------------------------------------

    sockaddr_in serverAddress {};

    serverAddress.sin_family =
        AF_INET;

    serverAddress.sin_addr.s_addr =
        htonl(
            INADDR_ANY
        );

    serverAddress.sin_port =
        htons(
            port
        );


    // --------------------------------------------------------
    // Bind
    // --------------------------------------------------------

    if (bind(
            listenFd,
            reinterpret_cast<sockaddr*>(
                &serverAddress
            ),
            sizeof(serverAddress)
        ) < 0) {

        close(
            listenFd
        );

        throw std::runtime_error(
            "Failed to bind listening socket"
        );
    }


    // --------------------------------------------------------
    // Listen
    // --------------------------------------------------------

    if (listen(
            listenFd,
            LISTEN_BACKLOG
        ) < 0) {

        close(
            listenFd
        );

        throw std::runtime_error(
            "Failed to listen on socket"
        );
    }


    return listenFd;
}


// ============================================================
// Constructor
// ============================================================

Acceptor::Acceptor(
    EventLoop& eventLoop,
    uint16_t port
)
    : eventLoop_(eventLoop),
      listenFd_(
          createListeningSocket(
              port
          )
      ),
      channel_(
          listenFd_
      ) {

    // 监听 Socket 主要关注：
    // “是否有新连接可以 accept”
    channel_.setEvents(
        EPOLLIN
    );


    channel_.setReadCallback(
        [this]() {
            handleAccept();
        }
    );


    channel_.setErrorCallback(
        [this]() {

            std::cerr
                << "Listening socket error: fd="
                << listenFd_
                << '\n';
        }
    );


    try {

        eventLoop_.addChannel(
            &channel_
        );

    } catch (...) {

        close(
            listenFd_
        );

        listenFd_ = -1;

        throw;
    }
}


// ============================================================
// Destructor
// ============================================================

Acceptor::~Acceptor() {
    try {

        eventLoop_.removeChannel(
            &channel_
        );

    } catch (...) {
        // 析构函数不能抛异常
    }


    if (listenFd_ >= 0) {

        close(
            listenFd_
        );

        listenFd_ = -1;
    }
}


// ============================================================
// Register new-connection callback
// ============================================================

void Acceptor::setNewConnectionCallback(
    NewConnectionCallback callback
) {
    newConnectionCallback_ =
        std::move(
            callback
        );
}


// ============================================================
// Accept all pending clients
// ============================================================

void Acceptor::handleAccept() {

    while (true) {

        sockaddr_in clientAddress {};

        socklen_t clientAddressLength =
            sizeof(
                clientAddress
            );


        const int clientFd =
            accept4(
                listenFd_,
                reinterpret_cast<sockaddr*>(
                    &clientAddress
                ),
                &clientAddressLength,
                SOCK_NONBLOCK |
                SOCK_CLOEXEC
            );


        if (clientFd < 0) {

            if (errno == EINTR) {
                continue;
            }


            if (errno == EAGAIN ||
                errno == EWOULDBLOCK) {

                // 当前待处理连接已经全部 accept 完。
                return;
            }


            std::cerr
                << "accept4 failed\n";

            return;
        }


        // ----------------------------------------------------
        // Convert client IP address
        // ----------------------------------------------------

        char clientIp[
            INET_ADDRSTRLEN
        ] {};


        if (inet_ntop(
                AF_INET,
                &clientAddress.sin_addr,
                clientIp,
                sizeof(clientIp)
            ) == nullptr) {

            std::cerr
                << "Failed to parse client IP\n";

            close(
                clientFd
            );

            continue;
        }


        const std::string clientName =
            std::string(
                clientIp
            ) +
            ":" +
            std::to_string(
                ntohs(
                    clientAddress.sin_port
                )
            );


        // ----------------------------------------------------
        // Notify upper layer
        // ----------------------------------------------------

        if (newConnectionCallback_) {

            newConnectionCallback_(
                clientFd,
                clientName
            );

        } else {

            // 没有上层处理者时不能泄漏 fd
            close(
                clientFd
            );
        }
    }
}
