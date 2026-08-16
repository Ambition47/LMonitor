#include "server/TcpServer.h"

#include <cerrno>
#include <iostream>
#include <stdexcept>
#include <string>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>


TcpServer::TcpServer(
    uint16_t port
)
    : port_(port) {

    // 创建 IPv4 TCP Socket
    serverFd_ =
        socket(
            AF_INET,
            SOCK_STREAM,
            0
        );

    if (serverFd_ < 0) {
        throw std::runtime_error(
            "Failed to create server socket"
        );
    }


    // 允许服务端重启后快速重新绑定端口
    int reuse = 1;

    if (setsockopt(
            serverFd_,
            SOL_SOCKET,
            SO_REUSEADDR,
            &reuse,
            sizeof(reuse)
        ) < 0) {

        close(serverFd_);
        serverFd_ = -1;

        throw std::runtime_error(
            "Failed to set SO_REUSEADDR"
        );
    }


    // 配置监听地址
    sockaddr_in serverAddress {};

    serverAddress.sin_family =
        AF_INET;

    serverAddress.sin_addr.s_addr =
        htonl(INADDR_ANY);

    serverAddress.sin_port =
        htons(port_);


    // 绑定 IP + Port
    if (bind(
            serverFd_,
            reinterpret_cast<sockaddr*>(
                &serverAddress
            ),
            sizeof(serverAddress)
        ) < 0) {

        close(serverFd_);
        serverFd_ = -1;

        throw std::runtime_error(
            "Failed to bind server socket"
        );
    }


    // 开始监听
    if (listen(
            serverFd_,
            16
        ) < 0) {

        close(serverFd_);
        serverFd_ = -1;

        throw std::runtime_error(
            "Failed to listen on server socket"
        );
    }
}


TcpServer::~TcpServer() {
    if (serverFd_ >= 0) {
        close(serverFd_);
    }
}


void TcpServer::run() {
    std::cout
        << "LMonitor TCP Server listening on port "
        << port_
        << "...\n";


    // ========================================================
    // Server 主循环
    // ========================================================

    while (true) {

        sockaddr_in clientAddress {};

        socklen_t clientAddressLength =
            sizeof(clientAddress);


        // ----------------------------------------------------
        // 等待客户端连接
        // ----------------------------------------------------

        std::cout
            << "\nWaiting for client connection...\n";


        const int clientFd =
            accept(
                serverFd_,
                reinterpret_cast<sockaddr*>(
                    &clientAddress
                ),
                &clientAddressLength
            );


        if (clientFd < 0) {

            if (errno == EINTR) {
                continue;
            }

            throw std::runtime_error(
                "Failed to accept client connection"
            );
        }


        // ----------------------------------------------------
        // 打印客户端地址
        // ----------------------------------------------------

        char clientIp[INET_ADDRSTRLEN] {};

        inet_ntop(
            AF_INET,
            &clientAddress.sin_addr,
            clientIp,
            sizeof(clientIp)
        );


        std::cout
            << "Client connected: "
            << clientIp
            << ":"
            << ntohs(
                clientAddress.sin_port
            )
            << '\n';


        // 保存尚未组成完整消息的数据
        std::string pendingData;

        char buffer[4096];


        // ====================================================
        // 当前客户端接收循环
        // ====================================================

        while (true) {

            const ssize_t bytesReceived =
                recv(
                    clientFd,
                    buffer,
                    sizeof(buffer),
                    0
                );


            if (bytesReceived > 0) {

                // TCP 是字节流：
                // 本次收到的数据先追加到 pendingData
                pendingData.append(
                    buffer,
                    static_cast<std::size_t>(
                        bytesReceived
                    )
                );


                const std::string delimiter =
                    "END\n";


                // --------------------------------------------
                // 一次 recv 可能包含多条完整消息
                // 所以需要循环解析
                // --------------------------------------------

                while (true) {

                    const std::size_t delimiterPosition =
                        pendingData.find(
                            delimiter
                        );


                    if (delimiterPosition ==
                        std::string::npos) {

                        // 当前还没有收到一条完整消息
                        break;
                    }


                    const std::size_t messageLength =
                        delimiterPosition +
                        delimiter.size();


                    const std::string message =
                        pendingData.substr(
                            0,
                            messageLength
                        );


                    // 从 pendingData 中删除已处理消息
                    pendingData.erase(
                        0,
                        messageLength
                    );


                    std::cout
                        << "\n========== Metrics Received ==========\n";

                    std::cout
                        << message;

                    std::cout
                        << "======================================\n";
                }

            } else if (bytesReceived == 0) {

                // 客户端正常关闭连接
                std::cout
                    << "\nClient disconnected: "
                    << clientIp
                    << '\n';

                break;

            } else {

                // recv 被信号打断，可以继续
                if (errno == EINTR) {
                    continue;
                }

                std::cerr
                    << "\nReceive error from client: "
                    << clientIp
                    << '\n';

                break;
            }
        }


        // ----------------------------------------------------
        // 当前客户端结束
        // ----------------------------------------------------

        close(clientFd);

        std::cout
            << "Client socket closed.\n";

        // while(true) 会重新回到 accept()
    }
}
