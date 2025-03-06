#ifndef MAIN_H
#define MAIN_H

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <getopt.h>
#include <netinet/in.h>
#include <cstdint>
#include <pthread.h>

struct ProgramArgs {
    std::string protocol;
    std::string hostname;
    std::string username;
    std::string secret;
    std::string displayName;

    uint16_t port = 4567;
    uint16_t timeout = 250;
    uint16_t messageID = 0;
    uint8_t retry_count = 3;

    int32_t sockfd;
    sockaddr_in serverAddr;

    std::string state = "start";

    std::vector<uint16_t> seenMessagesID;
};

#endif // MAIN_H