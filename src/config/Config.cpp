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



    values_.clear();



    std::string line;


    std::string currentSection;



    while(
        std::getline(
            file,
            line
        )
    )
    {

        // 去除空行

        if(
            line.empty()
        )
        {
            continue;
        }



        // 注释

        if(
            line[0] == '#'
        )
        {
            continue;
        }



        // -------------------------------
        // Section
        // -------------------------------

        if(
            line.front() == '['
            &&
            line.back() == ']'
        )
        {

            currentSection =
                line.substr(
                    1,
                    line.size() - 2
                );


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



        // -------------------------------
        // Build full key
        // -------------------------------

        std::string fullKey;



        if(
            !currentSection.empty()
        )
        {
            fullKey =
                currentSection
                +
                "."
                +
                key;
        }
        else
        {
            fullKey =
                key;
        }



        values_[fullKey] =
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
