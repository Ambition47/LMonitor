#include "network/TcpClient.h"

#include <cerrno>
#include <iostream>
#include <stdexcept>
#include <utility>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>


TcpClient::TcpClient(
    std::string serverIp,
    uint16_t serverPort
)
    : serverIp_(
          std::move(serverIp)
      ),
      serverPort_(
          serverPort
      ) {
}


TcpClient::~TcpClient() {
    disconnect();
}


// ============================================================
// Create TCP socket
// ============================================================

int TcpClient::createSocket() const {

    const int socketFd =
        socket(
            AF_INET,
            SOCK_STREAM |
            SOCK_CLOEXEC,
            0
        );


    if (socketFd < 0) {
        throw std::runtime_error(
            "Failed to create TCP socket"
        );
    }


    return socketFd;
}


// ============================================================
// Connect to monitoring server
// ============================================================

bool TcpClient::connectToServer() {

    // 如果已经连接，就不重复 connect。
    if (isConnected()) {
        return true;
    }


    int newSocketFd = -1;


    try {

        newSocketFd =
            createSocket();

    } catch (
        const std::exception& e
    ) {

        std::cerr
            << "TCP socket creation failed: "
            << e.what()
            << '\n';

        return false;
    }


    sockaddr_in serverAddress {};

    serverAddress.sin_family =
        AF_INET;

    serverAddress.sin_port =
        htons(
            serverPort_
        );


    const int conversionResult =
        inet_pton(
            AF_INET,
            serverIp_.c_str(),
            &serverAddress.sin_addr
        );


    if (conversionResult != 1) {

        std::cerr
            << "Invalid server IP address: "
            << serverIp_
            << '\n';


        close(
            newSocketFd
        );

        return false;
    }


    if (connect(
            newSocketFd,
            reinterpret_cast<sockaddr*>(
                &serverAddress
            ),
            sizeof(serverAddress)
        ) < 0) {

        close(
            newSocketFd
        );

        return false;
    }


    socketFd_ =
        newSocketFd;


    std::cout
        << "Connected to LMonitor server "
        << serverIp_
        << ":"
        << serverPort_
        << '\n';


    return true;
}


// ============================================================
// Disconnect
// ============================================================

void TcpClient::disconnect() {

    if (socketFd_ >= 0) {

        close(
            socketFd_
        );

        socketFd_ = -1;
    }
}


// ============================================================
// Connection state
// ============================================================

bool TcpClient::isConnected() const {

    return socketFd_ >= 0;
}


// ============================================================
// Send complete message
// ============================================================

bool TcpClient::sendAll(
    const std::string& data
) {

    if (!isConnected()) {
        return false;
    }


    std::size_t totalSent =
        0;


    while (totalSent <
           data.size()) {

        const ssize_t bytesSent =
            send(
                socketFd_,
                data.data() +
                    totalSent,
                data.size() -
                    totalSent,
                MSG_NOSIGNAL
            );


        if (bytesSent > 0) {

            totalSent +=
                static_cast<std::size_t>(
                    bytesSent
                );

            continue;
        }


        if (bytesSent == 0) {

            disconnect();

            return false;
        }


        // send < 0

        if (errno == EINTR) {
            continue;
        }


        // 包括：
        // EPIPE
        // ECONNRESET
        // ENOTCONN
        // 等连接错误。
        disconnect();

        return false;
    }


    return true;
}

