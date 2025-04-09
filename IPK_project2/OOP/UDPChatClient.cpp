#include "UDPChatClient.h"
#include <iostream>
#include <vector>
#include <cstring>
#include <chrono>
#include <thread>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdlib>
#include <algorithm>

// Pack structure to ensure no padding in header.
#pragma pack(push, 1)
struct MessageHeader {
    uint8_t type;
    uint16_t messageID;
    MessageHeader(uint8_t t, uint16_t id) : type(t), messageID(id) {}
};
#pragma pack(pop)

////////////////////////
// Constructor & Destructor
////////////////////////
UDPChatClient::UDPChatClient(const std::string &hostname, uint16_t port)
    : ChatClient(hostname, port)
{
    // Create a UDP socket.
    sockfd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd_ < 0) {
        perror("Socket creation failed");
        exit(1);
    }
}

UDPChatClient::~UDPChatClient() {
    // The socket is closed in the ChatClient destructor.
}

////////////////////////
// Overridden Virtual Methods
////////////////////////
void UDPChatClient::connectToServer() {
    // For UDP, there is no need for an explicit connection; the OS will auto-bind.
}

void UDPChatClient::auth(const std::string &username,
                           const std::string &secret,
                           const std::string &displayName) 
{
    // Set state to "auth" and get a unique message ID.
    setState("auth");
    uint16_t msgID = getNextMessageID();
    
    // Message type 0x02 for authentication.
    uint8_t type = 0x02;
    MessageHeader header(type, msgID);
    std::vector<uint8_t> buffer(reinterpret_cast<uint8_t*>(&header),
                                reinterpret_cast<uint8_t*>(&header) + sizeof(header));
    // Append username followed by a null terminator.
    buffer.insert(buffer.end(), username.begin(), username.end());
    buffer.push_back('\0');
    // Append display name.
    buffer.insert(buffer.end(), displayName.begin(), displayName.end());
    buffer.push_back('\0');
    // Append secret (password) followed by a null terminator.
    buffer.insert(buffer.end(), secret.begin(), secret.end());
    buffer.push_back('\0');

    sendUDP(buffer);
    checkReply(msgID);
}

void UDPChatClient::joinChannel(const std::string &channel) {
    // Set state to "join" and get a new message ID.
    setState("join");
    uint16_t msgID = getNextMessageID();
    
    // Message type 0x03 for joining a channel.
    uint8_t type = 0x03;
    MessageHeader header(type, msgID);
    std::vector<uint8_t> buffer(reinterpret_cast<uint8_t*>(&header),
                                reinterpret_cast<uint8_t*>(&header) + sizeof(header));
    // Append channel name followed by a null terminator.
    buffer.insert(buffer.end(), channel.begin(), channel.end());
    buffer.push_back('\0');
    
    // You may also wish to append a display name here.
    sendUDP(buffer);
    checkReply(msgID);
}

void UDPChatClient::sendMessage(const std::string &message) {
    uint16_t msgID = getNextMessageID();
    
    // Message type 0x04 for a standard message.
    uint8_t type = 0x04;
    MessageHeader header(type, msgID);
    std::vector<uint8_t> buffer(reinterpret_cast<uint8_t*>(&header),
                                reinterpret_cast<uint8_t*>(&header) + sizeof(header));
    // Append the message content followed by a null terminator.
    buffer.insert(buffer.end(), message.begin(), message.end());
    buffer.push_back('\0');

    sendUDP(buffer);
}

void UDPChatClient::sendError(const std::string &error) {
    setState("end");
    uint16_t msgID = getNextMessageID();
    
    // Message type 0xFE for error.
    uint8_t type = 0xFE;
    MessageHeader header(type, msgID);
    std::vector<uint8_t> buffer(reinterpret_cast<uint8_t*>(&header),
                                reinterpret_cast<uint8_t*>(&header) + sizeof(header));
    // Append error message (null terminated).
    buffer.insert(buffer.end(), error.begin(), error.end());
    buffer.push_back('\0');

    sendUDP(buffer);
}

void UDPChatClient::bye() {
    setState("end");
    uint16_t msgID = getNextMessageID();
    
    // Message type 0xFF for BYE.
    uint8_t type = 0xFF;
    MessageHeader header(type, msgID);
    std::vector<uint8_t> buffer(reinterpret_cast<uint8_t*>(&header),
                                reinterpret_cast<uint8_t*>(&header) + sizeof(header));
    sendUDP(buffer);
}

void UDPChatClient::ping() {
    uint16_t msgID = getNextMessageID();
    
    // Message type 0xFD for ping.
    uint8_t type = 0xFD;
    MessageHeader header(type, msgID);
    std::vector<uint8_t> buffer(reinterpret_cast<uint8_t*>(&header),
                                reinterpret_cast<uint8_t*>(&header) + sizeof(header));
    sendUDP(buffer);
}

////////////////////////
// Private Helper Methods
////////////////////////
void UDPChatClient::sendUDP(const std::vector<uint8_t> &buffer) {
    // Prepare the server address from the stored serverAddr_.
    struct sockaddr_in addr = serverAddr_;
    ssize_t sentBytes = sendto(sockfd_, buffer.data(), buffer.size(), 0,
                               (struct sockaddr *)&addr, sizeof(addr));
    if (sentBytes < 0) {
        perror("Error sending UDP packet");
    }
}

void UDPChatClient::confirm(uint16_t refMessageID) {
    // Build a confirmation message (type 0x00).
    uint8_t type = 0x00;
    MessageHeader header(type, refMessageID);
    std::vector<uint8_t> buffer(reinterpret_cast<uint8_t*>(&header),
                                reinterpret_cast<uint8_t*>(&header) + sizeof(header));
    sendUDP(buffer);
}

void UDPChatClient::checkReply(uint16_t messageID) {
    // Fork a child process to wait for a reply with a timeout.
    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "Failed to create child process in checkReply!" << std::endl;
        exit(1);
    } else if (pid == 0) {
        // In the child process, wait 5 seconds.
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        // Check if the reply corresponding to messageID has been received.
        if (std::find(messageRepliesID_.begin(), messageRepliesID_.end(), messageID) == messageRepliesID_.end()) {
            if (getState() != "end") {
                std::cerr << "Error: Didn't receive reply for message ID " << messageID << std::endl;
                // Use sendError to notify the error.
                sendError("No reply was received!");
                setState("end");
            }
        }
        _exit(0);
    }
    // The parent process continues normally.
}

////////////////////////
// Listener: Receives and processes incoming UDP messages.
////////////////////////
void UDPChatClient::listen() {
    struct sockaddr_in clientAddr;
    socklen_t addrLen = sizeof(clientAddr);
    uint8_t buffer[1024];

    while (getState() != "end") {
        ssize_t recvLen = recvfrom(sockfd_, buffer, sizeof(buffer) - 1, 0,
                                   (struct sockaddr*)&clientAddr, &addrLen);
        if (recvLen > 0) {
            // Null-terminate the buffer.
            buffer[recvLen] = '\0';
            // The first byte is the message type.
            uint8_t type = buffer[0];
            // The next 2 bytes represent the message ID.
            uint16_t recvMessageID = *(reinterpret_cast<uint16_t*>(buffer + 1));
            
            // Process the received message based on its type.
            switch (type) {
                case 0x01: { // Reply
                    uint8_t result = buffer[3]; // Result flag.
                    uint16_t refMessageID = *(reinterpret_cast<uint16_t*>(buffer + 4));
                    // Message content starts at byte 6.
                    char* ptr = reinterpret_cast<char*>(buffer + 6);
                    std::string msgContent(ptr);

                    std::cout << "Reply: " << (result ? "Success: " : "Failure: ") << msgContent << std::endl;
                    
                    // Update state if necessary.
                    if (getState() == "auth" && result)
                        setState("open");
                    else if (getState() == "join")
                        setState("open");
                    else if (getState() == "open")
                        setState("end");

                    // Record the reply if not already present.
                    if (std::find(messageRepliesID_.begin(), messageRepliesID_.end(), refMessageID) == messageRepliesID_.end()) {
                        std::cout << "Pushing ID " << refMessageID << std::endl;
                        messageRepliesID_.push_back(refMessageID);
                    }
                    
                    // Send confirmation.
                    confirm(recvMessageID);
                    break;
                }
                case 0x04: { // Regular message
                    char* ptr = reinterpret_cast<char*>(buffer + 3);
                    std::string message(ptr);
                    std::cout << "Message: " << message << std::endl;
                    confirm(recvMessageID);
                    break;
                }
                case 0xFD: { // Ping
                    confirm(recvMessageID);
                    break;
                }
                case 0xFE: { // Error
                    char* ptr = reinterpret_cast<char*>(buffer + 3);
                    std::string errorMsg(ptr);
                    std::cerr << "Error: " << errorMsg << std::endl;
                    confirm(recvMessageID);
                    setState("end");
                    break;
                }
                case 0xFF: { // BYE
                    confirm(recvMessageID);
                    setState("end");
                    break;
                }
                case 0x00: { // Confirmation
                    // Add the confirmation messageID to seen messages.
                    if (std::find(seenMessagesID_.begin(), seenMessagesID_.end(), recvMessageID) == seenMessagesID_.end()) {
                        seenMessagesID_.push_back(recvMessageID);
                    }
                    break;
                }
                default:
                    std::cout << "Unknown message type received." << std::endl;
                    break;
            }
        } else if (recvLen < 0) {
            perror("Error receiving UDP message");
        }
    }
}
