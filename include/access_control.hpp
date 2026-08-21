#ifndef ACCESS_CONTROL_HPP
#define ACCESS_CONTROL_HPP

#include <string>
#include <vector>

class AccessControl {
public:
    AccessControl(const std::string& config_file);

    // Checks if the request is allowed based on the blocklist rules
    bool is_allowed(const std::string& method, const std::string& host);

private:
    std::vector<std::string> blocked_hosts_;
    std::vector<std::string> blocked_methods_;
    std::vector<std::string> blocked_ips_;

    void load_config(const std::string& file_path);
};

#endif // ACCESS_CONTROL_HPP
