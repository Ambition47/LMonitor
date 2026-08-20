cmake_minimum_required(VERSION 3.16)

project(
    LMonitor
    LANGUAGES CXX
)


# ============================================================
# Threads
# ============================================================

find_package(
    Threads
    REQUIRED
)


# ============================================================
# C++ standard
# ============================================================

set(
    CMAKE_CXX_STANDARD
    17
)

set(
    CMAKE_CXX_STANDARD_REQUIRED
    ON
)

set(
    CMAKE_CXX_EXTENSIONS
    OFF
)


# ============================================================
# Common build options
# ============================================================

add_library(
    lmonitor_build_options
    INTERFACE
)


target_include_directories(
    lmonitor_build_options
    INTERFACE

    ${PROJECT_SOURCE_DIR}/include
)


target_compile_options(
    lmonitor_build_options
    INTERFACE

    -Wall
    -Wextra
    -Wpedantic
)


# ============================================================
# Common library
# ============================================================

add_library(
    lmonitor_common
    STATIC

    src/protocol/FrameCodec.cpp

    src/serializer/MetricsDeserializer.cpp

    src/log/Logger.cpp
)


target_link_libraries(
    lmonitor_common
    PRIVATE

    lmonitor_build_options
)


# ============================================================
# Agent
# ============================================================

add_executable(
    lmonitor_agent

    src/main.cpp

    src/agent/MonitorAgent.cpp

    src/collector/CpuCollector.cpp
    src/collector/MemoryCollector.cpp
    src/collector/LoadCollector.cpp
    src/collector/SystemCollector.cpp
    src/collector/DiskCollector.cpp
    src/collector/NetworkCollector.cpp
    src/collector/ProcessCollector.cpp

    src/serializer/MetricsSerializer.cpp

    src/network/TcpClient.cpp
)


target_link_libraries(
    lmonitor_agent
    PRIVATE

    lmonitor_common
    lmonitor_build_options
)


# ============================================================
# Server
# ============================================================

add_executable(
    lmonitor_server

    src/server_main.cpp

    src/server/TcpServer.cpp

    src/reactor/EventLoop.cpp
    src/reactor/Channel.cpp
    src/reactor/Acceptor.cpp

    src/network/TcpConnection.cpp

    src/thread/ThreadPool.cpp

    src/store/MetricsStore.cpp
)


target_link_libraries(
    lmonitor_server
    PRIVATE

    lmonitor_common
    lmonitor_build_options
    Threads::Threads
)
