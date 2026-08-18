#include "agent/MonitorAgent.h"
#include "serializer/MetricsSerializer.h"
#include "protocol/FrameCodec.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include <unistd.h>


// ============================================================
// Constructor
// ============================================================

MonitorAgent::MonitorAgent(
    double intervalSeconds,
    const std::string& serverIp,
    uint16_t serverPort
)
    : tcpClient_(
          serverIp,
          serverPort
      ) {

    // 自动检测默认网络接口
    networkInterface_ =
        networkCollector_.detectDefaultInterface();

    // 获取当前在线的逻辑 CPU 数量
    const long cpuCount =
        sysconf(_SC_NPROCESSORS_ONLN);

    if (cpuCount <= 0) {
        throw std::runtime_error(
            "Failed to get logical CPU count"
        );
    }

    cpuCount_ =
        static_cast<std::size_t>(cpuCount);

    // 检查采样周期是否合法
    if (intervalSeconds <= 0.0) {
        throw std::invalid_argument(
            "Sampling interval must be greater than 0"
        );
    }

    intervalSeconds_ =
        intervalSeconds;
}


// ============================================================
// Build unified metrics snapshot
// ============================================================

SystemMetrics MonitorAgent::buildMetrics(
    double cpuUsage,
    double actualIntervalSeconds,
    const MemoryInfo& memory,
    const LoadInfo& load,
    const SystemInfo& systemInfo,
    const DiskInfo& disk,
    const NetworkRate& networkRate,
    const std::vector<ProcessInfo>& processes
) const {
    SystemMetrics metrics;


    // --------------------------------------------------------
    // System
    // --------------------------------------------------------

    metrics.hostname =
        systemInfo.hostname;

    metrics.uptimeSeconds =
        systemInfo.uptimeSeconds;

    metrics.logicalCpuCount =
        cpuCount_;


    // --------------------------------------------------------
    // CPU
    // --------------------------------------------------------

    metrics.cpuUsagePercent =
        cpuUsage;


    // --------------------------------------------------------
    // Memory
    // --------------------------------------------------------

    metrics.memoryTotalKB =
        memory.totalKB;

    metrics.memoryUsedKB =
        memory.usedKB();

    metrics.memoryAvailableKB =
        memory.availableKB;

    metrics.memoryUsagePercent =
        memory.usagePercent();


    // --------------------------------------------------------
    // Load Average
    // --------------------------------------------------------

    metrics.load1 =
        load.load1;

    metrics.load5 =
        load.load5;

    metrics.load15 =
        load.load15;


    // --------------------------------------------------------
    // Disk
    // --------------------------------------------------------

    metrics.diskMountPoint =
        disk.mountPoint;

    metrics.diskTotalBytes =
        disk.totalBytes;

    metrics.diskUsedBytes =
        disk.usedBytes;

    metrics.diskAvailableBytes =
        disk.availableBytes;

    metrics.diskUsagePercent =
        disk.usagePercent();


    // --------------------------------------------------------
    // Network
    // --------------------------------------------------------

    metrics.networkInterface =
        networkInterface_;

    metrics.networkRxBytesPerSecond =
        networkRate.rxBytesPerSecond;

    metrics.networkTxBytesPerSecond =
        networkRate.txBytesPerSecond;


    // --------------------------------------------------------
    // Sampling
    // --------------------------------------------------------

    metrics.sampleIntervalSeconds =
        actualIntervalSeconds;


    // --------------------------------------------------------
    // Top Processes
    // --------------------------------------------------------

    const std::size_t topCount =
        std::min<std::size_t>(
            5,
            processes.size()
        );

    metrics.topProcesses.reserve(
        topCount
    );

    for (std::size_t i = 0;
         i < topCount;
         ++i) {

        const ProcessInfo& process =
            processes[i];

        ProcessMetric metric;

        metric.pid =
            process.pid;

        metric.name =
            process.name;

        metric.cpuUsagePercent =
            process.cpuUsage;

        metric.memoryUsagePercent =
            process.memoryUsage;

        metric.residentMemoryKB =
            process.residentMemoryKB;

        metrics.topProcesses.push_back(
            std::move(metric)
        );
    }


    return metrics;
}


// ============================================================
// Display metrics on terminal
// ============================================================

void MonitorAgent::displayMetrics(
    const SystemMetrics& metrics
) const {
    // --------------------------------------------------------
    // Unit conversion constants
    // --------------------------------------------------------

    const double kbToGiB =
        1024.0 * 1024.0;

    const double kbToMiB =
        1024.0;

    const double bytesToGiB =
        1024.0 * 1024.0 * 1024.0;

    const double bytesToKiB =
        1024.0;

    const double bytesToMiB =
        1024.0 * 1024.0;


    // --------------------------------------------------------
    // Memory conversion
    // --------------------------------------------------------

    const double totalMemoryGiB =
        static_cast<double>(
            metrics.memoryTotalKB
        ) / kbToGiB;

    const double usedMemoryGiB =
        static_cast<double>(
            metrics.memoryUsedKB
        ) / kbToGiB;

    const double availableMemoryGiB =
        static_cast<double>(
            metrics.memoryAvailableKB
        ) / kbToGiB;


    // --------------------------------------------------------
    // Disk conversion
    // --------------------------------------------------------

    const double totalDiskGiB =
        static_cast<double>(
            metrics.diskTotalBytes
        ) / bytesToGiB;

    const double usedDiskGiB =
        static_cast<double>(
            metrics.diskUsedBytes
        ) / bytesToGiB;

    const double availableDiskGiB =
        static_cast<double>(
            metrics.diskAvailableBytes
        ) / bytesToGiB;


    // --------------------------------------------------------
    // Network conversion
    // --------------------------------------------------------

    const double rxKiBPerSecond =
        metrics.networkRxBytesPerSecond /
        bytesToKiB;

    const double txKiBPerSecond =
        metrics.networkTxBytesPerSecond /
        bytesToKiB;

    const double rxMiBPerSecond =
        metrics.networkRxBytesPerSecond /
        bytesToMiB;

    const double txMiBPerSecond =
        metrics.networkTxBytesPerSecond /
        bytesToMiB;


    // --------------------------------------------------------
    // Uptime conversion
    // --------------------------------------------------------

    const uint64_t uptime =
        metrics.uptimeSeconds;

    const uint64_t days =
        uptime / 86400;

    const uint64_t hours =
        (uptime % 86400) / 3600;

    const uint64_t minutes =
        (uptime % 3600) / 60;


    // --------------------------------------------------------
    // Clear terminal
    // --------------------------------------------------------

    std::cout
        << "\033[2J\033[H";

    std::cout
        << std::fixed
        << std::setprecision(2);


    // --------------------------------------------------------
    // Header
    // --------------------------------------------------------

    std::cout
        << "========================================\n";

    std::cout
        << "            LMonitor Agent\n";

    std::cout
        << "========================================\n\n";


    // --------------------------------------------------------
    // System
    // --------------------------------------------------------

    std::cout
        << "Hostname        : "
        << metrics.hostname
        << '\n';

    std::cout
        << "Uptime          : "
        << days
        << " days "
        << hours
        << " hours "
        << minutes
        << " minutes\n";

    std::cout
        << "Logical CPUs    : "
        << metrics.logicalCpuCount
        << "\n\n";


    // --------------------------------------------------------
    // CPU
    // --------------------------------------------------------

    std::cout
        << "CPU Usage       : "
        << metrics.cpuUsagePercent
        << " %\n";

    std::cout
        << "Sample Interval : "
        << metrics.sampleIntervalSeconds
        << " s\n\n";


    // --------------------------------------------------------
    // Memory
    // --------------------------------------------------------

    std::cout
        << "Memory\n";

    std::cout
        << "  Total         : "
        << totalMemoryGiB
        << " GiB\n";

    std::cout
        << "  Used          : "
        << usedMemoryGiB
        << " GiB\n";

    std::cout
        << "  Available     : "
        << availableMemoryGiB
        << " GiB\n";

    std::cout
        << "  Usage         : "
        << metrics.memoryUsagePercent
        << " %\n\n";


    // --------------------------------------------------------
    // Load Average
    // --------------------------------------------------------

    std::cout
        << "Load Average\n";

    std::cout
        << "  1 min         : "
        << metrics.load1
        << '\n';

    std::cout
        << "  5 min         : "
        << metrics.load5
        << '\n';

    std::cout
        << "  15 min        : "
        << metrics.load15
        << "\n\n";


    // --------------------------------------------------------
    // Disk
    // --------------------------------------------------------

    std::cout
        << "Disk "
        << metrics.diskMountPoint
        << '\n';

    std::cout
        << "  Total         : "
        << totalDiskGiB
        << " GiB\n";

    std::cout
        << "  Used          : "
        << usedDiskGiB
        << " GiB\n";

    std::cout
        << "  Available     : "
        << availableDiskGiB
        << " GiB\n";

    std::cout
        << "  Usage         : "
        << metrics.diskUsagePercent
        << " %\n\n";


    // --------------------------------------------------------
    // Network
    // --------------------------------------------------------

    std::cout
        << "Network "
        << metrics.networkInterface
        << '\n';

    std::cout
        << "  RX Rate       : "
        << rxKiBPerSecond
        << " KiB/s  ("
        << rxMiBPerSecond
        << " MiB/s)\n";

    std::cout
        << "  TX Rate       : "
        << txKiBPerSecond
        << " KiB/s  ("
        << txMiBPerSecond
        << " MiB/s)\n\n";


    // --------------------------------------------------------
    // Top Processes
    // --------------------------------------------------------

    std::cout
        << "Top Processes (by CPU)\n\n";

    std::cout
        << std::left
        << std::setw(10) << "PID"
        << std::setw(12) << "CPU%"
        << std::setw(12) << "MEM%"
        << std::setw(14) << "RSS(MiB)"
        << "NAME"
        << '\n';

    std::cout
        << "------------------------------------------------------\n";


    for (const auto& process :
         metrics.topProcesses) {

        const double rssMiB =
            static_cast<double>(
                process.residentMemoryKB
            ) / kbToMiB;

        std::cout
            << std::left
            << std::setw(10)
            << process.pid

            << std::setw(12)
            << process.cpuUsagePercent

            << std::setw(12)
            << process.memoryUsagePercent

            << std::setw(14)
            << rssMiB

            << process.name
            << '\n';
    }


    std::cout
        << "\n========================================\n";

    std::cout.flush();
}


// ============================================================
// Main monitoring loop
// ============================================================

void MonitorAgent::run(
    volatile std::sig_atomic_t& runningFlag
) {
    // --------------------------------------------------------
    // Initial CPU snapshot
    // --------------------------------------------------------

    CpuTimes previousCpu =
        cpuCollector_.readCpuTimes();


    // --------------------------------------------------------
    // Initial network snapshot
    // --------------------------------------------------------

    NetworkStats previousNetwork =
        networkCollector_.collect(
            networkInterface_
        );

    auto previousNetworkTime =
        std::chrono::steady_clock::now();


    // --------------------------------------------------------
    // Initial process snapshot
    // --------------------------------------------------------

    std::vector<ProcessSnapshot> previousProcesses =
        processCollector_.collectSnapshots();


    // --------------------------------------------------------
    // Fixed sampling interval
    // --------------------------------------------------------

    const auto sampleInterval =
        std::chrono::duration_cast<
            std::chrono::steady_clock::duration
        >(
            std::chrono::duration<double>(
                intervalSeconds_
            )
        );


    auto nextSampleTime =
        std::chrono::steady_clock::now() +
        sampleInterval;


    // ========================================================
    // Monitoring loop
    // ========================================================

    while (runningFlag) {

        // 等待到下一个固定采样时间点
        std::this_thread::sleep_until(
            nextSampleTime
        );

        // sleep 期间可能收到 SIGINT / SIGTERM
        if (!runningFlag) {
            break;
        }


        // ----------------------------------------------------
        // CPU snapshot
        // ----------------------------------------------------

        CpuTimes currentCpu =
            cpuCollector_.readCpuTimes();


        // ----------------------------------------------------
        // Network snapshot
        // ----------------------------------------------------

        NetworkStats currentNetwork =
            networkCollector_.collect(
                networkInterface_
            );

        const auto currentNetworkTime =
            std::chrono::steady_clock::now();


        // ----------------------------------------------------
        // Process snapshot
        // ----------------------------------------------------

        std::vector<ProcessSnapshot> currentProcesses =
            processCollector_.collectSnapshots();


        // ----------------------------------------------------
        // Actual network sampling interval
        // ----------------------------------------------------

        const double actualIntervalSeconds =
            std::chrono::duration<double>(
                currentNetworkTime -
                previousNetworkTime
            ).count();


        // ----------------------------------------------------
        // CPU usage
        // ----------------------------------------------------

        const double cpuUsage =
            cpuCollector_.calculateUsage(
                previousCpu,
                currentCpu
            );


        const uint64_t totalCpuDelta =
            currentCpu.total() -
            previousCpu.total();


        // ----------------------------------------------------
        // Network rate
        // ----------------------------------------------------

        const NetworkRate networkRate =
            networkCollector_.calculateRate(
                previousNetwork,
                currentNetwork,
                actualIntervalSeconds
            );


        // ----------------------------------------------------
        // Instant metrics
        // ----------------------------------------------------

        const MemoryInfo memory =
            memoryCollector_.collect();

        const LoadInfo load =
            loadCollector_.collect();

        const SystemInfo systemInfo =
            systemCollector_.collect();

        const DiskInfo disk =
            diskCollector_.collect("/");


        // ----------------------------------------------------
        // Process usage
        // ----------------------------------------------------

        const std::vector<ProcessInfo> processes =
            processCollector_.calculateUsage(
                previousProcesses,
                currentProcesses,
                totalCpuDelta,
                memory.totalKB,
                cpuCount_
            );


        // ----------------------------------------------------
        // Build unified metrics snapshot
        // ----------------------------------------------------

        const SystemMetrics metrics =
            buildMetrics(
                cpuUsage,
                actualIntervalSeconds,
                memory,
                load,
                systemInfo,
                disk,
                networkRate,
                processes
            );


        // ----------------------------------------------------
        // Display
        // ----------------------------------------------------
        //终端显示
        displayMetrics(
            metrics
        );
       
	//序列化
	const std::string serializedMetrics =
    metricsSerializer_.serialize(
        metrics
    );
	const std::string framedMetrics =
    FrameCodec::encode(
        serializedMetrics
    );
      
	if (!tcpClient_.isConnected()) {

    std::cout
        << "Connecting to monitoring server...\n";


    if (!tcpClient_.connectToServer()) {

        std::cerr
            << "Unable to connect to monitoring server. "
            << "Will retry on the next sample.\n";

    }
}


if (tcpClient_.isConnected()) {

    if (!tcpClient_.sendAll(
            framedMetrics
        )) {

        std::cerr
            << "Failed to send metrics. "
            << "Connection will be retried "
            << "on the next sample.\n";
    }
}

        // ----------------------------------------------------
        // Current snapshot becomes previous snapshot
        // ----------------------------------------------------
          //当前快照变成下一轮previous
        previousCpu =
            currentCpu;

        previousNetwork =
            currentNetwork;

        previousNetworkTime =
            currentNetworkTime;

        previousProcesses =
            std::move(currentProcesses);


        // ----------------------------------------------------
        // Schedule next sample
        // ----------------------------------------------------

        nextSampleTime +=
            sampleInterval;
    }
}
