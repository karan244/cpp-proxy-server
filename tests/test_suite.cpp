#include "http_proxy.hpp"
#include "access_control.hpp"
#include <iostream>
#include <cassert>
#include <fstream>

// Simple lightweight test runner macros
int g_tests_passed = 0;
int g_tests_failed = 0;

#define TEST_ASSERT(condition, msg) \
    do { \
        if (condition) { \
            std::cout << "[PASS] " << msg << "\n"; \
            g_tests_passed++; \
        } else { \
            std::cerr << "[FAIL] " << msg << " (Line " << __LINE__ << ")\n"; \
            g_tests_failed++; \
        } \
    } while (0)

void test_http_parser_get_relative() {
    std::string raw = 
        "GET /index.html HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "User-Agent: curl/7.81.0\r\n\r\n";

    HttpRequest req = HttpParser::parse_request(raw);
    TEST_ASSERT(req.is_valid, "GET relative path is valid");
    TEST_ASSERT(req.method == "GET", "Method is GET");
    TEST_ASSERT(req.host == "example.com", "Host is example.com");
    TEST_ASSERT(req.port == "80", "Default port is 80");
    TEST_ASSERT(req.path == "/index.html", "Path is /index.html");
}

void test_http_parser_get_absolute() {
    std::string raw = 
        "GET http://example.com:8080/api/v1/test HTTP/1.1\r\n"
        "Host: example.com:8080\r\n\r\n";

    HttpRequest req = HttpParser::parse_request(raw);
    TEST_ASSERT(req.is_valid, "GET absolute URI is valid");
    TEST_ASSERT(req.method == "GET", "Method is GET");
    TEST_ASSERT(req.host == "example.com", "Host is example.com");
    TEST_ASSERT(req.port == "8080", "Port is 8080");
    TEST_ASSERT(req.path == "/api/v1/test", "Path is /api/v1/test");
}

void test_http_parser_connect() {
    std::string raw = 
        "CONNECT google.com:443 HTTP/1.1\r\n"
        "Host: google.com:443\r\n\r\n";

    HttpRequest req = HttpParser::parse_request(raw);
    TEST_ASSERT(req.is_valid, "CONNECT request is valid");
    TEST_ASSERT(req.method == "CONNECT", "Method is CONNECT");
    TEST_ASSERT(req.host == "google.com", "Host is google.com");
    TEST_ASSERT(req.port == "443", "Port is 443");
}

void test_http_parser_malformed() {
    std::string raw = "GARBAGE_REQUEST";
    HttpRequest req = HttpParser::parse_request(raw);
    TEST_ASSERT(!req.is_valid, "Malformed request is marked invalid");
}

void test_access_control() {
    // Create a temporary blocklist config file for unit testing
    std::string test_config = "test_blocklist.conf";
    std::ofstream out(test_config);
    out << "BLOCK_HOST badsite.com\n";
    out << "BLOCK_HOST malicious.org\n";
    out << "BLOCK_METHOD POST\n";
    out << "BLOCK_IP 192.168.1.50\n";
    out.close();

    AccessControl ac(test_config);

    // Allowed cases
    TEST_ASSERT(ac.is_allowed("GET", "example.com"), "GET example.com is ALLOWED");
    TEST_ASSERT(ac.is_allowed("GET", "google.com"), "GET google.com is ALLOWED");

    // Blocked cases
    TEST_ASSERT(!ac.is_allowed("GET", "badsite.com"), "GET badsite.com is BLOCKED");
    TEST_ASSERT(!ac.is_allowed("GET", "malicious.org"), "GET malicious.org is BLOCKED");
    TEST_ASSERT(!ac.is_allowed("POST", "example.com"), "POST example.com is BLOCKED");
    TEST_ASSERT(!ac.is_allowed("GET", "192.168.1.50"), "IP 192.168.1.50 is BLOCKED");

    // Clean up temporary config file
    std::remove(test_config.c_str());
}

int main() {
    std::cout << "========================================\n";
    std::cout << "       Running Proxy Unit Tests         \n";
    std::cout << "========================================\n";

    test_http_parser_get_relative();
    test_http_parser_get_absolute();
    test_http_parser_connect();
    test_http_parser_malformed();
    test_access_control();

    std::cout << "========================================\n";
    std::cout << "Summary: " << g_tests_passed << " passed, " 
              << g_tests_failed << " failed.\n";
    std::cout << "========================================\n";

    return (g_tests_failed == 0) ? 0 : 1;
}
