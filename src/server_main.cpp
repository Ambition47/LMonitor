#include "server/TcpServer.h"

#include "config/Config.h"
#include "log/Logger.h"

#include <cstdint>
#include <exception>
#include <iostream>
#include <string>


int main()
{

    try
    {

        // ====================================================
        // Load configuration
        // ====================================================

        Config config;


        const std::string configFile =
            "../config/lmonitor.conf";



        if(
            !config.load(
                configFile
            )
        )
        {

            std::cerr
                << "Failed to load configuration file: "
                << configFile
                << "\n";


            return 1;
        }



        Logger::instance().info(
            "LMonitor server starting..."
        );


        Logger::instance().info(
            "Configuration loaded successfully"
        );



        // ====================================================
        // Read server configuration
        // ====================================================

        const int tcpPort =
            config.getInt(
                "server.tcp_port",
                9000
            );



        const int httpPort =
            config.getInt(
                "server.http_port",
                8080
            );



        Logger::instance().info(
            "TCP server port: "
            +
            std::to_string(
                tcpPort
            )
        );


        Logger::instance().info(
            "HTTP server port: "
            +
            std::to_string(
                httpPort
            )
        );



        // ====================================================
        // Create server
        // ====================================================

        TcpServer server(
            static_cast<uint16_t>(
                tcpPort
            ),
            config
        );



        // ====================================================
        // Run server
        // ====================================================

        server.run();

    }
    catch(
        const std::exception& e
    )
    {

        Logger::instance().error(
            std::string(
                "Server exception: "
            )
            +
            e.what()
        );


        std::cerr
            << "Error: "
            << e.what()
            << '\n';


        return 1;

    }



    Logger::instance().info(
        "LMonitor server stopped"
    );


    return 0;
}
