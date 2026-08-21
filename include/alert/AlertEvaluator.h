#ifndef LMONITOR_ALERT_EVALUATOR_H
#define LMONITOR_ALERT_EVALUATOR_H

#include "model/SystemMetrics.h"

#include <string>
#include <vector>


enum class AlertLevel {
    Warning,
    Critical
};


struct Alert {
    std::string hostname;

    std::string metric;

    AlertLevel level =
        AlertLevel::Warning;

    double currentValue =
        0.0;

    double threshold =
        0.0;

    std::string message;
};


class AlertEvaluator {
public:
    AlertEvaluator();


    std::vector<Alert> evaluate(
        const SystemMetrics& metrics
    ) const;


    static const char* levelToString(
        AlertLevel level
    ) noexcept;


private:
    double cpuWarningThreshold_;

    double memoryWarningThreshold_;

    double diskCriticalThreshold_;
};


#endif
