#include "alert/AlertEvaluator.h"

#include <string>


// ============================================================
// Constructor
// ============================================================

AlertEvaluator::AlertEvaluator()
    :
    cpuWarningThreshold_(
        80.0
    ),
    memoryWarningThreshold_(
        85.0
    ),
    diskCriticalThreshold_(
        90.0
    ) {
}


// ============================================================
// Evaluate metrics
// ============================================================

std::vector<Alert> AlertEvaluator::evaluate(
    const SystemMetrics& metrics
) const {
    std::vector<Alert> alerts;


    // ========================================================
    // CPU
    // ========================================================

    if (
        metrics.cpuUsagePercent >=
        cpuWarningThreshold_
    ) {
        Alert alert;


        alert.hostname =
            metrics.hostname;


        alert.metric =
            "cpu";


        alert.level =
            AlertLevel::Warning;


        alert.currentValue =
            metrics.cpuUsagePercent;


        alert.threshold =
            cpuWarningThreshold_;


        alert.message =
            "CPU usage exceeded warning threshold";


        alerts.push_back(
            alert
        );
    }


    // ========================================================
    // Memory
    // ========================================================

    if (
        metrics.memoryUsagePercent >=
        memoryWarningThreshold_
    ) {
        Alert alert;


        alert.hostname =
            metrics.hostname;


        alert.metric =
            "memory";


        alert.level =
            AlertLevel::Warning;


        alert.currentValue =
            metrics.memoryUsagePercent;


        alert.threshold =
            memoryWarningThreshold_;


        alert.message =
            "Memory usage exceeded warning threshold";


        alerts.push_back(
            alert
        );
    }


    // ========================================================
    // Disk
    // ========================================================

    if (
        metrics.diskUsagePercent >=
        diskCriticalThreshold_
    ) {
        Alert alert;


        alert.hostname =
            metrics.hostname;


        alert.metric =
            "disk";


        alert.level =
            AlertLevel::Critical;


        alert.currentValue =
            metrics.diskUsagePercent;


        alert.threshold =
            diskCriticalThreshold_;


        alert.message =
            "Disk usage exceeded critical threshold";


        alerts.push_back(
            alert
        );
    }


    return alerts;
}


// ============================================================
// Level -> string
// ============================================================

const char* AlertEvaluator::levelToString(
    AlertLevel level
) noexcept {
    switch (
        level
    ) {
        case AlertLevel::Warning:

            return "WARNING";


        case AlertLevel::Critical:

            return "CRITICAL";
    }


    return "UNKNOWN";
}
