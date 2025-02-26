#include "ipk25-chat-udp.h"
#include <iostream>
#include <iomanip> //TODO debug - delete

//TODO debug - delete
void print_buffer(const std::vector<uint8_t> &buffer) {
    std::cout << "Buffer content (" << buffer.size() << " bytes): ";
    for (uint8_t byte : buffer) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') 
                  << static_cast<int>(byte) << " ";
    }
    std::cout << std::dec << std::endl;
}
//TODO debug - delete

bool receive_UDP(ProgramArgs *args) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::cerr << "Error creating socket\n";
        return false;
    }

    struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(server_addr);
    char buffer[1024];

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(args->port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Error binding socket\n";
        close(sockfd);
        return false;
    }

    fd_set readfds;
    struct timeval tv;

    for (uint8_t attempt = 0; attempt < args->retry_count; ++attempt) {
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        tv.tv_sec = args->timeout / 1000;
        tv.tv_usec = (args->timeout % 1000) * 1000;

        int ret = select(sockfd + 1, &readfds, NULL, NULL, &tv);
        if (ret > 0) {
            ssize_t recv_len = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0,
                                        (struct sockaddr *)&server_addr, &addr_len);
            if (recv_len > 0) {
                if ((recv_len == 3) && 
                    buffer[0] == 0x00 && 
                    buffer[1] == (args->messageID >> 8) &&
                    buffer[2] == (args->messageID & 0xFF)) {

                    std::cout << "Received confirmation: type = 0x00, refMessageID = " 
                              << std::hex << (int)buffer[1] << (int)buffer[2] << std::dec << "\n";
                    close(sockfd);
                    return true;
                }
                else
                    return false;
            }
        } else if (ret == 0) {
            std::cerr << "Timeout, retrying...\n";
        } else {
            std::cerr << "Error in select()\n";
            close(sockfd);
            return false;
        }
    }

    close(sockfd);
    return false;
}

void send_UDP(ProgramArgs *args, const std::vector<uint8_t>& buffer) {
    int sockfd;
    struct sockaddr_in serverAddr;
    struct hostent *server;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Error creating socket");
        exit(1);
    }

    server = gethostbyname(args->hostname.c_str());
    if (server == NULL) {
        std::cerr << "Error: No such host found" << std::endl;
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(args->port);
    memcpy(&serverAddr.sin_addr.s_addr, server->h_addr, server->h_length);

    ssize_t sentBytes = sendto(sockfd, buffer.data(), buffer.size(), 0,
                               (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    if (sentBytes < 0) {
        perror("Error sending UDP packet");
    }

    close(sockfd);

    if(!receive_UDP(args)) {
        std::cerr << "No response received." << std::endl; //TODO asi done - zobrazit chybu a ukonceni aplikace s error code
        //exit(1);
    }

    //TODO sprava poslana a prijato potvrzeni -> vse v poradku
    args->messageID++;
}

void udp_confirm(ProgramArgs *args, uint16_t refMessageID) {
    uint8_t type = 0x00;
    MessageHeader msgHeader(type, refMessageID);

    std::vector<uint8_t> buffer;

    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&msgHeader), 
                  reinterpret_cast<uint8_t*>(&msgHeader) + sizeof(msgHeader));

    print_buffer(buffer); //TODO debug
    send_UDP(args, buffer);
}

void udp_reply(ProgramArgs *args, uint16_t refMessageID, std::string MessageContent) {
    uint8_t type = 0x01;
    MessageHeader msgHeader(type, args->messageID);

    std::vector<uint8_t> buffer;

    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&msgHeader), 
                  reinterpret_cast<uint8_t*>(&msgHeader) + sizeof(msgHeader));

    //TODO 1 byte result
    uint8_t result = 0x01;  //TODO RESULT!
    buffer.push_back(result);

    buffer.push_back(static_cast<uint8_t>(refMessageID >> 8));
    buffer.push_back(static_cast<uint8_t>(refMessageID & 0xFF));

    buffer.insert(buffer.end(), MessageContent.begin(), MessageContent.end());
    buffer.push_back('\0');

    print_buffer(buffer); //TODO debug
    send_UDP(args, buffer);
}

void udp_auth(ProgramArgs *args) {
    uint8_t type = 0x02;
    MessageHeader msgHeader(type, args->messageID);
    
    std::vector<uint8_t> buffer;

    //TODO poslat auth spravu s username displayName a secret

    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&msgHeader), 
                  reinterpret_cast<uint8_t*>(&msgHeader) + sizeof(msgHeader));

    buffer.insert(buffer.end(), args->username.begin(), args->username.end());
    buffer.push_back('\0');

    buffer.insert(buffer.end(), args->displayName.begin(), args->displayName.end());
    buffer.push_back('\0');

    buffer.insert(buffer.end(), args->secret.begin(), args->secret.end());
    buffer.push_back('\0');

    print_buffer(buffer); //TODO debug
    send_UDP(args, buffer);
}

void udp_join(ProgramArgs *args, std::string channelID) {
    uint8_t type = 0x03;
    MessageHeader msgHeader(type, args->messageID);

    std::vector<uint8_t> buffer;

    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&msgHeader), 
                  reinterpret_cast<uint8_t*>(&msgHeader) + sizeof(msgHeader));

    buffer.insert(buffer.end(), channelID.begin(), channelID.end());
    buffer.push_back('\0');

    buffer.insert(buffer.end(), args->displayName.begin(), args->displayName.end());
    buffer.push_back('\0');

    print_buffer(buffer); //TODO debug
    send_UDP(args, buffer);
}

void udp_msg(ProgramArgs *args, std::string MessageContent) {
    uint8_t type = 0x04;
    MessageHeader msgHeader(type, args->messageID);

    std::vector<uint8_t> buffer;

    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&msgHeader), 
                  reinterpret_cast<uint8_t*>(&msgHeader) + sizeof(msgHeader));

    buffer.insert(buffer.end(), args->displayName.begin(), args->displayName.end());
    buffer.push_back('\0');

    buffer.insert(buffer.end(), MessageContent.begin(), MessageContent.end());
    buffer.push_back('\0');

    print_buffer(buffer); //TODO debug
    send_UDP(args, buffer);
}

void udp_err(ProgramArgs *args, std::string MessageContent) {
    uint8_t type = 0xFE;
    MessageHeader msgHeader(type, args->messageID);

    std::vector<uint8_t> buffer;

    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&msgHeader), 
                  reinterpret_cast<uint8_t*>(&msgHeader) + sizeof(msgHeader));

    buffer.insert(buffer.end(), args->displayName.begin(), args->displayName.end());
    buffer.push_back('\0');

    buffer.insert(buffer.end(), MessageContent.begin(), MessageContent.end());
    buffer.push_back('\0');

    print_buffer(buffer); //TODO debug
    send_UDP(args, buffer);
}

void udp_bye(ProgramArgs *args) {
    uint8_t type = 0xFF;
    MessageHeader msgHeader(type, args->messageID);

    std::vector<uint8_t> buffer;

    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&msgHeader), 
                  reinterpret_cast<uint8_t*>(&msgHeader) + sizeof(msgHeader));

    buffer.insert(buffer.end(), args->displayName.begin(), args->displayName.end());
    buffer.push_back('\0');

    print_buffer(buffer); //TODO debug
    send_UDP(args, buffer);
}

void udp_ping(ProgramArgs *args) {
    uint8_t type = 0xFD;
    MessageHeader msgHeader(type, args->messageID);

    std::vector<uint8_t> buffer;

    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&msgHeader), 
                  reinterpret_cast<uint8_t*>(&msgHeader) + sizeof(msgHeader));

    print_buffer(buffer); //TODO debug
    send_UDP(args, buffer);
}

void udp_listen(ProgramArgs *args, bool *exit_flag) {
    while(!(*exit_flag)) {
        std::cout << "listening" << std::endl;
    }
}
