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
#include <fcntl.h>
#include <csignal>
#include <netinet/tcp.h>
#include <netinet/ip.h> 
#include <pcap.h>
#include <netinet/ip_icmp.h>
#include <errno.h>
#include <cstring>

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

void send_syn_packet(int sock, const sockaddr_in& addr) {
    char buffer[40];
    memset(buffer, 0, sizeof(buffer));

    struct tcphdr *tcp_header = (struct tcphdr *)(buffer);

    tcp_header->source = htons(12345);
    tcp_header->dest = addr.sin_port;
    tcp_header->seq = htonl(0);
    tcp_header->ack_seq = 0;
    tcp_header->doff = 5;
    tcp_header->syn = 1;
    tcp_header->window = htons(65535);
    tcp_header->check = 0;
    tcp_header->urg_ptr = 0;

    if (sendto(sock, buffer, sizeof(struct tcphdr), 0, (sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "Failed to send TCP SYN packet: " << strerror(errno) << std::endl;
    }
}

int listen_for_response(int sock, int port, const std::string& IP, int timeout) {
    char buffer[BUFFER_SIZE];
    sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);

    struct timeval tv;
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    int recv_len = recvfrom(sock, buffer, BUFFER_SIZE, 0, (sockaddr*)&addr, &addr_len);
    std::cerr << "Received packet length: " << recv_len << std::endl;

    if (recv_len > 0) {
        std::cerr << "Received packet data: ";
        for (int i = 0; i < recv_len; ++i) {
            std::cerr << std::hex << (unsigned int)(unsigned char)buffer[i] << " ";
        }
        std::cerr << std::dec << std::endl;

        struct ip *iph = (struct ip *)buffer;
        struct tcphdr *tcph = (struct tcphdr *)(buffer + iph->ip_hl * 4);

        std::cerr << "IP Header Length: " << (int)iph->ip_hl << std::endl;
        std::cerr << "Protocol: " << (int)iph->ip_p << std::endl;
        std::cerr << "Source IP: " << inet_ntoa(addr.sin_addr) << std::endl;
        std::cerr << "Destination Port: " << ntohs(tcph->dest) << std::endl;
        std::cerr << "Source Port: " << ntohs(tcph->source) << std::endl;
        std::cerr << "TCP Flags: " << std::hex << (unsigned int)tcph->th_flags << std::dec << std::endl;

        if (iph->ip_p == IPPROTO_TCP) {
            int dst_port = ntohs(tcph->dest);
            if (dst_port == port) {
                if (tcph->syn && tcph->ack) {
                    std::cout << port << "/tcp open." << std::endl;
                    return 0;
                } else if (tcph->rst) {
                    std::cout << port << "/tcp closed." << std::endl;
                    return 0;
                }
                return 1;
            } else {
                std::cerr << "Port mismatch: Received " << dst_port << ", Expected " << port << std::endl;
                return 1;
            }
        }
    } else {
        std::cerr << "recvfrom error: " << strerror(errno) << std::endl;
        return 1;
    }
}


void scan_tcp(const std::vector<int>& tcp_ports, const std::string& IP, int timeout) {
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (sock < 0) {
        std::cerr << "Failed to create TCP socket: " << strerror(errno) << std::endl;
        return;
    }

    for (int port : tcp_ports) {
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, IP.c_str(), &addr.sin_addr);
        send_syn_packet(sock, addr);

        if (listen_for_response(sock, port, IP, timeout)) {
            send_syn_packet(sock, addr);
            if(listen_for_response(sock, port, IP, timeout)) 
                std::cout << port << "/tcp filtered" << std::endl;
        }
    }

    close(sock);
}

void scan_udp(std::vector<int> udp_ports, std::string IP, int timeout) { // TODO all ports timeout and they are marked as open 
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock < 0) {
        std::cerr << "Failed to create socket." << std::endl;
        exit(1);
    }

    for (int port : udp_ports) {
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

        if(recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *) &addr, &len) < 0) { //TODO ze zadani jsem pochopil (prisla zprava && icmp code 3 -> open; jinak close)... Co vic, cekat timeout? nekolik pokusu? nic tam neni napsano :(            
            std::cout << port << "/udp open" << std::endl;
        }
        else {
            struct ip* ip_hdr = (struct ip*)buffer;
            struct icmp* icmp_hdr = (struct icmp*)(buffer + (ip_hdr->ip_hl * 4));

            if (icmp_hdr->icmp_type == ICMP_DEST_UNREACH && icmp_hdr->icmp_code == ICMP_PORT_UNREACH)
                std::cout << port << "/udp closed." << std::endl;
            else 
                std::cout << port << "/udp open" << std::endl;

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
