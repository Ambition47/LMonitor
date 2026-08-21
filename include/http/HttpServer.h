#ifndef LMONITOR_HTTP_SERVER_H
#define LMONITOR_HTTP_SERVER_H


#include "alert/AlertManager.h"

#include "store/MetricsHistoryStore.h"
#include "store/MetricsStore.h"


#include <atomic>
#include <cstdint>
#include <string>
#include <thread>



class HttpServer
{

public:


    HttpServer(
        uint16_t port,
        MetricsStore& store,
        MetricsHistoryStore& historyStore,
        AlertManager& alertManager
    );



    ~HttpServer();



    HttpServer(
        const HttpServer&
    ) = delete;



    HttpServer& operator=(
        const HttpServer&
    ) = delete;



    void start();



    void stop();



private:


    void run();



    void handleClient(
        int clientFd
    );



    std::string buildHostsJson();



    std::string buildHostDetailJson(
        const std::string& hostname
    );



    std::string buildHostHistoryJson(
        const std::string& hostname
    );



    std::string buildAlertsJson();



    std::string loadDashboardHtml() const;



    static std::string jsonEscape(
        const std::string& value
    );



    static bool sendAll(
        int fd,
        const std::string& data
    );



private:


    uint16_t port_;



    MetricsStore& store_;



    MetricsHistoryStore& historyStore_;



    AlertManager& alertManager_;



    std::atomic<bool> running_{
        false
    };



    int listenFd_ =
        -1;



    std::thread thread_;

};


#endif
