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
# Common compile options
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


    # Protocol

    src/protocol/FrameCodec.cpp


    # Serializer

    src/serializer/MetricsSerializer.cpp

    src/serializer/MetricsDeserializer.cpp


    # Logger

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


    # Agent

    src/agent/MonitorAgent.cpp


    # Collector

    src/collector/CpuCollector.cpp

    src/collector/MemoryCollector.cpp

    src/collector/LoadCollector.cpp

    src/collector/SystemCollector.cpp

    src/collector/DiskCollector.cpp

    src/collector/NetworkCollector.cpp

    src/collector/ProcessCollector.cpp


    # Serializer

    src/serializer/MetricsSerializer.cpp


    # Network

    src/network/TcpClient.cpp
)



target_link_libraries(
    lmonitor_agent
    PRIVATE

    lmonitor_common

    lmonitor_build_options

    Threads::Threads
)



# ============================================================
# Server
# ============================================================

add_executable(
    lmonitor_server


    # Main

    src/server_main.cpp


    # Server

    src/server/TcpServer.cpp



    # Reactor

    src/reactor/EventLoop.cpp

    src/reactor/Channel.cpp

    src/reactor/Acceptor.cpp



    # Network

    src/network/TcpConnection.cpp



    # Thread

    src/thread/ThreadPool.cpp



    # Store

    src/store/MetricsStore.cpp
    src/store/MetricsHistoryStore.cpp

    src/store/StateTracker.cpp
    
    # Alert

    src/alert/AlertEvaluator.cpp
    src/alert/AlertManager.cpp


    # Timer

    src/timer/TimerFd.cpp



    # HTTP

    src/http/HttpResponse.cpp

    src/http/HttpServer.cpp
)



target_link_libraries(
    lmonitor_server
    PRIVATE


    lmonitor_common


    lmonitor_build_options


    Threads::Threads
)
