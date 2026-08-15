#include "collector/LoadCollector.h"

#include <fstream>
#include <stdexcept>

LoadInfo LoadCollector::collect() const {
    std::ifstream file("/proc/loadavg");

    if (!file.is_open()) {
        throw std::runtime_error(
            "Failed to open /proc/loadavg"
        );
    }

    LoadInfo info;

    if (!(file >> info.load1
               >> info.load5
               >> info.load15)) {
        throw std::runtime_error(
            "Failed to parse /proc/loadavg"
        );
    }

    return info;
}
