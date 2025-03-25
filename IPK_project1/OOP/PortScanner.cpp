#include "PortScanner.hpp"
#include "TCPScanner.hpp"
#include "UDPScanner.hpp"

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

void PortScanner::list_interfaces() const {
    struct ifaddrs *ifaddr = nullptr, *ifa = nullptr;
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        exit(1);
    }

    std::set<std::string> printed;
    for (ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;
        int family = ifa->ifa_addr->sa_family;
        if (family == AF_INET || family == AF_INET6) {
            std::string name(ifa->ifa_name);
            if (printed.find(name) == printed.end()) {
                std::cout << name << std::endl;
                printed.insert(name);
            }
        }
    }

    freeifaddrs(ifaddr);
}

std::string PortScanner::get_ip_from_iface(const std::string &iface) const {
    struct ifaddrs *ifaddr = nullptr, *ifa = nullptr;
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return "";
    }

    std::string result;
    for (ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;

        bool is_ipv6 = target_ip.find(':') != std::string::npos;

        if (std::string(ifa->ifa_name) == iface) {
            if (!is_ipv6 && ifa->ifa_addr->sa_family == AF_INET) {
                // IPv4
                char buf[INET_ADDRSTRLEN];
                auto *in = reinterpret_cast<struct sockaddr_in *>(ifa->ifa_addr);
                if (inet_ntop(AF_INET, &in->sin_addr, buf, sizeof(buf))) {
                    result = buf;
                    break;
                }
            } else if (is_ipv6 && ifa->ifa_addr->sa_family == AF_INET6) {
                // IPv6
                auto *in6 = reinterpret_cast<struct sockaddr_in6 *>(ifa->ifa_addr);
                // Skip link-local addresses
                if (IN6_IS_ADDR_LINKLOCAL(&in6->sin6_addr)) continue;

                char buf[INET6_ADDRSTRLEN];
                if (inet_ntop(AF_INET6, &in6->sin6_addr, buf, sizeof(buf))) {
                    result = buf;
                    break;
                }
            }
        }
    }

    freeifaddrs(ifaddr);
    return result;
}


std::vector<int> PortScanner::parse_ports(const std::string &port_range) const {
    std::vector<int> ports;
    std::istringstream stream(port_range);
    std::string token;

    if (port_range.find(',') != std::string::npos) {
        while (std::getline(stream, token, ',')) {
            ports.push_back(std::stoi(token));
        }
    } else if (port_range.find('-') != std::string::npos) {
        size_t dash_pos = port_range.find('-');
        int start_port = std::stoi(port_range.substr(0, dash_pos));
        int end_port = std::stoi(port_range.substr(dash_pos + 1));
        for (int port = start_port; port <= end_port; ++port) {
            ports.push_back(port);
        }
    } else {
        ports.push_back(std::stoi(port_range));
    }

    return ports;
}

std::string PortScanner::resolve_hostname(const std::string &hostname) const {
    struct hostent *he = gethostbyname(hostname.c_str());
    if (!he || !he->h_addr_list[0]) {
        std::cerr << "Failed to resolve hostname."  << std::endl;
        exit(1);
    }
    struct in_addr **addr_list = (struct in_addr **)he->h_addr_list;
    return std::string(inet_ntoa(*addr_list[0]));
}

void PortScanner::parse_arguments(int argc, char* argv[]) {

    if(argc == 1 || (argc == 2 && (std::strcmp(argv[1], "-i") == 0 || std::strcmp(argv[1], "--interface") == 0))) {
        list_interfaces();
        exit(0);
    }

    struct option long_options[] = {
        {"interface", required_argument, nullptr, 'i'},
        {"pt", required_argument, nullptr, 't'},
        {"pu", required_argument, nullptr, 'u'},
        {"wait", required_argument, nullptr, 'w'},
        {nullptr, 0, nullptr, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "i:t:u:w:", long_options, nullptr)) != -1) {
        switch (opt) {
            case 'i':
                interface = optarg;
                break;
            case 't':
                tcp_ports = parse_ports(optarg);
                break;
            case 'u':
                udp_ports = parse_ports(optarg);
                break;
            case 'w':
                timeout_ms = std::stoi(optarg);
                break;
            default:
                std::cerr << "Invalid argument!"  << std::endl;
                exit(1);
        }
    }

    if (optind < argc) {
        target_ip = argv[optind];
    } else {
        std::cerr << "No target specified!"  << std::endl;
        exit(1);
    }
}

void PortScanner::resolve_target_ip() {
    struct in_addr ipv4_test;
    struct in6_addr ipv6_test;

    if (inet_pton(AF_INET, target_ip.c_str(), &ipv4_test) != 1 &&
        inet_pton(AF_INET6, target_ip.c_str(), &ipv6_test) != 1) {
        target_ip = resolve_hostname(target_ip);
    }

    if (!interface.empty()) {
        source_ip = get_ip_from_iface(interface);
        if (source_ip.empty()) {
            //std::cerr << "Failed to determine source IP, using default" << std::endl;
            //source_ip = "192.168.1.100";
            exit(1);
        } else {
            //std::cerr << "Using source IP " << source_ip << " from interface " << interface << ""  << std::endl;
        }
    } else {
        source_ip = "192.168.1.100";
    }
}

void PortScanner::run() {
    if (!tcp_ports.empty()) {
        TCPScanner tcp(target_ip, source_ip, tcp_ports, timeout_ms);
        tcp.scan();
    }
    if (!udp_ports.empty()) {
        UDPScanner udp(target_ip, udp_ports, timeout_ms);
        udp.scan();
    }
}
