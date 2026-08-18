#include "server/TcpServer.h"

#include "network/TcpConnection.h"
#include "reactor/Acceptor.h"
#include "reactor/EventLoop.h"

#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>

#include <unistd.h>


namespace {

constexpr int MAX_EVENTS =
    64;

}  // namespace


// ============================================================
// Constructor
// ============================================================

TcpServer::TcpServer(
    uint16_t port
)
    : port_(port) {
}


// ============================================================
// Reactor server
// ============================================================

void TcpServer::run() {

    // ========================================================
    // Reactor EventLoop
    // ========================================================

    EventLoop eventLoop(
        MAX_EVENTS
    );


    // ========================================================
    // Active connections
    //
    // fd -> TcpConnection
    // ========================================================

    std::unordered_map<
        int,
        std::unique_ptr<TcpConnection>
    > connections;


    // ========================================================
    // Acceptor
    // ========================================================

    Acceptor acceptor(
        eventLoop,
        port_
    );


    // ========================================================
    // New connection callback
    // ========================================================

    acceptor.setNewConnectionCallback(
        [&](
            int clientFd,
            const std::string& clientName
        ) {

            // ------------------------------------------------
            // Defensive duplicate check
            // ------------------------------------------------

            if (connections.find(
                    clientFd
                ) != connections.end()) {

                std::cerr
                    << "Duplicate connection fd: "
                    << clientFd
                    << '\n';

                close(
                    clientFd
                );

                return;
            }


            // ------------------------------------------------
            // Create TcpConnection
            // ------------------------------------------------

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


                // TcpConnection 构造失败，
                // fd 所有权尚未成功交给连接对象。
                close(
                    clientFd
                );

                return;
            }


            // =================================================
            // Complete message callback
            // =================================================

            connection->setMessageCallback(
                [](
                    const std::string& name,
                    const std::string& message
                ) {

                    std::cout
                        << "\n"
                        << "========== Metrics Received ==========\n";

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


            // =================================================
            // Close callback
            //
            // 不立即 erase TcpConnection。
            //
            // 当前可能仍在：
            //
            // TcpConnection::handleRead()
            // Channel::handleEvent()
            //
            // 所以把删除操作放进 EventLoop 延迟任务队列。
            // =================================================

            connection->setCloseCallback(
                [&eventLoop, &connections](
                    int fd
                ) {

                    eventLoop.queueInLoop(
                        [&connections, fd]() {

                            const auto connectionIt =
                                connections.find(
                                    fd
                                );


                            if (connectionIt ==
                                connections.end()) {

                                return;
                            }


                            std::cout
                                << "Client disconnected: "
                                << connectionIt
                                    ->second
                                    ->clientName()
                                << "  fd="
                                << fd
                                << '\n';


                            // --------------------------------
                            // erase unique_ptr
                            //       ↓
                            // ~TcpConnection()
                            //       ↓
                            // removeChannel()
                            //       ↓
                            // close(fd)
                            // --------------------------------

                            connections.erase(
                                connectionIt
                            );
                        }
                    );
                }
            );


            // =================================================
            // Store ownership
            // =================================================

            const auto result =
                connections.emplace(
                    clientFd,
                    std::move(
                        connection
                    )
                );


            if (!result.second) {

                std::cerr
                    << "Failed to store TcpConnection: fd="
                    << clientFd
                    << '\n';

                return;
            }


            std::cout
                << "Client connected: "
                << clientName
                << "  fd="
                << clientFd
                << '\n';
        }
    );


    std::cout
        << "LMonitor Reactor Server listening on port "
        << port_
        << "...\n";


    // ========================================================
    // Start Reactor
    //
    // EventLoop now owns the main loop.
    // ========================================================

    eventLoop.loop();
}
