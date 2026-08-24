#include "agent/MonitorAgent.h"

#include "config/Config.h"

#include <csignal>
#include <iostream>
#include <stdexcept>
#include <string>


namespace
{

volatile std::sig_atomic_t g_running = 1;


void handleSignal(
    int signal
)
{
    if(
        signal == SIGINT ||
        signal == SIGTERM
    )
    {
        g_running = 0;
    }
}



bool registerSignalHandlers()
{
    struct sigaction action {};

    action.sa_handler =
        handleSignal;


    sigemptyset(
        &action.sa_mask
    );


    action.sa_flags = 0;



    if(
        sigaction(
            SIGINT,
            &action,
            nullptr
        )
        ==
        -1
    )
    {
        return false;
    }



    if(
        sigaction(
            SIGTERM,
            &action,
            nullptr
        )
        ==
        -1
    )
    {
        return false;
    }



    return true;
}

}



int main()
{

    try
    {

        // ====================================================
        // Load configuration
        // ====================================================

        Config config;


        if(
            !config.load(
                "../config/lmonitor.conf"
            )
        )
        {

            std::cerr
                <<
                "Failed to load configuration file\n";


            return 1;
        }



        // ====================================================
        // Read agent configuration
        // ====================================================

        const double interval =
            config.getDouble(
                "agent.interval",
                1.0
            );



        const std::string serverIp =
            config.get(
                "agent.server_ip",
                "127.0.0.1"
            );



        const uint16_t serverPort =
            static_cast<uint16_t>(
                config.getInt(
                    "agent.server_port",
                    9000
                )
            );



        std::cout
            << "Agent configuration:\n"
            << "  interval="
            << interval
            << "s\n"
            << "  server="
            << serverIp
            << ":"
            << serverPort
            << "\n";



        // ====================================================
        // Signal
        // ====================================================

        if(
            !registerSignalHandlers()
        )
        {

            std::cerr
                <<
                "Failed to register signal handlers\n";


            return 1;
        }



        // ====================================================
        // Create Agent
        // ====================================================

        MonitorAgent agent(
            interval,
            serverIp,
            serverPort
        );



        agent.run(
            g_running
        );



        std::cout
            <<
            "\nLMonitor Agent shutting down gracefully...\n";

    }
    catch(
        const std::exception& e
    )
    {

        std::cerr
            <<
            "Error: "
            <<
            e.what()
            <<
            '\n';


        return 1;
    }



    return 0;
}
