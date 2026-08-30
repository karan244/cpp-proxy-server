#include "server.hpp"
#include "logger.hpp"
#include <iostream>
#include <csignal>
#include <memory>

static std::unique_ptr<Server> g_server = nullptr;

void handle_signal(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        Logger::log("Received shutdown signal (" + std::to_string(signal) + "). Initiating graceful exit...");
        if (g_server) {
            g_server->stop();
        }
    }
}

int main() {
    // Ignore SIGPIPE to avoid process termination when remote client abruptly closes socket
    std::signal(SIGPIPE, SIG_IGN);

    // Register signal handlers for graceful shutdown
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    Logger::init("proxy.log");
    Logger::log("Starting Proxy Server...");
    
    int port = 8080;
    
    g_server = std::make_unique<Server>(port);
    g_server->run();

    Logger::log("Proxy Server stopped cleanly.");
    return 0;
}
