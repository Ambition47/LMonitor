#include "network/TcpClient.h"

#include "log/Logger.h"

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>


// ============================================================
// Constructor
// ============================================================

TcpClient::TcpClient(
    std::string serverIp,
    uint16_t serverPort
)
    : serverIp_(
          std::move(
              serverIp
          )
      ),
      serverPort_(
          serverPort
      ) {
}


// ============================================================
// Destructor
// ============================================================

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
// Connect to LMonitor Server
// ============================================================

bool TcpClient::connectToServer() {

    // --------------------------------------------------------
    // Already connected.
    // --------------------------------------------------------

    if (isConnected()) {
        return true;
    }


    int newSocketFd =
        -1;


    // --------------------------------------------------------
    // Create socket
    // --------------------------------------------------------

    try {

        newSocketFd =
            createSocket();

    } catch (
        const std::exception& e
    ) {

        Logger::instance().error(
            "TCP socket creation failed: " +
            std::string(
                e.what()
            )
        );


        return false;
    }


    // --------------------------------------------------------
    // Build server address
    // --------------------------------------------------------

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

        Logger::instance().error(
            "Invalid monitoring server IP address: " +
            serverIp_
        );


        close(
            newSocketFd
        );


        return false;
    }


    // --------------------------------------------------------
    // Connect
    // --------------------------------------------------------

    if (connect(
            newSocketFd,
            reinterpret_cast<sockaddr*>(
                &serverAddress
            ),
            sizeof(serverAddress)
        ) < 0) {

        const int savedErrno =
            errno;


        Logger::instance().warning(
            "Failed to connect to monitoring server " +
            serverIp_ +
            ":" +
            std::to_string(
                serverPort_
            ) +
            ": " +
            std::string(
                std::strerror(
                    savedErrno
                )
            )
        );


        close(
            newSocketFd
        );


        return false;
    }


    // --------------------------------------------------------
    // Connection established.
    // --------------------------------------------------------

    socketFd_ =
        newSocketFd;


    Logger::instance().info(
        "Connected to monitoring server " +
        serverIp_ +
        ":" +
        std::to_string(
            serverPort_
        )
    );


    return true;
}


// ============================================================
// Disconnect
// ============================================================

void TcpClient::disconnect() {

    if (socketFd_ < 0) {
        return;
    }


    close(
        socketFd_
    );


    socketFd_ =
        -1;
}


// ============================================================
// Connection state
// ============================================================

bool TcpClient::isConnected() const {

    return socketFd_ >= 0;
}


// ============================================================
// Send complete frame
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


        // ----------------------------------------------------
        // Successfully sent part of the frame.
        // ----------------------------------------------------

        if (bytesSent > 0) {

            totalSent +=
                static_cast<std::size_t>(
                    bytesSent
                );


            continue;
        }


        // ----------------------------------------------------
        // Socket unexpectedly returned zero.
        // ----------------------------------------------------

        if (bytesSent == 0) {

            Logger::instance().warning(
                "TCP connection closed while sending metrics"
            );


            disconnect();


            return false;
        }


        // ----------------------------------------------------
        // send() < 0
        // ----------------------------------------------------

        if (errno == EINTR) {

            continue;
        }


        const int savedErrno =
            errno;


        Logger::instance().warning(
            "Failed to send metrics to monitoring server " +
            serverIp_ +
            ":" +
            std::to_string(
                serverPort_
            ) +
            ": " +
            std::string(
                std::strerror(
                    savedErrno
                )
            )
        );


        // ----------------------------------------------------
        // Current connection can no longer be trusted.
        //
        // MonitorAgent will reconnect according to its
        // exponential backoff policy.
        // ----------------------------------------------------

        disconnect();


        return false;
    }


    return true;
}
