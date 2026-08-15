#include "agent/MonitorAgent.h"

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

MonitorAgent::MonitorAgent() {
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

    // 默认采样周期：1 秒
    intervalSeconds_ = 1.0;
}


void MonitorAgent::run(
    volatile std::sig_atomic_t& runningFlag
) {
    // ========================================
    // 第一次采样
    // 这些数据作为后续计算的 previous
    // ========================================

    CpuTimes previousCpu =
        cpuCollector_.readCpuTimes();

    auto previousNetworkTime =
    std::chrono::steady_clock::now();

NetworkStats previousNetwork =
    networkCollector_.collect(
        networkInterface_
    );

    std::vector<ProcessSnapshot> previousProcesses =
        processCollector_.collectSnapshots();

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
    // ========================================
    // 主监控循环
    // ========================================

    while (runningFlag) {

        // 等待一个采样周期
        std::this_thread::sleep_until(
              nextSampleTime
         );

        // 如果 sleep 期间收到退出信号，
        // 就不再进行下一轮采集
        if (!runningFlag) {
            break;
        }


        // ========================================
        // 第二次采样
        // ========================================

        CpuTimes currentCpu =
            cpuCollector_.readCpuTimes();

        const auto currentNetworkTime =
    std::chrono::steady_clock::now();

NetworkStats currentNetwork =
    networkCollector_.collect(
        networkInterface_
    );

        std::vector<ProcessSnapshot> currentProcesses =
            processCollector_.collectSnapshots();
          
	const double actualIntervalSeconds =
    std::chrono::duration<double>(
        currentNetworkTime -
        previousNetworkTime
    ).count();

        // ========================================
        // CPU 使用率
        // ========================================

        const double cpuUsage =
            cpuCollector_.calculateUsage(
                previousCpu,
                currentCpu
            );

        const uint64_t totalCpuDelta =
            currentCpu.total() -
            previousCpu.total();


        // ========================================
        // 网络速率
        // ========================================

        const NetworkRate networkRate =
            networkCollector_.calculateRate(
                previousNetwork,
                currentNetwork,
                actualIntervalSeconds
            );


        // ========================================
        // 采集当前即时信息
        // ========================================

        const MemoryInfo memory =
            memoryCollector_.collect();

        const LoadInfo load =
            loadCollector_.collect();

        const SystemInfo systemInfo =
            systemCollector_.collect();

        const DiskInfo disk =
            diskCollector_.collect("/");


        // ========================================
        // 计算进程 CPU / Memory 使用率
        // ========================================

        const std::vector<ProcessInfo> processes =
            processCollector_.calculateUsage(
                previousProcesses,
                currentProcesses,
                totalCpuDelta,
                memory.totalKB,
                cpuCount_
            );


        // ========================================
        // 单位换算
        // ========================================

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


        const double totalMemoryGiB =
            static_cast<double>(
                memory.totalKB
            ) / kbToGiB;

        const double usedMemoryGiB =
            static_cast<double>(
                memory.usedKB()
            ) / kbToGiB;

        const double availableMemoryGiB =
            static_cast<double>(
                memory.availableKB
            ) / kbToGiB;


        const double totalDiskGiB =
            static_cast<double>(
                disk.totalBytes
            ) / bytesToGiB;

        const double usedDiskGiB =
            static_cast<double>(
                disk.usedBytes
            ) / bytesToGiB;

        const double availableDiskGiB =
            static_cast<double>(
                disk.availableBytes
            ) / bytesToGiB;


        const double rxKiBPerSecond =
            networkRate.rxBytesPerSecond /
            bytesToKiB;

        const double txKiBPerSecond =
            networkRate.txBytesPerSecond /
            bytesToKiB;

        const double rxMiBPerSecond =
            networkRate.rxBytesPerSecond /
            bytesToMiB;

        const double txMiBPerSecond =
            networkRate.txBytesPerSecond /
            bytesToMiB;


        // ========================================
        // Uptime 转换
        // ========================================

        const uint64_t uptime =
            systemInfo.uptimeSeconds;

        const uint64_t days =
            uptime / 86400;

        const uint64_t hours =
            (uptime % 86400) / 3600;

        const uint64_t minutes =
            (uptime % 3600) / 60;


        // ========================================
        // 清屏
        // ========================================

        std::cout << "\033[2J\033[H";

        std::cout
            << std::fixed
            << std::setprecision(2);


        // ========================================
        // System
        // ========================================

        std::cout
            << "========================================\n";

        std::cout
            << "            LMonitor Agent\n";

        std::cout
            << "========================================\n\n";

        std::cout
            << "Hostname        : "
            << systemInfo.hostname
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
            << cpuCount_
            << "\n\n";


        // ========================================
        // CPU
        // ========================================

        std::cout
            << "CPU Usage       : "
            << cpuUsage
            << " %\n\n";


        // ========================================
        // Memory
        // ========================================

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
            << memory.usagePercent()
            << " %\n\n";


        // ========================================
        // Load Average
        // ========================================

        std::cout
            << "Load Average\n";

        std::cout
            << "  1 min         : "
            << load.load1
            << '\n';

        std::cout
            << "  5 min         : "
            << load.load5
            << '\n';

        std::cout
            << "  15 min        : "
            << load.load15
            << "\n\n";


        // ========================================
        // Disk
        // ========================================

        std::cout
            << "Disk "
            << disk.mountPoint
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
            << disk.usagePercent()
            << " %\n\n";


        // ========================================
        // Network
        // ========================================

        std::cout
            << "Network "
            << networkInterface_
            << '\n';

        std::cout
            << "  RX Rate       : "
            << rxKiBPerSecond
            << " KiB/s"
            << "  ("
            << rxMiBPerSecond
            << " MiB/s)\n";

        std::cout
            << "  TX Rate       : "
            << txKiBPerSecond
            << " KiB/s"
            << "  ("
            << txMiBPerSecond
            << " MiB/s)\n\n";


        // ========================================
        // Top Processes
        // ========================================

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

        const std::size_t topCount =
            std::min<std::size_t>(
                5,
                processes.size()
            );

        for (std::size_t i = 0;
             i < topCount;
             ++i) {

            const ProcessInfo& process =
                processes[i];

            const double rssMiB =
                static_cast<double>(
                    process.residentMemoryKB
                ) / kbToMiB;

            std::cout
                << std::left
                << std::setw(10)
                << process.pid

                << std::setw(12)
                << process.cpuUsage

                << std::setw(12)
                << process.memoryUsage

                << std::setw(14)
                << rssMiB

                << process.name
                << '\n';
        }

        std::cout
            << "\n========================================\n";

        std::cout.flush();


        // ========================================
        // 当前采样变成下一轮 previous
        // ========================================

        previousCpu =
            currentCpu;

        previousNetwork =
            currentNetwork;

	previousNetworkTime =
            currentNetworkTime;

        previousProcesses =
            std::move(currentProcesses);

	nextSampleTime +=
            sampleInterval;
    }
}
