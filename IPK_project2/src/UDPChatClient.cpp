#include "UDPChatClient.h"

#pragma pack(push, 1)
struct MessageHeader {
    uint8_t type;
    uint16_t messageID;
    MessageHeader(uint8_t t, uint16_t id) : type(t), messageID(htons(id)) {}
};
#pragma pack(pop)

UDPChatClient::UDPChatClient(const std::string &hostname, uint16_t port,  int retry_count, int timeout)
    : ChatClient(hostname, port), 
      retry_count_(retry_count), 
      timeout_(timeout)
{
    sockfd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd_ < 0) {
        std::cout << "ERROR: Socket creation failed." << std::endl;
        exit(1);
    }
}

UDPChatClient::~UDPChatClient() {
}

void UDPChatClient::connectToServer() {
}

void UDPChatClient::auth(const std::string &username,
                           const std::string &secret,
                           const std::string &displayName) {
    displayName_ = displayName;
    uint16_t msgID = getNextMessageID();
    uint8_t type = TYPE_AUTH;

    MessageHeader header(type, msgID);
    std::vector<uint8_t> buffer(reinterpret_cast<uint8_t*>(&header),
                                reinterpret_cast<uint8_t*>(&header) + sizeof(header));

    buffer.insert(buffer.end(), username.begin(), username.end());
    buffer.push_back('\0');

    buffer.insert(buffer.end(), displayName.begin(), displayName.end());
    buffer.push_back('\0');

    buffer.insert(buffer.end(), secret.begin(), secret.end());
    buffer.push_back('\0');

    setState("auth");
    sendUDP(buffer);
    checkReply(msgID);
}

void UDPChatClient::joinChannel(const std::string &channel) {
    uint16_t msgID = getNextMessageID();
    uint8_t type = TYPE_JOIN;

    MessageHeader header(type, msgID);
    std::vector<uint8_t> buffer(reinterpret_cast<uint8_t*>(&header),
                                reinterpret_cast<uint8_t*>(&header) + sizeof(header));

    buffer.insert(buffer.end(), channel.begin(), channel.end());
    buffer.push_back('\0');

    buffer.insert(buffer.end(), displayName_.begin(), displayName_.end());
    buffer.push_back('\0');
    
    setState("join");
    sendUDP(buffer);
    checkReply(msgID);
}

void UDPChatClient::sendMessage(const std::string &message) {
    uint16_t msgID = getNextMessageID();
    uint8_t type = TYPE_MSG;

    MessageHeader header(type, msgID);
    std::vector<uint8_t> buffer(reinterpret_cast<uint8_t*>(&header),
                                reinterpret_cast<uint8_t*>(&header) + sizeof(header));
    
    buffer.insert(buffer.end(), displayName_.begin(), displayName_.end());
    buffer.push_back('\0');
    
    buffer.insert(buffer.end(), message.begin(), message.end());
    buffer.push_back('\0');

    sendUDP(buffer);
}

void UDPChatClient::sendError(const std::string &error) {
    uint16_t msgID = getNextMessageID();
    uint8_t type = TYPE_ERR;

    MessageHeader header(type, msgID);
    std::vector<uint8_t> buffer(reinterpret_cast<uint8_t*>(&header),
                                reinterpret_cast<uint8_t*>(&header) + sizeof(header));
    
    buffer.insert(buffer.end(), displayName_.begin(), displayName_.end());
    buffer.push_back('\0');

    buffer.insert(buffer.end(), error.begin(), error.end());
    buffer.push_back('\0');

    sendUDP(buffer);
    setState("end");
}

void UDPChatClient::bye() {
    uint16_t msgID = getNextMessageID();
    
    uint8_t type = TYPE_BYE;
    MessageHeader header(type, msgID);
    std::vector<uint8_t> buffer(reinterpret_cast<uint8_t*>(&header),
                                reinterpret_cast<uint8_t*>(&header) + sizeof(header));
    
    if (displayName_ != "") {
        buffer.insert(buffer.end(), displayName_.begin(), displayName_.end());
    }
    else {
        std::string unknown = "unknown";
        buffer.insert(buffer.end(), unknown.begin(), unknown.end());
    }
    buffer.push_back('\0');

    sendUDP(buffer);
    setState("end");
}


void UDPChatClient::sendUDP(const std::vector<uint8_t> &buffer) {
    uint8_t type = *reinterpret_cast<const uint8_t*>(buffer.data());
    uint16_t msgID = ntohs(*reinterpret_cast<const uint16_t*>(buffer.data() + 1));
    
    int attempts = 0;
    
    while (attempts <= retry_count_) {
        struct sockaddr_in addr = serverAddr_;
        ssize_t sentBytes = sendto(sockfd_, buffer.data(), buffer.size(), 0,
                                   reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
        if (sentBytes < 0) {
            if (type == TYPE_ERR) {
                setState("end");
                exit(0);
            }
            std::cout << "ERROR: Error sending UDP packet" << std::endl;
        }
        if (type == TYPE_CONFIRM) 
            return;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout_));
        if (std::find(confirmIDs_.begin(), confirmIDs_.end(), msgID) != confirmIDs_.end()) {
           return;
        }
        else {
            attempts++;
        }
    }
    
    std::cout << "ERROR: No confirmation received." << std::endl;
    setState("end");
}

void UDPChatClient::confirm(uint16_t refMessageID) {
    uint8_t type = TYPE_CONFIRM;

    MessageHeader header(type, refMessageID);
    std::vector<uint8_t> buffer(reinterpret_cast<uint8_t*>(&header),
                                reinterpret_cast<uint8_t*>(&header) + sizeof(header));
    sendUDP(buffer);
}

void UDPChatClient::checkReply(uint16_t messageID) {
    auto startTime = std::chrono::steady_clock::now();
    const int timeoutMs = 5000;
    bool found = false;

    while (true) {
        if (std::find(replyIDs_.begin(), replyIDs_.end(), messageID) != replyIDs_.end()) {
            found = true;
            break;
        }
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count();
        if (elapsed >= timeoutMs) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    if (!found && getState() != "end") {
        std::cout << "Error: No reply was received." << std::endl;
        sendError("No reply was received.");
        setState("end");
    }
}

void UDPChatClient::listen() {
    struct sockaddr_in clientAddr;
    socklen_t addrLen = sizeof(clientAddr);
    uint8_t buffer[1024];

    while (getState() != "end") {
        ssize_t recvLen = recvfrom(sockfd_, buffer, sizeof(buffer) - 1, 0,
                                   reinterpret_cast<struct sockaddr*>(&clientAddr), &addrLen);
        if (recvLen > 0) {
                buffer[recvLen] = '\0';

                if (serverAddr_.sin_port != clientAddr.sin_port) {
                    serverAddr_.sin_port = clientAddr.sin_port;
                }

                uint8_t msgType = buffer[0];
                uint16_t recvMessageID = ntohs(*(reinterpret_cast<uint16_t*>(buffer + 1)));
                
            if (msgType != TYPE_CONFIRM) {
                if (std::find(seenIDs_.begin(), seenIDs_.end(), recvMessageID) != seenIDs_.end()) {
                    confirm(recvMessageID);
                    continue;
                }
                else {
                    seenIDs_.push_back(recvMessageID);
                }
            } 
            
            switch (msgType) {
                case TYPE_REPLY: {
                    uint8_t result = buffer[3];
                    uint16_t refMessageID = ntohs(*(reinterpret_cast<uint16_t*>(buffer + 4)));

                    char* p = reinterpret_cast<char*>(buffer + 6);
                    std::string replyText(p);
                    
                    std::cout << "Action " 
                            << (result ? "Success: " : "Failure: ") 
                            << replyText << std::endl;
                    
                    if ((getState() == "auth" && result) || getState() == "join") {
                        setState("open");
                    }
                    else if (getState() == "open") { 
                        setState("end");
                    }

                    if (std::find(replyIDs_.begin(), replyIDs_.end(), refMessageID) == replyIDs_.end()) {
                        replyIDs_.push_back(refMessageID);
                    }
                    
                    confirm(recvMessageID);
                    break;
                }
                case TYPE_MSG: {
                    if (getState() != "auth") {
                        char* p = reinterpret_cast<char*>(buffer + 3);
                        std::string displayName(p);

                        p += displayName.size() + 1;
                        std::string message(p);

                        std::cout << displayName << ": " << message << std::endl;
                        confirm(recvMessageID);
                    }
                    else {
                        sendError("Received message in auth state!");
                        setState("end");
                    }
                    break;
                }
                case TYPE_PING: {
                    confirm(recvMessageID);
                    break;
                }
                case TYPE_ERR: {
                    char* p = reinterpret_cast<char*>(buffer + 3);
                    std::string displayName(p);

                    p += displayName.size() + 1;
                    std::string errorMsg(p);

                    std::cout << "ERROR FROM " << displayName << ": " << errorMsg << std::endl;
                    confirm(recvMessageID);
                    exit(1);
                }
                case TYPE_BYE: {
                    confirm(recvMessageID);
                    setState("end");
                    break;
                }
                case TYPE_CONFIRM: {
                    if (std::find(confirmIDs_.begin(), confirmIDs_.end(), recvMessageID) == confirmIDs_.end()) {
                        confirmIDs_.push_back(recvMessageID);
                    }
                    break;
                }
                default:
                    confirm(recvMessageID);
                    std::cout << "ERROR: malformed message received." << std::endl;
                    sendError("Received malformed message!");
                    setState("end");
                    break;
            }
        } else if (recvLen < 0) {
            std::cout << "ERROR: Error receiving UDP message." << std::endl;
        }
    }
    exit(0);
}