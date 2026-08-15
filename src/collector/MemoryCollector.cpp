#include "collector/MemoryCollector.h"

#include <fstream>
#include <stdexcept>
#include <string>

MemoryInfo MemoryCollector::collect() const {
    std::ifstream file("/proc/meminfo");//这步就是打开/proc/meminfo

    if (!file.is_open()) {
        throw std::runtime_error("Failed to open /proc/meminfo");
    }

    MemoryInfo info;

    std::string key;
    uint64_t value;
    std::string unit;

    while (file >> key >> value >> unit) {    //这行是不断从文件中读取字段名，数值，单位。
        if (key == "MemTotal:") {
            info.totalKB = value;    //找到总内存
        } else if (key == "MemAvailable:") {
            info.availableKB = value;  //找到可用内存
        }

        if (info.totalKB != 0 && info.availableKB != 0) {
            break;   //找到两个后就停止读取
        }
    }

    if (info.totalKB == 0 || info.availableKB == 0) {    //异常检查：如果没有成功读到关键字段就抛出错误:
        throw std::runtime_error(
            "Failed to parse memory information"
        );
    }

    return info;
}
