#include "access_control.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>

AccessControl::AccessControl(const std::string& config_file) {
    load_config(config_file);
}

void AccessControl::load_config(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        std::cerr << "Warning: Could not open blocklist file: " << file_path << "\n";
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines or comments
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        std::string rule_type;
        std::string value;

        if (iss >> rule_type >> value) {
            if (rule_type == "BLOCK_HOST") {
                blocked_hosts_.push_back(value);
            } else if (rule_type == "BLOCK_METHOD") {
                blocked_methods_.push_back(value);
            } else if (rule_type == "BLOCK_IP") {
                blocked_ips_.push_back(value);
            }
        }
    }
    
    std::cout << "Loaded Access Control Rules: " 
              << blocked_hosts_.size() << " hosts, "
              << blocked_methods_.size() << " methods, "
              << blocked_ips_.size() << " IPs blocked.\n";
}

bool AccessControl::is_allowed(const std::string& method, const std::string& host) {
    // Check if the method is blocked
    for (const auto& blocked_method : blocked_methods_) {
        if (method == blocked_method) {
            return false;
        }
    }

    // Check if the host (or IP string in the host field) is blocked
    for (const auto& blocked_host : blocked_hosts_) {
        if (host == blocked_host) {
            return false;
        }
    }
    
    for (const auto& blocked_ip : blocked_ips_) {
        if (host == blocked_ip) {
            return false;
        }
    }

    return true; // Allowed!
}
