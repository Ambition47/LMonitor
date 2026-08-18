#ifndef LMONITOR_TCP_CLIENT_H
#define LMONITOR_TCP_CLIENT_H

#include <cstdint>
#include <string>


class TcpClient {
public:
    TcpClient(
        std::string serverIp,
        uint16_t serverPort
    );

    ~TcpClient();


    TcpClient(
        const TcpClient&
    ) = delete;

    TcpClient& operator=(
        const TcpClient&
    ) = delete;


    bool connectToServer();

    void disconnect();

    bool isConnected() const;


    bool sendAll(
        const std::string& data
    );


private:
    int createSocket() const;


private:
    std::string serverIp_;

    uint16_t serverPort_ = 0;

    int socketFd_ = -1;
};

#endif
