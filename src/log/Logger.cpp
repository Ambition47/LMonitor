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
    // If an existing log file is already too large,
    // rotate it before opening.
    // --------------------------------------------------------

    std::error_code fileSizeError;


    if (std::filesystem::exists(
            logFilePath_,
            fileSizeError
        ) &&
        !fileSizeError) {

        const std::uintmax_t existingFileSize =
            std::filesystem::file_size(
                logFilePath_,
                fileSizeError
            );


        if (!fileSizeError) {

            currentFileSize_ =
                static_cast<std::size_t>(
                    existingFileSize
                );
        }
    }


    if (currentFileSize_ >=
        MAX_LOG_FILE_SIZE) {

        rotateIfNeeded(
            0
        );
    }


    openLogFile();
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
    // Build complete log line before entering critical section.
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


    // +1 for '\n'
    const std::size_t incomingBytes =
        logLine.size() +
        1;


    // --------------------------------------------------------
    // Protect console output, file output and rotation.
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
    // Rotate before writing if the next record would exceed
    // the configured maximum file size.
    // --------------------------------------------------------

    rotateIfNeeded(
        incomingBytes
    );


    // --------------------------------------------------------
    // File output
    // --------------------------------------------------------

    if (logFile_.is_open()) {

        logFile_
            << logLine
            << '\n';


        logFile_.flush();


        currentFileSize_ +=
            incomingBytes;
    }
}


// ============================================================
// Open active log file
// ============================================================

void Logger::openLogFile() {

    // --------------------------------------------------------
    // If already open, close it first.
    // --------------------------------------------------------

    if (logFile_.is_open()) {

        logFile_.flush();

        logFile_.close();
    }


    logFile_.clear();


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

        return;
    }


    // --------------------------------------------------------
    // Refresh current file size.
    // --------------------------------------------------------

    std::error_code errorCode;


    const std::uintmax_t fileSize =
        std::filesystem::file_size(
            logFilePath_,
            errorCode
        );


    if (errorCode) {

        currentFileSize_ =
            0;

    } else {

        currentFileSize_ =
            static_cast<std::size_t>(
                fileSize
            );
    }
}


// ============================================================
// Rotate active log file when necessary
// ============================================================

void Logger::rotateIfNeeded(
    std::size_t incomingBytes
) {

    // --------------------------------------------------------
    // No rotation required.
    // --------------------------------------------------------

    if (currentFileSize_ +
            incomingBytes <=
        MAX_LOG_FILE_SIZE) {

        return;
    }


    // --------------------------------------------------------
    // Close current active file before rename.
    // --------------------------------------------------------

    if (logFile_.is_open()) {

        logFile_.flush();

        logFile_.close();
    }


    const std::filesystem::path activeLogPath(
        logFilePath_
    );


    const std::filesystem::path backupLogPath(
        logFilePath_ +
        ".1"
    );


    // --------------------------------------------------------
    // Delete the previous backup.
    //
    // Rotation policy:
    //
    // current.log
    // current.log.1
    //
    // Only one backup is preserved.
    // --------------------------------------------------------

    std::error_code errorCode;


    if (std::filesystem::exists(
            backupLogPath,
            errorCode
        )) {

        errorCode.clear();


        std::filesystem::remove(
            backupLogPath,
            errorCode
        );


        if (errorCode) {

            std::cerr
                << "Logger warning: failed to remove old rotated log: "
                << backupLogPath.string()
                << ": "
                << errorCode.message()
                << '\n';
        }
    }


    // --------------------------------------------------------
    // Rename current active log to .1
    // --------------------------------------------------------

    errorCode.clear();


    if (std::filesystem::exists(
            activeLogPath,
            errorCode
        ) &&
        !errorCode) {

        std::filesystem::rename(
            activeLogPath,
            backupLogPath,
            errorCode
        );


        if (errorCode) {

            std::cerr
                << "Logger warning: failed to rotate log file: "
                << activeLogPath.string()
                << ": "
                << errorCode.message()
                << '\n';


            // ------------------------------------------------
            // Keep the existing file usable even if rotation
            // failed.
            // ------------------------------------------------

            openLogFile();

            return;
        }
    }


    // --------------------------------------------------------
    // Start a fresh active log file.
    // --------------------------------------------------------

    currentFileSize_ =
        0;


    logFile_.clear();


    logFile_.open(
        logFilePath_,
        std::ios::out |
        std::ios::trunc
    );


    if (!logFile_.is_open()) {

        std::cerr
            << "Logger warning: failed to create new log file after rotation: "
            << logFilePath_
            << '\n';

        return;
    }


    currentFileSize_ =
        0;
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
    // Fallback
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


    std::filesystem::path projectDirectory;


    // --------------------------------------------------------
    // Development layout:
    //
    // ~/LMonitor/build/lmonitor_agent
    //
    // ->
    //
    // ~/LMonitor/logs/lmonitor_agent.log
    // --------------------------------------------------------

    if (executableDirectory.filename() == "build") {

    // 开发环境:
    // ~/LMonitor/build/lmonitor_agent
    //
    // 日志:
    // ~/LMonitor/logs/

    projectDirectory =
        executableDirectory
            .parent_path();

}
else if (
    executableDirectory.filename() == "bin" &&
    executableDirectory.parent_path().filename() == "lmonitor"
)
{

    // 生产环境:
    // /opt/lmonitor/bin/lmonitor_agent
    //
    // 日志:
    // /opt/lmonitor/logs/

    projectDirectory =
        executableDirectory
            .parent_path();

}
else {

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
