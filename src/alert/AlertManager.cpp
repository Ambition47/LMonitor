#include "alert/AlertManager.h"


#include "config/Config.h"
#include "log/Logger.h"



AlertManager::AlertManager(
    Config& config
)

    :

    fireDuration_(
        std::chrono::seconds(
            config.getInt(
                "fire_duration",
                5
            )
        )
    ),

    cpuThreshold_(
        config.getDouble(
            "cpu_threshold",
            80.0
        )
    ),

    memoryThreshold_(
        config.getDouble(
            "memory_threshold",
            90.0
        )
    )

{

}






std::string AlertManager::makeKey(
    const std::string& hostname,
    const std::string& metric
) const

{

    return hostname
        +
        "_"
        +
        metric;

}








std::vector<ManagedAlert>
AlertManager::update(
    const SystemMetrics& metrics
)

{

    std::lock_guard<std::mutex> lock(
        mutex_
    );



    std::vector<ManagedAlert> result;



    const auto now =
        std::chrono::steady_clock::now();




    auto rawAlerts =
        evaluator_.evaluate(
            metrics,
            cpuThreshold_,
            memoryThreshold_
        );



    std::unordered_map<
        std::string,
        bool
    > currentAlertKeys;





    for(
        const auto& alert :
        rawAlerts
    )

    {


        const std::string key =
            makeKey(
                alert.hostname,
                alert.metric
            );



        currentAlertKeys[key] =
            true;



        auto iterator =
            alerts_.find(
                key
            );



        if(
            iterator ==
            alerts_.end()
        )

        {

            ManagedAlert managed;



            managed.hostname =
                alert.hostname;


            managed.metric =
                alert.metric;


            managed.level =
                alert.level;


            managed.currentValue =
                alert.currentValue;


            managed.threshold =
                alert.threshold;


            managed.message =
                alert.message;



            managed.state =
                AlertState::Pending;



            managed.firstDetected =
                now;


            managed.lastUpdate =
                now;



            alerts_[key] =
                managed;



            continue;

        }





        ManagedAlert& managed =
            iterator->second;



        managed.currentValue =
            alert.currentValue;


        managed.lastUpdate =
            now;





        const auto duration =

            std::chrono::duration_cast<
                std::chrono::seconds
            >(
                now -
                managed.firstDetected
            );






        if(
            duration >= fireDuration_

            &&

            managed.state !=
                AlertState::Firing
        )

        {

            managed.state =
                AlertState::Firing;



            result.push_back(
                managed
            );

        }

    }








    for(
        auto& item :
        alerts_
    )

    {

        ManagedAlert& alert =
            item.second;



        if(
            currentAlertKeys.find(
                item.first
            )
            ==
            currentAlertKeys.end()
        )

        {


            if(
                alert.state ==
                AlertState::Firing
            )

            {

                alert.state =
                    AlertState::Normal;



                Logger::instance().info(

                    "Alert resolved: "

                    +

                    alert.hostname

                    +

                    " "

                    +

                    alert.metric

                );

            }

        }

    }





    return result;

}









std::vector<ManagedAlert>
AlertManager::getActiveAlerts()

{

    std::lock_guard<std::mutex> lock(
        mutex_
    );



    std::vector<ManagedAlert> result;



    for(
        const auto& item :
        alerts_
    )

    {

        if(
            item.second.state
            !=
            AlertState::Normal
        )

        {

            result.push_back(
                item.second
            );

        }

    }



    return result;

}
