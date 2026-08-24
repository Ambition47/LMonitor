#include "server/TcpServer.h"

#include "config/Config.h"

#include "alert/AlertManager.h"

#include "http/HttpServer.h"

#include "log/Logger.h"

#include "network/TcpConnection.h"

#include "reactor/Acceptor.h"
#include "reactor/Channel.h"
#include "reactor/EventLoop.h"

#include "serializer/MetricsDeserializer.h"

#include "store/MetricsHistoryStore.h"

#include "store/MetricsStore.h"
#include "store/StateTracker.h"

#include "thread/ThreadPool.h"

#include "timer/TimerFd.h"


#include <cerrno>
#include <csignal>
#include <cstddef>
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



constexpr std::size_t WORKER_THREAD_COUNT =
    4;



constexpr std::size_t MAX_WORKER_QUEUE_SIZE =
    1024;


}


// ============================================================
// Constructor
// ============================================================


TcpServer::TcpServer(
    uint16_t port,
    Config& config
)
    :
    port_(
        port
    ),
    httpPort_(
        static_cast<uint16_t>(
            config.getInt(
                "server.http_port",
                8080
            )
        )
    ),
    config_(
        config
    )
{

}


// ============================================================
// Run Reactor Server
// ============================================================

void TcpServer::run()
{


    // ========================================================
    // Reactor EventLoop
    // ========================================================

    EventLoop eventLoop(
        MAX_EVENTS
    );



    // ========================================================
    // Global Metrics Storage
    // ========================================================

    MetricsStore metricsStore;

    MetricsHistoryStore historyStore(
    120
);
    AlertManager alertManager(
    config_
);



    // ========================================================
    // Host State Tracker
    // ========================================================

    StateTracker stateTracker;



    // ========================================================
    // HTTP API Server
    //
    // Port:
    //      8080
    //
    // Data source:
    //      MetricsStore
    // ========================================================

    HttpServer httpServer(
    httpPort_,
    metricsStore,
    historyStore,
    alertManager
);


    httpServer.start();



    // ========================================================
    // Worker Thread Pool
    // ========================================================

    const int workerThreads =
    config_.getInt(
        "worker.threads",
        WORKER_THREAD_COUNT
    );


const int workerQueueSize =
    config_.getInt(
        "worker.queue_size",
        MAX_WORKER_QUEUE_SIZE
    );



ThreadPool threadPool(
    workerThreads,
    workerQueueSize
);



    Logger::instance().info(

        "Worker thread pool started: threads="

        +

        std::to_string(
            threadPool.threadCount()
        )

        +

        ", queue_capacity="

        +

        std::to_string(
            threadPool.maxQueueSize()
        )
    );



    // ========================================================
    // TimerFd
    //
    // Every 5 seconds:
    //
    // Scan MetricsStore
    // Check ONLINE / STALE / OFFLINE
    // ========================================================

    TimerFd statusTimer;



    statusTimer.start(
        5
    );



    Channel timerChannel(
        statusTimer.fd()
    );



    timerChannel.setEvents(
        EPOLLIN
    );



    timerChannel.setReadCallback(

        [
            &metricsStore,
            &stateTracker
        ]()
        {


            // consume timer event

            auto hosts =
                metricsStore.getAll();



            for (const auto& host :
                 hosts)
            {


                const auto currentStatus =
                    metricsStore.getStatus(
                        host
                    );



                const auto stateChange =
                    stateTracker.update(

                        host.metrics.hostname,

                        currentStatus
                    );



                if (!stateChange.changed)
                {
                    continue;
                }



                Logger::instance().warning(

                    "Host status changed: host="

                    +

                    host.metrics.hostname

                    +

                    ", "

                    +

                    MetricsStore::statusToString(

                        stateChange.oldStatus
                    )

                    +

                    " -> "

                    +

                    MetricsStore::statusToString(

                        stateChange.newStatus
                    )
                );

            }

        }

    );



    eventLoop.addChannel(
        &timerChannel
    );



    // ========================================================
    // Signal handling
    // ========================================================


    sigset_t signalMask;



    if (
        sigemptyset(
            &signalMask
        )
        < 0
    )
    {
        throw std::runtime_error(
            "sigemptyset failed"
        );
    }



    sigaddset(
        &signalMask,
        SIGINT
    );


    sigaddset(
        &signalMask,
        SIGTERM
    );



    if (
        sigprocmask(
            SIG_BLOCK,
            &signalMask,
            nullptr
        )
        < 0
    )
    {
        throw std::runtime_error(
            "sigprocmask failed"
        );
    }



    const int signalFd =
        signalfd(

            -1,

            &signalMask,

            SFD_NONBLOCK |
            SFD_CLOEXEC
        );



    if (
        signalFd < 0
    )
    {
        throw std::runtime_error(
            "signalfd create failed"
        );
    }



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
        ]()
        {

            signalfd_siginfo signalInfo {};



            const ssize_t result =
                read(

                    signalFd,

                    &signalInfo,

                    sizeof(signalInfo)

                );



            if (
                result
                ==
                sizeof(signalInfo)
            )
            {

                if (
                    signalInfo.ssi_signo
                    ==
                    SIGINT

                    ||

                    signalInfo.ssi_signo
                    ==
                    SIGTERM
                )
                {

                    Logger::instance().info(

                        "Shutdown signal received"

                    );


                    eventLoop.quit();

                }

            }

        }

    );



    eventLoop.addChannel(
        &signalChannel
    );



    // ========================================================
    // Connection container
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



    Logger::instance().info(

        "LMonitor Reactor Server listening on port "

        +

        std::to_string(
            port_
        )

    );



    // 后续部分：
 // ========================================================
// New connection callback
// ========================================================

acceptor.setNewConnectionCallback(

    [
         &threadPool,
    &metricsStore,
    &historyStore,
    &alertManager,
    &stateTracker,
    &eventLoop,
    &connections
    ]
    (
        int clientFd,
        const std::string& clientName
    )
    {


        Logger::instance().info(

            "Client connected: "

            +

            clientName

            +

            ", fd="

            +

            std::to_string(
                clientFd
            )

        );



        std::unique_ptr<TcpConnection>
            connection;



        try
        {

            connection =
                std::make_unique<TcpConnection>(

                    eventLoop,

                    clientFd,

                    clientName

                );

        }
        catch(
            const std::exception& e
        )
        {

            Logger::instance().error(

                "Create TcpConnection failed: "

                +

                std::string(
                    e.what()
                )

            );


            close(
                clientFd
            );


            return;
        }



        // ====================================================
        // Receive metrics callback
        //
        // Reactor:
        //      receive TCP data
        //
        // Worker:
        //      deserialize
        //      update MetricsStore
        //
        // Reactor:
        //      print result
        // ====================================================


        connection->setMessageCallback(

            [
                &threadPool,
                &metricsStore,
		&historyStore,
		&alertManager,
                &stateTracker,
                &eventLoop
            ]
            (
                const std::string& name,
                const std::string& message
            )
            {


                const std::string clientName =
                    name;



                const std::string metricsMessage =
                    message;



                const bool submitted =

                    threadPool.trySubmit(

                        [

                            &metricsStore,

			    &historyStore,

			    &alertManager,

                            &stateTracker,

                            &eventLoop,

                            clientName,

                            metricsMessage

                        ]()
                        {


                            SystemMetrics metrics;



                            try
                            {

                                MetricsDeserializer deserializer;



                                metrics =

                                    deserializer.deserialize(

                                        metricsMessage

                                    );

                            }
                            catch(
                                const std::exception& e
                            )
                            {

                                Logger::instance().warning(

                                    "Invalid metrics from "

                                    +

                                    clientName

                                    +

                                    ": "

                                    +

                                    e.what()

                                );


                                return;
                            }




                            const std::string hostname =

                                metrics.hostname;



                            const double cpu =

                                metrics.cpuUsagePercent;



                            const double memory =

                                metrics.memoryUsagePercent;



                            const double load1 =

                                metrics.load1;



                            const std::size_t processCount =

                                metrics.topProcesses.size();




                            // ====================================
                            // Store latest snapshot
                            // ====================================


                            try
                            
			   {
                               
                                historyStore.add(
                                  hostname,
                                  cpu,
                                  memory
                                  );

                                // ========================================================
// Alert evaluation
// ========================================================

auto alerts =
    alertManager.update(
        metrics
    );


for(
    const auto& alert :
    alerts
)
{

    if(
        alert.state ==
        AlertState::Firing
    )
    {

        Logger::instance().warning(
            "Alert firing: "
            +
            alert.hostname
            +
            " "
            +
            alert.metric
        );

    }

}



// ========================================================
// Store latest metrics
// ========================================================

metricsStore.update(
    std::move(metrics)
);

                            }
                            catch(
                                const std::exception& e
                            )
                            {

                                Logger::instance().error(

                                    "Store metrics failed: "

                                    +

                                    std::string(
                                        e.what()
                                    )

                                );


                                return;
                            }





                            // ====================================
                            // First ONLINE state
                            // ====================================


                            const auto change =

                                stateTracker.update(

                                    hostname,

                                    MetricsStore::HostStatus::Online

                                );



                            if(change.changed)
                            {

                                Logger::instance().info(

                                    "Host state initialized: "

                                    +

                                    hostname

                                    +

                                    " -> "

                                    +

                                    MetricsStore::statusToString(

                                        change.newStatus

                                    )

                                );
                            }





                            // ====================================
                            // Return to Reactor thread
                            // ====================================


                            eventLoop.queueInLoop(

                                [

                                    clientName,

                                    hostname,

                                    cpu,

                                    memory,

                                    load1,

                                    processCount

                                ]()
                                {


                                    Logger::instance().info(

                                        "Metrics stored: host="

                                        +

                                        hostname

                                        +

                                        ", cpu="

                                        +

                                        std::to_string(
                                            cpu
                                        )

                                        +

                                        "%, memory="

                                        +

                                        std::to_string(
                                            memory
                                        )

                                        +

                                        "%, load1="

                                        +

                                        std::to_string(
                                            load1
                                        )

                                        +

                                        ", processes="

                                        +

                                        std::to_string(
                                            processCount
                                        )

                                    );

                                }

                            );


                        }

                    );





                if(!submitted)
                {

                    Logger::instance().warning(

                        "Worker queue full, drop metrics from "

                        +

                        clientName

                    );

                }

            }

        );






        // ====================================================
        // Connection close callback
        // ====================================================


        connection->setCloseCallback(

            [

                &connections

            ]

            (

                int fd

            )

            {


                auto iterator =

                    connections.find(
                        fd
                    );



                if(iterator != connections.end())
                {

                    Logger::instance().info(

                        "Client disconnected fd="

                        +

                        std::to_string(
                            fd
                        )

                    );


                    connections.erase(
                        iterator
                    );

                }

            }

        );





        // ====================================================
        // Save connection ownership
        // ====================================================


        connections.emplace(

            clientFd,

            std::move(
                connection
            )

        );



    }

);





// ============================================================
// Start Reactor loop
// ============================================================


eventLoop.loop();





// ============================================================
// Shutdown
// ============================================================


Logger::instance().info(

    "Stopping LMonitor Server..."

);





// HTTP shutdown

httpServer.stop();





// Timer shutdown

statusTimer.stop();





eventLoop.removeChannel(

    &timerChannel

);



eventLoop.removeChannel(

    &signalChannel

);



close(

    signalFd

);





Logger::instance().info(

    "LMonitor Server stopped gracefully"

);


}
