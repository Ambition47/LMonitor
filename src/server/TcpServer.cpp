#include "server/TcpServer.h"

#include "log/Logger.h"
#include "network/TcpConnection.h"
#include "reactor/Acceptor.h"
#include "reactor/Channel.h"
#include "reactor/EventLoop.h"
#include "serializer/MetricsDeserializer.h"
#include "store/MetricsStore.h"
#include "thread/ThreadPool.h"

#include <cerrno>
#include <cstddef>
#include <csignal>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

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
    : port_(
          port
      ) {
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
    // MetricsStore
    //
    // IMPORTANT:
    // MetricsStore is declared before ThreadPool so that
    // ThreadPool is destroyed first during shutdown.
    //
    // Worker tasks capture MetricsStore by reference.
    // Therefore all workers must finish before MetricsStore
    // itself is destroyed.
    // ========================================================

    MetricsStore metricsStore;


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


    Logger::instance().info(
        "Worker thread pool started: threads=" +
        std::to_string(
            threadPool.threadCount()
        ) +
        ", queue_capacity=" +
        std::to_string(
            threadPool.maxQueueSize()
        )
    );


    // ========================================================
    // Block SIGINT / SIGTERM
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

                        Logger::instance().info(
                            "Shutdown signal received, "
                            "stopping Reactor Server"
                        );


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
            &metricsStore,
            &connections
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

                Logger::instance().error(
                    "Duplicate connection fd: " +
                    std::to_string(
                        clientFd
                    )
                );


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

                Logger::instance().error(
                    "Failed to create TcpConnection: " +
                    std::string(
                        e.what()
                    )
                );


                close(
                    clientFd
                );


                return;
            }


            // =================================================
            // Complete metrics message callback
            //
            // Reactor thread:
            //     receives complete payload
            //
            // Worker thread:
            //     deserialize
            //     validate
            //     update MetricsStore
            //
            // Reactor thread:
            //     log lightweight processing result
            // =================================================

            connection->setMessageCallback(
                [
                    &threadPool,
                    &metricsStore,
                    &eventLoop
                ](
                    const std::string& name,
                    const std::string& message
                ) {

                    const std::string clientName =
                        name;


                    const std::string metricsMessage =
                        message;


                    const bool submitted =
                        threadPool.trySubmit(
                            [
                                &metricsStore,
                                &eventLoop,
                                clientName,
                                metricsMessage
                            ]() {

                                // =================================
                                // Worker:
                                // deserialize and validate payload
                                // =================================

                                SystemMetrics metrics;


                                try {

                                    MetricsDeserializer
                                        deserializer;


                                    metrics =
                                        deserializer.deserialize(
                                            metricsMessage
                                        );

                                } catch (
                                    const std::exception& e
                                ) {

                                    Logger::instance().warning(
                                        "Rejected invalid metrics from " +
                                        clientName +
                                        ": " +
                                        std::string(
                                            e.what()
                                        )
                                    );


                                    return;
                                }


                                // =================================
                                // Extract lightweight summary before
                                // moving the metrics object into store.
                                // =================================

                                const std::string hostname =
                                    metrics.hostname;


                                const double cpuUsagePercent =
                                    metrics.cpuUsagePercent;


                                const double memoryUsagePercent =
                                    metrics.memoryUsagePercent;


                                const double load1 =
                                    metrics.load1;


                                const std::size_t processCount =
                                    metrics.topProcesses.size();


                                // =================================
                                // Store latest metrics by hostname.
                                //
                                // Existing host:
                                //     overwrite previous snapshot
                                //
                                // New host:
                                //     insert new snapshot
                                // =================================

                                try {

                                    metricsStore.update(
                                        std::move(
                                            metrics
                                        )
                                    );

                                } catch (
                                    const std::exception& e
                                ) {

                                    Logger::instance().error(
                                        "Failed to store metrics from " +
                                        clientName +
                                        ": " +
                                        std::string(
                                            e.what()
                                        )
                                    );


                                    return;
                                }


                                const std::size_t monitoredHostCount =
                                    metricsStore.size();


                                // =================================
                                // Return lightweight result to Reactor
                                // =================================

                                eventLoop.queueInLoop(
                                    [
                                        clientName,
                                        hostname,
                                        cpuUsagePercent,
                                        memoryUsagePercent,
                                        load1,
                                        processCount,
                                        monitoredHostCount
                                    ]() {

                                        Logger::instance().info(
                                            "Metrics stored: client=" +
                                            clientName +
                                            ", host=" +
                                            hostname +
                                            ", cpu=" +
                                            std::to_string(
                                                cpuUsagePercent
                                            ) +
                                            "%, memory=" +
                                            std::to_string(
                                                memoryUsagePercent
                                            ) +
                                            "%, load1=" +
                                            std::to_string(
                                                load1
                                            ) +
                                            ", processes=" +
                                            std::to_string(
                                                processCount
                                            ) +
                                            ", monitored_hosts=" +
                                            std::to_string(
                                                monitoredHostCount
                                            )
                                        );
                                    }
                                );
                            }
                        );


                    // =========================================
                    // Backpressure
                    // =========================================

                    if (!submitted) {

                        Logger::instance().warning(
                            "Worker queue full, dropping metrics from " +
                            clientName +
                            ", pending=" +
                            std::to_string(
                                threadPool.queueSize()
                            ) +
                            "/" +
                            std::to_string(
                                threadPool.maxQueueSize()
                            )
                        );
                    }
                }
            );


            // =================================================
            // Connection close callback
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


                            Logger::instance().info(
                                "Client disconnected: " +
                                connectionIt
                                    ->second
                                    ->clientName() +
                                ", fd=" +
                                std::to_string(
                                    fd
                                )
                            );


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

                Logger::instance().error(
                    "Failed to store TcpConnection: fd=" +
                    std::to_string(
                        clientFd
                    )
                );


                return;
            }


            Logger::instance().info(
                "Client connected: " +
                clientName +
                ", fd=" +
                std::to_string(
                    clientFd
                )
            );
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
    // Reactor stopped
    // ========================================================

    eventLoop.removeChannel(
        &signalChannel
    );


    close(
        signalFd
    );


    Logger::instance().info(
        "LMonitor Reactor Server stopped gracefully: "
        "monitored_hosts=" +
        std::to_string(
            metricsStore.size()
        )
    );
}
