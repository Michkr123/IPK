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
#include <net/if.h>
#include <pcap.h>  // Include libpcap header

const int BUFFER_SIZE = 1500;

TCPScanner::TCPScanner(const std::string& dst, const std::string& src,
                       const std::vector<int>& p, int timeout)
    : dst_ip(dst), src_ip(src), ports(p), timeout_ms(timeout) {}

// --- Existing checksum functions ---

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

unsigned short calculate_checksum(unsigned short* buf, int len) {
    unsigned long sum = 0;
    while (len > 1) {
        sum += *buf++;
        len -= 2;
    }
    if (len == 1)
        sum += *(unsigned char*)buf;
    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);
    return static_cast<unsigned short>(~sum);
}

// --- IPv4 functions (unchanged) ---
void send_syn_packet(int sock, const std::string &src_ip, const std::string &dst_ip,
                     uint16_t src_port, uint16_t dst_port) {
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

int listen_for_response(int sock, uint16_t src_port, uint16_t dst_port,
                        const std::string &dst_ip, int timeout_ms) {
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

// --- Modified IPv6 send function (no bind here) ---
void send_syn_packet_ipv6(int sock, const std::string& src_ip, const std::string& dst_ip,
                          uint16_t src_port, uint16_t dst_port) {
    // Build the packet buffer: IPv6 header + TCP header.
    char packet[sizeof(struct ip6_hdr) + sizeof(struct tcphdr)];
    memset(packet, 0, sizeof(packet));
    // Build IPv6 header.
    struct ip6_hdr* ip6h = reinterpret_cast<struct ip6_hdr*>(packet);
    ip6h->ip6_flow = htonl(0x60000000);         // Version 6; zero TC and flow label.
    ip6h->ip6_plen = htons(sizeof(struct tcphdr)); // Payload length = TCP header.
    ip6h->ip6_nxt  = IPPROTO_TCP;                // Next header = TCP.
    ip6h->ip6_hops = 64;                         // Hop limit.
    if (inet_pton(AF_INET6, src_ip.c_str(), &ip6h->ip6_src) != 1) {
        std::cerr << "Invalid source IPv6 address\n";
        return;
    }
    if (inet_pton(AF_INET6, dst_ip.c_str(), &ip6h->ip6_dst) != 1) {
        std::cerr << "Invalid destination IPv6 address\n";
        return;
    }
    // Build TCP header.
    struct tcphdr* tcph = reinterpret_cast<struct tcphdr*>(packet + sizeof(struct ip6_hdr));
    tcph->source = htons(src_port);
    tcph->dest   = htons(dst_port);
    tcph->seq    = htonl(rand());
    tcph->doff   = 5;  // 5 x 4 = 20 bytes.
    tcph->syn    = 1;
    tcph->window = htons(65535);
    tcph->check  = 0; // Zero before checksum.
    // Build IPv6 pseudo-header.
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
    char pseudo_packet[sizeof(pseudo_hdr) + sizeof(struct tcphdr)];
    memcpy(pseudo_packet, &pseudo_hdr, sizeof(pseudo_hdr));
    memcpy(pseudo_packet + sizeof(pseudo_hdr), tcph, sizeof(struct tcphdr));
    tcph->check = calculate_checksum(reinterpret_cast<unsigned short*>(pseudo_packet),
                                     sizeof(pseudo_packet));
    if (tcph->check == 0) tcph->check = 0xFFFF;
    // Set destination address.
    struct sockaddr_in6 dst_addr;
    memset(&dst_addr, 0, sizeof(dst_addr));
    dst_addr.sin6_family = AF_INET6;
    dst_addr.sin6_port = 0;
    if (inet_pton(AF_INET6, dst_ip.c_str(), &dst_addr.sin6_addr) != 1) {
        std::cerr << "Invalid destination IPv6 address\n";
        return;
    }
    // Send the packet.
    if (sendto(sock, packet, sizeof(packet), 0,
               reinterpret_cast<struct sockaddr*>(&dst_addr), sizeof(dst_addr)) < 0) {
        perror("sendto");
    }
}

// --- New function using libpcap to listen for IPv6 responses ---
int listen_for_response_ipv6_pcap(const std::string &iface,
                                  unsigned short src_port, unsigned short dst_port,
                                  const std::string &dst_ip, int timeout_ms) {
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t *handle = pcap_open_live(iface.c_str(), 65535, 1, timeout_ms, errbuf);
    if (!handle) {
        std::cerr << "pcap_open_live: " << errbuf << std::endl;
        return 1;
    }
    // Build a filter: capture IPv6 TCP packets where
    // source IP = dst_ip, source port = dst_port, destination port = src_port.
    char filter_exp[256];
    snprintf(filter_exp, sizeof(filter_exp),
             "ip6 and tcp and src host %s and src port %d and dst port %d",
             dst_ip.c_str(), dst_port, src_port);
    struct bpf_program fp;
    if (pcap_compile(handle, &fp, filter_exp, 0, PCAP_NETMASK_UNKNOWN) == -1) {
        std::cerr << "pcap_compile error: " << pcap_geterr(handle) << std::endl;
        pcap_close(handle);
        return 1;
    }
    if (pcap_setfilter(handle, &fp) == -1) {
        std::cerr << "pcap_setfilter error: " << pcap_geterr(handle) << std::endl;
        pcap_freecode(&fp);
        pcap_close(handle);
        return 1;
    }
    pcap_freecode(&fp);
    
    // Loop to capture packets.
    struct pcap_pkthdr *header;
    const u_char *packet;
    int ret;
    while ((ret = pcap_next_ex(handle, &header, &packet)) >= 0) {
        if (ret == 0) continue; // timeout, no packet yet
        // Adjust for Ethernet header if needed.
        int offset = 0;
        int dlt = pcap_datalink(handle);
        if (dlt == DLT_EN10MB) { // Ethernet
            offset = 14;
        }
        if (header->caplen < offset + sizeof(struct ip6_hdr) + sizeof(struct tcphdr))
            continue;
        struct ip6_hdr *ip6h_cap = (struct ip6_hdr*)(packet + offset);
        if (ip6h_cap->ip6_nxt != IPPROTO_TCP)
            continue;
        struct tcphdr *tcph_cap = (struct tcphdr*)(packet + offset + sizeof(struct ip6_hdr));
        if (ntohs(tcph_cap->dest) != src_port || ntohs(tcph_cap->source) != dst_port)
            continue;
        if (tcph_cap->syn && tcph_cap->ack) {
            std::cout << dst_ip << " " << dst_port << " tcp open" << std::endl;
            pcap_close(handle);
            return 0;
        } else if (tcph_cap->rst) {
            std::cout << dst_ip << " " << dst_port << " tcp closed" << std::endl;
            pcap_close(handle);
            return 0;
        }
    }
    pcap_close(handle);
    return 1; // If no response, treat as filtered.
}

int listen_for_response_ipv6(int sock, unsigned short src_port, unsigned short dst_port,
                               const std::string &IP, int timeout_ms) {
    // For IPv6, use libpcap instead of recvfrom.
    // Change "wlp3s0" to your actual interface name.
    return listen_for_response_ipv6_pcap("wlp3s0", src_port, dst_port, IP, timeout_ms);
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
        src_addr.sin6_port = 0;
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
    } else {
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
