cmake_minimum_required(VERSION 3.16)

project(LMonitor LANGUAGES CXX)

find_package(Threads REQUIRED)

# ============================================================
# C++ standard
# ============================================================

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_EXTENSIONS OFF)


# ============================================================
# Project-wide build options
#
# INTERFACE library:
#   - does not generate .a or .so
#   - only carries common build configuration
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
#
# Shared by Agent and Server.
# ============================================================

add_library(
    lmonitor_common
    STATIC

    src/protocol/FrameCodec.cpp
)


target_link_libraries(
    lmonitor_common

    PRIVATE
        lmonitor_build_options
)


# ============================================================
# LMonitor Agent
# ============================================================

add_executable(
    lmonitor_agent

    # --------------------------------------------------------
    # Entry
    # --------------------------------------------------------

    src/main.cpp


    # --------------------------------------------------------
    # Agent
    # --------------------------------------------------------

    src/agent/MonitorAgent.cpp


    # --------------------------------------------------------
    # System metric collectors
    # --------------------------------------------------------

    src/collector/CpuCollector.cpp

    src/collector/MemoryCollector.cpp

    src/collector/LoadCollector.cpp

    src/collector/SystemCollector.cpp

    src/collector/DiskCollector.cpp

    src/collector/NetworkCollector.cpp

    src/collector/ProcessCollector.cpp


    # --------------------------------------------------------
    # Serialization
    # --------------------------------------------------------

    src/serializer/MetricsSerializer.cpp


    # --------------------------------------------------------
    # Network
    # --------------------------------------------------------

    src/network/TcpClient.cpp
)


target_link_libraries(
    lmonitor_agent

    PRIVATE
        lmonitor_common
        lmonitor_build_options
)


# ============================================================
# LMonitor Reactor Server
# ============================================================

add_executable(
    lmonitor_server

    # --------------------------------------------------------
    # Entry
    # --------------------------------------------------------

    src/server_main.cpp


    # --------------------------------------------------------
    # Server
    # --------------------------------------------------------

    src/server/TcpServer.cpp


    # --------------------------------------------------------
    # Reactor
    # --------------------------------------------------------

    src/reactor/EventLoop.cpp

    src/reactor/Channel.cpp

    src/reactor/Acceptor.cpp


    # --------------------------------------------------------
    # Network connection
    # --------------------------------------------------------

    src/network/TcpConnection.cpp
)


target_link_libraries(
    lmonitor_server

    PRIVATE
        lmonitor_common
        lmonitor_build_options
	Threads::Threads
)
