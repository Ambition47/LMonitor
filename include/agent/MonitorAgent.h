#ifndef LMONITOR_MONITOR_AGENT_H
#define LMONITOR_MONITOR_AGENT_H

#include "collector/CpuCollector.h"
#include "collector/DiskCollector.h"
#include "collector/LoadCollector.h"
#include "collector/MemoryCollector.h"
#include "collector/NetworkCollector.h"
#include "collector/ProcessCollector.h"
#include "collector/SystemCollector.h"

#include <csignal>
#include <cstddef>
#include <string>

class MonitorAgent {
public:
    MonitorAgent();

    void run(
        volatile std::sig_atomic_t& runningFlag
    );

private:
    CpuCollector cpuCollector_;
    MemoryCollector memoryCollector_;
    LoadCollector loadCollector_;
    SystemCollector systemCollector_;
    DiskCollector diskCollector_;
    NetworkCollector networkCollector_;
    ProcessCollector processCollector_;

    std::string networkInterface_;

    std::size_t cpuCount_ = 0;

    double intervalSeconds_ = 1.0;
};

#endif
