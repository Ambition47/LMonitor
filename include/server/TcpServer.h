#ifndef LMONITOR_TCP_SERVER_H
#define LMONITOR_TCP_SERVER_H

#include <cstdint>


class TcpServer {
public:
    explicit TcpServer(
        uint16_t port
    );

    void run();


private:
    uint16_t port_ = 0;
};

#endif
