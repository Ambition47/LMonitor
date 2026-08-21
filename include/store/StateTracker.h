#ifndef LMONITOR_STATE_TRACKER_H
#define LMONITOR_STATE_TRACKER_H


#include "store/MetricsStore.h"


#include <mutex>
#include <string>
#include <unordered_map>



class StateTracker {

public:

    struct StateChange {

        bool changed =
            false;


        MetricsStore::HostStatus oldStatus =
            MetricsStore::HostStatus::Offline;


        MetricsStore::HostStatus newStatus =
            MetricsStore::HostStatus::Offline;
    };



    StateChange update(
        const std::string& hostname,
        MetricsStore::HostStatus currentStatus
    );



    void clear();



private:

    std::mutex mutex_;



    std::unordered_map<
        std::string,
        MetricsStore::HostStatus
    > lastStates_;

};


#endif
