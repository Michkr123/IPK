#include "Utils.h"
#include <sstream>
#include <cctype>
#include <cstring>
#include <netdb.h>
#include <iostream>
#include <cstdlib>
#include <getopt.h>

// Example implementation for parseArguments using getopt.
Options parseArguments(int argc, char *argv[]) {
    Options opts;
    int c;
    
    // Use getopt to parse flags: -t for protocol, -s for hostname, -p for port.
    while ((c = getopt(argc, argv, "t:s:p:")) != -1) {
        switch(c) {
            case 't':
                opts.protocol = optarg;
                break;
            case 's':
                opts.hostname = optarg;
                break;
            case 'p':
                try {
                    opts.port = static_cast<uint16_t>(std::stoi(optarg));
                } catch (const std::invalid_argument&) {
                    std::cerr << "Invalid port number provided." << std::endl;
                    exit(1);
                }
                break;
            default:
                std::cerr << "Usage: -t <protocol> -s <hostname> [-p <port>]" << std::endl;
                exit(1);
        }
    }
    return opts;
}

std::vector<std::string> split(const std::string &str) {
    std::vector<std::string> tokens;
    std::istringstream iss(str);
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

bool isValidString(const std::string &str) {
    for (char c : str) {
        if (!(std::isalnum(c) || c == '-' || c == '_')) {
            return false;
        }
    }
    return true;
}

bool isPrintableChar(const std::string &str) {
    for (char c : str) {
        if (c < 0x21 || c > 0x7E) {
            return false;
        }
    }
    return true;
}

bool isValidMessage(const std::string &message) {
    for (char c : message) {
        if(c != 0x0A && (c < 0x20 || c > 0x7E)) {
            return false;
        }
    }
    return true;
}

void resolve_hostname(const std::string &hostname, uint16_t port, struct sockaddr_in &serverAddr) {
    struct addrinfo hints, *res;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;       // IPv4
    hints.ai_socktype = SOCK_STREAM;  // Default to TCP; for UDP, you could use SOCK_DGRAM

    if (getaddrinfo(hostname.c_str(), nullptr, &hints, &res) != 0) {
         std::cerr << "Error: No such host found: " << hostname << std::endl;
         exit(1);
    }
    
    struct sockaddr_in *ipv4 = reinterpret_cast<struct sockaddr_in *>(res->ai_addr);
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr = ipv4->sin_addr;
    serverAddr.sin_port = htons(port);
    
    freeaddrinfo(res);
}
