#include "serializer/MetricsDeserializer.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>


namespace {

// ============================================================
// Protocol constants
// ============================================================

constexpr const char* PROTOCOL_VERSION =
    "LMONITOR/1";


// ============================================================
// Parse unsigned 64-bit integer
// ============================================================

uint64_t parseUint64(
    const std::string& value,
    const std::string& fieldName
) {
    if (value.empty()) {

        throw std::runtime_error(
            "Empty integer field: " +
            fieldName
        );
    }


    std::size_t parsedCharacters =
        0;


    unsigned long long parsedValue =
        0;


    try {

        parsedValue =
            std::stoull(
                value,
                &parsedCharacters,
                10
            );

    } catch (
        const std::exception&
    ) {

        throw std::runtime_error(
            "Invalid unsigned integer for field '" +
            fieldName +
            "': " +
            value
        );
    }


    if (parsedCharacters !=
        value.size()) {

        throw std::runtime_error(
            "Invalid trailing characters in field '" +
            fieldName +
            "': " +
            value
        );
    }


    return static_cast<uint64_t>(
        parsedValue
    );
}


// ============================================================
// Parse size_t safely
// ============================================================

std::size_t parseSize(
    const std::string& value,
    const std::string& fieldName
) {
    const uint64_t parsedValue =
        parseUint64(
            value,
            fieldName
        );


    if (parsedValue >
        static_cast<uint64_t>(
            std::numeric_limits<
                std::size_t
            >::max()
        )) {

        throw std::runtime_error(
            "Value too large for field '" +
            fieldName +
            "'"
        );
    }


    return static_cast<std::size_t>(
        parsedValue
    );
}


// ============================================================
// Parse int safely
// ============================================================

int parseInt(
    const std::string& value,
    const std::string& fieldName
) {
    if (value.empty()) {

        throw std::runtime_error(
            "Empty integer field: " +
            fieldName
        );
    }


    std::size_t parsedCharacters =
        0;


    long long parsedValue =
        0;


    try {

        parsedValue =
            std::stoll(
                value,
                &parsedCharacters,
                10
            );

    } catch (
        const std::exception&
    ) {

        throw std::runtime_error(
            "Invalid integer for field '" +
            fieldName +
            "': " +
            value
        );
    }


    if (parsedCharacters !=
        value.size()) {

        throw std::runtime_error(
            "Invalid trailing characters in field '" +
            fieldName +
            "': " +
            value
        );
    }


    if (parsedValue <
            std::numeric_limits<int>::min() ||
        parsedValue >
            std::numeric_limits<int>::max()) {

        throw std::runtime_error(
            "Integer out of range for field '" +
            fieldName +
            "'"
        );
    }


    return static_cast<int>(
        parsedValue
    );
}


// ============================================================
// Parse finite double
// ============================================================

double parseDouble(
    const std::string& value,
    const std::string& fieldName
) {
    if (value.empty()) {

        throw std::runtime_error(
            "Empty floating-point field: " +
            fieldName
        );
    }


    std::size_t parsedCharacters =
        0;


    double parsedValue =
        0.0;


    try {

        parsedValue =
            std::stod(
                value,
                &parsedCharacters
            );

    } catch (
        const std::exception&
    ) {

        throw std::runtime_error(
            "Invalid floating-point value for field '" +
            fieldName +
            "': " +
            value
        );
    }


    if (parsedCharacters !=
        value.size()) {

        throw std::runtime_error(
            "Invalid trailing characters in field '" +
            fieldName +
            "': " +
            value
        );
    }


    if (!std::isfinite(
            parsedValue
        )) {

        throw std::runtime_error(
            "Non-finite value for field '" +
            fieldName +
            "'"
        );
    }


    return parsedValue;
}


// ============================================================
// Split one process record
//
// Format:
//
// process=
// pid,cpu%,mem%,rssKB,name
//
// IMPORTANT:
//
// Process names may theoretically contain commas.
//
// Therefore we split only the first four commas.
// Everything after the fourth comma belongs to the name.
// ============================================================

ProcessMetric parseProcess(
    const std::string& value
) {
    std::vector<std::string>
        fields;


    fields.reserve(
        5
    );


    std::size_t fieldStart =
        0;


    for (std::size_t i = 0;
         i < 4;
         ++i) {

        const std::size_t commaPosition =
            value.find(
                ',',
                fieldStart
            );


        if (commaPosition ==
            std::string::npos) {

            throw std::runtime_error(
                "Malformed process record: " +
                value
            );
        }


        fields.push_back(
            value.substr(
                fieldStart,
                commaPosition -
                    fieldStart
            )
        );


        fieldStart =
            commaPosition +
            1;
    }


    fields.push_back(
        value.substr(
            fieldStart
        )
    );


    if (fields.size() != 5) {

        throw std::runtime_error(
            "Malformed process record"
        );
    }


    ProcessMetric process;


    process.pid =
        parseInt(
            fields[0],
            "process.pid"
        );


    if (process.pid < 0) {

        throw std::runtime_error(
            "Negative process pid"
        );
    }


    process.cpuUsagePercent =
        parseDouble(
            fields[1],
            "process.cpu_usage_percent"
        );


    process.memoryUsagePercent =
        parseDouble(
            fields[2],
            "process.memory_usage_percent"
        );


    process.residentMemoryKB =
        parseUint64(
            fields[3],
            "process.resident_memory_kb"
        );


    process.name =
        fields[4];


    if (process.name.empty()) {

        throw std::runtime_error(
            "Process name must not be empty"
        );
    }


    return process;
}


// ============================================================
// Ensure one required field appears only once
// ============================================================

void markRequiredField(
    std::unordered_set<std::string>& fields,
    const std::string& key
) {
    const auto result =
        fields.insert(
            key
        );


    if (!result.second) {

        throw std::runtime_error(
            "Duplicate field: " +
            key
        );
    }
}


// ============================================================
// Verify all mandatory fields
// ============================================================

void verifyRequiredFields(
    const std::unordered_set<std::string>& fields
) {
    static const std::vector<std::string>
        requiredFields = {

            "hostname",
            "uptime_seconds",
            "logical_cpus",

            "cpu_usage_percent",

            "memory_total_kb",
            "memory_used_kb",
            "memory_available_kb",
            "memory_usage_percent",

            "load_1",
            "load_5",
            "load_15",

            "disk_mount",
            "disk_total_bytes",
            "disk_used_bytes",
            "disk_available_bytes",
            "disk_usage_percent",

            "network_interface",
            "network_rx_bytes_per_second",
            "network_tx_bytes_per_second",

            "sample_interval_seconds",

            "process_count"
        };


    for (const auto& field :
         requiredFields) {

        if (fields.find(
                field
            ) == fields.end()) {

            throw std::runtime_error(
                "Missing required field: " +
                field
            );
        }
    }
}

}  // namespace


// ============================================================
// Deserialize LMONITOR/1 payload
// ============================================================

SystemMetrics MetricsDeserializer::deserialize(
    const std::string& payload
) const {

    if (payload.empty()) {

        throw std::runtime_error(
            "Empty metrics payload"
        );
    }


    std::istringstream input(
        payload
    );


    // ========================================================
    // Protocol version
    // ========================================================

    std::string line;


    if (!std::getline(
            input,
            line
        )) {

        throw std::runtime_error(
            "Missing protocol version"
        );
    }


    if (!line.empty() &&
        line.back() == '\r') {

        line.pop_back();
    }


    if (line !=
        PROTOCOL_VERSION) {

        throw std::runtime_error(
            "Unsupported metrics protocol version: " +
            line
        );
    }


    SystemMetrics metrics;


    std::unordered_set<std::string>
        receivedFields;


    std::size_t expectedProcessCount =
        0;


    bool processCountSeen =
        false;


    // ========================================================
    // Parse fields
    // ========================================================

    while (std::getline(
        input,
        line
    )) {

        // ----------------------------------------------------
        // Support CRLF input as well as LF.
        // ----------------------------------------------------

        if (!line.empty() &&
            line.back() == '\r') {

            line.pop_back();
        }


        if (line.empty()) {
            continue;
        }


        const std::size_t equalsPosition =
            line.find(
                '='
            );


        if (equalsPosition ==
            std::string::npos) {

            throw std::runtime_error(
                "Malformed metrics line: " +
                line
            );
        }


        const std::string key =
            line.substr(
                0,
                equalsPosition
            );


        const std::string value =
            line.substr(
                equalsPosition +
                1
            );


        if (key.empty()) {

            throw std::runtime_error(
                "Metrics field name must not be empty"
            );
        }


        // ====================================================
        // Repeated process records
        // ====================================================

        if (key ==
            "process") {

            metrics.topProcesses.push_back(
                parseProcess(
                    value
                )
            );


            continue;
        }


        // ====================================================
        // Required scalar fields may appear only once.
        // ====================================================

        markRequiredField(
            receivedFields,
            key
        );


        // ====================================================
        // System
        // ====================================================

        if (key ==
            "hostname") {

            if (value.empty()) {

                throw std::runtime_error(
                    "hostname must not be empty"
                );
            }


            metrics.hostname =
                value;
        }

        else if (key ==
                 "uptime_seconds") {

            metrics.uptimeSeconds =
                parseUint64(
                    value,
                    key
                );
        }

        else if (key ==
                 "logical_cpus") {

            metrics.logicalCpuCount =
                parseSize(
                    value,
                    key
                );


            if (metrics.logicalCpuCount ==
                0) {

                throw std::runtime_error(
                    "logical_cpus must be greater than 0"
                );
            }
        }


        // ====================================================
        // CPU
        // ====================================================

        else if (key ==
                 "cpu_usage_percent") {

            metrics.cpuUsagePercent =
                parseDouble(
                    value,
                    key
                );
        }


        // ====================================================
        // Memory
        // ====================================================

        else if (key ==
                 "memory_total_kb") {

            metrics.memoryTotalKB =
                parseUint64(
                    value,
                    key
                );
        }

        else if (key ==
                 "memory_used_kb") {

            metrics.memoryUsedKB =
                parseUint64(
                    value,
                    key
                );
        }

        else if (key ==
                 "memory_available_kb") {

            metrics.memoryAvailableKB =
                parseUint64(
                    value,
                    key
                );
        }

        else if (key ==
                 "memory_usage_percent") {

            metrics.memoryUsagePercent =
                parseDouble(
                    value,
                    key
                );
        }


        // ====================================================
        // Load
        // ====================================================

        else if (key ==
                 "load_1") {

            metrics.load1 =
                parseDouble(
                    value,
                    key
                );
        }

        else if (key ==
                 "load_5") {

            metrics.load5 =
                parseDouble(
                    value,
                    key
                );
        }

        else if (key ==
                 "load_15") {

            metrics.load15 =
                parseDouble(
                    value,
                    key
                );
        }


        // ====================================================
        // Disk
        // ====================================================

        else if (key ==
                 "disk_mount") {

            if (value.empty()) {

                throw std::runtime_error(
                    "disk_mount must not be empty"
                );
            }


            metrics.diskMountPoint =
                value;
        }

        else if (key ==
                 "disk_total_bytes") {

            metrics.diskTotalBytes =
                parseUint64(
                    value,
                    key
                );
        }

        else if (key ==
                 "disk_used_bytes") {

            metrics.diskUsedBytes =
                parseUint64(
                    value,
                    key
                );
        }

        else if (key ==
                 "disk_available_bytes") {

            metrics.diskAvailableBytes =
                parseUint64(
                    value,
                    key
                );
        }

        else if (key ==
                 "disk_usage_percent") {

            metrics.diskUsagePercent =
                parseDouble(
                    value,
                    key
                );
        }


        // ====================================================
        // Network
        // ====================================================

        else if (key ==
                 "network_interface") {

            if (value.empty()) {

                throw std::runtime_error(
                    "network_interface must not be empty"
                );
            }


            metrics.networkInterface =
                value;
        }

        else if (key ==
                 "network_rx_bytes_per_second") {

            metrics.networkRxBytesPerSecond =
                parseDouble(
                    value,
                    key
                );
        }

        else if (key ==
                 "network_tx_bytes_per_second") {

            metrics.networkTxBytesPerSecond =
                parseDouble(
                    value,
                    key
                );
        }


        // ====================================================
        // Sampling
        // ====================================================

        else if (key ==
                 "sample_interval_seconds") {

            metrics.sampleIntervalSeconds =
                parseDouble(
                    value,
                    key
                );


            if (metrics.sampleIntervalSeconds <=
                0.0) {

                throw std::runtime_error(
                    "sample_interval_seconds must be greater than 0"
                );
            }
        }


        // ====================================================
        // Processes
        // ====================================================

        else if (key ==
                 "process_count") {

            expectedProcessCount =
                parseSize(
                    value,
                    key
                );


            processCountSeen =
                true;
        }


        // ====================================================
        // Unknown field
        //
        // Reject unknown fields for protocol version 1 so that
        // malformed or mismatched messages cannot silently
        // pass validation.
        // ====================================================

        else {

            throw std::runtime_error(
                "Unknown metrics field: " +
                key
            );
        }
    }


    // ========================================================
    // Validate required fields
    // ========================================================

    verifyRequiredFields(
        receivedFields
    );


    if (!processCountSeen) {

        throw std::runtime_error(
            "Missing process_count"
        );
    }


    // ========================================================
    // Validate process count
    // ========================================================

    if (metrics.topProcesses.size() !=
        expectedProcessCount) {

        throw std::runtime_error(
            "Process count mismatch: expected " +
            std::to_string(
                expectedProcessCount
            ) +
            ", received " +
            std::to_string(
                metrics.topProcesses.size()
            )
        );
    }


    return metrics;
}
