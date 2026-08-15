#include "agent/MonitorAgent.h"

#include <csignal>
#include <iostream>

namespace {

volatile std::sig_atomic_t g_running = 1;

void handleSignal(int signal) {
    if (signal == SIGINT ||
        signal == SIGTERM) {
        g_running = 0;
    }
}

bool registerSignalHandlers() {
    struct sigaction action {};

    action.sa_handler = handleSignal;

    sigemptyset(&action.sa_mask);

    action.sa_flags = 0;

    if (sigaction(
            SIGINT,
            &action,
            nullptr
        ) == -1) {
        return false;
    }

    if (sigaction(
            SIGTERM,
            &action,
            nullptr
        ) == -1) {
        return false;
    }

    return true;
}

}  // namespace


int main() {
    try {
        // 注册 SIGINT / SIGTERM
        if (!registerSignalHandlers()) {
            std::cerr
                << "Error: Failed to register "
                << "signal handlers\n";

            return 1;
        }

        // 创建并初始化监控 Agent
        MonitorAgent agent;

        // 启动监控循环
        agent.run(g_running);

        // run() 正常退出后执行
        std::cout
            << "\nLMonitor Agent "
            << "shutting down gracefully...\n";

    } catch (const std::exception& e) {
        std::cerr
            << "Error: "
            << e.what()
            << '\n';

        return 1;
    }

    return 0;
}
