#ifndef LMONITOR_METRICS_SERIALIZER_H
#define LMONITOR_METRICS_SERIALIZER_H

#include "model/SystemMetrics.h"

#include <string>

class MetricsSerializer {
public:
    std::string serialize(
        const SystemMetrics& metrics
    ) const;
};

#endif
