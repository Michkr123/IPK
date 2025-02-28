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
