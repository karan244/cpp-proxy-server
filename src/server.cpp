#include "server.hpp"
#include "http_proxy.hpp" // Added for Phase 3 HTTP parsing
#include "logger.hpp"     // Added for Phase 8 Logging
#include <iostream>
#include <unistd.h>      // For close(), read(), write()
#include <sys/socket.h>  // For socket(), bind(), listen(), accept()
#include <netinet/in.h>  // For sockaddr_in
#include <arpa/inet.h>   // For inet_ntoa
#include <netdb.h>       // For getaddrinfo(), freeaddrinfo()
#include <poll.h>        // For poll() for bidirectional forwarding
#include <cstring>       // For memset

Server::Server(int port) : port_(port), server_fd_(-1) {
    // Initialize the thread pool with 4 worker threads
    thread_pool_ = std::make_unique<ThreadPool>(4);
    
    // Initialize Access Control with the blocklist config file
    access_control_ = std::make_unique<AccessControl>("config/blocklist.conf");
}

Server::~Server() {
    stop();
}

void Server::stop() {
    if (is_running_.exchange(false)) {
        Logger::log("Shutting down server...");
        if (server_fd_ != -1) {
            close(server_fd_);
            server_fd_ = -1;
        }
    }
}

void Server::run() {
    // 1. Create a socket
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        std::cerr << "Failed to create socket\n";
        return;
    }

    int opt = 1;
    if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "Failed to set socket options\n";
        close(server_fd_);
        server_fd_ = -1;
        return;
    }

    struct sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port_);

    if (bind(server_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Failed to bind to port " << port_ << "\n";
        close(server_fd_);
        server_fd_ = -1;
        return;
    }

    if (listen(server_fd_, 10) < 0) {
        std::cerr << "Failed to listen on socket\n";
        close(server_fd_);
        server_fd_ = -1;
        return;
    }

    is_running_ = true;
    Logger::log("Server listening on port " + std::to_string(port_) + " with 4 worker threads...");

    // Accept loop with poll timeout for clean shutdown
    struct pollfd pfd;
    pfd.fd = server_fd_;
    pfd.events = POLLIN;

    while (is_running_) {
        pfd.fd = server_fd_;
        int poll_res = poll(&pfd, 1, 500); // 500ms timeout to periodically check is_running_

        if (poll_res < 0) {
            if (errno == EINTR) continue; // Interrupted by signal
            Logger::log("ERROR: Server poll failed");
            break;
        }

        if (poll_res == 0 || !(pfd.revents & POLLIN)) {
            continue; // Timeout, check is_running_ again
        }

        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        // accept() incoming client connection
        int client_fd = accept(server_fd_, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (is_running_) {
                Logger::log("ERROR: Failed to accept connection");
            }
            continue;
        }

        std::string client_ip = inet_ntoa(client_addr.sin_addr);

        // Enqueue connection handling into thread pool
        thread_pool_->enqueue_task([this, client_fd, client_ip]() {
            this->handle_client(client_fd, client_ip);
        });
    }

    Logger::log("Server run loop exited.");
}

void Server::handle_client(int client_fd, const std::string& client_ip) {
    char buffer[4096];
    std::memset(buffer, 0, sizeof(buffer));

    // Read data from the client
    ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read <= 0) {
        close(client_fd);
        return;
    }

    // Convert the raw C-style buffer into a C++ string so we can easily parse it
    std::string raw_request(buffer, bytes_read);

    // Phase 3: Parse the HTTP request to find out where the client wants to go
    HttpRequest parsed_req = HttpParser::parse_request(raw_request);

    if (!parsed_req.is_valid) {
        Logger::log("WARNING: Received invalid HTTP request from " + client_ip);
        close(client_fd);
        return;
    }

    // Phase 6: Access Control Filtering
    if (!access_control_->is_allowed(parsed_req.method, parsed_req.host)) {
        Logger::log_request(client_ip, parsed_req.method, parsed_req.host, parsed_req.port, "BLOCKED");
        
        std::string block_response;
        if (parsed_req.method == "CONNECT") {
            block_response = "HTTP/1.1 403 Forbidden\r\n\r\nProxy Access Denied by Blocklist";
        } else {
            block_response = 
                "HTTP/1.1 403 Forbidden\r\n"
                "Content-Type: text/html\r\n"
                "Connection: close\r\n\r\n"
                "<html><body><h1>403 Forbidden</h1><p>This request was blocked by the proxy administrator.</p></body></html>";
        }
        send(client_fd, block_response.c_str(), block_response.length(), 0);
        close(client_fd);
        return;
    }

    // Phase 8: Log the allowed request
    Logger::log_request(client_ip, parsed_req.method, parsed_req.host, parsed_req.port, "ALLOWED");

    // Connect dynamically to the requested destination
    int dest_fd = connect_to_host(parsed_req.host, parsed_req.port);

    if (dest_fd < 0) {
        Logger::log("ERROR: Failed to connect to destination " + parsed_req.host);
        std::string error_response = "HTTP/1.1 502 Bad Gateway\r\n\r\n";
        send(client_fd, error_response.c_str(), error_response.length(), 0);
        close(client_fd);
        return;
    }

    if (parsed_req.method == "CONNECT") {
        // Phase 5: HTTPS Tunneling
        std::string success_response = "HTTP/1.1 200 Connection Established\r\n\r\n";
        send(client_fd, success_response.c_str(), success_response.length(), 0);
        Logger::log("SUCCESS: Established HTTPS tunnel to " + parsed_req.host);
    } else {
        // Normal HTTP: Forward the exact raw request bytes we received from the client
        send(dest_fd, buffer, bytes_read, 0);
    }

    // Phase 7: Use our clear abstraction for bidirectional forwarding
    forward_data(client_fd, dest_fd);

    close(dest_fd);
    close(client_fd);
    Logger::log("Tunnel closed for " + parsed_req.host);
}

void Server::forward_data(int client_fd, int dest_fd) {
    // Bidirectional Data Forwarding using poll()
    struct pollfd fds[2];
    fds[0].fd = client_fd;
    fds[0].events = POLLIN; // Wait for client to send data
    
    fds[1].fd = dest_fd;
    fds[1].events = POLLIN; // Wait for destination to send data

    char buffer[8192];

    while (true) {
        int poll_count = poll(fds, 2, -1);
        if (poll_count < 0) {
            Logger::log("ERROR: Poll error");
            break;
        }

        // Check if the CLIENT sent us data
        if (fds[0].revents & POLLIN) {
            ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer), 0);
            if (bytes_read <= 0) break; // Client disconnected

            // Handle partial sends (ensure all bytes are actually sent)
            ssize_t total_sent = 0;
            while (total_sent < bytes_read) {
                ssize_t sent = send(dest_fd, buffer + total_sent, bytes_read - total_sent, 0);
                if (sent <= 0) break;
                total_sent += sent;
            }
            if (total_sent < bytes_read) break; // Destination socket error
        }

        // Check if the DESTINATION sent us data
        if (fds[1].revents & POLLIN) {
            ssize_t bytes_read = recv(dest_fd, buffer, sizeof(buffer), 0);
            if (bytes_read <= 0) break; // Destination disconnected

            // Handle partial sends
            ssize_t total_sent = 0;
            while (total_sent < bytes_read) {
                ssize_t sent = send(client_fd, buffer + total_sent, bytes_read - total_sent, 0);
                if (sent <= 0) break;
                total_sent += sent;
            }
            if (total_sent < bytes_read) break; // Client socket error
        }
        
        // Handle socket hangups or errors
        if ((fds[0].revents & (POLLERR | POLLHUP)) || (fds[1].revents & (POLLERR | POLLHUP))) {
            break;
        }
    }
}

int Server::connect_to_host(const std::string& hostname, const std::string& port) {
    struct addrinfo hints, *res, *p;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;     // IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP

    // 1. DNS Resolution
    int status = getaddrinfo(hostname.c_str(), port.c_str(), &hints, &res);
    if (status != 0) {
        std::cerr << "getaddrinfo error: " << gai_strerror(status) << "\n";
        return -1;
    }

    int dest_fd = -1;

    // 2. Iterate through the results and try to connect
    for (p = res; p != nullptr; p = p->ai_next) {
        // Create the socket
        dest_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (dest_fd < 0) {
            continue; // Socket creation failed, try the next address
        }

        // Attempt to connect
        if (connect(dest_fd, p->ai_addr, p->ai_addrlen) == 0) {
            break; // Successfully connected!
        }

        // Connection failed, close the socket and try the next one
        close(dest_fd);
        dest_fd = -1;
    }

    freeaddrinfo(res); // Free the memory allocated by getaddrinfo

    return dest_fd; // Returns a valid FD on success, or -1 on failure
}
