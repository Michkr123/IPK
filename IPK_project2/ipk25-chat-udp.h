#ifndef UDP_H
#define UDP_H

#include <string>
#include <stdint.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include "ipk25-chat.h"

struct UDPHeader {
    uint16_t source_port;
    uint16_t destination_port;
    uint16_t length;
    uint16_t checksum;

    UDPHeader(uint16_t srcPort, uint16_t destPort, uint16_t len, uint16_t chksum)
        : source_port(htons(srcPort)), destination_port(htons(destPort)), 
          length(htons(len)), checksum(htons(chksum)) {}
};

#pragma pack(push, 1) //TODO delete? no padding when inserting
struct MessageHeader {
    uint8_t type;
    uint16_t MessageID;
    MessageHeader(uint8_t type, uint16_t MessageID)
    : type(type), MessageID(htons(MessageID)) {}
};

void udp_reply(ProgramArgs *args, uint16_t refMessageID, std::string MessageContent);
void udp_auth(ProgramArgs *arg);
void udp_join(ProgramArgs *args, std::string channelID);
void udp_msg(ProgramArgs *args, std::string MessageContent);
void udp_err(ProgramArgs *args, std::string MessageContent);
void udp_bye(ProgramArgs *args);
void udp_ping(ProgramArgs *args);
void udp_listen(ProgramArgs *args, bool *exit_flag);

#endif // UDP_H
