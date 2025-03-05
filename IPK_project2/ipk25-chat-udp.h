#ifndef UDP_H
#define UDP_H

#include <string>
#include <stdint.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <chrono>
#include <algorithm>
#include "ipk25-chat.h"

#pragma pack(push, 1)
struct MessageHeader {
    uint8_t type;
    uint16_t MessageID;
    MessageHeader(uint8_t type, uint16_t MessageID)
    : type(type), MessageID(MessageID) {}

    void toNetworkByteOrder() {
        MessageID = htons(MessageID);
    }
};

void udp_auth(ProgramArgs *arg);
void udp_join(ProgramArgs *args, std::string channelID);
void udp_msg(ProgramArgs *args, std::string MessageContent);
void udp_err(ProgramArgs *args, std::string MessageContent);
void udp_bye(ProgramArgs *args);
void udp_ping(ProgramArgs *args);
void* udp_listen(void* arg);

#endif // UDP_H
