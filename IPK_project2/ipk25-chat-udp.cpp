#include "ipk25-chat-udp.h"
#include <iostream>
#include <iomanip> //TODO debug - delete

//TODO debug - delete
// void print_buffer(const std::vector<uint8_t> &buffer) {
//     std::cout << "Buffer content (" << buffer.size() << " bytes): ";
//     for (uint8_t byte : buffer) {
//         std::cout << std::hex << std::setw(2) << std::setfill('0') 
//                   << static_cast<int>(byte) << " ";
//     }
//     std::cout << std::dec << std::endl;
// }

// void print_raw_buffer(const uint8_t* buffer, size_t length) {
//     std::cout << "Buffer (" << length << " bytes): ";
//     for (size_t i = 0; i < length; i++) {
//         std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)buffer[i] << " ";
//     }
//     std::cout << std::dec << std::endl;
// }
//TODO debug - delete

void udp_check_reply(ProgramArgs *args, uint16_t messageID) {
    pid_t pid = fork();
    if(pid < 0) {
        exit(1);
    }
    else if(pid == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        if (!(std::find(args->messageReplysID.begin(), args->messageReplysID.end(), messageID) != args->messageReplysID.end())) {
            if(args->state != "end") {
                std::cerr << "Error: Didn't recieve reply of a sent AUTH/JOIN message!" << std::endl;
                udp_err(args, "No reply was recieved!"); //TODO message
                args->state = "end";
            } 
        }
        _exit(0);
    }
}

void send_UDP(ProgramArgs *args, const std::vector<uint8_t>& buffer) {
    for(int i = 0; i <= args->retry_count; i++) {
        struct sockaddr_in serverAddr;
        memset(&serverAddr, 0, sizeof(serverAddr));

        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr = args->serverAddr.sin_addr;
        serverAddr.sin_port = htons(args->port);

        ssize_t sentBytes = sendto(args->sockfd, buffer.data(), buffer.size(), 0,
                                (struct sockaddr*)&serverAddr, sizeof(serverAddr));
        if (sentBytes < 0) {
            perror("Error sending UDP packet");
        }

        uint8_t type = buffer[0];

        //std::cout << "buffer[0]: " << std::hex << static_cast<int>(type) << std::endl;

        if(type != 0x00) {
            std::this_thread::sleep_for(std::chrono::milliseconds(args->timeout));

            if (std::find(args->seenMessagesID.begin(), args->seenMessagesID.end(), args->messageID) != args->seenMessagesID.end()) {
                //std::cout << "Message " << args->messageID << " confirmed! Stopping retries." << std::endl;
                i = args->retry_count;
                args->messageID++;
            } else {
                if (i == args->retry_count) {
                    std::cerr << "Error: Didn't recieve confirmation of a sent message!" << std::endl;
                    exit(1); //TODO messages not confirmed
                }
                //std::cout << "Message " << args->messageID << " not confirmed yet." << std::endl;
            }
        }
        else {
            //std::cout << "Sendind confirm to port: " << ntohs(serverAddr.sin_port) << std::endl;
            return;
        }
    }   
    //TODO nedostali jsme confirm?     

    //args->messageID++;
}

void udp_confirm(ProgramArgs *args, uint16_t refMessageID) {
    uint8_t type = 0x00;
    MessageHeader msgHeader(type, refMessageID);
    //msgHeader.toNetworkByteOrder();

    std::vector<uint8_t> buffer;

    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&msgHeader), 
                  reinterpret_cast<uint8_t*>(&msgHeader) + sizeof(msgHeader));

    //print_buffer(buffer); //TODO debug
    send_UDP(args, buffer);
}

void udp_auth(ProgramArgs *args) {
    uint8_t type = 0x02;
    MessageHeader msgHeader(type, args->messageID);
    
    std::vector<uint8_t> buffer;

    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&msgHeader), 
                  reinterpret_cast<uint8_t*>(&msgHeader) + sizeof(msgHeader));

    buffer.insert(buffer.end(), args->username.begin(), args->username.end());
    buffer.push_back('\0');

    buffer.insert(buffer.end(), args->displayName.begin(), args->displayName.end());
    buffer.push_back('\0');

    buffer.insert(buffer.end(), args->secret.begin(), args->secret.end());
    buffer.push_back('\0');

    //print_buffer(buffer); //TODO debug
    args->state = "auth";
    uint16_t messageID = args->messageID;
    send_UDP(args, buffer);
    udp_check_reply(args, messageID);
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

    //print_buffer(buffer); //TODO debug
    args->state = "join";
    uint16_t messageID = args->messageID;
    send_UDP(args, buffer);
    udp_check_reply(args, messageID);
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

    //print_buffer(buffer); //TODO debug
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

    //print_buffer(buffer); //TODO debug
    args->state = "end"; //TODO asi?
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

    //print_buffer(buffer); //TODO debug
    args->state = "end";
    send_UDP(args, buffer);
}

void udp_ping(ProgramArgs *args) {
    uint8_t type = 0xFD;
    MessageHeader msgHeader(type, args->messageID);

    std::vector<uint8_t> buffer;

    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&msgHeader), 
                  reinterpret_cast<uint8_t*>(&msgHeader) + sizeof(msgHeader));

    //print_buffer(buffer); //TODO debug
    send_UDP(args, buffer);
}

void* udp_listen(void* arg) {
    ProgramArgs* args = static_cast<ProgramArgs*>(arg);
    struct sockaddr_in client_addr;
    client_addr.sin_addr.s_addr = INADDR_ANY; 
    socklen_t addr_len = sizeof(client_addr);
    uint8_t buffer[1024];

    //std::cout << "UDP Listener started on port " << args->port << "\n";

    while (args->state != "end") {
        ssize_t recv_len = recvfrom(args->sockfd, buffer, sizeof(buffer) - 1, 0, 
                                    (struct sockaddr *)&client_addr, &addr_len);
            if (recv_len > 0) {
            buffer[recv_len] = '\0'; // Ensure null termination

            uint8_t type = buffer[0];
            uint16_t MessageID = *(uint16_t *)(buffer + 1);

            switch (type) {
                case 0x01: { // Reply
                    uint8_t result = buffer[3]; //TODO never receive reply :(
                    uint16_t refMessageID = *(uint16_t *)(buffer + 4);


                    char* ptr = reinterpret_cast<char*>(buffer + 6);
                    std::string messageContent(ptr);
                    
                    //std::cout << "Incoming port: " << ntohs(client_addr.sin_port) << std::endl;
                    if(result)
                        args->port = ntohs(client_addr.sin_port);

                    std::cout << "Action " << (result ? "Success: " : "Failure: ") << messageContent << std::endl;
                    if(args->state == "auth" && result) {
                        args->state = "open";
                    }
                    else if(args->state == "join") {
                        args->state = "open";
                    }
                    else if(args->state == "open") {
                        args->state = "end";
                    }

                    if (std::find(args->messageReplysID.begin(), args->messageReplysID.end(), refMessageID) == args->messageReplysID.end()) {
                        args->messageReplysID.push_back(refMessageID);
                    }

                    udp_confirm(args, MessageID);
                    break;
                }
                case 0x04: { // Message
                    char* ptr = reinterpret_cast<char*>(buffer + 3);
                    std::string displayName(ptr);
                    ptr += displayName.size() + 1;

                    std::string messageContents(ptr);
                    std::cout << displayName << ": " << messageContents << std::endl;

                    udp_confirm(args, MessageID);

                    if (args->state == "auth") {
                        udp_err(args, "error"); //TODO error message -> exit
                        args->state = "end";
                    }
                    break;
                }
                case 0xFD: {
                    //std::cout << "ping received" << std::endl;
                    udp_confirm(args, MessageID);
                    break;
                }
                case 0xFE: { // Error
                    char* ptr = reinterpret_cast<char*>(buffer + 3);
                    std::string displayName(ptr);
                    ptr += displayName.size() + 1;

                    std::string messageContents(ptr);
                    std::cout << "ERROR FROM " << displayName << ": " << messageContents << std::endl;

                    udp_confirm(args, MessageID);
                    args->state = "end";
                    break;
                }
                case 0xFF: { // Bye
                    //std::cout << "Bye received" << std::endl;
                    
                    udp_confirm(args, MessageID);
                    args->state = "end";
                    break;
                }
                case 0x00: { // Confirmation
                    //std::cout << "Confirm received for MessageID: " << MessageID << std::endl;
                    if (std::find(args->seenMessagesID.begin(), args->seenMessagesID.end(), MessageID) == args->seenMessagesID.end()) {
                        args->seenMessagesID.push_back(MessageID);
                    }
                    break;
                }
                default:
                    break;
            }
        } else if (recv_len < 0) {
            std::cerr << "Error receiving UDP message\n";
        }
    }

    //std::cout << "UDP Listener shutting down...\n";
    return nullptr;
}
