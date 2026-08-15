#ifndef LMONITOR_SYSTEM_METRICS_H
#define LMONITOR_SYSTEM_METRICS_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct ProcessMetric {
    int pid = 0;

    std::string name;

    double cpuUsagePercent = 0.0;
    double memoryUsagePercent = 0.0;

    uint64_t residentMemoryKB = 0;
};


struct SystemMetrics {
    // =========================
    // System
    // =========================

    std::string hostname;

    uint64_t uptimeSeconds = 0;

    std::size_t logicalCpuCount = 0;


    // =========================
    // CPU
    // =========================

    double cpuUsagePercent = 0.0;


    // =========================
    // Memory
    // 单位：KB
    // =========================

    uint64_t memoryTotalKB = 0;
    uint64_t memoryUsedKB = 0;
    uint64_t memoryAvailableKB = 0;

    double memoryUsagePercent = 0.0;


    // =========================
    // Load Average
    // =========================

    double load1 = 0.0;
    double load5 = 0.0;
    double load15 = 0.0;


    // =========================
    // Disk
    // 单位：Bytes
    // =========================

    std::string diskMountPoint;

    uint64_t diskTotalBytes = 0;
    uint64_t diskUsedBytes = 0;
    uint64_t diskAvailableBytes = 0;

    double diskUsagePercent = 0.0;


    // =========================
    // Network
    // =========================

    std::string networkInterface;

    double networkRxBytesPerSecond = 0.0;
    double networkTxBytesPerSecond = 0.0;


    // =========================
    // Sampling
    // =========================

    double sampleIntervalSeconds = 0.0;


    // =========================
    // Processes
    // =========================

    std::vector<ProcessMetric> topProcesses;
};

#endif
