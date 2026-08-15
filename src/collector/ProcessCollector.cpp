#include "collector/ProcessCollector.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>

namespace fs = std::filesystem;

namespace {

// 判断 /proc 下的目录是不是 PID 目录
bool isPidDirectory(const fs::directory_entry& entry) {
    if (!entry.is_directory()) {
        return false;
    }

    const std::string name =
        entry.path().filename().string();

    if (name.empty()) {
        return false;
    }

    return std::all_of(
        name.begin(),
        name.end(),
        [](unsigned char ch) {
            return std::isdigit(ch);
        }
    );
}


// 读取进程名称
bool readProcessName(
    int pid,
    std::string& processName
) {
    const std::string path =
        "/proc/" +
        std::to_string(pid) +
        "/comm";

    std::ifstream file(path);

    if (!file.is_open()) {
        return false;
    }

    if (!std::getline(file, processName)) {
        return false;
    }

    return !processName.empty();
}


// 读取进程 CPU 累计时间
bool readProcessCpuTimes(
    int pid,
    uint64_t& userTime,
    uint64_t& systemTime
) {
    const std::string path =
        "/proc/" +
        std::to_string(pid) +
        "/stat";

    std::ifstream file(path);

    if (!file.is_open()) {
        return false;
    }

    std::string line;

    if (!std::getline(file, line)) {
        return false;
    }

    // 找进程名称最后的右括号
    const std::size_t rightParen =
        line.rfind(')');

    if (rightParen == std::string::npos) {
        return false;
    }

    if (rightParen + 2 >= line.size()) {
        return false;
    }

    // 跳过 pid 和 (comm)，从 state 开始解析
    const std::string remaining =
        line.substr(rightParen + 2);

    std::istringstream iss(remaining);

    char state;

    int ppid;
    int pgrp;
    int session;
    int ttyNr;
    int tpgid;

    unsigned long flags;

    unsigned long minflt;
    unsigned long cminflt;
    unsigned long majflt;
    unsigned long cmajflt;

    if (!(iss >> state
              >> ppid
              >> pgrp
              >> session
              >> ttyNr
              >> tpgid
              >> flags
              >> minflt
              >> cminflt
              >> majflt
              >> cmajflt
              >> userTime
              >> systemTime)) {
        return false;
    }

    return true;
}


// 读取进程实际驻留内存 VmRSS
bool readProcessMemory(
    int pid,
    uint64_t& residentMemoryKB
) {
    const std::string path =
        "/proc/" +
        std::to_string(pid) +
        "/status";

    std::ifstream file(path);

    if (!file.is_open()) {
        return false;
    }

    std::string line;

    while (std::getline(file, line)) {
        if (line.rfind("VmRSS:", 0) == 0) {

            std::istringstream iss(line);

            std::string key;
            uint64_t value;
            std::string unit;

            if (!(iss >> key >> value >> unit)) {
                return false;
            }

            residentMemoryKB = value;

            return true;
        }
    }

    return false;
}

}  // namespace


// ============================================================
// 采集当前所有进程的原始快照
// ============================================================

std::vector<ProcessSnapshot>
ProcessCollector::collectSnapshots() const {
    std::vector<ProcessSnapshot> snapshots;

    for (const auto& entry :
         fs::directory_iterator("/proc")) {

        if (!isPidDirectory(entry)) {
            continue;
        }

        ProcessSnapshot snapshot;

        try {
            snapshot.pid =
                std::stoi(
                    entry.path()
                         .filename()
                         .string()
                );
        } catch (...) {
            continue;
        }

        if (!readProcessName(
                snapshot.pid,
                snapshot.name)) {
            continue;
        }

        if (!readProcessCpuTimes(
                snapshot.pid,
                snapshot.userTime,
                snapshot.systemTime)) {
            continue;
        }

        if (!readProcessMemory(
                snapshot.pid,
                snapshot.residentMemoryKB)) {
            continue;
        }

        snapshots.push_back(snapshot);
    }

    return snapshots;
}


// ============================================================
// 根据前后两次快照计算 CPU / Memory 使用率
// ============================================================

std::vector<ProcessInfo>
ProcessCollector::calculateUsage(
    const std::vector<ProcessSnapshot>& previous,
    const std::vector<ProcessSnapshot>& current,
    uint64_t totalCpuDelta,
    uint64_t totalMemoryKB,
    std::size_t cpuCount
) const {
    std::vector<ProcessInfo> result;

    if (totalCpuDelta == 0 ||
        totalMemoryKB == 0 ||
        cpuCount == 0) {
        return result;
    }

    // PID -> 上一次采样中的进程快照
    std::unordered_map<
        int,
        const ProcessSnapshot*
    > previousByPid;

    // 提前预留空间，减少 unordered_map 扩容
    previousByPid.reserve(previous.size());

    for (const auto& process : previous) {
        previousByPid[process.pid] =
            &process;
    }

    for (const auto& currentProcess : current) {

        const auto it =
            previousByPid.find(
                currentProcess.pid
            );

        // 当前进程在上一轮不存在，
        // 说明它可能是刚启动的进程
        if (it == previousByPid.end()) {
            continue;
        }

        const ProcessSnapshot& previousProcess =
            *(it->second);

        // 防御性检查
        if (currentProcess.totalCpuTime() <
            previousProcess.totalCpuTime()) {
            continue;
        }

        const uint64_t processCpuDelta =
            currentProcess.totalCpuTime() -
            previousProcess.totalCpuTime();

        ProcessInfo info;

        info.pid =
            currentProcess.pid;

        info.name =
            currentProcess.name;

        info.residentMemoryKB =
            currentProcess.residentMemoryKB;

        info.cpuUsage =
            100.0 *
            static_cast<double>(
                processCpuDelta
            ) /
            static_cast<double>(
                totalCpuDelta
            ) *
            static_cast<double>(
                cpuCount
            );

        info.memoryUsage =
            100.0 *
            static_cast<double>(
                currentProcess.residentMemoryKB
            ) /
            static_cast<double>(
                totalMemoryKB
            );

        result.push_back(info);
    }

    std::sort(
        result.begin(),
        result.end(),
        [](const ProcessInfo& a,
           const ProcessInfo& b) {
            return a.cpuUsage >
                   b.cpuUsage;
        }
    );

    return result;
}



