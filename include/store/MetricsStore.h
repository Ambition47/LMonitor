#ifndef LMONITOR_METRICS_STORE_H
#define LMONITOR_METRICS_STORE_H

#include "model/SystemMetrics.h"

#include <chrono>
#include <cstddef>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>


class MetricsStore {
public:
    // ========================================================
    // One latest metrics record for one Agent host
    // ========================================================

    struct StoredMetrics {
        SystemMetrics metrics;

        std::chrono::system_clock::time_point
            updatedAt;
    };


    // ========================================================
    // Insert or replace latest metrics for a hostname
    // ========================================================

    void update(
        SystemMetrics metrics
    );


    // ========================================================
    // Query one host
    //
    // true  -> found
    // false -> host does not exist
    // ========================================================

    bool getLatest(
        const std::string& hostname,
        StoredMetrics& result
    ) const;


    // ========================================================
    // Get snapshot of all hosts
    // ========================================================

    std::vector<StoredMetrics>
    getAll() const;


    // ========================================================
    // Number of monitored hosts
    // ========================================================

    std::size_t size() const;


    bool empty() const;


private:
    mutable std::mutex mutex_;


    std::unordered_map<
        std::string,
        StoredMetrics
    > metricsByHostname_;
};

#endif
