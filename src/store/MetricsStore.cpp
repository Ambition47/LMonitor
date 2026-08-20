#include "store/MetricsStore.h"

#include <chrono>
#include <stdexcept>
#include <utility>


// ============================================================
// Insert or replace latest metrics
// ============================================================

void MetricsStore::update(
    SystemMetrics metrics
) {

    // --------------------------------------------------------
    // hostname is the current unique identity for one Agent.
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
    // Multiple Worker threads may update different hosts at
    // the same time.
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
// Determine status from one StoredMetrics snapshot
// ============================================================

MetricsStore::HostStatus
MetricsStore::getStatus(
    const StoredMetrics& storedMetrics
) const {

    const auto now =
        std::chrono::system_clock::now();


    // --------------------------------------------------------
    // Protect against unexpected clock adjustments.
    //
    // If updatedAt appears to be in the future, treat the
    // record as freshly updated instead of calculating a
    // negative age.
    // --------------------------------------------------------

    if (storedMetrics.updatedAt >
        now) {

        return HostStatus::Online;
    }


    const auto age =
        std::chrono::duration_cast<
            std::chrono::seconds
        >(
            now -
            storedMetrics.updatedAt
        );


    if (age <=
        ONLINE_THRESHOLD) {

        return HostStatus::Online;
    }


    if (age <=
        STALE_THRESHOLD) {

        return HostStatus::Stale;
    }


    return HostStatus::Offline;
}


// ============================================================
// Query status directly by hostname
// ============================================================

bool MetricsStore::getStatus(
    const std::string& hostname,
    HostStatus& status
) const {

    StoredMetrics storedMetrics;


    if (!getLatest(
            hostname,
            storedMetrics
        )) {

        return false;
    }


    status =
        getStatus(
            storedMetrics
        );


    return true;
}


// ============================================================
// Status to readable string
// ============================================================

const char* MetricsStore::statusToString(
    HostStatus status
) noexcept {

    switch (status) {

        case HostStatus::Online:

            return "ONLINE";


        case HostStatus::Stale:

            return "STALE";


        case HostStatus::Offline:

            return "OFFLINE";
    }


    return "UNKNOWN";
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
