#include "Utils.h"
#include <sstream>
#include <cctype>
#include <cstring>
#include <netdb.h>
#include <iostream>
#include <cstdlib>
#include <getopt.h>

void help() {
    std::cout << std::endl;
    std::cout << "Usage: ./chatClient -t <protocol> -s <hostname/IP> -p <port> -d <timeout> -r <retransmissions>\n";
    std::cout << "       protocol:          udp or tcp. (required)" << std::endl;
    std::cout << "       hostname:          server hostname or ip. (required)" << std::endl;
    std::cout << "       port:              server port. (optional)" << std::endl;
    std::cout << "       timeout:           UDP confirmation timeout (in milliseconds). (optional)" << std::endl;
    std::cout << "       retransmissions:   maximum number of UDP retransmissions. (optional)" << std::endl;
    std::cout << std::endl;
    std::cout << "Default values for optional arguments:" << std::endl;
    std::cout << "       port:              4567" << std::endl;
    std::cout << "       timeout:           250ms" << std::endl;
    std::cout << "       retransmissions:   3" << std::endl;
    exit(0);
}

void commandHelp() {
    std::cout << std::endl;
    std::cout << "Supported client commands:" << std::endl;
    std::cout << "  /auth {Username} {Secret} {DisplayName} - Autorises to the server. (use once)" << std::endl;
    std::cout << "  /join {ChannelID}                       - Changes the channel." << std::endl;
    std::cout << "  /rename {DisplayName}                   - Changes the display name." << std::endl;
    std::cout << "  /err {MessageContent}                   - Sends error to the server." << std::endl;
    std::cout << "  /bye                                    - disconnects from the server." << std::endl;
    std::cout << "  {messageContent}                        - Sends message." << std::endl;
}

Options parseArguments(int argc, char *argv[]) {
    Options opts;
    int c;
    while ((c = getopt(argc, argv, "t:s:p:d:r:")) != -1) {
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
                    std::cout << "ERROR: Invalid port number provided." << std::endl;
                    exit(1);
                }
                break;
            case 'd':
                try {
                    opts.timeout = static_cast<uint16_t>(std::stoi(optarg));
                } catch (const std::invalid_argument&) {
                    std::cout << "ERROR: Invalid timeout provided." << std::endl;
                    exit(1);
                }
                break;
            case 'r':
                try {
                    opts.retry_count = static_cast<uint8_t>(std::stoi(optarg));
                } catch (const std::invalid_argument&) {
                    std::cout << "ERROR: Invalid retry count provided." << std::endl;
                    exit(1);
                }
                break;
            default:
                help();
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