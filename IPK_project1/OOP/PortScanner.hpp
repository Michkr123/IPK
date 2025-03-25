#pragma once
#include <string>
#include <vector>

class PortScanner {
private:
    std::string interface;
    std::string target_ip;
    std::string source_ip;
    std::vector<int> tcp_ports;
    std::vector<int> udp_ports;
    int timeout_ms = 5000;

public:
    void parse_arguments(int argc, char* argv[]);
    void resolve_target_ip();
    void run();

private:
    void list_interfaces() const;
    std::string get_ip_from_iface(const std::string &iface) const;
    std::vector<int> parse_ports(const std::string &port_range) const;
    std::string resolve_hostname(const std::string &hostname) const;
};
