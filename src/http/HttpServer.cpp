#include "http/HttpServer.h"

#include "alert/AlertEvaluator.h"
#include "http/HttpResponse.h"
#include "log/Logger.h"

#include <arpa/inet.h>

#include <cerrno>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include <netinet/in.h>

#include <sys/socket.h>
#include <sys/types.h>

#include <unistd.h>


namespace {

constexpr int BACKLOG = 64;

constexpr std::size_t BUFFER_SIZE = 8192;

}


// ============================================================
// Constructor
// ============================================================

HttpServer::HttpServer(
    uint16_t port,
    MetricsStore& store,
    MetricsHistoryStore& historyStore,
    AlertManager& alertManager
)
    :
    port_(port),
    store_(store),
    historyStore_(historyStore),
    alertManager_(alertManager)
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
    bool expected = false;


    if (
        !running_.compare_exchange_strong(
            expected,
            true
        )
    )
    {
        return;
    }


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
    running_.store(false);


    if(listenFd_ >= 0)
    {
        shutdown(
            listenFd_,
            SHUT_RDWR
        );


        close(
            listenFd_
        );


        listenFd_ = -1;
    }


    if(thread_.joinable())
    {
        thread_.join();
    }
}



// ============================================================
// HTTP server main loop
// ============================================================

void HttpServer::run()
{
    try
    {

        listenFd_ =
            socket(
                AF_INET,
                SOCK_STREAM | SOCK_CLOEXEC,
                0
            );


        if(listenFd_ < 0)
        {
            throw std::runtime_error(
                "HTTP socket failed"
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


        sockaddr_in address{};


        address.sin_family =
            AF_INET;


        address.sin_addr.s_addr =
            htonl(
                INADDR_ANY
            );


        address.sin_port =
            htons(
                port_
            );


        if(
            bind(
                listenFd_,
                reinterpret_cast<sockaddr*>(
                    &address
                ),
                sizeof(address)
            ) < 0
        )
        {
            throw std::runtime_error(
                "HTTP bind failed"
            );
        }



        if(
            listen(
                listenFd_,
                BACKLOG
            ) < 0
        )
        {
            throw std::runtime_error(
                "HTTP listen failed"
            );
        }



        Logger::instance().info(
            "HTTP listening on port "
            +
            std::to_string(
                port_
            )
        );



        while(
            running_
        )
        {

            sockaddr_in clientAddress{};


            socklen_t clientLength =
                sizeof(
                    clientAddress
                );


            int clientFd =
                accept4(
                    listenFd_,
                    reinterpret_cast<sockaddr*>(
                        &clientAddress
                    ),
                    &clientLength,
                    SOCK_CLOEXEC
                );


            if(clientFd < 0)
            {
                if(errno == EINTR)
                {
                    continue;
                }


                break;
            }


            handleClient(
                clientFd
            );


            close(
                clientFd
            );
        }

    }
    catch(
        const std::exception& e
    )
    {

        Logger::instance().error(
            e.what()
        );

    }


    running_.store(false);
}



// ============================================================
// Handle request
// ============================================================

void HttpServer::handleClient(
    int clientFd
)
{

    char buffer[
        BUFFER_SIZE
    ]{};



    ssize_t length =
        recv(
            clientFd,
            buffer,
            sizeof(buffer)-1,
            0
        );



    if(length <= 0)
    {
        return;
    }



    std::string request(
        buffer,
        static_cast<size_t>(
            length
        )
    );



    auto lineEnd =
        request.find(
            "\r\n"
        );



    if(
        lineEnd ==
        std::string::npos
    )
    {
        sendAll(
            clientFd,
            HttpResponse::badRequest()
        );

        return;
    }



    std::istringstream stream(
        request.substr(
            0,
            lineEnd
        )
    );


    std::string method;

    std::string path;

    std::string version;


    stream
        >>
        method
        >>
        path
        >>
        version;



    if(
        method != "GET"
    )
    {
        sendAll(
            clientFd,
            HttpResponse::badRequest()
        );

        return;
    }



    std::string response;



    // ========================================================
    // Dashboard
    // ========================================================

    if(
        path == "/"
        ||
        path == "/index.html"
    )
    {

        response =
            HttpResponse::okHtml(
                loadDashboardHtml()
            );

    }



    // ========================================================
    // Alerts
    // ========================================================

    else if(
        path ==
        "/api/alerts"
    )
    {

        response =
            HttpResponse::okJson(
                buildAlertsJson()
            );

    }



    // ========================================================
    // Hosts
    // ========================================================

    else if(
        path ==
        "/api/hosts"
    )
    {

        response =
            HttpResponse::okJson(
                buildHostsJson()
            );

    }

    // ========================================================
// Host history
//
// /api/hosts/{hostname}/history
// ========================================================

else if(
    path.rfind(
        "/api/hosts/",
        0
    ) == 0
    &&
    path.find(
        "/history"
    )
    != std::string::npos
)
{

    const std::string prefix =
        "/api/hosts/";


    const std::string suffix =
        "/history";


    const std::size_t start =
        prefix.size();


    const std::size_t end =
        path.find(
            suffix
        );


    std::string hostname =
        path.substr(
            start,
            end-start
        );



    response =
        HttpResponse::okJson(
            buildHostHistoryJson(
                hostname
            )
        );

}




// ========================================================
// Host detail
//
// /api/hosts/{hostname}
// ========================================================

else if(
    path.rfind(
        "/api/hosts/",
        0
    ) == 0
)
{

    std::string hostname =
        path.substr(
            std::string(
                "/api/hosts/"
            ).size()
        );


    response =
        HttpResponse::okJson(
            buildHostDetailJson(
                hostname
            )
        );

}




else
{

    response =
        HttpResponse::notFound();

}




sendAll(
    clientFd,
    response
);


}



// ============================================================
// Hosts JSON
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



    for(
        const auto& host :
        hosts
    )
    {

        if(!first)
        {
            json << ",";
        }


        first = false;



        json
            << "{";



        json
            << "\"hostname\":\""
            << jsonEscape(
                host.metrics.hostname
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
            << host.metrics.load1
            << ",";



        json
            << "\"uptime\":"
            << host.metrics.uptimeSeconds;



        json
            << "}";

    }



    json
        << "]";


    return json.str();

}



// ============================================================
// Host detail JSON
// ============================================================

std::string HttpServer::buildHostDetailJson(
    const std::string& hostname
)
{

    MetricsStore::StoredMetrics stored;



    if(
        !store_.getLatest(
            hostname,
            stored
        )
    )
    {
        return "{}";
    }



    const auto& metrics =
        stored.metrics;



    std::ostringstream json;



    json
        << "{";



    json
        << "\"hostname\":\""
        << jsonEscape(
            metrics.hostname
        )
        << "\",";



    json
        << "\"cpu\":"
        << metrics.cpuUsagePercent
        << ",";



    json
        << "\"memory\":"
        << metrics.memoryUsagePercent
        << ",";



    json
        << "\"uptime_seconds\":"
        << metrics.uptimeSeconds
        << ",";



    json
        << "\"logical_cpus\":"
        << metrics.logicalCpuCount
        << ",";



    json
        << "\"load\":{";


    json
        << "\"load1\":"
        << metrics.load1
        << ",";


    json
        << "\"load5\":"
        << metrics.load5
        << ",";


    json
        << "\"load15\":"
        << metrics.load15;



    json
        << "},";



    json
        << "\"disk\":{";


    json
        << "\"usage_percent\":"
        << metrics.diskUsagePercent;


    json
        << "},";



    json
        << "\"network\":{";


    json
        << "\"interface\":\""
        << jsonEscape(
            metrics.networkInterface
        )
        << "\"";


    json
        << "}";



    json
        << "}";



    return json.str();

}



// ============================================================
// History JSON
// ============================================================

std::string HttpServer::buildHostHistoryJson(
    const std::string& hostname
)
{

    auto history =
        historyStore_.get(
            hostname
        );



    std::ostringstream json;


    json
        << "[";



    bool first =
        true;



    for(
        const auto& point :
        history
    )
    {

        if(!first)
        {
            json
                << ",";
        }


        first =
            false;



        json
            << "{";


        json
            << "\"timestamp\":"
            << point.timestampMs
            << ",";


        json
            << "\"cpu\":"
            << point.cpuUsagePercent
            << ",";


        json
            << "\"memory\":"
            << point.memoryUsagePercent;



        json
            << "}";

    }



    json
        << "]";


    return json.str();

}

// ============================================================
// Alerts JSON
//
// GET /api/alerts
// ============================================================

std::string HttpServer::buildAlertsJson()
{

    const auto alerts =
        alertManager_.getActiveAlerts();


    std::ostringstream json;



    json
        << "[";



    bool first =
        true;



    for(
        const auto& alert :
        alerts
    )
    {


            if(!first)
            {
                json
                    << ",";
            }


            first =
                false;



            json
                << "{";



            json
                << "\"hostname\":\""
                << jsonEscape(
                    alert.hostname
                )
                << "\",";



            json
                << "\"metric\":\""
                << jsonEscape(
                    alert.metric
                )
                << "\",";



            json
                << "\"state\":\"";


	    switch(
            alert.state
        )
        {

            case AlertState::Pending:

                json
                    << "PENDING";

                break;



            case AlertState::Firing:

                json
                    << "FIRING";

                break;



            default:

                json
                    << "NORMAL";

                break;

        }



        json
            << "\",";



        json
            << "\"current_value\":"
            << alert.currentValue
            << ",";



        json
            << "\"threshold\":"
            << alert.threshold
            << ",";



        json
            << "\"message\":\""
            << jsonEscape(
                alert.message
            )
            << "\"";



        json
            << "}";

    }



    json
        << "]";



    return json.str();


    }





// ============================================================
// Load dashboard HTML
// ============================================================

std::string HttpServer::loadDashboardHtml() const
{

    const std::string paths[] =
    {
        "../web/index.html",

        "web/index.html",

        "./web/index.html"
    };



    for(
        const auto& path :
        paths
    )
    {

        std::ifstream file(
            path
        );



        if(
            !file.is_open()
        )
        {
            continue;
        }



        std::ostringstream buffer;


        buffer
            << file.rdbuf();



        return buffer.str();

    }



    return "";

}





// ============================================================
// JSON escape
// ============================================================

std::string HttpServer::jsonEscape(
    const std::string& value
)
{

    std::string result;



    result.reserve(
        value.size()
    );



    for(
        char c :
        value
    )
    {

        switch(c)
        {

            case '\\':

                result += "\\\\";

                break;



            case '"':

                result += "\\\"";

                break;



            case '\n':

                result += "\\n";

                break;



            case '\r':

                result += "\\r";

                break;



            case '\t':

                result += "\\t";

                break;



            default:

                result += c;

                break;

        }

    }



    return result;

}





// ============================================================
// Send all data
// ============================================================

bool HttpServer::sendAll(
    int fd,
    const std::string& data
)
{

    std::size_t sent =
        0;



    while(
        sent <
        data.size()
    )
    {

        ssize_t result =
            send(
                fd,
                data.data()
                    + sent,
                data.size()
                    - sent,
                MSG_NOSIGNAL
            );



        if(
            result > 0
        )
        {

            sent +=
                static_cast<std::size_t>(
                    result
                );


            continue;

        }



        if(
            result < 0 &&
            errno == EINTR
        )
        {
            continue;
        }



        return false;

    }



    return true;

}
