#include "ipk25-chat-udp.h"
#include <iostream>

void udp_confirm(ProgramArgs *args, uint16_t refMessageID) {
    uint8_t type = 0x00;
    MessageHeader msgHeader(type, refMessageID);

    //TODO jen poslat confirm message

    std::cout << "Confirm udp" << std::endl;
}

void udp_reply(ProgramArgs *args, uint16_t refMessageID, std::string MessageContent) {
    uint8_t type = 0x01;
    MessageHeader msgHeader(type, args->messageID);

    //TODO poslat reply s result, refmessageID a messageContent

    std::cout << "Reply udp" << std::endl;
}

void udp_auth(ProgramArgs *args) {
    uint8_t type = 0x02;
    MessageHeader msgHeader(type, args->messageID);

    //TODO poslat auth spravu s username displayName a secret

    

    std::cout << "Auth udp" << std::endl;
}

void udp_join(ProgramArgs *args, std::string channelID) {
    uint8_t type = 0x03;
    MessageHeader msgHeader(type, args->messageID);

    //TODO poslat join s channelID a DisplayName

    std::cout << "Join udp" << std::endl;
}

void udp_msg(ProgramArgs *args, std::string MessageContent) {
    uint8_t type = 0x04;
    MessageHeader msgHeader(type, args->messageID);

    //TODO poslat msg s DisplayName a MessageContent

    std::cout << "Message udp" << std::endl;
}

void udp_err(ProgramArgs *args, std::string MessageContent) {
    uint8_t type = 0xFE;
    MessageHeader msgHeader(type, args->messageID);

    //TODO poslat err s DisplayName a MessageContent

    std::cout << "Error udp" << std::endl;
}

void udp_bye(ProgramArgs *args) {
    uint8_t type = 0xFF;
    MessageHeader msgHeader(type, args->messageID);

    //TODO poslat bye s DisplayName

    std::cout << "Bye udp" << std::endl;
}

void udp_ping(ProgramArgs *args) {
    uint8_t type = 0xFD;
    MessageHeader msgHeader(type, args->messageID);

    //TODO jen poslat ping message

    std::cout << "Ping udp" << std::endl;
}

