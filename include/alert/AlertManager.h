#ifndef LMONITOR_ALERT_MANAGER_H
#define LMONITOR_ALERT_MANAGER_H


#include "alert/AlertEvaluator.h"
#include "model/SystemMetrics.h"


#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>



class Config;



enum class AlertState
{

    Normal,

    Pending,

    Firing

};



struct ManagedAlert
{

    std::string hostname;


    std::string metric;


    AlertLevel level =
        AlertLevel::Warning;


    AlertState state =
        AlertState::Normal;


    double currentValue =
        0.0;


    double threshold =
        0.0;


    std::string message;



    std::chrono::steady_clock::time_point firstDetected;



    std::chrono::steady_clock::time_point lastUpdate;

};



class AlertManager
{

public:


    explicit AlertManager(
        Config& config
    );



    std::vector<ManagedAlert> update(
        const SystemMetrics& metrics
    );



    std::vector<ManagedAlert> getActiveAlerts();



private:


    std::string makeKey(
        const std::string& hostname,
        const std::string& metric
    ) const;



private:


    AlertEvaluator evaluator_;



    std::mutex mutex_;



    std::unordered_map<
        std::string,
        ManagedAlert
    > alerts_;



    std::chrono::seconds fireDuration_;


    double cpuThreshold_ = 80.0;


    double memoryThreshold_ = 90.0;

};


#endif
