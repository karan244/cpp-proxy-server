#include "http_proxy.hpp"
#include <sstream>

HttpRequest HttpParser::parse_request(const std::string& raw_request) {
    HttpRequest req;
    req.is_valid = false;
    req.port = "80"; // Default to HTTP port 80 unless specified otherwise

    // We use a string stream to easily read the request line by line
    std::istringstream stream(raw_request);
    std::string request_line;
    
    // The very first line of an HTTP request looks like:
    // GET http://example.com/some/path HTTP/1.1
    // OR
    // GET /some/path HTTP/1.1
    if (!std::getline(stream, request_line)) {
        return req;
    }

    std::istringstream line_stream(request_line);
    std::string version;
    std::string url;
    
    // Extract the three parts of the first line (Method, URL, Version)
    line_stream >> req.method >> url >> version;

    // After the first line, the client sends "Headers" (Key: Value)
    // We want to find the "Host: " header as a fallback in case the URL doesn't contain the domain.
    std::string header_line;
    while (std::getline(stream, header_line) && header_line != "\r") {
        if (header_line.find("Host: ") == 0) {
            req.host = header_line.substr(6);
            // Remove the hidden '\r' character at the end of the line if it exists
            if (!req.host.empty() && req.host.back() == '\r') {
                req.host.pop_back();
            }
        }
    }

    // Now let's figure out exactly where to connect.
    // Case 1: HTTPS CONNECT request (e.g., CONNECT example.com:443 HTTP/1.1)
    // The URL is simply the hostname and port.
    if (req.method == "CONNECT") {
        size_t colon_pos = url.find(':');
        if (colon_pos != std::string::npos) {
            req.host = url.substr(0, colon_pos);
            req.port = url.substr(colon_pos + 1);
        } else {
            req.host = url;
            req.port = "443"; // Default HTTPS port
        }
        req.path = ""; // CONNECT requests don't have a path
    }
    // Case 2: The browser sent an absolute URL (e.g., http://example.com:8080/path)
    else if (url.find("http://") == 0) {
        size_t host_start = 7; // Skip past "http://"
        size_t path_start = url.find('/', host_start);
        std::string host_port;

        if (path_start != std::string::npos) {
            host_port = url.substr(host_start, path_start - host_start);
            req.path = url.substr(path_start);
        } else {
            host_port = url.substr(host_start);
            req.path = "/";
        }

        // Check if there is a custom port specified (e.g., example.com:8080)
        size_t colon_pos = host_port.find(':');
        if (colon_pos != std::string::npos) {
            req.host = host_port.substr(0, colon_pos);
            req.port = host_port.substr(colon_pos + 1);
        } else {
            req.host = host_port;
        }
    } 
    // Case 3: The browser just sent a path (e.g., /path) and put the domain in the Host header
    else {
        req.path = url;
        
        // We already extracted req.host from the "Host: " header earlier
        // But the Host header might also contain a port (e.g., Host: example.com:8080)
        size_t colon_pos = req.host.find(':');
        if (colon_pos != std::string::npos) {
            req.port = req.host.substr(colon_pos + 1);
            req.host = req.host.substr(0, colon_pos);
        }
    }

    // If we successfully found both a method and a destination host, it's a valid request!
    if (!req.method.empty() && !req.host.empty()) {
        req.is_valid = true;
    }

    return req;
}
