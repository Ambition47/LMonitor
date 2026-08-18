#include "server/TcpServer.h"

#include "network/TcpConnection.h"
#include "reactor/Acceptor.h"
#include "reactor/EventLoop.h"

#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

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
    // Reactor event loop
    // ========================================================

    EventLoop eventLoop(
        MAX_EVENTS
    );


    // ========================================================
    // Active TCP connections
    //
    // fd -> TcpConnection
    // ========================================================

    std::unordered_map<
        int,
        std::unique_ptr<TcpConnection>
    > connections;


    // ========================================================
    // Deferred close queue
    //
    // TcpConnection 的 callback 中不能立即销毁自己，
    // 所以先记录 fd。
    // ========================================================

    std::unordered_set<int>
        connectionsToClose;


    // ========================================================
    // Acceptor
    //
    // Acceptor 自己负责：
    //
    // socket()
    // bind()
    // listen()
    // accept4()
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
            // Defensive check
            // ------------------------------------------------

            if (connections.find(
                    clientFd
                ) != connections.end()) {

                std::cerr
                    << "Duplicate connection fd: "
                    << clientFd
                    << '\n';

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

                // 注意：
                //
                // 如果 TcpConnection 构造失败，
                // clientFd 仍然由上层负责关闭。
                //
                // 当前 Acceptor 已经把 fd 交给 callback，
                // 所以这里必须关闭。
                //
                close(
                    clientFd
                );

                return;
            }


            // =================================================
            // Complete metrics message callback
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
            // Close request callback
            // =================================================

            connection->setCloseCallback(
                [&connectionsToClose](
                    int fd
                ) {

                    // TcpConnection 可能正在执行：
                    //
                    // handleRead()
                    // Channel::handleEvent()
                    //
                    // 所以这里不能：
                    //
                    // connections.erase(fd)
                    //
                    // 只进行延迟销毁标记。
                    connectionsToClose.insert(
                        fd
                    );
                }
            );


            // =================================================
            // Store connection ownership
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
    // Main Reactor loop
    // ========================================================

    while (true) {

        // ----------------------------------------------------
        // epoll_wait()
        //
        // EventLoop
        //     ↓
        // Channel
        //     ↓
        // Callback
        // ----------------------------------------------------

        eventLoop.loopOnce();


        // ====================================================
        // Deferred connection cleanup
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


            // ------------------------------------------------
            // erase unique_ptr
            //       ↓
            // ~TcpConnection()
            //       ↓
            // EventLoop::removeChannel()
            //       ↓
            // close(fd)
            // ------------------------------------------------

            connections.erase(
                connectionIt
            );
        }


        connectionsToClose.clear();
    }
}
