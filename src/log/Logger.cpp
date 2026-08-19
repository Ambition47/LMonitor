#include "log/Logger.h"

#include <array>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>

#include <unistd.h>


namespace {

constexpr std::size_t EXECUTABLE_PATH_BUFFER_SIZE =
    4096;

}  // namespace


// ============================================================
// Constructor
// ============================================================

Logger::Logger()
    : logFilePath_(
          detectLogFilePath()
      ) {

    // --------------------------------------------------------
    // Create parent log directory if necessary.
    // --------------------------------------------------------

    const std::filesystem::path logPath(
        logFilePath_
    );


    const std::filesystem::path parentDirectory =
        logPath.parent_path();


    if (!parentDirectory.empty()) {

        std::error_code errorCode;


        std::filesystem::create_directories(
            parentDirectory,
            errorCode
        );


        if (errorCode) {

            std::cerr
                << "Logger warning: failed to create log directory: "
                << parentDirectory.string()
                << ": "
                << errorCode.message()
                << '\n';
        }
    }


    // --------------------------------------------------------
    // Open in append mode.
    //
    // Existing logs are preserved between process restarts.
    // --------------------------------------------------------

    logFile_.open(
        logFilePath_,
        std::ios::out |
        std::ios::app
    );


    if (!logFile_.is_open()) {

        std::cerr
            << "Logger warning: failed to open log file: "
            << logFilePath_
            << '\n';
    }
}


// ============================================================
// Destructor
// ============================================================

Logger::~Logger() {

    std::lock_guard<std::mutex>
        lock(
            mutex_
        );


    if (logFile_.is_open()) {

        logFile_.flush();

        logFile_.close();
    }
}


// ============================================================
// Singleton instance
// ============================================================

Logger& Logger::instance() {

    static Logger logger;

    return logger;
}


// ============================================================
// INFO
// ============================================================

void Logger::info(
    const std::string& message
) {

    log(
        Level::Info,
        message
    );
}


// ============================================================
// WARNING
// ============================================================

void Logger::warning(
    const std::string& message
) {

    log(
        Level::Warning,
        message
    );
}


// ============================================================
// ERROR
// ============================================================

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
    // Build the complete log line before taking the lock.
    // --------------------------------------------------------

    std::ostringstream stream;


    stream
        << currentTimestamp()
        << " ["
        << levelToString(
            level
        )
        << "]"
        << " [thread="
        << std::this_thread::get_id()
        << "] "
        << message;


    const std::string logLine =
        stream.str();


    // --------------------------------------------------------
    // Protect both console and file output.
    //
    // One complete log line is written atomically relative to
    // other Logger users.
    // --------------------------------------------------------

    std::lock_guard<std::mutex>
        lock(
            mutex_
        );


    // --------------------------------------------------------
    // Console output
    // --------------------------------------------------------

    if (level == Level::Error) {

        std::cerr
            << logLine
            << '\n';

        std::cerr.flush();

    } else {

        std::cout
            << logLine
            << '\n';

        std::cout.flush();
    }


    // --------------------------------------------------------
    // File output
    // --------------------------------------------------------

    if (logFile_.is_open()) {

        logFile_
            << logLine
            << '\n';


        // Current logging volume is small, so flushing every
        // line keeps important monitoring events immediately
        // visible on disk.
        logFile_.flush();
    }
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
// Current timestamp
// ============================================================

std::string Logger::currentTimestamp() {

    const auto now =
        std::chrono::system_clock::now();


    const std::time_t currentTime =
        std::chrono::system_clock::to_time_t(
            now
        );


    std::tm localTime {};


    if (localtime_r(
            &currentTime,
            &localTime
        ) == nullptr) {

        return "0000-00-00 00:00:00";
    }


    std::ostringstream stream;


    stream
        << std::put_time(
            &localTime,
            "%Y-%m-%d %H:%M:%S"
        );


    return stream.str();
}


// ============================================================
// Detect default log file path
//
// Expected development layout:
//
//   ~/LMonitor/
//       build/
//           lmonitor_agent
//           lmonitor_server
//
//       logs/
//           lmonitor_agent.log
//           lmonitor_server.log
//
// The executable name is used to separate Agent and Server
// logs automatically.
// ============================================================

std::string Logger::detectLogFilePath() {

    std::array<
        char,
        EXECUTABLE_PATH_BUFFER_SIZE
    > buffer {};


    const ssize_t length =
        readlink(
            "/proc/self/exe",
            buffer.data(),
            buffer.size() - 1
        );


    // --------------------------------------------------------
    // Fallback if /proc/self/exe cannot be resolved.
    // --------------------------------------------------------

    if (length <= 0) {

        return "logs/lmonitor.log";
    }


    buffer[
        static_cast<std::size_t>(
            length
        )
    ] = '\0';


    const std::filesystem::path executablePath(
        buffer.data()
    );


    const std::string executableName =
        executablePath
            .filename()
            .string();


    const std::filesystem::path executableDirectory =
        executablePath
            .parent_path();


    // --------------------------------------------------------
    // Development build layout:
    //
    // /home/.../LMonitor/build/lmonitor_agent
    //
    // becomes:
    //
    // /home/.../LMonitor/logs/lmonitor_agent.log
    // --------------------------------------------------------

    std::filesystem::path projectDirectory;


    if (executableDirectory
            .filename() ==
        "build") {

        projectDirectory =
            executableDirectory
                .parent_path();

    } else {

        // ----------------------------------------------------
        // Fallback for binaries not located directly inside a
        // directory named "build".
        //
        // Place logs next to the executable in logs/.
        // ----------------------------------------------------

        projectDirectory =
            executableDirectory;
    }


    const std::filesystem::path logDirectory =
        projectDirectory /
        "logs";


    const std::filesystem::path logFilePath =
        logDirectory /
        (
            executableName +
            ".log"
        );


    return logFilePath.string();
}


// ============================================================
// Log file path accessor
// ============================================================

const std::string&
Logger::logFilePath() const noexcept {

    return logFilePath_;
}
