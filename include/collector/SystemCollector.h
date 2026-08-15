#ifndef LMONITOR_SYSTEM_COLLECTOR_H
#define LMONITOR_SYSTEM_COLLECTOR_H

#include <cstdint>
#include <string>

struct SystemInfo {
    std::string hostname;
    uint64_t uptimeSeconds = 0;
};

class SystemCollector {
public:
    SystemInfo collect() const;
};

#endif
