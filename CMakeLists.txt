cmake_minimum_required(VERSION 3.16)

project(LMonitor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================
# Common library
# ============================================================

add_library(lmonitor_common STATIC
    src/protocol/FrameCodec.cpp
)

target_include_directories(lmonitor_common
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_options(lmonitor_common
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)



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
    src/serializer/MetricsSerializer.cpp
    src/network/TcpClient.cpp

)

target_link_libraries(lmonitor_agent
    PRIVATE
        lmonitor_common
)

add_executable(lmonitor_server
    src/server_main.cpp
    src/server/TcpServer.cpp

    src/reactor/EventLoop.cpp
    src/reactor/Channel.cpp

    src/network/TcpConnection.cpp
    src/reactor/Acceptor.cpp

   
)

target_link_libraries(lmonitor_server
    PRIVATE
        lmonitor_common
)

target_include_directories(
    lmonitor_server
    PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_compile_options(
    lmonitor_server
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
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
