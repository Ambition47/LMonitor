#ifndef LMONITOR_LOGGER_H
#define LMONITOR_LOGGER_H

#include <mutex>
#include <string>


class Logger {
public:
    enum class Level {
        Info,
        Warning,
        Error
    };


    static Logger& instance();


    void log(
        Level level,
        const std::string& message
    );


    void info(
        const std::string& message
    );


    void warning(
        const std::string& message
    );


    void error(
        const std::string& message
    );


private:
    Logger() = default;


    Logger(
        const Logger&
    ) = delete;


    Logger& operator=(
        const Logger&
    ) = delete;


    static const char* levelToString(
        Level level
    );


    static std::string currentTimestamp();


private:
    std::mutex mutex_;
};

#endif
