#pragma once
#include <string>
#include <vector>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <pcap.h>

class UDPScanner {
private:
    std::string iface;
    std::string dst_ip;
    std::vector<int> ports;
    int timeout_ms;

public:
    UDPScanner(const std::string& dst, const std::vector<int>& p, int timeout);
    void scan();
};
