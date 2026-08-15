#include "collector/CpuCollector.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

CpuTimes CpuCollector::readCpuTimes() const {
    std::ifstream file("/proc/stat");

    if (!file.is_open()) {
        throw std::runtime_error("Failed to open /proc/stat");
    }

    std::string line;

    if (!std::getline(file, line)) {
        throw std::runtime_error("Failed to read /proc/stat");
    }

    std::istringstream iss(line);

    std::string cpuLabel;
    CpuTimes times;

    iss >> cpuLabel
        >> times.user
        >> times.nice
        >> times.system
        >> times.idle
        >> times.iowait
        >> times.irq
        >> times.softirq
        >> times.steal;

    if (cpuLabel != "cpu") {
        throw std::runtime_error("Invalid /proc/stat format");
    }

    return times;
}

double CpuCollector::calculateUsage(
    const CpuTimes& previous,
    const CpuTimes& current
) const {
    const uint64_t previousTotal = previous.total();
    const uint64_t currentTotal = current.total();

    const uint64_t previousIdle = previous.idleTime();
    const uint64_t currentIdle = current.idleTime();

    const uint64_t totalDelta = currentTotal - previousTotal;
    const uint64_t idleDelta = currentIdle - previousIdle;

    if (totalDelta == 0) {
        return 0.0;
    }

    return 100.0 *
           static_cast<double>(totalDelta - idleDelta) /
           static_cast<double>(totalDelta);
}
