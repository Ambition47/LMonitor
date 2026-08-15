#ifndef LMONITOR_PROCESS_COLLECTOR_H
#define LMONITOR_PROCESS_COLLECTOR_H

#include <cstdint>
#include <string>
#include <vector>

struct ProcessSnapshot {
    int pid = 0;

    std::string name;

    uint64_t userTime = 0;
    uint64_t systemTime = 0;

    uint64_t residentMemoryKB = 0;

    uint64_t totalCpuTime() const {
        return userTime + systemTime;
    }
};

struct ProcessInfo {
    int pid = 0;

    std::string name;

    double cpuUsage = 0.0;
    double memoryUsage = 0.0;

    uint64_t residentMemoryKB = 0;
};

class ProcessCollector {
public:
    std::vector<ProcessSnapshot> collectSnapshots() const;

    std::vector<ProcessInfo> calculateUsage(
        const std::vector<ProcessSnapshot>& previous,
        const std::vector<ProcessSnapshot>& current,
        uint64_t totalCpuDelta,
        uint64_t totalMemoryKB,
        std::size_t cpuCount
    ) const;
};

#endif
