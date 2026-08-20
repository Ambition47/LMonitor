#include "http/HttpServer.h"

#include "http/HttpResponse.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <utility>


#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>



namespace {

constexpr int BACKLOG = 16;

constexpr int BUFFER_SIZE = 4096;


}


// ============================================================
// Constructor
// ============================================================

HttpServer::HttpServer(
    uint16_t port,
    MetricsStore& store
)
    :
    port_(
        port
    ),
    store_(
        store
    ),
    running_(
        false
    ),
    listenFd_(
        -1
    )
{
}



// ============================================================
// Destructor
// ============================================================

HttpServer::~HttpServer()
{
    stop();
}



// ============================================================
// Start
// ============================================================

void HttpServer::start()
{

    if (running_) {

        return;
    }


    running_ =
        true;


    thread_ =
        std::thread(
            &HttpServer::run,
            this
        );
}



// ============================================================
// Stop
// ============================================================

void HttpServer::stop()
{

    if (!running_) {

        return;
    }


    running_ =
        false;


    if (listenFd_ >= 0) {

        close(
            listenFd_
        );

        listenFd_ =
            -1;
    }


    if (thread_.joinable()) {

        thread_.join();
    }
}



// ============================================================
// HTTP thread
// ============================================================

void HttpServer::run()
{

    // --------------------------------------------------------
    // socket()
    // --------------------------------------------------------

    listenFd_ =
        socket(
            AF_INET,
            SOCK_STREAM,
            0
        );


    if (listenFd_ < 0) {

        throw std::runtime_error(
            "HTTP socket create failed"
        );
    }



    int reuse = 1;


    setsockopt(
        listenFd_,
        SOL_SOCKET,
        SO_REUSEADDR,
        &reuse,
        sizeof(reuse)
    );



    // --------------------------------------------------------
    // bind()
    // --------------------------------------------------------

    sockaddr_in address {};

    address.sin_family =
        AF_INET;


    address.sin_addr.s_addr =
        INADDR_ANY;


    address.sin_port =
        htons(
            port_
        );



    if (bind(
            listenFd_,
            reinterpret_cast<sockaddr*>(
                &address
            ),
            sizeof(address)
        ) < 0) {


        throw std::runtime_error(
            "HTTP bind failed: "
            +
            std::string(
                strerror(
                    errno
                )
            )
        );
    }



    // --------------------------------------------------------
    // listen()
    // --------------------------------------------------------

    if (listen(
            listenFd_,
            BACKLOG
        ) < 0) {


        throw std::runtime_error(
            "HTTP listen failed"
        );
    }



    std::cout
        << "[HTTP] listening on port "
        << port_
        << std::endl;



    // --------------------------------------------------------
    // accept loop
    // --------------------------------------------------------

    while (running_) {


        sockaddr_in clientAddr {};

        socklen_t clientLen =
            sizeof(
                clientAddr
            );


        const int clientFd =
            accept(
                listenFd_,
                reinterpret_cast<sockaddr*>(
                    &clientAddr
                ),
                &clientLen
            );


        if (clientFd < 0) {


            if (!running_) {

                break;
            }


            continue;
        }



        char buffer[
            BUFFER_SIZE
        ] {};



        const ssize_t bytesRead =
            read(
                clientFd,
                buffer,
                sizeof(buffer)-1
            );



        if (bytesRead <= 0) {

            close(
                clientFd
            );

            continue;
        }



        std::string request(
            buffer,
            bytesRead
        );



        std::string response;



        // ====================================================
        // Route
        // ====================================================

        if (
            request.find(
                "GET /api/hosts"
            )
            !=
            std::string::npos
        ) {


            response =
                HttpResponse::ok(
                    buildHostsJson()
                );

        }
        else {


            response =
                HttpResponse::notFound();

        }



        send(
            clientFd,
            response.data(),
            response.size(),
            0
        );



        close(
            clientFd
        );
    }
}



// ============================================================
// Build JSON
// ============================================================

std::string HttpServer::buildHostsJson()
{

    auto hosts =
        store_.getAll();



    std::ostringstream json;



    json
        << "[";



    bool first =
        true;



    for (const auto& host :
         hosts) {


        if (!first) {

            json
                << ",";
        }


        first =
            false;



        const auto status =
            store_.getStatus(
                host
            );



        json
            << "{";


        json
            << "\"hostname\":\""
            << host.metrics.hostname
            << "\",";


        json
            << "\"status\":\""
            << MetricsStore::statusToString(
                    status
               )
            << "\",";


        json
            << "\"cpu\":"
            << host.metrics.cpuUsagePercent
            << ",";


        json
            << "\"memory\":"
            << host.metrics.memoryUsagePercent
            << ",";


        json
            << "\"load1\":"
            << host.metrics.load1;



        json
            << "}";

    }



    json
        << "]";



    return json.str();
}
