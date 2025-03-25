#pragma once
#include <string>
#include <vector>

class UDPScanner {
private:
    std::string dst_ip;
    std::vector<int> ports;
    int timeout_ms;

public:
    UDPScanner(const std::string& dst, const std::vector<int>& p, int timeout);
    void scan();
};
