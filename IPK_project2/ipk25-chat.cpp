#include <iostream>
#include <cstring>
#include <getopt.h>
#include <cstdint>

//TODO Message start at 0 - increments when message sent
//TODO user: -t [tcp/udp] -s [IP/hostname]
//TODO constants: -p 4567, -d 250 (timeout)
void help() {
    std::cout << "help" << std::endl;
    exit(0);
}

void parse_arguments(int argc, char *argv[], std::string &protocol, std::string &hostname, uint16_t &port, uint16_t &timeout, uint8_t &retry_count) {
    int c;
    while ((c = getopt(argc, argv, "t:s:p:d:r:")) != -1) {
        switch (c) {
            case 't':
                protocol = optarg;
                break;
            case 's':
                hostname = optarg;
                break;
            case 'p':
                port = static_cast<uint16_t>(std::stoi(optarg));
                break;
            case 'd':
                timeout = static_cast<uint16_t>(std::stoi(optarg));
                break;
            case 'r':
                retry_count = static_cast<uint8_t>(std::stoi(optarg));
                break;
            default:
                std::cerr << "Usage: " << argv[0] << " -t [protocol] -s [hostname] -p [port] -d [timeout] -r [retry_count]" << std::endl;
                exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char *argv[]) {
    if(argc == 2 && std::strcmp(argv[1], "-h") == 0) {
        help();
    }

    std::string protocol, hostname;
    uint16_t port = 4567, timeout = 250;
    uint8_t retry_count = 3;

    parse_arguments(argc, argv, protocol, hostname, port, timeout, retry_count);
    
    std::cout << "Protocol: " << protocol << std::endl;
    std::cout << "Hostname: " << hostname << std::endl;
    std::cout << "Port: " << port << std::endl;
    std::cout << "Timeout: " << timeout << std::endl;
    std::cout << "Retry Count: " << (int)retry_count << std::endl;
    return 0;
}
