#ifndef LMONITOR_MEMORY_COLLECTOR_H
#define LMONITOR_MEMORY_COLLECTOR_H

#include <cstdint>

struct MemoryInfo {
    uint64_t totalKB = 0;
    uint64_t availableKB = 0;

    uint64_t usedKB() const {
        return totalKB - availableKB;
    }

    double usagePercent() const {
        if (totalKB == 0) {
            return 0.0;
        }

        return 100.0 *
               static_cast<double>(usedKB()) /
               static_cast<double>(totalKB);
    }
};

class MemoryCollector {
public:
    MemoryInfo collect() const;  //采集当前时刻的内存状态
};

#endif
