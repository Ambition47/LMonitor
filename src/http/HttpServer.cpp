#include "http/HttpServer.h"

#include "http/HttpResponse.h"
#include "log/Logger.h"

#include <arpa/inet.h>

#include <cerrno>
#include <cstring>
#include <exception>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#include <netinet/in.h>

#include <sys/socket.h>
#include <sys/types.h>

#include <unistd.h>


namespace {

constexpr int BACKLOG =
    64;


constexpr std::size_t BUFFER_SIZE =
    8192;

}  // namespace


// ============================================================
// Constructor
// ============================================================

HttpServer::HttpServer(
    uint16_t port,
    MetricsStore& store
)
    : port_(
          port
      ),
      store_(
          store
      ) {
}


// ============================================================
// Destructor
// ============================================================

HttpServer::~HttpServer() {
    stop();
}


// ============================================================
// Start HTTP server thread
// ============================================================

void HttpServer::start() {
    bool expected =
        false;


    if (!running_.compare_exchange_strong(
            expected,
            true
        )) {

        return;
    }


    thread_ =
        std::thread(
            &HttpServer::run,
            this
        );
}


// ============================================================
// Stop HTTP server
// ============================================================

void HttpServer::stop() {
    running_.store(
        false
    );


    if (listenFd_ >= 0) {

        shutdown(
            listenFd_,
            SHUT_RDWR
        );


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
// HTTP server main thread
// ============================================================

void HttpServer::run() {
    try {

        // ----------------------------------------------------
        // Create socket
        // ----------------------------------------------------

        listenFd_ =
            socket(
                AF_INET,
                SOCK_STREAM |
                SOCK_CLOEXEC,
                0
            );


        if (listenFd_ < 0) {

            throw std::runtime_error(
                "HTTP socket creation failed: " +
                std::string(
                    std::strerror(
                        errno
                    )
                )
            );
        }


        // ----------------------------------------------------
        // SO_REUSEADDR
        // ----------------------------------------------------

        int reuse =
            1;


        if (setsockopt(
                listenFd_,
                SOL_SOCKET,
                SO_REUSEADDR,
                &reuse,
                sizeof(reuse)
            ) < 0) {

            throw std::runtime_error(
                "HTTP setsockopt(SO_REUSEADDR) failed"
            );
        }


        // ----------------------------------------------------
        // Bind
        // ----------------------------------------------------

        sockaddr_in address {};


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


        if (bind(
                listenFd_,
                reinterpret_cast<sockaddr*>(
                    &address
                ),
                sizeof(address)
            ) < 0) {

            throw std::runtime_error(
                "HTTP bind failed: " +
                std::string(
                    std::strerror(
                        errno
                    )
                )
            );
        }


        // ----------------------------------------------------
        // Listen
        // ----------------------------------------------------

        if (listen(
                listenFd_,
                BACKLOG
            ) < 0) {

            throw std::runtime_error(
                "HTTP listen failed: " +
                std::string(
                    std::strerror(
                        errno
                    )
                )
            );
        }


        Logger::instance().info(
            "HTTP Server listening on port " +
            std::to_string(
                port_
            )
        );


        // ====================================================
        // Accept loop
        // ====================================================

        while (running_.load()) {

            sockaddr_in clientAddress {};


            socklen_t clientLength =
                sizeof(
                    clientAddress
                );


            const int clientFd =
                accept4(
                    listenFd_,
                    reinterpret_cast<sockaddr*>(
                        &clientAddress
                    ),
                    &clientLength,
                    SOCK_CLOEXEC
                );


            if (clientFd < 0) {

                if (!running_.load()) {

                    break;
                }


                if (errno == EINTR) {

                    continue;
                }


                Logger::instance().warning(
                    "HTTP accept failed: " +
                    std::string(
                        std::strerror(
                            errno
                        )
                    )
                );


                continue;
            }


            handleClient(
                clientFd
            );


            close(
                clientFd
            );
        }

    } catch (
        const std::exception& e
    ) {

        if (running_.load()) {

            Logger::instance().error(
                "HTTP Server error: " +
                std::string(
                    e.what()
                )
            );
        }
    }


    running_.store(
        false
    );


    if (listenFd_ >= 0) {

        close(
            listenFd_
        );


        listenFd_ =
            -1;
    }


    Logger::instance().info(
        "HTTP Server stopped"
    );
}


// ============================================================
// Handle one HTTP client
// ============================================================

void HttpServer::handleClient(
    int clientFd
) {
    char buffer[
        BUFFER_SIZE
    ] {};


    const ssize_t bytesRead =
        recv(
            clientFd,
            buffer,
            sizeof(buffer) - 1,
            0
        );


    if (bytesRead <= 0) {

        return;
    }


    const std::string request(
        buffer,
        static_cast<std::size_t>(
            bytesRead
        )
    );


    // --------------------------------------------------------
    // Parse request line:
    //
    // GET /api/hosts HTTP/1.1
    // --------------------------------------------------------

    const std::size_t lineEnd =
        request.find(
            "\r\n"
        );


    if (lineEnd ==
        std::string::npos) {

        sendAll(
            clientFd,
            HttpResponse::badRequest()
        );

        return;
    }


    const std::string requestLine =
        request.substr(
            0,
            lineEnd
        );


    std::istringstream lineStream(
        requestLine
    );


    std::string method;

    std::string path;

    std::string version;


    lineStream
        >> method
        >> path
        >> version;


    if (method.empty() ||
        path.empty() ||
        version.empty()) {

        sendAll(
            clientFd,
            HttpResponse::badRequest()
        );

        return;
    }


    if (method !=
        "GET") {

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

    if (path == "/" ||
        path == "/index.html") {

        const std::string html =
            loadDashboardHtml();


        if (html.empty()) {

            response =
                HttpResponse::internalServerError();

        } else {

            response =
                HttpResponse::okHtml(
                    html
                );
        }
    }


    // ========================================================
    // Host list
    // ========================================================

    else if (path ==
             "/api/hosts") {

        response =
            HttpResponse::okJson(
                buildHostsJson()
            );
    }


    // ========================================================
    // Host details
    //
    // /api/hosts/server-ubuntu2404
    // ========================================================

    else if (path.rfind(
                 "/api/hosts/",
                 0
             ) == 0) {

        const std::string hostname =
            path.substr(
                std::string(
                    "/api/hosts/"
                ).size()
            );


        if (hostname.empty()) {

            response =
                HttpResponse::badRequest();

        } else {

            MetricsStore::StoredMetrics
                storedMetrics;


            if (!store_.getLatest(
                    hostname,
                    storedMetrics
                )) {

                response =
                    HttpResponse::notFound();

            } else {

                response =
                    HttpResponse::okJson(
                        buildHostDetailJson(
                            hostname
                        )
                    );
            }
        }
    }


    // ========================================================
    // 404
    // ========================================================

    else {

        response =
            HttpResponse::notFound();
    }


    sendAll(
        clientFd,
        response
    );
}


// ============================================================
// Build host-list JSON
// ============================================================

std::string HttpServer::buildHostsJson() {
    const auto hosts =
        store_.getAll();


    std::ostringstream json;


    json
        << '[';


    bool first =
        true;


    for (const auto& host :
         hosts) {

        if (!first) {

            json
                << ',';
        }


        first =
            false;


        const auto status =
            store_.getStatus(
                host
            );


        json
            << '{'

            << "\"hostname\":\""
            << jsonEscape(
                host.metrics.hostname
            )
            << "\","

            << "\"status\":\""
            << MetricsStore::statusToString(
                status
            )
            << "\","

            << "\"cpu\":"
            << host.metrics.cpuUsagePercent
            << ','

            << "\"memory\":"
            << host.metrics.memoryUsagePercent
            << ','

            << "\"load1\":"
            << host.metrics.load1
            << ','

            << "\"uptime\":"
            << host.metrics.uptimeSeconds

            << '}';
    }


    json
        << ']';


    return json.str();
}


// ============================================================
// Build single-host detail JSON
// ============================================================

std::string HttpServer::buildHostDetailJson(
    const std::string& hostname
) {
    MetricsStore::StoredMetrics
        host;


    if (!store_.getLatest(
            hostname,
            host
        )) {

        return "{}";
    }


    const auto status =
        store_.getStatus(
            host
        );


    const SystemMetrics& metrics =
        host.metrics;


    std::ostringstream json;


    json
        << '{';


    // --------------------------------------------------------
    // Basic information
    // --------------------------------------------------------

    json
        << "\"hostname\":\""
        << jsonEscape(
            metrics.hostname
        )
        << "\",";


    json
        << "\"status\":\""
        << MetricsStore::statusToString(
            status
        )
        << "\",";


    json
        << "\"uptime_seconds\":"
        << metrics.uptimeSeconds
        << ',';


    json
        << "\"logical_cpus\":"
        << metrics.logicalCpuCount
        << ',';


    // --------------------------------------------------------
    // CPU
    // --------------------------------------------------------

    json
        << "\"cpu\":"
        << metrics.cpuUsagePercent
        << ',';


    // --------------------------------------------------------
    // Memory
    // --------------------------------------------------------

    json
        << "\"memory\":{"

        << "\"total_kb\":"
        << metrics.memoryTotalKB
        << ','

        << "\"used_kb\":"
        << metrics.memoryUsedKB
        << ','

        << "\"available_kb\":"
        << metrics.memoryAvailableKB
        << ','

        << "\"usage_percent\":"
        << metrics.memoryUsagePercent

        << "},";


    // --------------------------------------------------------
    // Load
    // --------------------------------------------------------

    json
        << "\"load\":{"

        << "\"load1\":"
        << metrics.load1
        << ','

        << "\"load5\":"
        << metrics.load5
        << ','

        << "\"load15\":"
        << metrics.load15

        << "},";


    // --------------------------------------------------------
    // Disk
    // --------------------------------------------------------

    json
        << "\"disk\":{"

        << "\"mount\":\""
        << jsonEscape(
            metrics.diskMountPoint
        )
        << "\","

        << "\"total_bytes\":"
        << metrics.diskTotalBytes
        << ','

        << "\"used_bytes\":"
        << metrics.diskUsedBytes
        << ','

        << "\"available_bytes\":"
        << metrics.diskAvailableBytes
        << ','

        << "\"usage_percent\":"
        << metrics.diskUsagePercent

        << "},";


    // --------------------------------------------------------
    // Network
    // --------------------------------------------------------

    json
        << "\"network\":{"

        << "\"interface\":\""
        << jsonEscape(
            metrics.networkInterface
        )
        << "\","

        << "\"rx_bytes_per_second\":"
        << metrics.networkRxBytesPerSecond
        << ','

        << "\"tx_bytes_per_second\":"
        << metrics.networkTxBytesPerSecond

        << "},";


    // --------------------------------------------------------
    // Processes
    // --------------------------------------------------------

    json
        << "\"processes\":[";


    bool firstProcess =
        true;


    for (const auto& process :
         metrics.topProcesses) {

        if (!firstProcess) {

            json
                << ',';
        }


        firstProcess =
            false;


        json
            << '{'

            << "\"pid\":"
            << process.pid
            << ','

            << "\"name\":\""
            << jsonEscape(
                process.name
            )
            << "\","

            << "\"cpu\":"
            << process.cpuUsagePercent
            << ','

            << "\"memory\":"
            << process.memoryUsagePercent
            << ','

            << "\"rss_kb\":"
            << process.residentMemoryKB

            << '}';
    }


    json
        << ']';


    json
        << '}';


    return json.str();
}


// ============================================================
// Load dashboard HTML
// ============================================================

std::string HttpServer::loadDashboardHtml() const {
    // --------------------------------------------------------
    // Development layout:
    //
    // ~/LMonitor/build/lmonitor_server
    // ~/LMonitor/web/index.html
    //
    // Server is normally launched from build/, so first try:
    //
    // ../web/index.html
    // --------------------------------------------------------

    const std::string paths[] = {
        "../web/index.html",
        "web/index.html",
        "./web/index.html"
    };


    for (const auto& path :
         paths) {

        std::ifstream input(
            path
        );


        if (!input.is_open()) {

            continue;
        }


        std::ostringstream buffer;


        buffer
            << input.rdbuf();


        return buffer.str();
    }


    Logger::instance().error(
        "Dashboard HTML not found"
    );


    return "";
}


// ============================================================
// Escape string for JSON
// ============================================================

std::string HttpServer::jsonEscape(
    const std::string& value
) {
    std::string result;


    result.reserve(
        value.size()
    );


    for (const char character :
         value) {

        switch (character) {

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

                result += character;
                break;
        }
    }


    return result;
}


// ============================================================
// Send complete HTTP response
// ============================================================

bool HttpServer::sendAll(
    int fd,
    const std::string& data
) {
    std::size_t sent =
        0;


    while (sent <
           data.size()) {

        const ssize_t bytesSent =
            send(
                fd,
                data.data() +
                    sent,
                data.size() -
                    sent,
                MSG_NOSIGNAL
            );


        if (bytesSent > 0) {

            sent +=
                static_cast<std::size_t>(
                    bytesSent
                );


            continue;
        }


        if (bytesSent < 0 &&
            errno == EINTR) {

            continue;
        }


        return false;
    }


    return true;
}
