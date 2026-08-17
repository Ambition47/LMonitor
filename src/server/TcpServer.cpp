#include "server/TcpServer.h"

#include "reactor/Channel.h"
#include "reactor/EventLoop.h"

#include <cerrno>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>

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
// Parse complete LMonitor protocol messages
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


    try {
        setNonBlocking(
            serverFd_
        );

    } catch (...) {

        close(serverFd_);
        serverFd_ = -1;

        throw;
    }


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
// Reactor server
// ============================================================

void TcpServer::run() {

    EventLoop eventLoop(
        MAX_EVENTS
    );


    // ========================================================
    // Client state
    // ========================================================

    std::unordered_map<
        int,
        std::unique_ptr<Channel>
    > clientChannels;


    std::unordered_map<
        int,
        std::string
    > pendingDataByClient;


    std::unordered_map<
        int,
        std::string
    > clientNames;


    // callback 中不直接删除 Channel。
    // 这里只记录本轮结束后需要清理的 fd。
    std::unordered_set<int>
        clientsToClose;


    // ========================================================
    // Listening Channel
    // ========================================================

    Channel serverChannel(
        serverFd_
    );

    serverChannel.setEvents(
        EPOLLIN
    );


    // ========================================================
    // Accept callback
    // ========================================================

    serverChannel.setReadCallback(
        [&]() {

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

                        // 当前所有待连接客户端都 accept 完了。
                        break;
                    }


                    std::cerr
                        << "accept4 failed\n";

                    break;
                }


                // --------------------------------------------
                // Build readable client name
                // --------------------------------------------

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


                // ============================================
                // Create Channel for this client
                // ============================================

                auto clientChannel =
                    std::make_unique<Channel>(
                        clientFd
                    );


                clientChannel->setEvents(
                    EPOLLIN |
                    EPOLLRDHUP
                );


                // ============================================
                // Read callback
                // ============================================

                clientChannel->setReadCallback(
                    [&, clientFd]() {

                        // 如果 error callback 已经把它标记为关闭，
                        // 本轮不再继续读取。
                        if (clientsToClose.find(
                                clientFd
                            ) != clientsToClose.end()) {

                            return;
                        }


                        char buffer[
                            BUFFER_SIZE
                        ];


                        while (true) {

                            const ssize_t bytesReceived =
                                recv(
                                    clientFd,
                                    buffer,
                                    sizeof(buffer),
                                    0
                                );


                            if (bytesReceived > 0) {

                                const auto pendingIt =
                                    pendingDataByClient.find(
                                        clientFd
                                    );


                                if (pendingIt ==
                                    pendingDataByClient.end()) {

                                    clientsToClose.insert(
                                        clientFd
                                    );

                                    return;
                                }


                                pendingIt->second.append(
                                    buffer,
                                    static_cast<std::size_t>(
                                        bytesReceived
                                    )
                                );


                                const auto nameIt =
                                    clientNames.find(
                                        clientFd
                                    );


                                const std::string currentClientName =
                                    nameIt !=
                                    clientNames.end()
                                        ? nameIt->second
                                        : "unknown";


                                processMessages(
                                    pendingIt->second,
                                    currentClientName
                                );


                            } else if (
                                bytesReceived == 0
                            ) {

                                // 对端正常关闭连接。
                                clientsToClose.insert(
                                    clientFd
                                );

                                return;


                            } else {

                                if (errno == EINTR) {
                                    continue;
                                }


                                if (errno == EAGAIN ||
                                    errno == EWOULDBLOCK) {

                                    // 当前 Socket 数据已经读空。
                                    return;
                                }


                                clientsToClose.insert(
                                    clientFd
                                );

                                return;
                            }
                        }
                    }
                );


                // ============================================
                // Close callback
                // ============================================

                clientChannel->setCloseCallback(
                    [&, clientFd]() {

                        clientsToClose.insert(
                            clientFd
                        );
                    }
                );


                // ============================================
                // Error callback
                // ============================================

                clientChannel->setErrorCallback(
                    [&, clientFd]() {

                        std::cerr
                            << "Socket error: fd="
                            << clientFd
                            << '\n';

                        clientsToClose.insert(
                            clientFd
                        );
                    }
                );


                // ============================================
                // Store client application state
                // ============================================

                pendingDataByClient.emplace(
                    clientFd,
                    std::string {}
                );


                clientNames.emplace(
                    clientFd,
                    clientName
                );


                // Channel 必须先存下来，
                // 确保其地址在注册到 epoll 后持续有效。
                const auto insertResult =
                    clientChannels.emplace(
                        clientFd,
                        std::move(
                            clientChannel
                        )
                    );


                if (!insertResult.second) {

                    std::cerr
                        << "Duplicate client fd: "
                        << clientFd
                        << '\n';

                    pendingDataByClient.erase(
                        clientFd
                    );

                    clientNames.erase(
                        clientFd
                    );

                    close(
                        clientFd
                    );

                    continue;
                }


                Channel* channel =
                    insertResult.first
                        ->second
                        .get();


                // ============================================
                // Register Channel with EventLoop
                // ============================================

                try {

                    eventLoop.addChannel(
                        channel
                    );

                } catch (
                    const std::exception& e
                ) {

                    std::cerr
                        << "Failed to add client Channel: "
                        << e.what()
                        << '\n';


                    clientChannels.erase(
                        clientFd
                    );

                    pendingDataByClient.erase(
                        clientFd
                    );

                    clientNames.erase(
                        clientFd
                    );

                    close(
                        clientFd
                    );

                    continue;
                }


                std::cout
                    << "Client connected: "
                    << clientName
                    << "  fd="
                    << clientFd
                    << '\n';
            }
        }
    );


    // Listening socket error.
    serverChannel.setErrorCallback(
        [&]() {

            std::cerr
                << "Listening socket error\n";
        }
    );


    // ========================================================
    // Register listening Channel
    // ========================================================

    eventLoop.addChannel(
        &serverChannel
    );


    std::cout
        << "LMonitor Reactor Server listening on port "
        << port_
        << "...\n";


    // ========================================================
    // Reactor main loop
    // ========================================================

    while (true) {

        // ----------------------------------------------------
        // epoll_wait + Channel callback dispatch
        // ----------------------------------------------------

        eventLoop.loopOnce();


        // ====================================================
        // Deferred client cleanup
        // ====================================================

        for (const int clientFd :
             clientsToClose) {

            const auto channelIt =
                clientChannels.find(
                    clientFd
                );


            if (channelIt ==
                clientChannels.end()) {

                continue;
            }


            const auto nameIt =
                clientNames.find(
                    clientFd
                );


            if (nameIt !=
                clientNames.end()) {

                std::cout
                    << "Client disconnected: "
                    << nameIt->second
                    << "  fd="
                    << clientFd
                    << '\n';

            } else {

                std::cout
                    << "Client disconnected: fd="
                    << clientFd
                    << '\n';
            }


            // -----------------------------------------------
            // Correct destruction order:
            //
            // 1. remove from epoll
            // 2. close socket
            // 3. remove application state
            // 4. destroy Channel
            // -----------------------------------------------

            try {

                eventLoop.removeChannel(
                    channelIt
                        ->second
                        .get()
                );

            } catch (
                const std::exception& e
            ) {

                std::cerr
                    << "Failed to remove Channel: "
                    << e.what()
                    << '\n';
            }


            close(
                clientFd
            );


            pendingDataByClient.erase(
                clientFd
            );


            clientNames.erase(
                clientFd
            );


            clientChannels.erase(
                channelIt
            );
        }


        clientsToClose.clear();
    }
}
