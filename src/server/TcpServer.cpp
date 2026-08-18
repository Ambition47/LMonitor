#include "server/TcpServer.h"
#include "thread/ThreadPool.h"

#include "network/TcpConnection.h"
#include "reactor/Acceptor.h"
#include "reactor/Channel.h"
#include "reactor/EventLoop.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <cerrno>
#include <csignal>
#include <stdexcept>

#include <thread>

#include <sys/epoll.h>
#include <unistd.h>
#include <sys/signalfd.h>


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

    const std::thread::id eventLoopThreadId =
    std::this_thread::get_id();


std::cout
    << "[Reactor] EventLoop thread id: "
    << eventLoopThreadId
    << '\n';

    constexpr std::size_t WORKER_THREAD_COUNT =
    4;


ThreadPool threadPool(
    WORKER_THREAD_COUNT
);


std::cout
    << "Worker thread pool started with "
    << threadPool.threadCount()
    << " threads.\n";

    // ========================================================
// Block SIGINT / SIGTERM.
//
// Signals will no longer invoke asynchronous signal
// handlers. They will be received through signalfd instead.
// ========================================================

sigset_t signalMask;

if (sigemptyset(
        &signalMask
    ) < 0) {

    throw std::runtime_error(
        "Failed to initialize signal mask"
    );
}


if (sigaddset(
        &signalMask,
        SIGINT
    ) < 0) {

    throw std::runtime_error(
        "Failed to add SIGINT to signal mask"
    );
}


if (sigaddset(
        &signalMask,
        SIGTERM
    ) < 0) {

    throw std::runtime_error(
        "Failed to add SIGTERM to signal mask"
    );
}


if (sigprocmask(
        SIG_BLOCK,
        &signalMask,
        nullptr
    ) < 0) {

    throw std::runtime_error(
        "Failed to block server signals"
    );
}

const int signalFd =
    signalfd(
        -1,
        &signalMask,
        SFD_NONBLOCK |
        SFD_CLOEXEC
    );


if (signalFd < 0) {
    throw std::runtime_error(
        "Failed to create signalfd"
    );
}

Channel signalChannel(
    signalFd
);


signalChannel.setEvents(
    EPOLLIN
);

signalChannel.setReadCallback(
    [&eventLoop, signalFd]() {

        while (true) {

            signalfd_siginfo signalInfo {};

            const ssize_t bytesRead =
                read(
                    signalFd,
                    &signalInfo,
                    sizeof(signalInfo)
                );


            if (bytesRead ==
                static_cast<ssize_t>(
                    sizeof(signalInfo)
                )) {

                if (signalInfo.ssi_signo ==
                        SIGINT ||
                    signalInfo.ssi_signo ==
                        SIGTERM) {

                    std::cout
                        << "\nShutdown signal received. "
                        << "Stopping Reactor Server...\n";


                    eventLoop.quit();

                    return;
                }


                continue;
            }


            if (bytesRead < 0) {

                if (errno == EINTR) {
                    continue;
                }


                if (errno == EAGAIN ||
                    errno == EWOULDBLOCK) {

                    return;
                }


                std::cerr
                    << "Failed to read signalfd\n";

                return;
            }


            return;
        }
    }
);

eventLoop.addChannel(
    &signalChannel
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
    [
        &threadPool,
        &eventLoop,
        eventLoopThreadId
    ](
        const std::string& name,
        const std::string& message
    ) {





    const std::thread::id currentThreadId =
            std::this_thread::get_id();


        std::cout
            << "[I/O] Message received on thread: "
            << currentThreadId
            << "  EventLoop thread: "
            << eventLoopThreadId
            << '\n';
        // ====================================================
        // Important:
        //
        // TcpConnection callback runs in EventLoop thread.
        //
        // Copy name/message into the worker task so that
        // the worker does not depend on TcpConnection lifetime.
        // ====================================================

        const std::string clientName =
            name;

        const std::string metricsMessage =
            message;


        threadPool.submit(
            [
                &eventLoop,
                clientName,
                metricsMessage,
		eventLoopThreadId
            ]() {

	    const std::thread::id workerThreadId =
            std::this_thread::get_id();


        std::cout
            << "[Worker] Processing metrics on thread: "
            << workerThreadId
            << '\n';

                // ============================================
                // Worker Thread
                //
                // For now perform a small piece of processing:
                //
                // 1. count bytes
                // 2. count lines
                //
                // Later this location can perform:
                //
                // protocol parsing
                // aggregation
                // database persistence
                // alert calculation
                // ============================================

                const std::size_t messageBytes =
                    metricsMessage.size();


                const std::size_t lineCount =
                    static_cast<std::size_t>(
                        std::count(
                            metricsMessage.begin(),
                            metricsMessage.end(),
                            '\n'
                        )
                    );


                // ============================================
                // Return result to EventLoop thread
                // ============================================

                eventLoop.queueInLoop(
                    [
                        clientName,
                        metricsMessage,
                        messageBytes,
                        lineCount,
			workerThreadId,
                       eventLoopThreadId
                    ]() {

		    const std::thread::id callbackThreadId =
    std::this_thread::get_id();


std::cout
    << "[Reactor Callback] thread: "
    << callbackThreadId
    << "  worker was: "
    << workerThreadId
    << "  expected EventLoop: "
    << eventLoopThreadId
    << '\n';

                        std::cout
                            << "\n"
                            << "========== Metrics Processed ==========\n";

                        std::cout
                            << "Client: "
                            << clientName
                            << '\n';


                        std::cout
                            << "Message bytes: "
                            << messageBytes
                            << '\n';


                        std::cout
                            << "Message lines: "
                            << lineCount
                            << '\n';


                        std::cout
                            << metricsMessage;


                        std::cout
                            << "=======================================\n";
                    }
                );
            }
        );
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


// ========================================================
// Reactor has stopped.
//
// Remove signal Channel before closing its fd.
// ========================================================

eventLoop.removeChannel(
    &signalChannel
);


close(
    signalFd
);


std::cout
    << "LMonitor Reactor Server stopped gracefully.\n";
}
