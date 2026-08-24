#ifndef LMONITOR_CONFIG_H
#define LMONITOR_CONFIG_H


#include <string>
#include <unordered_map>


class Config
{

public:


    bool load(
        const std::string& filename
    );



    std::string get(
        const std::string& key,
        const std::string& defaultValue = ""
    ) const;



    int getInt(
        const std::string& key,
        int defaultValue = 0
    ) const;



    double getDouble(
        const std::string& key,
        double defaultValue = 0.0
    ) const;



private:

    std::unordered_map<
        std::string,
        std::string
    > values_;

};


#endif
