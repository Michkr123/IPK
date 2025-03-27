#include "UDPScanner.hpp"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp6.h>
#include <sys/socket.h>
#include <sys/select.h>

const int BUFFER_SIZE = 1500;

/**
 * @brief Constructs a UDPScanner object.
 * 
 * @param dst Destination IP address (IPv4 or IPv6).
 * @param p Ports to scan.
 * @param timeout Timeout in milliseconds to wait for ICMP replies.
 */
UDPScanner::UDPScanner(const std::string& dst, const std::vector<int>& p, int timeout)
    : dst_ip(dst), ports(p), timeout_ms(timeout) {}

/**
 * @brief Receives an ICMPv4 Port Unreachable response.
 * 
 * @param recv_sock Raw socket for receiving ICMP.
 * @param timeout_ms Timeout in milliseconds.
 * @return true If port is closed (ICMP type 3 code 3 received).
 * @return false If no such ICMP message received in time.
 */
static bool receive_icmp(int recv_sock, int timeout_ms) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(recv_sock, &fds);
    struct timeval tv{timeout_ms/1000, (timeout_ms%1000)*1000};
    if (select(recv_sock+1, &fds, nullptr, nullptr, &tv) <= 0) return false;

    char buf[BUFFER_SIZE];
    sockaddr_in src{};
    socklen_t len = sizeof(src);
    ssize_t n = recvfrom(recv_sock, buf, sizeof(buf), 0, (sockaddr*)&src, &len);
    if (n < (int)(sizeof(struct iphdr) + sizeof(struct icmphdr))) return false;

    struct iphdr *ip = (struct iphdr*)buf;
    struct icmphdr *icmp = (struct icmphdr*)(buf + (ip->ihl*4));
    return (icmp->type == ICMP_DEST_UNREACH && icmp->code == ICMP_PORT_UNREACH);
}

/**
 * @brief Receives an ICMPv6 Port Unreachable response.
 * 
 * @param recv_sock Raw socket for receiving ICMPv6.
 * @param timeout_ms Timeout in milliseconds.
 * @return true If port is closed (ICMPv6 type 1 code 4 received).
 * @return false If no such ICMPv6 message received in time.
 */
static bool receive_icmp6(int recv_sock, int timeout_ms) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(recv_sock, &fds);
    struct timeval tv{timeout_ms/1000, (timeout_ms%1000)*1000};
    if (select(recv_sock+1, &fds, nullptr, nullptr, &tv) <= 0) return false;

    char buf[BUFFER_SIZE];
    sockaddr_in6 src{};
    socklen_t len = sizeof(src);
    ssize_t n = recvfrom(recv_sock, buf, sizeof(buf), 0, (sockaddr*)&src, &len);
    if (n < (int)(sizeof(struct icmp6_hdr))) return false;

    struct icmp6_hdr *icmp6 = (struct icmp6_hdr*)buf;
    return (icmp6->icmp6_type == 1 && icmp6->icmp6_code == 4);
}

/**
 * @brief Performs UDP port scan on the target IP address.
 * 
 * Uses raw sockets to send empty datagrams and checks for ICMP unreachable replies.
 */
void UDPScanner::scan() {
    bool is_ipv6 = dst_ip.find(':') != std::string::npos;

    if (is_ipv6) {
        int send_sock = socket(AF_INET6, SOCK_DGRAM, 0);
        int recv_sock = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
        if (send_sock < 0 || recv_sock < 0) {
            perror("socket");
            return;
        }

        for (auto port : ports) {
            sockaddr_in6 dst{};
            dst.sin6_family = AF_INET6;
            dst.sin6_port = htons(port);
            inet_pton(AF_INET6, dst_ip.c_str(), &dst.sin6_addr);

            sendto(send_sock, nullptr, 0, 0, (sockaddr*)&dst, sizeof(dst));

            bool closed = receive_icmp6(recv_sock, timeout_ms);
            std::cout << dst_ip << " " << port << " udp " << (closed ? "closed\n" : "open\n");
        }

        close(send_sock);
        close(recv_sock);
    } else {
        int send_sock = socket(AF_INET, SOCK_DGRAM, 0);
        int recv_sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
        if (send_sock < 0 || recv_sock < 0) {
            perror("socket");
            return;
        }

        for (auto port : ports) {
            sockaddr_in dst{};
            dst.sin_family = AF_INET;
            dst.sin_port = htons(port);
            inet_pton(AF_INET, dst_ip.c_str(), &dst.sin_addr);

            sendto(send_sock, nullptr, 0, 0, (sockaddr*)&dst, sizeof(dst));

            bool closed = receive_icmp(recv_sock, timeout_ms);
            std::cout << dst_ip << " " << port << " udp " << (closed ? "closed\n" : "open\n");
        }

        close(send_sock);
        close(recv_sock);
    }
}
