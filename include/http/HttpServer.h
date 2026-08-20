#ifndef LMONITOR_HTTP_SERVER_H
#define LMONITOR_HTTP_SERVER_H


#include "store/MetricsStore.h"


#include <cstdint>
#include <memory>
#include <thread>



class HttpServer {

public:

    HttpServer(
        uint16_t port,
        MetricsStore& store
    );


    ~HttpServer();



    void start();


    void stop();



private:

    void run();



    std::string buildHostsJson();


private:

    uint16_t port_;


    MetricsStore& store_;


    bool running_;


    int listenFd_;


    std::thread thread_;

};


#endif
