#include "UDPScanner.hpp"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

UDPScanner::UDPScanner(const std::string& dst, const std::vector<int>& p, int timeout)
    : dst_ip(dst), ports(p), timeout_ms(timeout) {}


void UDPScanner::scan() {
    bool is_ipv6 = dst_ip.find(':') != std::string::npos;
    srand(time(nullptr));

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
            sendto(sock6, buffer, sizeof(buffer), 0, (sockaddr*)&addr6, sizeof(addr6));

            struct timeval tv{};
            tv.tv_sec = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;
            setsockopt(sock6, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            socklen_t len = sizeof(addr6);

            if (recvfrom(sock6, buffer, sizeof(buffer), 0, (sockaddr*)&addr6, &len) < 0) {
                std::cout << dst_ip << " " << port << " udp open" << std::endl;
            } else {
                std::cout << dst_ip << " " << port << " udp open" << std::endl;
            }
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

            struct timeval tv{};
            tv.tv_sec = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;
            setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            socklen_t len = sizeof(addr);

            if (recvfrom(sock, buffer, sizeof(buffer), 0, (sockaddr*)&addr, &len) < 0) {
                std::cout << dst_ip << " " << port << " udp open" << std::endl;
            } else {
                std::cout << dst_ip << " " << port << " udp open" << std::endl;
            }
        }

        close(sock);
    }
}
