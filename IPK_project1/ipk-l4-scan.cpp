#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <getopt.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <csignal>

const int BUFFER_SIZE = 1024;

void parse_args(int argc, char *argv[], std::string &interface, std::string &port_range_tcp, std::string &port_range_udp, std::string &IP, int *timeout) {
    int opt;
    struct option long_options[] = {
        {"interface", required_argument, nullptr, 'i'},
        {"pt", required_argument, nullptr, 't'},
        {"pu", required_argument, nullptr, 'u'},
        {nullptr, 0, nullptr, 0}
    };

    while((opt = getopt_long(argc, argv, "i:t:u:w:", long_options, nullptr)) != -1) {
        switch(opt) { // TODO exit pokud je zadan jeden vicekrat
            case 'i':
                if(!interface.empty()) {
                    std::cout << "duplicate" << std::endl;
                    exit(1);
                }
                interface = optarg;
                break;
            case 't':
                if(!port_range_tcp.empty()) {
                    std::cout << "duplicate" << std::endl;
                    exit(1);
                }
                port_range_tcp = optarg;
                break;
            case 'u':
                if(!port_range_udp.empty()) {
                    std::cout << "duplicate" << std::endl;
                    exit(1);
                }
                port_range_udp = optarg;
                break;
            case 'w':
                *timeout = std::stoi(optarg);
                break;
            default:
                std::cerr << "Invalid argument!\n";
                exit(1);
        }
    }

    if(optind < argc) {
        IP = argv[optind];
    } else {
        std::cerr << "No IP/domain specified!\n";
        exit(1);
    }
}

void print_vector(const std::vector<int> &vec) {
    for(const int &val : vec) {
        std::cout << val << " ";
    }
    std::cout << std::endl;
}

std::vector<int> parse_ports(const std::string &port_range) {
    std::vector<int> ports;
    std::istringstream stream(port_range);
    std::string token;

    if(port_range.find(',') != std::string::npos) {
        while(std::getline(stream, token, ',')) {
            ports.push_back(std::stoi(token));
        }
    }
    else if(port_range.find('-') != std::string::npos) {
        size_t dash_pos = port_range.find('-');
        int start_port = std::stoi(port_range.substr(0, dash_pos));
        int end_port = std::stoi(port_range.substr(dash_pos + 1));
        for(int port = start_port; port <= end_port; ++port) {
            ports.push_back(port);
        }
    }
    else {
        ports.push_back(std::stoi(port_range));
    }

    return ports;
}

std::string resolve_hostname(const std::string& hostname) {
    struct hostent* he = gethostbyname(hostname.c_str());
    if (he == nullptr || he->h_addr_list[0] == nullptr) {
        std::cerr << "Failed to resolve hostname." << std::endl;
        exit(1);
    }

    struct in_addr** addr_list = (struct in_addr**)he->h_addr_list;
    return std::string(inet_ntoa(*addr_list[0]));
}

void scan_tcp(std::vector<int> tcp_ports, std::string IP, int timeout) {
}


void scan_udp(std::vector<int> udp_ports, std::string IP, int timeout) { // TODO all ports timeout and they are marked as closed
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock < 0) {
        std::cerr << "Failed to create socket." << std::endl;
        exit(1);
    }

    for(int i = 0; i < static_cast<int>(udp_ports.size()); i++) {
        int port = udp_ports[i];
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, IP.c_str(), &addr.sin_addr);

        char buffer[1] = {0};

        if(sendto(sock, buffer, sizeof(buffer), 0, (sockaddr*)&addr, sizeof(addr)) < 0) {
            std::cerr << "Failed sending UDP packet." << std::endl;
            close(sock);
            exit(1);
        }

        struct timeval tv;
        tv.tv_sec = timeout / 1000;
        tv.tv_usec =(timeout % 1000) * 1000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
        socklen_t len = sizeof(addr); 

        if(recvfrom(sock, buffer, sizeof(buffer), 0,(struct sockaddr *) &addr, &len) < 0) {
            std::cout << port << "/udp closed" << std::endl;
        }
        else {
            std::cout << port << "/udp open." << std::endl;
        }
    }

    close(sock);
}

void signal_handler(int) {
    exit(0);
}


int main(int argc, char *argv[]) {
    signal(SIGINT, signal_handler); 

    std::string interface, port_range_tcp, port_range_udp, IP;
    std::vector<int> tcp_ports, udp_ports;
    int timeout = 1000; //TODO defualt 5000

    parse_args(argc, argv, interface, port_range_tcp, port_range_udp, IP, &timeout);

    IP = resolve_hostname(IP);

    if(!port_range_tcp.empty())
        tcp_ports = parse_ports(port_range_tcp);
    if(!port_range_udp.empty())
        udp_ports = parse_ports(port_range_udp);

    //std::cout << "Interface: " << interface << "\n";
    //std::cout << "TCP Port Range: ";
    // print_vector(tcp_ports);
    // std::cout << "UDP Port Range: ";
    // print_vector(udp_ports);
    // std::cout << "IP/Hostname: " << IP << std::endl;
    // std::cout << "Timeout: " << timeout << std::endl; 
    // std::cout << "____________________________________" << std::endl;

    std::cout << "Interesting ports on " << IP << ":" << std::endl; //TODO format "hostname (IP):"
    if(!port_range_tcp.empty())
        scan_tcp(tcp_ports, IP, timeout);
    if(!port_range_udp.empty())
        scan_udp(udp_ports, IP, timeout);

    return 0;
}
