#include "server/TcpServer.h"

#include "reactor/EventLoop.h"

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
// Set fd to non-blocking mode
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
// Parse complete LMonitor messages
// ============================================================

void processMessages(
    std::string& pendingData,
    const std::string& clientName
) {
    const std::string delimiter =
        "END\n";


    while (true) {

        const std::size_t delimiterPosition =
            pendingData.find(
                delimiter
            );


        if (delimiterPosition ==
            std::string::npos) {

            break;
        }


        const std::size_t messageLength =
            delimiterPosition +
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
    // Create TCP listening socket
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
    // Allow port reuse
    // --------------------------------------------------------

    int reuse = 1;


    if (setsockopt(
            serverFd_,
            SOL_SOCKET,
            SO_REUSEADDR,
            &reuse,
            sizeof(reuse)
        ) < 0) {

        close(
            serverFd_
        );

        serverFd_ = -1;


        throw std::runtime_error(
            "Failed to set SO_REUSEADDR"
        );
    }


    // --------------------------------------------------------
    // Listening socket must be non-blocking
    // --------------------------------------------------------

    try {

        setNonBlocking(
            serverFd_
        );

    } catch (...) {

        close(
            serverFd_
        );

        serverFd_ = -1;

        throw;
    }


    // --------------------------------------------------------
    // Configure server address
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

        close(
            serverFd_
        );

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

        close(
            serverFd_
        );

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
// Server event loop
// ============================================================

void TcpServer::run() {

    // ========================================================
    // EventLoop owns the epoll instance
    // ========================================================

    EventLoop eventLoop(
        MAX_EVENTS
    );


    // --------------------------------------------------------
    // Register listening socket
    // --------------------------------------------------------

    eventLoop.addFd(
        serverFd_,
        EPOLLIN
    );


    std::cout
        << "LMonitor Reactor Server listening on port "
        << port_
        << "...\n";


    // ========================================================
    // Per-client state
    // ========================================================

    // fd -> unfinished TCP stream data
    std::unordered_map<
        int,
        std::string
    > pendingDataByClient;


    // fd -> readable client address
    std::unordered_map<
        int,
        std::string
    > clientNames;


    // ========================================================
    // Event loop
    // ========================================================

    while (true) {

        // ----------------------------------------------------
        // Wait until one or more registered fds become ready
        // ----------------------------------------------------

        const int readyCount =
            eventLoop.wait();


        // ----------------------------------------------------
        // Process all ready events
        // ----------------------------------------------------

        for (int i = 0;
             i < readyCount;
             ++i) {

            const epoll_event& event =
                eventLoop.getEvent(
                    i
                );


            const int eventFd =
                event.data.fd;


            const uint32_t eventFlags =
                event.events;


            // =================================================
            // Listening socket:
            // new TCP connections are waiting
            // =================================================

            if (eventFd ==
                serverFd_) {

                while (true) {

                    sockaddr_in clientAddress {};


                    socklen_t clientAddressLength =
                        sizeof(
                            clientAddress
                        );


                    // -----------------------------------------
                    // Accept client directly as non-blocking
                    // -----------------------------------------

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

                            // No more connections waiting.
                            break;
                        }


                        std::cerr
                            << "accept4 failed\n";

                        break;
                    }


                    // -----------------------------------------
                    // Convert client IP address
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
                    // Register client with EventLoop
                    // -----------------------------------------

                    try {

                        eventLoop.addFd(
                            clientFd,
                            EPOLLIN |
                            EPOLLRDHUP
                        );

                    } catch (
                        const std::exception& e
                    ) {

                        std::cerr
                            << "Failed to register client: "
                            << e.what()
                            << '\n';

                        close(
                            clientFd
                        );

                        continue;
                    }


                    // -----------------------------------------
                    // Create per-client state
                    // -----------------------------------------

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


                // ---------------------------------------------
                // Non-blocking socket:
                // keep reading until EAGAIN
                // ---------------------------------------------

                while (true) {

                    const ssize_t bytesReceived =
                        recv(
                            eventFd,
                            buffer,
                            sizeof(buffer),
                            0
                        );


                    if (bytesReceived > 0) {

                        const auto pendingIt =
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

                        // Remote peer closed normally.
                        shouldClose =
                            true;

                        break;


                    } else {

                        if (errno == EINTR) {
                            continue;
                        }


                        if (errno == EAGAIN ||
                            errno == EWOULDBLOCK) {

                            // Current socket buffer is drained.
                            break;
                        }


                        shouldClose =
                            true;

                        break;
                    }
                }
            }


            // -------------------------------------------------
            // Connection closed or socket error
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
            // Cleanup disconnected client
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


                // First stop EventLoop monitoring this fd.
                try {

                    eventLoop.removeFd(
                        eventFd
                    );

                } catch (
                    const std::exception& e
                ) {

                    std::cerr
                        << "Failed to remove client from EventLoop: "
                        << e.what()
                        << '\n';
                }


                // Then release the socket.
                close(
                    eventFd
                );


                // Finally remove application state.
                pendingDataByClient.erase(
                    eventFd
                );


                clientNames.erase(
                    eventFd
                );
            }
        }
    }
}
