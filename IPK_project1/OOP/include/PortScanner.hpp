#pragma once
#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <getopt.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <set>

class PortScanner {
private:
    std::string interface;
    std::string target_ip;
    std::string source_ip;
    std::string source_ip6;
    std::vector<int> tcp_ports;
    std::vector<int> udp_ports;
    int timeout_ms = 5000;

public:
    void parse_arguments(int argc, char* argv[]);
    void get_source_ip();
    void run();

private:
    void list_interfaces() const;
    std::string get_ip_from_iface(const std::string &iface, bool ipv6) const;
    std::vector<int> parse_ports(const std::string &port_range) const;
    std::vector<std::string> resolve_hostname(const std::string &hostname) const;
};
