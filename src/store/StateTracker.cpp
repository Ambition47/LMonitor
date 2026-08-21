#include "store/StateTracker.h"



StateTracker::StateChange
StateTracker::update(
    const std::string& hostname,
    MetricsStore::HostStatus currentStatus
)
{

    std::lock_guard<std::mutex>
        lock(
            mutex_
        );



    StateChange result;



    const auto iterator =
        lastStates_.find(
            hostname
        );



    // ========================================================
    // First time seeing this host
    // ========================================================

    if (iterator ==
        lastStates_.end()) {


        lastStates_[hostname] =
            currentStatus;



        result.changed =
            true;


        result.oldStatus =
            MetricsStore::HostStatus::Offline;


        result.newStatus =
            currentStatus;



        return result;
    }




    // ========================================================
    // Existing host
    // ========================================================

    result.oldStatus =
        iterator->second;



    result.newStatus =
        currentStatus;



    if (iterator->second !=
        currentStatus) {


        result.changed =
            true;



        iterator->second =
            currentStatus;
    }



    return result;
}





void StateTracker::clear()
{

    std::lock_guard<std::mutex>
        lock(
            mutex_
        );


    lastStates_.clear();
}
