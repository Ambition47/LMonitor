#include "server/TcpServer.h"

#include <cerrno>
#include <fcntl.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>


namespace {

constexpr int MAX_EVENTS = 64;
constexpr int BUFFER_SIZE = 4096;


// ============================================================
// Set socket to non-blocking mode
// ============================================================

void setNonBlocking(
    int fd
) {
    const int flags =
        fcntl(
            fd,
            F_GETFL,
            0
        );

    if (flags < 0) {
        throw std::runtime_error(
            "Failed to get socket flags"
        );
    }

    if (fcntl(
            fd,
            F_SETFL,
            flags | O_NONBLOCK
        ) < 0) {

        throw std::runtime_error(
            "Failed to set socket non-blocking"
        );
    }
}


// ============================================================
// Parse all complete LMonitor messages from pending data
// ============================================================

void processMessages(
    std::string& pendingData,
    const std::string& clientName
) {
    const std::string delimiter =
        "END\n";

    while (true) {

        const std::size_t position =
            pendingData.find(
                delimiter
            );

        if (position ==
            std::string::npos) {

            break;
        }

        const std::size_t messageLength =
            position +
            delimiter.size();

        const std::string message =
            pendingData.substr(
                0,
                messageLength
            );

        pendingData.erase(
            0,
            messageLength
        );

        std::cout
            << "\n========== Metrics Received ==========\n";

        std::cout
            << "Client: "
            << clientName
            << '\n';

        std::cout
            << message;

        std::cout
            << "======================================\n";
    }
}

}  // namespace


// ============================================================
// Constructor
// ============================================================

TcpServer::TcpServer(
    uint16_t port
)
    : port_(port) {

    // --------------------------------------------------------
    // Create TCP socket
    // --------------------------------------------------------

    serverFd_ =
        socket(
            AF_INET,
            SOCK_STREAM,
            0
        );

    if (serverFd_ < 0) {
        throw std::runtime_error(
            "Failed to create server socket"
        );
    }


    // --------------------------------------------------------
    // Allow quick port reuse
    // --------------------------------------------------------

    int reuse = 1;

    if (setsockopt(
            serverFd_,
            SOL_SOCKET,
            SO_REUSEADDR,
            &reuse,
            sizeof(reuse)
        ) < 0) {

        close(serverFd_);

        serverFd_ = -1;

        throw std::runtime_error(
            "Failed to set SO_REUSEADDR"
        );
    }


    // --------------------------------------------------------
    // Set listening socket to non-blocking mode
    // --------------------------------------------------------

    try {

        setNonBlocking(
            serverFd_
        );

    } catch (...) {

        close(serverFd_);

        serverFd_ = -1;

        throw;
    }


    // --------------------------------------------------------
    // Server address
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
            port_
        );


    // --------------------------------------------------------
    // Bind
    // --------------------------------------------------------

    if (bind(
            serverFd_,
            reinterpret_cast<sockaddr*>(
                &serverAddress
            ),
            sizeof(serverAddress)
        ) < 0) {

        close(serverFd_);

        serverFd_ = -1;

        throw std::runtime_error(
            "Failed to bind server socket"
        );
    }


    // --------------------------------------------------------
    // Listen
    // --------------------------------------------------------

    if (listen(
            serverFd_,
            SOMAXCONN
        ) < 0) {

        close(serverFd_);

        serverFd_ = -1;

        throw std::runtime_error(
            "Failed to listen on server socket"
        );
    }
}


// ============================================================
// Destructor
// ============================================================

TcpServer::~TcpServer() {
    if (serverFd_ >= 0) {

        close(
            serverFd_
        );

        serverFd_ = -1;
    }
}


// ============================================================
// epoll event loop
// ============================================================

void TcpServer::run() {

    // --------------------------------------------------------
    // Create epoll instance
    // --------------------------------------------------------

    const int epollFd =
        epoll_create1(
            EPOLL_CLOEXEC
        );

    if (epollFd < 0) {
        throw std::runtime_error(
            "Failed to create epoll instance"
        );
    }


    // --------------------------------------------------------
    // Add listening socket to epoll
    // --------------------------------------------------------

    epoll_event serverEvent {};

    serverEvent.events =
        EPOLLIN;

    serverEvent.data.fd =
        serverFd_;


    if (epoll_ctl(
            epollFd,
            EPOLL_CTL_ADD,
            serverFd_,
            &serverEvent
        ) < 0) {

        close(
            epollFd
        );

        throw std::runtime_error(
            "Failed to add server socket to epoll"
        );
    }


    std::cout
        << "LMonitor epoll Server listening on port "
        << port_
        << "...\n";


    // --------------------------------------------------------
    // Each client needs its own TCP receive buffer
    // --------------------------------------------------------

    std::unordered_map<
        int,
        std::string
    > pendingDataByClient;


    // fd -> "IP:port"
    std::unordered_map<
        int,
        std::string
    > clientNames;


    epoll_event events[
        MAX_EVENTS
    ];


    // ========================================================
    // Main epoll event loop
    // ========================================================

    while (true) {

        const int readyCount =
            epoll_wait(
                epollFd,
                events,
                MAX_EVENTS,
                -1
            );


        if (readyCount < 0) {

            if (errno == EINTR) {
                continue;
            }

            close(
                epollFd
            );

            throw std::runtime_error(
                "epoll_wait failed"
            );
        }


        // ====================================================
        // Process ready events
        // ====================================================

        for (int i = 0;
             i < readyCount;
             ++i) {

            const int eventFd =
                events[i].data.fd;

            const uint32_t eventFlags =
                events[i].events;


            // =================================================
            // Listening socket ready:
            // one or more new clients are waiting
            // =================================================

            if (eventFd ==
                serverFd_) {

                while (true) {

                    sockaddr_in clientAddress {};

                    socklen_t clientAddressLength =
                        sizeof(
                            clientAddress
                        );


                    const int clientFd =
                        accept4(
                            serverFd_,
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

                            // All pending connections
                            // have been accepted.
                            break;
                        }

                        std::cerr
                            << "accept4 failed\n";

                        break;
                    }


                    // -----------------------------------------
                    // Convert client address to readable string
                    // -----------------------------------------

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


                    // -----------------------------------------
                    // Add client socket to epoll
                    // -----------------------------------------

                    epoll_event clientEvent {};

                    clientEvent.events =
                        EPOLLIN |
                        EPOLLRDHUP;

                    clientEvent.data.fd =
                        clientFd;


                    if (epoll_ctl(
                            epollFd,
                            EPOLL_CTL_ADD,
                            clientFd,
                            &clientEvent
                        ) < 0) {

                        std::cerr
                            << "Failed to add client to epoll\n";

                        close(
                            clientFd
                        );

                        continue;
                    }


                    pendingDataByClient.emplace(
                        clientFd,
                        std::string {}
                    );

                    clientNames.emplace(
                        clientFd,
                        clientName
                    );


                    std::cout
                        << "Client connected: "
                        << clientName
                        << "  fd="
                        << clientFd
                        << '\n';
                }


                continue;
            }


            // =================================================
            // Client socket event
            // =================================================

            bool shouldClose =
                false;


            // -------------------------------------------------
            // Client has readable data
            // -------------------------------------------------

            if (eventFlags &
                EPOLLIN) {

                char buffer[
                    BUFFER_SIZE
                ];


                // Because socket is non-blocking,
                // read until EAGAIN.
                while (true) {

                    const ssize_t bytesReceived =
                        recv(
                            eventFd,
                            buffer,
                            sizeof(buffer),
                            0
                        );


                    if (bytesReceived > 0) {

                        auto pendingIt =
                            pendingDataByClient.find(
                                eventFd
                            );


                        if (pendingIt ==
                            pendingDataByClient.end()) {

                            shouldClose =
                                true;

                            break;
                        }


                        pendingIt->second.append(
                            buffer,
                            static_cast<std::size_t>(
                                bytesReceived
                            )
                        );


                        const auto nameIt =
                            clientNames.find(
                                eventFd
                            );


                        const std::string clientName =
                            nameIt !=
                            clientNames.end()
                                ? nameIt->second
                                : "unknown";


                        processMessages(
                            pendingIt->second,
                            clientName
                        );


                    } else if (
                        bytesReceived == 0
                    ) {

                        // Peer performed orderly shutdown.
                        shouldClose =
                            true;

                        break;


                    } else {

                        if (errno == EINTR) {
                            continue;
                        }


                        if (errno == EAGAIN ||
                            errno == EWOULDBLOCK) {

                            // No more data for now.
                            break;
                        }


                        shouldClose =
                            true;

                        break;
                    }
                }
            }


            // -------------------------------------------------
            // Client closed / socket error
            // -------------------------------------------------

            if (eventFlags &
                (
                    EPOLLERR |
                    EPOLLHUP |
                    EPOLLRDHUP
                )) {

                shouldClose =
                    true;
            }


            // -------------------------------------------------
            // Remove disconnected client
            // -------------------------------------------------

            if (shouldClose) {

                const auto nameIt =
                    clientNames.find(
                        eventFd
                    );


                if (nameIt !=
                    clientNames.end()) {

                    std::cout
                        << "Client disconnected: "
                        << nameIt->second
                        << "  fd="
                        << eventFd
                        << '\n';

                } else {

                    std::cout
                        << "Client disconnected: fd="
                        << eventFd
                        << '\n';
                }


                epoll_ctl(
                    epollFd,
                    EPOLL_CTL_DEL,
                    eventFd,
                    nullptr
                );


                close(
                    eventFd
                );


                pendingDataByClient.erase(
                    eventFd
                );


                clientNames.erase(
                    eventFd
                );
            }
        }
    }


    // 当前版本 while(true) 不会到这里。
    close(
        epollFd
    );
}
