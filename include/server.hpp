#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <memory>
#include <atomic>
#include "thread_pool.hpp"
#include "access_control.hpp"

class Server {
public:
    Server(int port);
    ~Server();

    // Start the server to listen and accept connections
    void run();

    // Gracefully stop the server
    void stop();

private:
    int port_;
    int server_fd_; // File descriptor for the listening socket
    std::atomic<bool> is_running_{false};
    
    // The thread pool for handling client connections concurrently
    std::unique_ptr<ThreadPool> thread_pool_;
    
    // Access Control engine (Phase 6)
    std::unique_ptr<AccessControl> access_control_;

    // Handle an individual client connection
    void handle_client(int client_fd, const std::string& client_ip);

    // Phase 2: Establish an outbound connection to a destination server
    int connect_to_host(const std::string& hostname, const std::string& port);

    // Phase 7: Clear abstraction for Bidirectional Data Forwarding
    void forward_data(int client_fd, int dest_fd);
};

#endif // SERVER_HPP
