#include "serializer/MetricsSerializer.h"

#include <iomanip>
#include <sstream>


std::string MetricsSerializer::serialize(
    const SystemMetrics& metrics
) const {
    std::ostringstream output;

    output
        << std::fixed
        << std::setprecision(2);


    // 协议版本
    output
        << "LMONITOR/1\n";


    // System
    output
        << "hostname="
        << metrics.hostname
        << '\n';

    output
        << "uptime_seconds="
        << metrics.uptimeSeconds
        << '\n';

    output
        << "logical_cpus="
        << metrics.logicalCpuCount
        << '\n';


    // CPU
    output
        << "cpu_usage_percent="
        << metrics.cpuUsagePercent
        << '\n';


    // Memory
    output
        << "memory_total_kb="
        << metrics.memoryTotalKB
        << '\n';

    output
        << "memory_used_kb="
        << metrics.memoryUsedKB
        << '\n';

    output
        << "memory_available_kb="
        << metrics.memoryAvailableKB
        << '\n';

    output
        << "memory_usage_percent="
        << metrics.memoryUsagePercent
        << '\n';


    // Load
    output
        << "load_1="
        << metrics.load1
        << '\n';

    output
        << "load_5="
        << metrics.load5
        << '\n';

    output
        << "load_15="
        << metrics.load15
        << '\n';


    // Disk
    output
        << "disk_mount="
        << metrics.diskMountPoint
        << '\n';

    output
        << "disk_total_bytes="
        << metrics.diskTotalBytes
        << '\n';

    output
        << "disk_used_bytes="
        << metrics.diskUsedBytes
        << '\n';

    output
        << "disk_available_bytes="
        << metrics.diskAvailableBytes
        << '\n';

    output
        << "disk_usage_percent="
        << metrics.diskUsagePercent
        << '\n';


    // Network
    output
        << "network_interface="
        << metrics.networkInterface
        << '\n';

    output
        << "network_rx_bytes_per_second="
        << metrics.networkRxBytesPerSecond
        << '\n';

    output
        << "network_tx_bytes_per_second="
        << metrics.networkTxBytesPerSecond
        << '\n';


    // Sampling
    output
        << "sample_interval_seconds="
        << metrics.sampleIntervalSeconds
        << '\n';


    // Processes
    output
        << "process_count="
        << metrics.topProcesses.size()
        << '\n';

    for (const auto& process :
         metrics.topProcesses) {

        output
            << "process="
            << process.pid
            << ','
            << process.cpuUsagePercent
            << ','
            << process.memoryUsagePercent
            << ','
            << process.residentMemoryKB
            << ','
            << process.name
            << '\n';
    }


    return output.str();
}
