#ifndef LMONITOR_LOAD_COLLECTOR_H
#define LMONITOR_LOAD_COLLECTOR_H

struct LoadInfo {
    double load1 = 0.0;
    double load5 = 0.0;
    double load15 = 0.0;
};

class LoadCollector {
public:
    LoadInfo collect() const;
};

#endif
