#include "store/MetricsStore.h"

#include <stdexcept>
#include <utility>


// ============================================================
// Insert or replace latest metrics
// ============================================================

void MetricsStore::update(
    SystemMetrics metrics
) {

    // --------------------------------------------------------
    // hostname is the unique identity used by the current
    // in-memory store.
    // --------------------------------------------------------

    if (metrics.hostname.empty()) {

        throw std::invalid_argument(
            "MetricsStore cannot store metrics with empty hostname"
        );
    }


    const std::string hostname =
        metrics.hostname;


    StoredMetrics storedMetrics;


    storedMetrics.metrics =
        std::move(
            metrics
        );


    storedMetrics.updatedAt =
        std::chrono::system_clock::now();


    // --------------------------------------------------------
    // Multiple Worker threads may update the store
    // concurrently.
    // --------------------------------------------------------

    std::lock_guard<std::mutex>
        lock(
            mutex_
        );


    metricsByHostname_.insert_or_assign(
        hostname,
        std::move(
            storedMetrics
        )
    );
}


// ============================================================
// Query latest metrics for one host
// ============================================================

bool MetricsStore::getLatest(
    const std::string& hostname,
    StoredMetrics& result
) const {

    std::lock_guard<std::mutex>
        lock(
            mutex_
        );


    const auto iterator =
        metricsByHostname_.find(
            hostname
        );


    if (iterator ==
        metricsByHostname_.end()) {

        return false;
    }


    result =
        iterator->second;


    return true;
}


// ============================================================
// Get snapshot of all hosts
// ============================================================

std::vector<MetricsStore::StoredMetrics>
MetricsStore::getAll() const {

    std::lock_guard<std::mutex>
        lock(
            mutex_
        );


    std::vector<StoredMetrics>
        result;


    result.reserve(
        metricsByHostname_.size()
    );


    for (const auto& entry :
         metricsByHostname_) {

        result.push_back(
            entry.second
        );
    }


    return result;
}


// ============================================================
// Number of hosts
// ============================================================

std::size_t MetricsStore::size() const {

    std::lock_guard<std::mutex>
        lock(
            mutex_
        );


    return metricsByHostname_.size();
}


// ============================================================
// Empty check
// ============================================================

bool MetricsStore::empty() const {

    std::lock_guard<std::mutex>
        lock(
            mutex_
        );


    return metricsByHostname_.empty();
}
