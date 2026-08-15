#ifndef LMONITOR_CPU_COLLECTOR_H
#define LMONITOR_CPU_COLLECTOR_H

#include <cstdint>

struct CpuTimes {
    uint64_t user = 0;
    uint64_t nice = 0;
    uint64_t system = 0;
    uint64_t idle = 0;
    uint64_t iowait = 0;
    uint64_t irq = 0;
    uint64_t softirq = 0;
    uint64_t steal = 0;

    uint64_t total() const {
        return user
             + nice
             + system
             + idle
             + iowait
             + irq
             + softirq
             + steal;
    }

    uint64_t idleTime() const {
        return idle + iowait;
    }
};

class CpuCollector {
public:
    CpuTimes readCpuTimes() const;

    double calculateUsage(
        const CpuTimes& previous,
        const CpuTimes& current
    ) const;
};

#endif
