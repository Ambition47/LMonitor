#ifndef LMONITOR_METRICS_DESERIALIZER_H
#define LMONITOR_METRICS_DESERIALIZER_H

#include "model/SystemMetrics.h"

#include <string>


class MetricsDeserializer {
public:
    SystemMetrics deserialize(
        const std::string& payload
    ) const;
};

#endif
