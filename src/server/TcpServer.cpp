#include "server/TcpServer.h"

#include "network/TcpConnection.h"
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
// Reactor server
// ============================================================

void TcpServer::run() {

    EventLoop eventLoop(
        MAX_EVENTS
    );


    // ========================================================
    // Every active Agent corresponds to one TcpConnection
    // ========================================================

    std::unordered_map<
        int,
        std::unique_ptr<TcpConnection>
    > connections;


    // TcpConnection callback 不直接删除自己。
    // 这里只记录本轮事件处理完成后需要关闭的 fd。
    std::unordered_set<int>
        connectionsToClose;


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
    // New connection callback
    // ========================================================

    serverChannel.setReadCallback(
        [&]() {

            while (true) {

                sockaddr_in clientAddress {};

                socklen_t clientAddressLength =
                    sizeof(
                        clientAddress
                    );


                // --------------------------------------------
                // Accept new client as non-blocking socket
                // --------------------------------------------

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

                        // 当前积压队列中的连接已经全部 accept。
                        break;
                    }


                    std::cerr
                        << "accept4 failed\n";

                    break;
                }


                // --------------------------------------------
                // Convert client IP to readable string
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


                // --------------------------------------------
                // Defensive duplicate-fd check
                // --------------------------------------------

                if (connections.find(
                        clientFd
                    ) != connections.end()) {

                    std::cerr
                        << "Duplicate client fd: "
                        << clientFd
                        << '\n';

                    close(
                        clientFd
                    );

                    continue;
                }


                // ============================================
                // Create TcpConnection
                // ============================================

                std::unique_ptr<TcpConnection>
                    connection;


                try {

                    connection =
                        std::make_unique<
                            TcpConnection
                        >(
                            eventLoop,
                            clientFd,
                            clientName
                        );

                } catch (
                    const std::exception& e
                ) {

                    std::cerr
                        << "Failed to create TcpConnection: "
                        << e.what()
                        << '\n';


                    // 构造失败时 TcpConnection 没有成功取得所有权，
                    // 所以这里由 TcpServer 关闭 fd。
                    close(
                        clientFd
                    );

                    continue;
                }


                // ============================================
                // Complete-message callback
                // ============================================

                connection->setMessageCallback(
                    [](
                        const std::string& name,
                        const std::string& message
                    ) {

                        std::cout
                            << "\n========== Metrics Received ==========\n";

                        std::cout
                            << "Client: "
                            << name
                            << '\n';

                        std::cout
                            << message;

                        std::cout
                            << "======================================\n";
                    }
                );


                // ============================================
                // Close request callback
                // ============================================

                connection->setCloseCallback(
                    [&connectionsToClose](
                        int fd
                    ) {

                        // 注意：
                        // 这里不能直接 erase(connection)。
                        //
                        // 当前可能仍处于
                        // Channel::handleEvent()
                        // 或 TcpConnection::handleRead()
                        // 调用栈中。
                        //
                        // 因此只做延迟关闭标记。
                        connectionsToClose.insert(
                            fd
                        );
                    }
                );


                // ============================================
                // Store ownership
                // ============================================

                const auto result =
                    connections.emplace(
                        clientFd,
                        std::move(
                            connection
                        )
                    );


                if (!result.second) {

                    // 正常情况下不会发生。
                    std::cerr
                        << "Failed to store connection: fd="
                        << clientFd
                        << '\n';

                    // connection 插入失败时，
                    // unique_ptr 仍由临时对象管理并会析构。
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


    // ========================================================
    // Listening socket error callback
    // ========================================================

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
        // epoll_wait
        //     ↓
        // EventLoop
        //     ↓
        // Channel
        //     ↓
        // Callback
        // ----------------------------------------------------

        eventLoop.loopOnce();


        // ====================================================
        // Deferred destruction
        // ====================================================

        for (const int clientFd :
             connectionsToClose) {

            const auto connectionIt =
                connections.find(
                    clientFd
                );


            if (connectionIt ==
                connections.end()) {

                continue;
            }


            std::cout
                << "Client disconnected: "
                << connectionIt
                    ->second
                    ->clientName()
                << "  fd="
                << clientFd
                << '\n';


            // 这里不需要：
            //
            // eventLoop.removeChannel(...)
            // close(clientFd)
            //
            // 因为 TcpConnection 析构函数会负责：
            //
            // removeChannel()
            // close(fd)
            //
            // erase unique_ptr
            //      ↓
            // ~TcpConnection()
            connections.erase(
                connectionIt
            );
        }


        connectionsToClose.clear();
    }
}
