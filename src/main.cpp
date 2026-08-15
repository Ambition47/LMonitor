#include "agent/MonitorAgent.h"

#include <csignal>
#include <iostream>
#include <stdexcept>
#include <string>


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


int main(
    int argc,
    char* argv[]
) {
     double intervalSeconds = 1.0;

for (int i = 1; i < argc; ++i) {
    const std::string argument =
        argv[i];

    if (argument == "--interval") {

        if (i + 1 >= argc) {
            throw std::invalid_argument(
                "--interval requires a value"
            );
        }

        intervalSeconds =
            std::stod(argv[++i]);

        if (intervalSeconds <= 0.0) {
            throw std::invalid_argument(
                "Interval must be greater than 0"
            );
        }

    } else if (argument == "--help" ||
               argument == "-h") {

        std::cout
            << "Usage: "
            << argv[0]
            << " [--interval seconds]\n\n"
            << "Options:\n"
            << "  --interval <seconds>"
            << "  Sampling interval, default: 1.0\n"
            << "  -h, --help"
            << "          Show this help message\n";

        return 0;

    } else {
        throw std::invalid_argument(
            "Unknown argument: " +
            argument
        );
    }
}



    try {
        // 注册 SIGINT / SIGTERM
        if (!registerSignalHandlers()) {
            std::cerr
                << "Error: Failed to register "
                << "signal handlers\n";

            return 1;
        }

        MonitorAgent agent(
    intervalSeconds
);

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
