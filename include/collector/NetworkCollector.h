#ifndef LMONITOR_NETWORK_COLLECTOR_H
#define LMONITOR_NETWORK_COLLECTOR_H

#include <cstdint>
#include <string>

struct NetworkStats {
    std::string interfaceName;

    uint64_t rxBytes = 0;
    uint64_t txBytes = 0;
};

struct NetworkRate {
    double rxBytesPerSecond = 0.0;
    double txBytesPerSecond = 0.0;
};

class NetworkCollector {
public:
    std::string detectDefaultInterface() const;

    NetworkStats collect(
        const std::string& interfaceName
    ) const;

    NetworkRate calculateRate(
        const NetworkStats& previous,
        const NetworkStats& current,
        double intervalSeconds
    ) const;
};

#endif
