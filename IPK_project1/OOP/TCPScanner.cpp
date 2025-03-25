#include "TCPScanner.hpp"
#include <iostream>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

// For IPV6 bind
#include <net/if.h>

const int BUFFER_SIZE = 1500;

TCPScanner::TCPScanner(const std::string& dst, const std::string& src,
                       const std::vector<int>& p, int timeout)
    : dst_ip(dst), src_ip(src), ports(p), timeout_ms(timeout) {}

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
    struct {
        uint32_t src;
        uint32_t dst;
        uint8_t zero;
        uint8_t proto;
        uint16_t length;
    } pseudo_header;

    pseudo_header.src = iph->saddr;
    pseudo_header.dst = iph->daddr;
    pseudo_header.zero = 0;
    pseudo_header.proto = IPPROTO_TCP;
    pseudo_header.length = htons(tcp_len);

    unsigned long sum = 0;
    const unsigned short *p = (unsigned short*)&pseudo_header;
    for (size_t i = 0; i < sizeof(pseudo_header)/2; ++i)
        sum += *p++;

    p = (unsigned short*)tcph;
    for (int i = 0; i < tcp_len / 2; ++i)
        sum += *p++;

    if (tcp_len % 2 == 1)
        sum += *((unsigned char*)tcph + tcp_len - 1) << 8;
    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);
    return ~sum;
}

// Helper function to calculate checksum over a buffer.
unsigned short calculate_checksum(unsigned short* buf, int len) {
    unsigned long sum = 0;
    while (len > 1) {
        sum += *buf++;
        len -= 2;
    }
    if (len == 1) {
        sum += *(unsigned char*)buf;
    }
    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);
    return static_cast<unsigned short>(~sum);
}

void send_syn_packet(int sock, const std::string &src_ip, const std::string &dst_ip, uint16_t src_port, uint16_t dst_port) {
    char packet[sizeof(struct iphdr) + sizeof(struct tcphdr)] = {0};

    struct iphdr *iph = (struct iphdr *)packet;
    struct tcphdr *tcph = (struct tcphdr *)(packet + sizeof(struct iphdr));

    iph->ihl = 5;
    iph->version = 4;
    iph->tot_len = htons(sizeof(packet));
    iph->ttl = 64;
    iph->protocol = IPPROTO_TCP;
    iph->saddr = inet_addr(src_ip.c_str());
    iph->daddr = inet_addr(dst_ip.c_str());
    iph->check = ip_checksum((unsigned short*)iph, sizeof(struct iphdr));

    tcph->source = htons(src_port);
    tcph->dest = htons(dst_port);
    tcph->seq = htonl(rand());
    tcph->doff = 5;
    tcph->syn = 1;
    tcph->window = htons(65535);
    tcph->check = tcp_checksum(iph, tcph, sizeof(struct tcphdr));

    sockaddr_in sin{};
    sin.sin_family = AF_INET;
    sin.sin_port = tcph->dest;
    sin.sin_addr.s_addr = iph->daddr;

    sendto(sock, packet, sizeof(packet), 0, (sockaddr*)&sin, sizeof(sin));
}

int listen_for_response(int sock, uint16_t src_port, uint16_t dst_port, const std::string &dst_ip, int timeout_ms) {
    char buffer[BUFFER_SIZE];
    sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    for (int i = 0; i < 30; ++i) {
        int recv_len = recvfrom(sock, buffer, sizeof(buffer), 0, (sockaddr*)&addr, &addr_len);
        if (recv_len < 0) return 1;

        struct ip *iph = (struct ip*)buffer;
        if (iph->ip_p == IPPROTO_TCP) {
            int ip_hdr_len = iph->ip_hl * 4;
            struct tcphdr *tcph = (struct tcphdr*)(buffer + ip_hdr_len);

            if (ntohs(tcph->dest) == src_port && ntohs(tcph->source) == dst_port) {
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
    return 1;
}

void send_syn_packet_ipv6(int sock, const std::string& src_ip, const std::string& dst_ip, uint16_t src_port, uint16_t dst_port) {
    // Build the packet buffer: IPv6 header + TCP header.
    char packet[sizeof(struct ip6_hdr) + sizeof(struct tcphdr)];
    memset(packet, 0, sizeof(packet));
    
    // Build the IPv6 header.
    struct ip6_hdr* ip6h = reinterpret_cast<struct ip6_hdr*>(packet);
    ip6h->ip6_flow = htonl(0x60000000);         // Version 6; zero TC and flow label.
    ip6h->ip6_plen = htons(sizeof(struct tcphdr)); // Payload length is the TCP header.
    ip6h->ip6_nxt  = IPPROTO_TCP;                // Next header is TCP.
    ip6h->ip6_hops = 64;                         // Hop limit.
    if (inet_pton(AF_INET6, src_ip.c_str(), &ip6h->ip6_src) != 1) {
        std::cerr << "Invalid source IPv6 address\n";
        return;
    }
    if (inet_pton(AF_INET6, dst_ip.c_str(), &ip6h->ip6_dst) != 1) {
        std::cerr << "Invalid destination IPv6 address\n";
        return;
    }
    
    // Build the TCP header.
    struct tcphdr* tcph = reinterpret_cast<struct tcphdr*>(packet + sizeof(struct ip6_hdr));
    tcph->source = htons(src_port);
    tcph->dest   = htons(dst_port);
    tcph->seq    = htonl(rand());
    tcph->doff   = 5;    // 5 x 4 = 20 bytes TCP header.
    tcph->syn    = 1;    // SYN flag.
    tcph->window = htons(65535);
    tcph->check  = 0;    // Initialize checksum to 0.
    
    // Build the IPv6 pseudo-header for TCP checksum calculation.
    struct {
        struct in6_addr src;
        struct in6_addr dst;
        uint32_t tcp_len;
        uint8_t zeros[3];
        uint8_t next_hdr;
    } pseudo_hdr;
    pseudo_hdr.src = ip6h->ip6_src;
    pseudo_hdr.dst = ip6h->ip6_dst;
    pseudo_hdr.tcp_len = htonl(sizeof(struct tcphdr));
    memset(pseudo_hdr.zeros, 0, sizeof(pseudo_hdr.zeros));
    pseudo_hdr.next_hdr = IPPROTO_TCP;
    
    // Concatenate pseudo-header and TCP header.
    char pseudo_packet[sizeof(pseudo_hdr) + sizeof(struct tcphdr)];
    memcpy(pseudo_packet, &pseudo_hdr, sizeof(pseudo_hdr));
    memcpy(pseudo_packet + sizeof(pseudo_hdr), tcph, sizeof(struct tcphdr));
    
    // Calculate the TCP checksum.
    tcph->check = calculate_checksum(reinterpret_cast<unsigned short*>(pseudo_packet),
                                     sizeof(pseudo_packet));
    
    // Set up the destination address.
    struct sockaddr_in6 dst_addr;
    memset(&dst_addr, 0, sizeof(dst_addr));
    dst_addr.sin6_family = AF_INET6;
    dst_addr.sin6_port = htons(dst_port);
    if (inet_pton(AF_INET6, dst_ip.c_str(), &dst_addr.sin6_addr) != 1) {
        std::cerr << "Invalid destination IPv6 address\n";
        return;
    }
    
    // Send the packet.
    if (sendto(sock, packet, sizeof(packet), 0,
               reinterpret_cast<struct sockaddr*>(&dst_addr), sizeof(dst_addr)) < 0) {
        perror("sendto");
    } else {
        std::cout << "IPv6 SYN packet sent to " << dst_ip << std::endl;
    }
}

int listen_for_response_ipv6(int sock, unsigned short src_port, unsigned short dst_port, const std::string &IP, int timeout_ms) {
    char buf[1500];
    sockaddr_in6 addr {};
    socklen_t len = sizeof(addr);

    struct timeval tv = {timeout_ms / 1000, (timeout_ms % 1000) * 1000};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    int n = recvfrom(sock, buf, sizeof(buf), 0, (sockaddr*)&addr, &len);
    if (n <= 0) return 1;

    // Assume TCP header follows IPv6 header (40 bytes)
    struct tcphdr* tcph = (struct tcphdr*)(buf + sizeof(struct ip6_hdr));

    if (ntohs(tcph->dest) != src_port || ntohs(tcph->source) != dst_port)
        return 1;

    if (tcph->syn && tcph->ack) {
        std::cout << IP << " " << dst_port << " tcp open" << std::endl;
        return 0;
    }
    if (tcph->rst) {
        std::cout << IP << " " << dst_port << " tcp closed" << std::endl;
        return 0;
    }

    return 1; // treat as filtered if nothing conclusive
}

void TCPScanner::scan() {
    bool is_ipv6 = dst_ip.find(':') != std::string::npos;

    int sock = socket(is_ipv6 ? AF_INET6 : AF_INET, SOCK_RAW, is_ipv6 ? IPPROTO_RAW : IPPROTO_TCP);
    if (sock < 0) {
        std::cerr << "TCP socket error: " << strerror(errno) << std::endl;
        return;
    }

    int one = 1;
    if (is_ipv6) {
        setsockopt(sock, IPPROTO_IPV6, IPV6_HDRINCL, &one, sizeof(one));
        // Bind the IPv6 socket once to the source address.
        struct sockaddr_in6 src_addr;
        memset(&src_addr, 0, sizeof(src_addr));
        src_addr.sin6_family = AF_INET6;
        src_addr.sin6_port = 0; // use port 0 for raw socket
        if (inet_pton(AF_INET6, src_ip.c_str(), &src_addr.sin6_addr) != 1) {
            std::cerr << "Invalid IPv6 source address\n";
            close(sock);
            return;
        }
        if (bind(sock, reinterpret_cast<struct sockaddr*>(&src_addr), sizeof(src_addr)) < 0) {
            perror("bind");
            close(sock);
            return;
        }
    }
    else {
        setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one));
    }

    srand(time(nullptr));

    for (int port : ports) {
        unsigned short src_port = 20000 + (rand() % 20000);

        if (is_ipv6) {
            send_syn_packet_ipv6(sock, src_ip, dst_ip, src_port, port);

            if (listen_for_response_ipv6(sock, src_port, port, dst_ip, timeout_ms)) {
                send_syn_packet_ipv6(sock, src_ip, dst_ip, src_port, port);
                if (listen_for_response_ipv6(sock, src_port, port, dst_ip, timeout_ms)) {
                    std::cout << dst_ip << " " << port << " tcp filtered" << std::endl;
                }
            }
        } else {
            send_syn_packet(sock, src_ip, dst_ip, src_port, port);

            if (listen_for_response(sock, src_port, port, dst_ip, timeout_ms)) {
                send_syn_packet(sock, src_ip, dst_ip, src_port, port);
                if (listen_for_response(sock, src_port, port, dst_ip, timeout_ms)) {
                    std::cout << dst_ip << " " << port << " tcp filtered" << std::endl;
                }
            }
        }
    }

    close(sock);
}
