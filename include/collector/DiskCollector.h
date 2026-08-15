#ifndef LMONITOR_DISK_COLLECTOR_H
#define LMONITOR_DISK_COLLECTOR_H

#include <cstdint>
#include <string>

struct DiskInfo {
    std::string mountPoint;

    uint64_t totalBytes = 0;
    uint64_t usedBytes = 0;
    uint64_t availableBytes = 0;

    double usagePercent() const {
    const uint64_t usableBytes =
        usedBytes + availableBytes;

    if (usableBytes == 0) {
        return 0.0;
    }

    return 100.0 *
           static_cast<double>(usedBytes) /
           static_cast<double>(usableBytes);
}
};

class DiskCollector {
public:
    DiskInfo collect(
        const std::string& path = "/"
    ) const;
};

#endif
