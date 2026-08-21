#include "server.hpp"
#include "logger.hpp"
#include <iostream>

int main() {
    Logger::init("proxy.log");
    Logger::log("Starting Proxy Server Phase 8...");
    
    int port = 8080;
    
    Server server(port);
    server.run();

    return 0;
}
