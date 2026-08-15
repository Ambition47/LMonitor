#include "collector/NetworkCollector.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

std::string
NetworkCollector::detectDefaultInterface() const {
    std::ifstream file("/proc/net/route");

    if (!file.is_open()) {
        throw std::runtime_error(
            "Failed to open /proc/net/route"
        );
    }

    std::string line;

    // 跳过第一行表头
    std::getline(file, line);

    while (std::getline(file, line)) {
        std::istringstream iss(line);

        std::string interfaceName;
        std::string destination;
        std::string gateway;
        unsigned int flags = 0;

        if (!(iss >> interfaceName
                  >> destination
                  >> gateway
                  >> std::hex
                  >> flags)) {
            continue;
        }

        if (destination == "00000000") {
            return interfaceName;
        }
    }

    throw std::runtime_error(
        "Default network interface not found"
    );
}



NetworkStats NetworkCollector::collect(
    const std::string& interfaceName
) const {
    std::ifstream file("/proc/net/dev");

    if (!file.is_open()) {
        throw std::runtime_error(
            "Failed to open /proc/net/dev"
        );
    }

    std::string line;

    // 跳过前两行表头
    std::getline(file, line);
    std::getline(file, line);

    while (std::getline(file, line)) {
        std::size_t colonPos = line.find(':');

        if (colonPos == std::string::npos) {
            continue;
        }

        std::string name =
            line.substr(0, colonPos);

        // 去掉接口名前后的空格
        const std::size_t first =
            name.find_first_not_of(" \t");

        const std::size_t last =
            name.find_last_not_of(" \t");

        if (first == std::string::npos ||
            last == std::string::npos) {
            continue;
        }

        name = name.substr(
            first,
            last - first + 1
        );

        if (name != interfaceName) {
            continue;
        }

        std::string data =
            line.substr(colonPos + 1);

        std::istringstream iss(data);

        NetworkStats stats;
        stats.interfaceName = name;

        uint64_t rxPackets;
        uint64_t rxErrs;
        uint64_t rxDrop;
        uint64_t rxFifo;
        uint64_t rxFrame;
        uint64_t rxCompressed;
        uint64_t rxMulticast;

        uint64_t txPackets;
        uint64_t txErrs;
        uint64_t txDrop;
        uint64_t txFifo;
        uint64_t txColls;
        uint64_t txCarrier;
        uint64_t txCompressed;

        iss >> stats.rxBytes
            >> rxPackets
            >> rxErrs
            >> rxDrop
            >> rxFifo
            >> rxFrame
            >> rxCompressed
            >> rxMulticast
            >> stats.txBytes
            >> txPackets
            >> txErrs
            >> txDrop
            >> txFifo
            >> txColls
            >> txCarrier
            >> txCompressed;

        if (iss.fail()) {
            throw std::runtime_error(
                "Failed to parse network interface: "
                + interfaceName
            );
        }

        return stats;
    }

    throw std::runtime_error(
        "Network interface not found: "
        + interfaceName
    );
}

NetworkRate NetworkCollector::calculateRate(
    const NetworkStats& previous,
    const NetworkStats& current,
    double intervalSeconds
) const {
    if (intervalSeconds <= 0.0) {
        throw std::runtime_error(
            "Invalid network sampling interval"
        );
    }

    NetworkRate rate;

    if (current.rxBytes >= previous.rxBytes) {
        rate.rxBytesPerSecond =
            static_cast<double>(
                current.rxBytes - previous.rxBytes
            ) / intervalSeconds;
    }

    if (current.txBytes >= previous.txBytes) {
        rate.txBytesPerSecond =
            static_cast<double>(
                current.txBytes - previous.txBytes
            ) / intervalSeconds;
    }

    return rate;
}
