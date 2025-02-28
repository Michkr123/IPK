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
    int sockfd = args->sockfd;
    struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(server_addr);
    char buffer[1024];

    fd_set readfds;
    struct timeval tv;

    for (uint8_t attempt = 0; attempt < args->retry_count; attempt++) {
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
                    return true;
                }
                else {
                    attempt--; // Handle message type mismatch
                }
            }
        } else if (ret == 0) {
            std::cerr << "Timeout, retrying...\n";
        } else {
            std::cerr << "Error in select()\n";
            return false;
        }
    }

    return false;
}

void send_UDP(ProgramArgs *args, const std::vector<uint8_t>& buffer) {
    int sockfd = args->sockfd;
    struct sockaddr_storage serverAddr;  // Generic struct for IPv4 & IPv6
    socklen_t addrLen;

    struct addrinfo hints, *res;
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;  // Allow both IPv4 and IPv6
    hints.ai_socktype = SOCK_DGRAM;

    if (getaddrinfo(args->hostname.c_str(), nullptr, &hints, &res) != 0) {
        std::cerr << "Error: No such host found" << std::endl;
        return;
    }

    if (res->ai_family == AF_INET) {
        struct sockaddr_in *ipv4 = (struct sockaddr_in *)res->ai_addr;
        memcpy(&serverAddr, ipv4, sizeof(struct sockaddr_in));
        ((struct sockaddr_in*)&serverAddr)->sin_port = htons(args->port);
        addrLen = sizeof(struct sockaddr_in);
    } else if (res->ai_family == AF_INET6) {
        struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)res->ai_addr;
        memcpy(&serverAddr, ipv6, sizeof(struct sockaddr_in6));
        ((struct sockaddr_in6*)&serverAddr)->sin6_port = htons(args->port);
        addrLen = sizeof(struct sockaddr_in6);
    } else {
        std::cerr << "Error: Unsupported address family" << std::endl;
        freeaddrinfo(res);
        return;
    }

    freeaddrinfo(res);

    ssize_t sentBytes = sendto(sockfd, buffer.data(), buffer.size(), 0,
                               (struct sockaddr*)&serverAddr, addrLen);
    if (sentBytes < 0) {
        perror("Error sending UDP packet");
    }

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
//     uint8_t type = 0x01;
//     MessageHeader msgHeader(type, args->messageID);

//     std::vector<uint8_t> buffer;

//     buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&msgHeader), 
//                   reinterpret_cast<uint8_t*>(&msgHeader) + sizeof(msgHeader));

//     //TODO 1 byte result
//     uint8_t result = 0x01;  //TODO RESULT!
//     buffer.push_back(result);

//     buffer.push_back(static_cast<uint8_t>(refMessageID >> 8));
//     buffer.push_back(static_cast<uint8_t>(refMessageID & 0xFF));

//     buffer.insert(buffer.end(), MessageContent.begin(), MessageContent.end());
//     buffer.push_back('\0');

//     print_buffer(buffer); //TODO debug
//     send_UDP(args, buffer);
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
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    uint8_t buffer[1024];

    std::cout << "UDP Listener started on port " << args->port << "\n";

    while (!(*exit_flag)) {
        ssize_t recv_len = recvfrom(args->sockfd, buffer, sizeof(buffer) - 1, 0, 
                                    (struct sockaddr *)&client_addr, &addr_len);
                                    
        if (recv_len > 0 && buffer[0] != 0x00) {
            uint8_t type = buffer[0];
            uint16_t MessageID = ntohs(*(uint16_t *)(buffer + 1));

            switch(type) {
                case 0x01: { // TODO REPLY
                    uint8_t result = buffer[3];
                    uint16_t refMessageID = ntohs(*(uint16_t *)(buffer + 4));

                    char* ptr = (char*)(buffer + 6);
                    std::string messageContent(ptr);
                
                    std::cout << "Action " << (result ? "Success: " : "Failure: ") << messageContent << std::endl;
                
                    udp_confirm(args, MessageID);
                    break;
                }
                case 0x04: { // TODO MSG
                    char* ptr = (char*)(buffer + 3);
                    std::string displayName(ptr);

                    ptr += displayName.size() + 1;
                    std::string messageContents(ptr);
                
                    std::cout << displayName << ": " << messageContents << std::endl;
                
                    udp_confirm(args, MessageID);
                    break;
                }
                case 0xFE: { // TODO ERR
                    char* ptr = (char*)(buffer + 3);
                    std::string displayName(ptr);

                    ptr += displayName.size() + 1;
                    std::string messageContents(ptr);
                
                    std::cout << "ERROR FROM " << displayName << ": " << messageContents << std::endl;
                
                    udp_confirm(args, MessageID);
                    break;
                }
                case 0xFF: { // TODO BYE
                    std::cout << "Bye received" << std::endl;
                    udp_confirm(args, MessageID);
                    break;
                }
                case 0x00: { // TODO Confirm
                    std::cout << "Confirm received" << std::endl;
                    break;
                }
                default:
                    break;
            }
        } else if (recv_len < 0) {
            std::cerr << "Error receiving UDP message\n";
        }
    }

    std::cout << "UDP Listener shutting down...\n";
}
