#include "server/TcpServer.h"
#include "log/Logger.h"


#include "network/TcpConnection.h"
#include "reactor/Acceptor.h"
#include "reactor/Channel.h"
#include "reactor/EventLoop.h"
#include "thread/ThreadPool.h"

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <csignal>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>

#include <sys/epoll.h>
#include <sys/signalfd.h>
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
// Reactor Server
// ============================================================

void TcpServer::run() {

    // ========================================================
    // EventLoop
    // ========================================================

    EventLoop eventLoop(
        MAX_EVENTS
    );


    // ========================================================
    // Record Reactor thread id
    // ========================================================

    const std::thread::id eventLoopThreadId =
        std::this_thread::get_id();


    std::cout
        << "[Reactor] EventLoop thread id: "
        << eventLoopThreadId
        << '\n';


    // ========================================================
    // Worker ThreadPool
    // ========================================================

    constexpr std::size_t WORKER_THREAD_COUNT =
        4;


    constexpr std::size_t MAX_WORKER_QUEUE_SIZE =
        1024;


    ThreadPool threadPool(
        WORKER_THREAD_COUNT,
        MAX_WORKER_QUEUE_SIZE
    );


    std::cout
        << "Worker thread pool started with "
        << threadPool.threadCount()
        << " threads, queue capacity="
        << threadPool.maxQueueSize()
        << ".\n";


    // ========================================================
    // Block SIGINT / SIGTERM
    //
    // These signals will be handled through signalfd
    // instead of traditional asynchronous signal handlers.
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


    // ========================================================
    // Create signalfd
    // ========================================================

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


    // ========================================================
    // Signal Channel
    // ========================================================

    Channel signalChannel(
        signalFd
    );


    signalChannel.setEvents(
        EPOLLIN
    );


    signalChannel.setReadCallback(
        [
            &eventLoop,
            signalFd
        ]() {

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


                    Logger::instance().error(
    "Failed to read signalfd"
);

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
    // Active TCP connections
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
        [
            &eventLoop,
            &threadPool,
            &connections,
            eventLoopThreadId
        ](
            int clientFd,
            const std::string& clientName
        ) {

            // ------------------------------------------------
            // Defensive duplicate fd check
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


            // =================================================
            // Create TcpConnection
            // =================================================

            std::unique_ptr<TcpConnection>
                connection;


            try {

                connection =
                    std::make_unique<TcpConnection>(
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


                close(
                    clientFd
                );


                return;
            }


            // =================================================
            // Complete metrics message callback
            //
            // This callback runs in the EventLoop thread.
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

                    // =========================================
                    // Verify that network callback is running
                    // in Reactor thread.
                    // =========================================

                    const std::thread::id currentThreadId =
                        std::this_thread::get_id();


                    std::cout
                        << "[I/O] Message received on thread: "
                        << currentThreadId
                        << "  EventLoop thread: "
                        << eventLoopThreadId
                        << '\n';


                    // =========================================
                    // Copy message data before handing it to
                    // a Worker thread.
                    //
                    // Never capture these references directly.
                    // =========================================

                    const std::string clientName =
                        name;


                    const std::string metricsMessage =
                        message;


                    // =========================================
                    // Submit to bounded worker queue.
                    // =========================================

                    const bool submitted =
                        threadPool.trySubmit(
                            [
                                &eventLoop,
                                clientName,
                                metricsMessage,
                                eventLoopThreadId
                            ]() {

                                // =================================
                                // Worker Thread
                                // =================================

                                const std::thread::id workerThreadId =
                                    std::this_thread::get_id();


                                std::cout
                                    << "[Worker] Processing metrics on thread: "
                                    << workerThreadId
                                    << '\n';


                                // =================================
                                // Temporary worker processing.
                                //
                                // Later this can become:
                                //
                                // protocol parsing
                                // aggregation
                                // database storage
                                // alert evaluation
                                // =================================

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


                                // =================================
                                // Return result to EventLoop thread.
                                //
                                // queueInLoop()
                                //     ↓
                                // eventfd
                                //     ↓
                                // epoll wakeup
                                //     ↓
                                // Reactor thread
                                // =================================

                                eventLoop.queueInLoop(
                                    [
                                        clientName,
                                        metricsMessage,
                                        messageBytes,
                                        lineCount,
                                        workerThreadId,
                                        eventLoopThreadId
                                    ]() {

                                        const std::thread::id
                                            callbackThreadId =
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


                    // =========================================
                    // Backpressure
                    //
                    // Worker queue is full:
                    //
                    // do NOT block Reactor thread.
                    //
                    // Drop this metrics sample instead.
                    // =========================================

                    if (!submitted) {

                        std::cerr
                            << "[Backpressure] Worker queue full. "
                            << "Dropping metrics from "
                            << clientName
                            << ". Pending tasks="
                            << threadPool.queueSize()
                            << "/"
                            << threadPool.maxQueueSize()
                            << '\n';
                    }
                }
            );


            // =================================================
            // Connection close callback
            //
            // TcpConnection cannot destroy itself directly
            // while Channel callback is still running.
            //
            // Therefore destruction is deferred through
            // EventLoop::queueInLoop().
            // =================================================

            connection->setCloseCallback(
                [
                    &eventLoop,
                    &connections
                ](
                    int fd
                ) {

                    eventLoop.queueInLoop(
                        [
                            &connections,
                            fd
                        ]() {

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


                            // ---------------------------------
                            // Destroying unique_ptr causes:
                            //
                            // ~TcpConnection()
                            //      ↓
                            // removeChannel()
                            //      ↓
                            // close(clientFd)
                            // ---------------------------------

                            connections.erase(
                                connectionIt
                            );
                        }
                    );
                }
            );


            // =================================================
            // Store TcpConnection ownership
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


    // ========================================================
    // Start Reactor
    // ========================================================

    Logger::instance().info(
    "LMonitor Reactor Server listening on port " +
    std::to_string(
        port_
    )
);


    eventLoop.loop();


    // ========================================================
    // Reactor stopped.
    //
    // Remove signalfd Channel before closing signalFd.
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
