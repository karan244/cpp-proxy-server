#ifndef HTTP_PROXY_HPP
#define HTTP_PROXY_HPP

#include <string>

// A simple structure to hold the parts of an HTTP request that we care about
struct HttpRequest {
    std::string method;  // e.g., "GET", "POST"
    std::string host;    // e.g., "example.com"
    std::string port;    // e.g., "80"
    std::string path;    // e.g., "/index.html"
    bool is_valid;       // true if we successfully parsed the request
};

class HttpParser {
public:
    // Takes the raw text sent by the browser and extracts the destination
    static HttpRequest parse_request(const std::string& raw_request);
};

#endif // HTTP_PROXY_HPP
