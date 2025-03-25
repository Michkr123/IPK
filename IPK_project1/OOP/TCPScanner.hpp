#pragma once
#include <string>
#include <vector>

class TCPScanner {
private:
    std::string dst_ip;
    std::string src_ip;
    std::vector<int> ports;
    int timeout_ms;

public:
    TCPScanner(const std::string& dst, const std::string& src, const std::vector<int>& p, int timeout);
    void scan();
};
