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
    // Host liveness state
    // ========================================================

    enum class HostStatus {
        Online,
        Stale,
        Offline
    };


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
    // Determine current status of one stored record
    // ========================================================

    HostStatus getStatus(
        const StoredMetrics& storedMetrics
    ) const;


    // ========================================================
    // Query current status directly by hostname
    //
    // true  -> host exists and status was returned
    // false -> host does not exist
    // ========================================================

    bool getStatus(
        const std::string& hostname,
        HostStatus& status
    ) const;


    // ========================================================
    // Convert status enum to readable text
    // ========================================================

    static const char* statusToString(
        HostStatus status
    ) noexcept;


    // ========================================================
    // Number of monitored hosts
    // ========================================================

    std::size_t size() const;


    bool empty() const;


private:
    // ========================================================
    // Liveness thresholds
    //
    // age <= 5 sec:
    //     ONLINE
    //
    // 5 sec < age <= 15 sec:
    //     STALE
    //
    // age > 15 sec:
    //     OFFLINE
    // ========================================================

    static constexpr std::chrono::seconds
        ONLINE_THRESHOLD {
            5
        };


    static constexpr std::chrono::seconds
        STALE_THRESHOLD {
            15
        };


private:
    mutable std::mutex mutex_;


    std::unordered_map<
        std::string,
        StoredMetrics
    > metricsByHostname_;
};

#endif
