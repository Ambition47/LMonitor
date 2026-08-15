#include "collector/DiskCollector.h"

#include <stdexcept>
#include <sys/statvfs.h>

DiskInfo DiskCollector::collect(
    const std::string& path
) const {
    struct statvfs fsInfo;

    if (statvfs(path.c_str(), &fsInfo) != 0) {
        throw std::runtime_error(
            "Failed to get disk information for: " + path
        );
    }

    DiskInfo info;

    info.mountPoint = path;

    const uint64_t blockSize =
        static_cast<uint64_t>(fsInfo.f_frsize);

    info.totalBytes =
        static_cast<uint64_t>(fsInfo.f_blocks) *
        blockSize;

    info.availableBytes =
        static_cast<uint64_t>(fsInfo.f_bavail) *
        blockSize;

    const uint64_t freeBytes =
        static_cast<uint64_t>(fsInfo.f_bfree) *
        blockSize;

    info.usedBytes =
        info.totalBytes - freeBytes;

    return info;
}
