#include "UDPScanner.hpp"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <pcap.h>  // libpcap header

UDPScanner::UDPScanner(const std::string& dst, const std::vector<int>& p, int timeout)
    : dst_ip(dst), ports(p), timeout_ms(timeout) {}

// Helper: capture ICMP unreachable message using libpcap.
// For IPv4, we look for ICMP type 3, code 3; for IPv6, for ICMPv6 type 1, code 4.
int capture_udp_icmp(const std::string &iface, const std::string &dst_ip,
                     int dst_port, int timeout_ms, bool is_ipv6) {
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t *handle = pcap_open_live(iface.c_str(), 65535, 1, timeout_ms, errbuf);
    if (!handle) {
        std::cerr << "pcap_open_live: " << errbuf << std::endl;
        return -1;
    }
    char filter_exp[256];
    if (!is_ipv6) {
        // For IPv4, capture ICMP unreachable (type 3, code 3) destined to the target.
        snprintf(filter_exp, sizeof(filter_exp),
                 "icmp and (icmp[0] == 3 and icmp[1] == 3) and dst host %s", dst_ip.c_str());
    } else {
        // For IPv6, capture ICMPv6 Destination Unreachable (type 1, code 4) destined to the target.
        snprintf(filter_exp, sizeof(filter_exp),
                 "icmp6 and (icmp6[0] == 1 and icmp6[1] == 4) and dst host %s", dst_ip.c_str());
    }
    struct bpf_program fp;
    if (pcap_compile(handle, &fp, filter_exp, 0, PCAP_NETMASK_UNKNOWN) == -1) {
        std::cerr << "pcap_compile error: " << pcap_geterr(handle) << std::endl;
        pcap_close(handle);
        return -1;
    }
    if (pcap_setfilter(handle, &fp) == -1) {
        std::cerr << "pcap_setfilter error: " << pcap_geterr(handle) << std::endl;
        pcap_freecode(&fp);
        pcap_close(handle);
        return -1;
    }
    pcap_freecode(&fp);

    struct pcap_pkthdr *header;
    const u_char *packet;
    int ret = pcap_next_ex(handle, &header, &packet);
    pcap_close(handle);
    if (ret > 0) {
        // Got an ICMP unreachable message => UDP port is closed.
        return 0;
    } else {
        // No ICMP response captured within timeout => consider port open.
        return 1;
    }
}

void UDPScanner::scan() {
    bool is_ipv6 = dst_ip.find(':') != std::string::npos;
    srand(time(nullptr));

    // Use the same interface as specified in your TCPScanner.
    // Adjust this value or pass it as a parameter if needed.
    std::string iface = "wlp3s0";

    if (is_ipv6) {
        int sock6 = socket(AF_INET6, SOCK_DGRAM, 0);
        if (sock6 < 0) {
            std::cerr << "Failed to create IPv6 UDP socket: " << strerror(errno) << std::endl;
            return;
        }

        for (int port : ports) {
            sockaddr_in6 addr6{};
            addr6.sin6_family = AF_INET6;
            addr6.sin6_port = htons(port);
            if (inet_pton(AF_INET6, dst_ip.c_str(), &addr6.sin6_addr) != 1) {
                std::cerr << "Invalid IPv6 address: " << dst_ip << std::endl;
                continue;
            }
            char buffer[1] = {0};
            // Send a zero-length (or 1-byte) UDP packet.
            sendto(sock6, buffer, sizeof(buffer), 0, (sockaddr*)&addr6, sizeof(addr6));

            // Use libpcap to listen for an ICMPv6 "port unreachable" message.
            int res = capture_udp_icmp(iface, dst_ip, port, timeout_ms, true);
            if (res == 0)
                std::cout << dst_ip << " " << port << " udp closed" << std::endl;
            else if (res == 1)
                std::cout << dst_ip << " " << port << " udp open" << std::endl;
            else
                std::cout << dst_ip << " " << port << " udp filtered" << std::endl;
        }

        close(sock6);
    } else {
        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0) {
            std::cerr << "Failed to create UDP socket: " << strerror(errno) << std::endl;
            return;
        }

        for (int port : ports) {
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            inet_pton(AF_INET, dst_ip.c_str(), &addr.sin_addr);

            char buffer[1] = {0};
            sendto(sock, buffer, sizeof(buffer), 0, (sockaddr*)&addr, sizeof(addr));

            int res = capture_udp_icmp(iface, dst_ip, port, timeout_ms, false);
            if (res == 0)
                std::cout << dst_ip << " " << port << " udp closed" << std::endl;
            else if (res == 1)
                std::cout << dst_ip << " " << port << " udp open" << std::endl;
            else
                std::cout << dst_ip << " " << port << " udp filtered" << std::endl;
        }

        close(sock);
    }
}
