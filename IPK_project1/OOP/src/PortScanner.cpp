#include "PortScanner.hpp"
#include "TCPScanner.hpp"
#include "UDPScanner.hpp"

/**
 * @brief Lists all network interfaces that have an IPv4 or IPv6 address.
 * 
 * This function uses getifaddrs() to retrieve and print interface names to stdout.
 */
void PortScanner::list_interfaces() const {
    struct ifaddrs *ifaddr = nullptr, *ifa = nullptr;
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        exit(1);
    }
    std::set<std::string> seen;
    for (ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;
        int family = ifa->ifa_addr->sa_family;
        if (family == AF_INET || family == AF_INET6) {
            std::string name(ifa->ifa_name);
            if (seen.find(name) == seen.end()) {
                std::cout << name << std::endl;
                seen.insert(name);
            }
        }
    }
    freeifaddrs(ifaddr);
}

/**
 * @brief Gets the first IPv4 or IPv6 address associated with a network interface.
 * 
 * @param iface Name of the network interface (e.g., "wlp3s0").
 * @param ipv6 True for IPv6 address, false for IPv4.
 * @return std::string The IP address as a string, or an empty string if not found.
 */
std::string PortScanner::get_ip_from_iface(const std::string &iface, bool ipv6) const {
    struct ifaddrs *ifaddr = nullptr, *ifa = nullptr;
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return "";
    }
    std::string result;
    for (ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;
        if (std::string(ifa->ifa_name) != iface) continue;

        if (!ipv6 && ifa->ifa_addr->sa_family == AF_INET) {
            char buf[INET_ADDRSTRLEN];
            auto *sin = reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr);
            if (inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf))) {
                result = buf;
                break;
            }
        } else if (ipv6 && ifa->ifa_addr->sa_family == AF_INET6) {
            auto *sin6 = reinterpret_cast<struct sockaddr_in6*>(ifa->ifa_addr);
            if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr)) continue;
            char buf[INET6_ADDRSTRLEN];
            if (inet_ntop(AF_INET6, &sin6->sin6_addr, buf, sizeof(buf))) {
                result = buf;
                break;
            }
        }
    }
    freeifaddrs(ifaddr);
    return result;
}

/**
 * @brief Parses a port specification string into a list of integers.
 * 
 * Supports single ports (e.g. "80"), ranges (e.g. "20-25"), and comma-separated values (e.g. "53,67,123").
 * 
 * @param port_range The string containing the port specification.
 * @return std::vector<int> A vector of parsed port numbers.
 */
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
        int start = std::stoi(port_range.substr(0, dash_pos));
        int end = std::stoi(port_range.substr(dash_pos + 1));
        for (int port = start; port <= end; ++port) {
            ports.push_back(port);
        }
    } else {
        ports.push_back(std::stoi(port_range));
    }
    return ports;
}

/**
 * @brief Resolves a hostname to its corresponding IPv4 and/or IPv6 addresses.
 * 
 * @param hostname The hostname to resolve (e.g. "google.com").
 * @return std::vector<std::string> A list of resolved IP addresses.
 */
std::vector<std::string> PortScanner::resolve_hostname(const std::string &hostname) const {
    std::vector<std::string> addrs;
    struct addrinfo hints{}, *res, *rp;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(hostname.c_str(), nullptr, &hints, &res) != 0) {
        std::cerr << "Failed to resolve hostname: " << hostname << "\n";
        exit(1);
    }
    char buf[INET6_ADDRSTRLEN];
    for (rp = res; rp; rp = rp->ai_next) {
        if (rp->ai_family == AF_INET) {
            struct sockaddr_in *sin = (struct sockaddr_in*)rp->ai_addr;
            inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf));
            addrs.push_back(buf);
        } else if (rp->ai_family == AF_INET6) {
            struct sockaddr_in6 *sin6 = (struct sockaddr_in6*)rp->ai_addr;
            inet_ntop(AF_INET6, &sin6->sin6_addr, buf, sizeof(buf));
            addrs.push_back(buf);
        }
    }
    freeaddrinfo(res);
    return addrs;
}

/**
 * @brief Prints help/usage information and exits the program.
 */
void print_help() {
    std::cout << "help" << std::endl;
    exit(0);
}

/**
 * @brief Parses command-line arguments and stores configuration in member variables.
 * 
 * Recognized options:
 * - `-i, --interface`: Specify network interface
 * - `-pt`: TCP ports (single, range, or list)
 * - `-pu`: UDP ports
 * - `-w, --wait`: Timeout in milliseconds
 * - `-h, --help`: Show help
 * 
 * @param argc Argument count.
 * @param argv Argument vector.
 */
void PortScanner::parse_arguments(int argc, char* argv[]) {
    if (argc == 1 ||
       (argc == 2 && (std::strcmp(argv[1], "-i") == 0 || std::strcmp(argv[1], "--interface") == 0))) {
        list_interfaces();
        exit(0);
    }
    struct option long_options[] = {
        {"interface", required_argument, nullptr, 'i'},
        {"pt", required_argument, nullptr, 't'},
        {"pu", required_argument, nullptr, 'u'},
        {"wait", required_argument, nullptr, 'w'},
        {"help", no_argument, nullptr, 'h'},
        {nullptr, 0, nullptr, 0}
    };
    int opt;
    while ((opt = getopt_long(argc, argv, "i:t:u:w:h", long_options, nullptr)) != -1) {
        switch (opt) {
            case 'i': interface = optarg; break;
            case 't': tcp_ports = parse_ports(optarg); break;
            case 'u': udp_ports = parse_ports(optarg); break;
            case 'w': timeout_ms = std::stoi(optarg); break;
            case 'h': print_help(); break;
            default:
                std::cerr << "Invalid argument!\n";
                exit(1);
        }
    }
    if (optind < argc)
        target_ip = argv[optind];
    else {
        std::cerr << "No target specified!\n";
        exit(1);
    }
}

/**
 * @brief Determines the source IP address for the selected interface.
 * 
 * Sets both `source_ip` (IPv4) and `source_ip6` (IPv6) for use during scanning.
 * Exits if no usable IP is found.
 */
void PortScanner::get_source_ip() {
    if (!interface.empty()) {
        source_ip  = get_ip_from_iface(interface, false);
        source_ip6 = get_ip_from_iface(interface, true);
        if (source_ip.empty() && source_ip6.empty()) {
            std::cerr << "No usable source address on " << interface << "\n";
            exit(1);
        }
    } else {
        exit(1);
    }
}

/**
 * @brief Runs TCP and/or UDP scans on all resolved IP addresses for the target.
 * 
 * Uses the selected source IP and ports to initialize TCP/UDP scanners.
 */
void PortScanner::run() {
    std::vector<std::string> addrs = resolve_hostname(target_ip);
    for (auto &ip : addrs) {
        bool is_ipv6 = (ip.find(':') != std::string::npos);
        std::string src = is_ipv6 ? source_ip6 : source_ip;
        if (src.empty()) {
            std::cout << ip << " skipped (no source " 
                      << (is_ipv6 ? "IPv6" : "IPv4") << ")\n";
            continue;
        }
        if (!tcp_ports.empty()) {
            TCPScanner tcp(interface, ip, src, tcp_ports, timeout_ms);
            tcp.scan();
        }
        if (!udp_ports.empty()) {
            UDPScanner udp(ip, udp_ports, timeout_ms);
            udp.scan();
        }
    }
}
