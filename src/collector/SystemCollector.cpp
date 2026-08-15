#include "collector/SystemCollector.h"

#include <fstream>
#include <stdexcept>
#include <unistd.h>

SystemInfo SystemCollector::collect() const {
    SystemInfo info;

    char hostname[256];

    if (gethostname(hostname, sizeof(hostname)) != 0) {
        throw std::runtime_error(
            "Failed to get hostname"
        );
    }

    info.hostname = hostname;

    std::ifstream file("/proc/uptime");

    if (!file.is_open()) {
        throw std::runtime_error(
            "Failed to open /proc/uptime"
        );
    }

    double uptimeSeconds = 0.0;

    if (!(file >> uptimeSeconds)) {
        throw std::runtime_error(
            "Failed to parse /proc/uptime"
        );
    }

    info.uptimeSeconds =
        static_cast<uint64_t>(uptimeSeconds);

    return info;
}
