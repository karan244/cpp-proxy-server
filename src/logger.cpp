#include "logger.hpp"
#include <iostream>
#include <ctime>
#include <iomanip>
#include <sstream>

std::mutex Logger::mutex_;
std::ofstream Logger::log_stream_;

void Logger::init(const std::string& log_file) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (log_stream_.is_open()) {
        log_stream_.close();
    }
    log_stream_.open(log_file, std::ios::out | std::ios::app);
}

void Logger::log(const std::string& message) {
    // Get current time
    std::time_t now = std::time(nullptr);
    std::tm* local_time = std::localtime(&now);
    
    char time_buf[64];
    std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", local_time);

    // Thread-safe logging: Lock the mutex so threads don't overwrite each other's text
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string full_log = "[" + std::string(time_buf) + "] " + message + "\n";
    
    // Write to file if open
    if (log_stream_.is_open()) {
        log_stream_ << full_log;
        log_stream_.flush();
    }
    
    // Also print to console
    std::cout << full_log;
}

void Logger::log_request(const std::string& client_ip, const std::string& method, 
                         const std::string& host, const std::string& port, 
                         const std::string& status) {
    std::ostringstream oss;
    oss << "Client: " << client_ip << " | "
        << "Request: " << method << " " << host << ":" << port << " | "
        << "Status: " << status;
        
    log(oss.str());
}
