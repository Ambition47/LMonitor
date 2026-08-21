#ifndef LMONITOR_METRICS_HISTORY_STORE_H
#define LMONITOR_METRICS_HISTORY_STORE_H

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>


struct MetricsHistoryPoint {
    std::int64_t timestampMs = 0;

    double cpuUsagePercent = 0.0;

    double memoryUsagePercent = 0.0;
};


class MetricsHistoryStore {
public:
    explicit MetricsHistoryStore(
        std::size_t maxPointsPerHost = 120
    );


    void add(
        const std::string& hostname,
        double cpuUsagePercent,
        double memoryUsagePercent
    );


    std::vector<MetricsHistoryPoint> get(
        const std::string& hostname
    ) const;


    void clear(
        const std::string& hostname
    );


    void clear();


private:
    std::size_t maxPointsPerHost_;


    mutable std::mutex mutex_;


    std::unordered_map<
        std::string,
        std::deque<MetricsHistoryPoint>
    > histories_;
};


#endif
