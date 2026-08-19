#include "log/Logger.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>


// ============================================================
// Singleton instance
// ============================================================

Logger& Logger::instance() {

    static Logger logger;

    return logger;
}


// ============================================================
// Public helper functions
// ============================================================

void Logger::info(
    const std::string& message
) {

    log(
        Level::Info,
        message
    );
}


void Logger::warning(
    const std::string& message
) {

    log(
        Level::Warning,
        message
    );
}


void Logger::error(
    const std::string& message
) {

    log(
        Level::Error,
        message
    );
}


// ============================================================
// Main logging function
// ============================================================

void Logger::log(
    Level level,
    const std::string& message
) {

    // --------------------------------------------------------
    // Multiple worker threads may write logs concurrently.
    //
    // Protect one complete log line with one mutex.
    // --------------------------------------------------------

    std::lock_guard<std::mutex>
        lock(
            mutex_
        );


    std::ostream& output =
        level == Level::Error
            ? std::cerr
            : std::cout;


    output
        << currentTimestamp()
        << " ["
        << levelToString(
            level
        )
        << "]"
        << " [thread="
        << std::this_thread::get_id()
        << "] "
        << message
        << '\n';
}


// ============================================================
// Log level conversion
// ============================================================

const char* Logger::levelToString(
    Level level
) {

    switch (level) {

        case Level::Info:

            return "INFO";


        case Level::Warning:

            return "WARN";


        case Level::Error:

            return "ERROR";
    }


    return "UNKNOWN";
}


// ============================================================
// Current local timestamp
// ============================================================

std::string Logger::currentTimestamp() {

    const auto now =
        std::chrono::system_clock::now();


    const std::time_t currentTime =
        std::chrono::system_clock::to_time_t(
            now
        );


    std::tm localTime {};


    localtime_r(
        &currentTime,
        &localTime
    );


    std::ostringstream stream;


    stream
        << std::put_time(
            &localTime,
            "%Y-%m-%d %H:%M:%S"
        );


    return stream.str();
}
