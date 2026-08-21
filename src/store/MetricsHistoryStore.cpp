#include "store/MetricsHistoryStore.h"

#include <chrono>


MetricsHistoryStore::MetricsHistoryStore(
    std::size_t maxPointsPerHost
)
    : maxPointsPerHost_(
          maxPointsPerHost
      ) {

    if (maxPointsPerHost_ == 0) {
        maxPointsPerHost_ = 1;
    }
}


// ============================================================
// Add one history sample
// ============================================================

void MetricsHistoryStore::add(
    const std::string& hostname,
    double cpuUsagePercent,
    double memoryUsagePercent
) {
    if (hostname.empty()) {
        return;
    }


    const auto now =
        std::chrono::system_clock::now();


    const auto timestampMs =
        std::chrono::duration_cast<
            std::chrono::milliseconds
        >(
            now.time_since_epoch()
        ).count();


    MetricsHistoryPoint point;


    point.timestampMs =
        timestampMs;


    point.cpuUsagePercent =
        cpuUsagePercent;


    point.memoryUsagePercent =
        memoryUsagePercent;


    std::lock_guard<std::mutex> lock(
        mutex_
    );


    auto& history =
        histories_[hostname];


    history.push_back(
        point
    );


    while (
        history.size() >
        maxPointsPerHost_
    ) {
        history.pop_front();
    }
}


// ============================================================
// Get host history
// ============================================================

std::vector<MetricsHistoryPoint>
MetricsHistoryStore::get(
    const std::string& hostname
) const {
    std::lock_guard<std::mutex> lock(
        mutex_
    );


    const auto iterator =
        histories_.find(
            hostname
        );


    if (
        iterator ==
        histories_.end()
    ) {
        return {};
    }


    return std::vector<MetricsHistoryPoint>(
        iterator->second.begin(),
        iterator->second.end()
    );
}


// ============================================================
// Clear one host
// ============================================================

void MetricsHistoryStore::clear(
    const std::string& hostname
) {
    std::lock_guard<std::mutex> lock(
        mutex_
    );


    histories_.erase(
        hostname
    );
}


// ============================================================
// Clear all
// ============================================================

void MetricsHistoryStore::clear() {
    std::lock_guard<std::mutex> lock(
        mutex_
    );


    histories_.clear();
}
