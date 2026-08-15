cmake_minimum_required(VERSION 3.16)

project(LMonitor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(lmonitor_agent
    src/main.cpp

    src/agent/MonitorAgent.cpp

    src/collector/CpuCollector.cpp
    src/collector/MemoryCollector.cpp
    src/collector/LoadCollector.cpp
    src/collector/SystemCollector.cpp
    src/collector/DiskCollector.cpp
    src/collector/NetworkCollector.cpp
    src/collector/ProcessCollector.cpp
)

target_include_directories(
    lmonitor_agent
    PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_compile_options(
    lmonitor_agent
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)
