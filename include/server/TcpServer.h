#ifndef LMONITOR_TCP_SERVER_H
#define LMONITOR_TCP_SERVER_H


#include <cstdint>


class Config;


class TcpServer
{

public:

    explicit TcpServer(
        uint16_t port,
        Config& config
    );


    void run();



private:

    uint16_t port_ = 0;


    Config& config_;

};


#endif
