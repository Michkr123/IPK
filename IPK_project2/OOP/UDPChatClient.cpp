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
UDPChatClient::UDPChatClient(const std::string &hostname, uint16_t port,  int retry_count, int timeout)
    : ChatClient(hostname, port), 
      retry_count_(retry_count), 
      timeout_(timeout)
{
    // Create a UDP socket.
    sockfd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd_ < 0) {
        std::cout << "ERROR: Socket creation failed." << std::endl;
        exit(1);
    }

    // Allocate shared memory for reply IDs.
    replyIDs_ = static_cast<SharedIDs*>(mmap(nullptr, sizeof(SharedIDs),
                                PROT_READ | PROT_WRITE,
                                MAP_SHARED | MAP_ANONYMOUS, -1, 0));
    if (replyIDs_ == MAP_FAILED) {
        std::cout << "ERROR: mmap failed for replyIDs." << std::endl;
        exit(1);
    }
    replyIDs_->count = 0;

    // Allocate shared memory for seen IDs.
    confirmIDs_ = static_cast<SharedIDs*>(mmap(nullptr, sizeof(SharedIDs),
                                PROT_READ | PROT_WRITE,
                                MAP_SHARED | MAP_ANONYMOUS, -1, 0));
    if (confirmIDs_ == MAP_FAILED) {
        std::cout << "ERROR: mmap failed for confirmIDs." << std::endl;
        exit(1);
    }
    confirmIDs_->count = 0;
}

UDPChatClient::~UDPChatClient() {
    // Unmap the shared memory regions.
    munmap(replyIDs_, sizeof(SharedIDs));
    munmap(confirmIDs_, sizeof(SharedIDs));
    // The socket is closed in the ChatClient destructor.
}

////////////////////////
// Overridden Virtual Methods
////////////////////////
void UDPChatClient::connectToServer() {
    // For UDP, no explicit connection is needed; the OS automatically handles binding.
}

void UDPChatClient::auth(const std::string &username,
                           const std::string &secret,
                           const std::string &displayName) 
{
    setState("auth");
    displayName_ = displayName;
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

    buffer.insert(buffer.end(), displayName_.begin(), displayName_.end());
    buffer.push_back('\0');
    
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
    
    // Insert the display name (stored in displayName_) followed by a null terminator.
    buffer.insert(buffer.end(), displayName_.begin(), displayName_.end());
    buffer.push_back('\0');
    
    // Append the actual message content, followed by a null terminator.
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
    
    buffer.insert(buffer.end(), displayName_.begin(), displayName_.end());
    buffer.push_back('\0');

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
    
    buffer.insert(buffer.end(), displayName_.begin(), displayName_.end());
    buffer.push_back('\0');

    sendUDP(buffer);
}

////////////////////////
// Private Helper Methods
////////////////////////
void UDPChatClient::sendUDP(const std::vector<uint8_t> &buffer) {
    // Extract the message ID from the header.
    // Our header layout: byte 0 = type, bytes 1-2 = messageID.
    uint8_t type = *reinterpret_cast<const uint8_t*>(buffer.data());
    uint16_t msgID = *reinterpret_cast<const uint16_t*>(buffer.data() + 1);
    
    int attempts = 0;
    bool confirmed = false;
    
    while (attempts <= retry_count_ && !confirmed) {
        // Send the UDP packet.
        struct sockaddr_in addr = serverAddr_;
        ssize_t sentBytes = sendto(sockfd_, buffer.data(), buffer.size(), 0,
                                   reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
        if (sentBytes < 0) {
            std::cout << "ERROR: Error sending UDP packet" << std::endl;
        }
        if(type == 0x00) 
            return;
        
        // Sleep for the full timeout period (without intermediate checks).
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout_));
        
        // Also check the confirmIDs_ array (confirmation messages may be stored there).
        if (!confirmed) {
            for (size_t i = 0; i < confirmIDs_->count; i++) {
                if (confirmIDs_->ids[i] == msgID) {
                    confirmed = true;
                    break;
                }
            }
        }
    }
    
    if (!confirmed) {
        std::cout << "ERROR: No confirmation received." << std::endl;
        sendError("No reply was received.");
        setState("end");
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
        std::cout << "ERROR: Failed to create child process!" << std::endl;
        exit(1);
    } else if (pid == 0) {
        // In the child process, wait 5 seconds.
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        bool found = false;
        for (size_t i = 0; i < replyIDs_->count; i++) {
            if (replyIDs_->ids[i] == messageID) {
                found = true;

                break;
            }
        }
        if (!found && getState() != "end") {
            std::cout << "Error: Didn't receive reply." << std::endl;
            sendError("No reply was received!");
            setState("end");
        }
        _exit(0);
    }
    // Parent process continues.
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
                                   reinterpret_cast<struct sockaddr*>(&clientAddr), &addrLen);
        if (recvLen > 0) {
            buffer[recvLen] = '\0'; // Null-terminate the buffer

            // Immediately update server port if different.
            if (serverAddr_.sin_port != clientAddr.sin_port)
                serverAddr_.sin_port = clientAddr.sin_port;

            // The first byte is the message type.
            uint8_t msgType = buffer[0];
            // The next 2 bytes represent the message ID.
            uint16_t recvMessageID = *(reinterpret_cast<uint16_t*>(buffer + 1));
            
            switch (msgType) {
                case 0x01: { // Reply message
                    uint8_t result = buffer[3]; // Result flag.
                    uint16_t refMessageID = *(reinterpret_cast<uint16_t*>(buffer + 4));

                    char* p = reinterpret_cast<char*>(buffer + 6);
                    std::string replyText(p);
                    
                    std::cout << "Action " 
                              << (result ? "Success: " : "Failure: ") 
                              << replyText << std::endl;
                    
                    // Update state.
                    if ((getState() == "auth" && result) || getState() == "join")
                        setState("open");
                    else if (getState() == "open")
                        setState("end");

                    // Store the reply's reference message ID in replyIDs_ if not already stored.
                    bool exists = false;
                    for (size_t i = 0; i < replyIDs_->count; i++) {
                        if (replyIDs_->ids[i] == refMessageID) {
                            exists = true;
                            break;
                        }
                    }
                    if (!exists) {
                        if (replyIDs_->count < MAX_MESSAGE_IDS) {
                            replyIDs_->ids[replyIDs_->count++] = refMessageID;
                        } else {
                            std::cout << "ERROR: Reply ID array is full!" << std::endl;
                            setState("end");
                        }
                    }
                    
                    // Send confirmation using the header message ID.
                    confirm(recvMessageID);
                    break;
                }
                case 0x04: { // Regular chat message
                    // Starting at byte 3: first field is display name.
                    char* p = reinterpret_cast<char*>(buffer + 3);
                    std::string dispName(p);
                    p += dispName.size() + 1; // Advance pointer to skip display name and its null.
                    // Next field is the message content.
                    std::string message(p);
                    std::cout << dispName << ": " << message << std::endl;
                    confirm(recvMessageID);
                    break;
                }
                case 0xFD: { // Ping
                    confirm(recvMessageID);
                    break;
                }
                case 0xFE: { // Error
                    // Starting at byte 3: first field is display name.
                    char* p = reinterpret_cast<char*>(buffer + 3);
                    std::string dispName(p);
                    p += dispName.size() + 1; // Skip display name.
                    // Next field is the error message.
                    std::string errorMsg(p);
                    std::cout << "ERROR FROM " << dispName << ": " << errorMsg << std::endl;
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
                    // For confirmation messages, we record the message ID in the seenIDs_ array.
                    bool exists = false;
                    for (size_t i = 0; i < confirmIDs_->count; i++) {
                        if (confirmIDs_->ids[i] == recvMessageID) {
                            exists = true;
                            break;
                        }
                    }
                    if (!exists) {
                        if (confirmIDs_->count < MAX_MESSAGE_IDS) {
                            confirmIDs_->ids[confirmIDs_->count++] = recvMessageID;
                        } else {
                            std::cout << "ERROR: confirm messages array is full!" << std::endl;
                            setState("end");
                        }
                    }
                    break;
                }
                default:
                    std::cout << "ERROR: malformed message received." << std::endl;
                    setState("end");
                    break;
            }
        } else if (recvLen < 0) {
            std::cout << "ERROR: Error receiving UDP message." << std::endl;
        }
    }
}
