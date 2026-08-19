#ifndef LMONITOR_LOGGER_H
#define LMONITOR_LOGGER_H

#include <fstream>
#include <mutex>
#include <string>


class Logger {
public:
    enum class Level {
        Info,
        Warning,
        Error
    };


    // ========================================================
    // Singleton
    // ========================================================

    static Logger& instance();


    // ========================================================
    // Main logging interface
    // ========================================================

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


    // ========================================================
    // Log file information
    // ========================================================

    const std::string& logFilePath() const noexcept;


private:
    Logger();


    ~Logger();


    Logger(
        const Logger&
    ) = delete;


    Logger& operator=(
        const Logger&
    ) = delete;


    // ========================================================
    // Helpers
    // ========================================================

    static const char* levelToString(
        Level level
    );


    static std::string currentTimestamp();


    static std::string detectLogFilePath();


private:
    std::mutex mutex_;


    std::ofstream logFile_;


    std::string logFilePath_;
};

#endif
