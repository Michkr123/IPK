#pragma once
#include <string>
#include <vector>
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
#include <pcap.h>

const int BUFFER_SIZE = 1500;

class TCPScanner {
private:
    std::string iface;
    std::string dst_ip;
    std::string src_ip;
    std::vector<int> ports;
    int timeout_ms;

public:
    TCPScanner(const std::string& interface, const std::string& dst, const std::string& src, const std::vector<int>& p, int timeout);
    void scan();
};
