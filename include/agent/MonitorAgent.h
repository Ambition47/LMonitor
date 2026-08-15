#ifndef LMONITOR_MONITOR_AGENT_H
#define LMONITOR_MONITOR_AGENT_H

#include "collector/CpuCollector.h"
#include "collector/DiskCollector.h"
#include "collector/LoadCollector.h"
#include "collector/MemoryCollector.h"
#include "collector/NetworkCollector.h"
#include "collector/ProcessCollector.h"
#include "collector/SystemCollector.h"
#include "model/SystemMetrics.h"


#include <csignal>
#include <cstddef>
#include <string>
#include <vector>


class MonitorAgent {
public:
    explicit MonitorAgent(
    double intervalSeconds = 1.0
);
            
    void run(
        volatile std::sig_atomic_t& runningFlag
    );

private:
        SystemMetrics buildMetrics(
        double cpuUsage,
        double actualIntervalSeconds,
        const MemoryInfo& memory,
        const LoadInfo& load,
        const SystemInfo& systemInfo,
        const DiskInfo& disk,
        const NetworkRate& networkRate,
        const std::vector<ProcessInfo>& processes
    ) const;

    void displayMetrics(
        const SystemMetrics& metrics
    ) const;

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
