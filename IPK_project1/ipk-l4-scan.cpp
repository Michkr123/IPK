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
#include <netinet/ip6.h> 
#include <pcap.h>
#include <netinet/ip_icmp.h>
#include <errno.h>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <set>

#include <ifaddrs.h>
#include <sys/types.h>

static const char* DEFAULT_SOURCE_IP = "192.168.1.100";

const int BUFFER_SIZE = 1500;
static const int DEFAULT_TIMEOUT_MS = 5000;


void list_interfaces() {
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

std::string get_ip_from_iface(const std::string &iface) {
    struct ifaddrs *ifaddr = nullptr;
    struct ifaddrs *ifa    = nullptr;

    if (getifaddrs(&ifaddr) == -1) {
        std::perror("getifaddrs");
        return "";
    }

    std::string result;
    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr) {
            continue;
        }
        if (std::string(ifa->ifa_name) == iface &&
            ifa->ifa_addr->sa_family == AF_INET)
        {
            char buf[INET_ADDRSTRLEN];
            auto *in = reinterpret_cast<struct sockaddr_in *>(ifa->ifa_addr);
            if (inet_ntop(AF_INET, &in->sin_addr, buf, sizeof(buf))) {
                result = buf;
                break;
            }
        }
    }
    freeifaddrs(ifaddr);
    return result;
}

static std::string interfaceArg;
static std::string port_range_tcp;
static std::string port_range_udp;
static std::string IP;
static int timeout_ms = DEFAULT_TIMEOUT_MS;

void parse_args(int argc, char *argv[],
                std::string &interface,
                std::string &port_range_tcp,
                std::string &port_range_udp,
                std::string &IP,
                int *timeout) {
    int opt;
    struct option long_options[] = {
        {"interface", required_argument, nullptr, 'i'},
        {"pt", required_argument, nullptr, 't'},
        {"pu", required_argument, nullptr, 'u'},
        {"wait", required_argument, nullptr, 'w'},
        {nullptr, 0, nullptr, 0}
    };

    while((opt = getopt_long(argc, argv, "i:t:u:w:", long_options, nullptr)) != -1) {
        switch(opt) {
            case 'i':
                if(!interface.empty()) {
                    std::cerr << "duplicate interface option\n";
                    exit(1);
                }
                interface = optarg;
                break;
            case 't':
                if(!port_range_tcp.empty()) {
                    std::cerr << "duplicate TCP port range\n";
                    exit(1);
                }
                port_range_tcp = optarg;
                break;
            case 'u':
                if(!port_range_udp.empty()) {
                    std::cerr << "duplicate UDP port range\n";
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
        std::cerr << "Failed to resolve hostname.\n";
        exit(1);
    }
    struct in_addr** addr_list = (struct in_addr**)he->h_addr_list;
    return std::string(inet_ntoa(*addr_list[0]));
}

struct pseudo_header {
    uint32_t src;
    uint32_t dst;
    uint8_t  zero;
    uint8_t  proto;
    uint16_t length;
};

unsigned short ip_checksum(unsigned short *buf, int len) {
    unsigned long sum = 0;
    while (len > 1) {
        sum += *buf++;
        len -= 2;
    }
    if (len == 1) {
        unsigned short tmp = 0;
        *(unsigned char*)(&tmp) = *(unsigned char*)buf;
        sum += tmp;
    }
    while (sum >> 16) {
        sum = (sum & 0xffff) + (sum >> 16);
    }
    return (unsigned short)(~sum);
}

unsigned short tcp_checksum(const struct iphdr *iph, const struct tcphdr *tcph, int tcp_len) {
    pseudo_header ph;
    ph.src    = iph->saddr;
    ph.dst    = iph->daddr;
    ph.zero   = 0;
    ph.proto  = IPPROTO_TCP;
    ph.length = htons(tcp_len);

    unsigned long sum = 0;
    const unsigned short *p = (unsigned short *)&ph;
    int ph_len = sizeof(ph);

    while (ph_len > 1) {
        sum += *p++;
        ph_len -= 2;
    }

    p = (unsigned short*)tcph;
    int len = tcp_len;
    while (len > 1) {
        sum += *p++;
        len -= 2;
    }
    if (len == 1) {
        unsigned short tmp = 0;
        *(unsigned char*)(&tmp) = *(unsigned char*)p;
        sum += tmp;
    }

    while (sum >> 16) sum = (sum & 0xffff) + (sum >> 16);
    return (unsigned short)(~sum);
}

void send_syn_packet_ipheader(int sock,
                            const std::string &src_ip,
                            const std::string &dst_ip,
                            unsigned short src_port,
                            unsigned short dst_port)
{
    char packet[sizeof(struct iphdr) + sizeof(struct tcphdr)];
    memset(packet, 0, sizeof(packet));

    struct iphdr  *iph  = (struct iphdr *) packet;
    struct tcphdr *tcph = (struct tcphdr *)(packet + sizeof(struct iphdr));

    iph->ihl      = 5;
    iph->version  = 4;
    iph->tos      = 0;
    iph->tot_len  = htons(sizeof(packet));
    iph->id       = htons(rand() % 65535);
    iph->frag_off = 0;
    iph->ttl      = 64;
    iph->protocol = IPPROTO_TCP;
    iph->check    = 0;
    iph->saddr    = inet_addr(src_ip.c_str());
    iph->daddr    = inet_addr(dst_ip.c_str());

    tcph->source  = htons(src_port);
    tcph->dest    = htons(dst_port);
    tcph->seq     = htonl(rand());
    tcph->ack_seq = 0;
    tcph->doff    = 5; 
    tcph->syn     = 1;
    tcph->window  = htons(65535);
    tcph->check   = 0;
    tcph->urg_ptr = 0;

    iph->check = ip_checksum((unsigned short*)packet, sizeof(struct iphdr));

    int tcp_len = sizeof(struct tcphdr);
    tcph->check = tcp_checksum(iph, tcph, tcp_len);

    sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family      = AF_INET;
    sin.sin_port        = tcph->dest;
    sin.sin_addr.s_addr = iph->daddr;

    if (sendto(sock, packet, sizeof(packet), 0, 
            (sockaddr*)&sin, sizeof(sin)) < 0)
    {
        std::cerr << "Failed to send TCP SYN packet: " << strerror(errno) << std::endl;
    }
}

int listen_for_response_ipheader(int sock, 
                                unsigned short src_port, 
                                unsigned short dst_port,
                                const std::string &dst_ip,
                                int timeout)
{
    char buffer[BUFFER_SIZE];
    sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);

    struct timeval tv;
    tv.tv_sec  = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    for (int i = 0; i < 30; i++) {
        memset(buffer, 0, sizeof(buffer));
        int recv_len = recvfrom(sock, buffer, sizeof(buffer), 0, 
                                (sockaddr*)&addr, &addr_len);
        if (recv_len < 0) {
            return 1;
        }
        struct ip *iph = (struct ip*)buffer;
        if (iph->ip_p == IPPROTO_TCP) {
            int ip_hdr_len = iph->ip_hl * 4;
            if (recv_len >= ip_hdr_len + (int)sizeof(struct tcphdr)) {
                struct tcphdr *tcph = (struct tcphdr*)(buffer + ip_hdr_len);
                unsigned short rcv_dst_port = ntohs(tcph->dest);
                unsigned short rcv_src_port = ntohs(tcph->source);
                if (rcv_dst_port == src_port && rcv_src_port == dst_port) {
                    if (tcph->syn && tcph->ack) {
                        std::cout << dst_ip << " " << dst_port << " tcp open" << std::endl;
                        return 0;
                    } else if (tcph->rst) {
                        std::cout << dst_ip << " " << dst_port << " tcp closed" << std::endl;
                        return 0;
                    }
                }
            }
        }
    }
    return 1;
}

void send_syn_packet_ipv6(int sock, const std::string& src_ip, const std::string& dst_ip, uint16_t src_port, uint16_t dst_port) {
    char packet[sizeof(struct ip6_hdr) + sizeof(struct tcphdr)] = {0};

    // IPv6 header
    struct ip6_hdr *ip6h = (struct ip6_hdr *)packet;
    inet_pton(AF_INET6, src_ip.c_str(), &ip6h->ip6_src);
    inet_pton(AF_INET6, dst_ip.c_str(), &ip6h->ip6_dst);
    ip6h->ip6_flow = htonl((6 << 28) | (0 << 20) | 0); // version, traffic class, flow label
    ip6h->ip6_plen = htons(sizeof(struct tcphdr));
    ip6h->ip6_nxt = IPPROTO_TCP;
    ip6h->ip6_hops = 64;

    // TCP header
    struct tcphdr *tcph = (struct tcphdr *)(packet + sizeof(struct ip6_hdr));
    tcph->source = htons(src_port);
    tcph->dest = htons(dst_port);
    tcph->seq = htonl(rand());
    tcph->doff = 5;
    tcph->syn = 1;
    tcph->window = htons(65535);
    tcph->check = 0; // set below

    // Pseudo header for checksum
    struct {
        struct in6_addr src;
        struct in6_addr dst;
        uint32_t length;
        uint8_t zero[3];
        uint8_t next_hdr;
    } pseudo_hdr{};

    inet_pton(AF_INET6, src_ip.c_str(), &pseudo_hdr.src);
    inet_pton(AF_INET6, dst_ip.c_str(), &pseudo_hdr.dst);
    pseudo_hdr.length = htonl(sizeof(struct tcphdr));
    pseudo_hdr.next_hdr = IPPROTO_TCP;

    // Combine pseudo-header + TCP for checksum
    char checksum_buf[sizeof(pseudo_hdr) + sizeof(struct tcphdr)];
    memcpy(checksum_buf, &pseudo_hdr, sizeof(pseudo_hdr));
    memcpy(checksum_buf + sizeof(pseudo_hdr), tcph, sizeof(struct tcphdr));

    unsigned long sum = 0;
    for (size_t i = 0; i < sizeof(checksum_buf); i += 2) {
        uint16_t word = (checksum_buf[i] << 8) + (i + 1 < sizeof(checksum_buf) ? checksum_buf[i + 1] : 0);
        sum += word;
    }
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    tcph->check = ~((uint16_t)sum);

    // Destination
    struct sockaddr_in6 dst{};
    dst.sin6_family = AF_INET6;
    dst.sin6_port = tcph->dest;
    inet_pton(AF_INET6, dst_ip.c_str(), &dst.sin6_addr);

    // Send
    if (sendto(sock, packet, sizeof(packet), 0, (struct sockaddr *)&dst, sizeof(dst)) < 0) {
        std::cerr << "IPv6 SYN send failed: " << strerror(errno) << std::endl;
    }
}

int listen_for_response_ipv6(int sock, unsigned short src_port, unsigned short dst_port, const std::string &IP, int timeout_ms) {
    char buf[BUFFER_SIZE];
    sockaddr_in6 addr; socklen_t len=sizeof(addr);
    timeval tv{timeout_ms/1000,(timeout_ms%1000)*1000};
    setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));

    int n = recvfrom(sock, buf, sizeof(buf),0,(sockaddr*)&addr,&len);
    if(n>0) {
        struct tcphdr *tcph=(struct tcphdr*)(buf+40);
        if(ntohs(tcph->dest)==src_port && ntohs(tcph->source)==dst_port) {
            if(tcph->syn && tcph->ack) { std::cout<<IP<<" "<<dst_port<<" tcp open\n"; return 0; }
            if(tcph->rst)         { std::cout<<IP<<" "<<dst_port<<" tcp closed\n"; return 0; }
        }
    }
    return 1;
}

void scan_tcp(const std::vector<int>& tcp_ports, 
              const std::string& dst_ip, 
              int timeout_ms,
              const std::string &source_ip)
{
    bool is_ipv6 = dst_ip.find(':') != std::string::npos;

    if (is_ipv6) {
        srand(time(nullptr));
        for (int port : tcp_ports) {
            unsigned short src_port = 20000 + (rand() % 20000);

            int sock6 = socket(AF_INET6, SOCK_RAW, IPPROTO_RAW);
            if (sock6 < 0) {
                std::cerr << "Failed to create raw IPv6 TCP socket: " << strerror(errno) << std::endl;
                continue;
            }

            sockaddr_in6 addr6{};
            addr6.sin6_family = AF_INET6;
            addr6.sin6_port = htons(port);
            if (inet_pton(AF_INET6, dst_ip.c_str(), &addr6.sin6_addr) != 1) {
                std::cerr << "Invalid IPv6 address: " << dst_ip << std::endl;
                close(sock6);
                continue;
            }

            send_syn_packet_ipv6(sock6, source_ip, dst_ip, src_port, port);

            if (listen_for_response_ipv6(sock6, src_port, (unsigned short)port, dst_ip, timeout_ms)) {
                send_syn_packet_ipv6(sock6, source_ip, dst_ip, src_port, port);
                if (listen_for_response_ipv6(sock6, src_port, (unsigned short)port, dst_ip, timeout_ms)) {
                    std::cout << dst_ip << " " << port << " tcp filtered" << std::endl;
                }
            }

            close(sock6);
        }

    } else {
        int sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
        if (sock < 0) {
            std::cerr << "Failed to create raw TCP socket: " << strerror(errno) << std::endl;
            return;
        }
        int one = 1;
        if (setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0) {
            std::cerr << "setsockopt(IP_HDRINCL) failed: " << strerror(errno) << std::endl;
            close(sock);
            return;
        }

        srand(time(nullptr));

        for (int port : tcp_ports) {
            unsigned short src_port = 20000 + (rand() % 20000);
            send_syn_packet_ipheader(sock, source_ip, dst_ip, src_port, (unsigned short)port);

            if (listen_for_response_ipheader(sock, src_port, (unsigned short)port, dst_ip, timeout_ms)) {
                send_syn_packet_ipheader(sock, source_ip, dst_ip, src_port, (unsigned short)port);
                if (listen_for_response_ipheader(sock, src_port, (unsigned short)port, dst_ip, timeout_ms)) {
                    std::cout << dst_ip << " " << port << " tcp filtered" << std::endl;
                }
            }
        }

        close(sock);
    }
}

void scan_udp(const std::vector<int>& udp_ports, 
            const std::string& IP, 
            int timeout) 
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock < 0) {
        std::cerr << "Failed to create UDP socket: " << strerror(errno) << std::endl;
        return;
    }

    for (int port : udp_ports) {
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(port);
        inet_pton(AF_INET, IP.c_str(), &addr.sin_addr);

        char buffer[1] = {0};

        if(sendto(sock, buffer, sizeof(buffer), 0, 
                (sockaddr*)&addr, sizeof(addr)) < 0)
        {
            std::cerr << "Failed sending UDP packet.\n";
            continue;
        }

        struct timeval tv;
        tv.tv_sec  = timeout / 1000;
        tv.tv_usec = (timeout % 1000) * 1000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
        socklen_t len = sizeof(addr);

        if(recvfrom(sock, buffer, sizeof(buffer), 0, 
                    (struct sockaddr *) &addr, &len) < 0)
        {
            std::cout << IP << " " << port << " udp open" << std::endl;
        }
        else {
            std::cout << IP << " " << port << " udp open" << std::endl;
        }
    }

    close(sock);
}

void signal_handler(int) {
    exit(0);
}

int main(int argc, char *argv[]) {
    signal(SIGINT, signal_handler);

    if(argc == 1 || (argc == 2 && (std::strcmp(argv[1], "-i") == 0 || std::strcmp(argv[1], "--interface") == 0))) {
        list_interfaces();
        exit(0);
    }

    parse_args(argc, argv, interfaceArg, port_range_tcp, port_range_udp, IP, &timeout_ms);

    struct in_addr ipv4_test;
    struct in6_addr ipv6_test;

    if (inet_pton(AF_INET, IP.c_str(), &ipv4_test) == 1 || 
        inet_pton(AF_INET6, IP.c_str(), &ipv6_test) == 1) {
    } else {
        IP = resolve_hostname(IP);
    }

    std::string source_ip;
    if (!interfaceArg.empty()) {
        source_ip = get_ip_from_iface(interfaceArg);
        if (source_ip.empty()) {
            std::cerr << "Could not find IPv4 address for interface: " << interfaceArg << std::endl;
            source_ip = DEFAULT_SOURCE_IP;
        } else {
            std::cerr << "Using source IP " << source_ip 
                    << " from interface " << interfaceArg << std::endl;
        }
    } else {
        source_ip = DEFAULT_SOURCE_IP;
        std::cerr << "No interface provided. Using fallback source IP: " 
                << source_ip << std::endl;
    }

    std::vector<int> tcp_ports, udp_ports;
    if(!port_range_tcp.empty()) {
        tcp_ports = parse_ports(port_range_tcp);
    }
    if(!port_range_udp.empty()) {
        udp_ports = parse_ports(port_range_udp);
    }

    if(!tcp_ports.empty()) {
        scan_tcp(tcp_ports, IP, timeout_ms, source_ip);
    }
    if(!udp_ports.empty()) {
        scan_udp(udp_ports, IP, timeout_ms);
    }

    return 0;
}
