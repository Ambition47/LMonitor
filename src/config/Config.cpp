#include "config/Config.h"


#include <fstream>
#include <sstream>



bool Config::load(
    const std::string& filename
)
{

    std::ifstream file(
        filename
    );


    if(!file.is_open())
    {
        return false;
    }



    std::string line;



    while(
        std::getline(
            file,
            line
        )
    )
    {

        if(
            line.empty()
        )
        {
            continue;
        }



        if(
            line[0] == '#'
        )
        {
            continue;
        }



        auto pos =
            line.find('=');



        if(
            pos == std::string::npos
        )
        {
            continue;
        }



        std::string key =
            line.substr(
                0,
                pos
            );


        std::string value =
            line.substr(
                pos + 1
            );



        values_[key] =
            value;

    }


    return true;
}




std::string Config::get(
    const std::string& key,
    const std::string& defaultValue
) const
{

    auto iterator =
        values_.find(
            key
        );


    if(
        iterator ==
        values_.end()
    )
    {
        return defaultValue;
    }


    return iterator->second;
}





int Config::getInt(
    const std::string& key,
    int defaultValue
) const
{

    auto value =
        get(
            key
        );


    if(
        value.empty()
    )
    {
        return defaultValue;
    }


    return std::stoi(
        value
    );

}

double Config::getDouble(
    const std::string& key,
    double defaultValue
) const
{

    auto value =
        get(
            key
        );


    if(
        value.empty()
    )
    {
        return defaultValue;
    }


    return std::stod(
        value
    );

}
