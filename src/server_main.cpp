#include "server/TcpServer.h"

#include <exception>
#include <iostream>


int main() {
    try {
        constexpr uint16_t port =
            9000;

        TcpServer server(
            port
        );

        server.run();

    } catch (const std::exception& e) {
        std::cerr
            << "Error: "
            << e.what()
            << '\n';

        return 1;
    }

    return 0;
}
