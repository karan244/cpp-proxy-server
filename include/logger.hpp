#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <string>
#include <mutex>
#include <fstream>

class Logger {
public:
    // Initialize the logger to write to a specific file (e.g., proxy.log)
    static void init(const std::string& log_file);
    
    // Log a message safely from any thread
    static void log(const std::string& message);
    
    // Helper function for standard proxy request logging
    static void log_request(const std::string& client_ip, const std::string& method, 
                            const std::string& host, const std::string& port, 
                            const std::string& status);

private:
    static std::mutex mutex_;
    static std::ofstream log_stream_;
};

#endif // LOGGER_HPP
