#include "ChatClient.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <cstring>
#include <iostream>
#include <cstdlib>
#include <unistd.h>

// Constructor: resolves the hostname and initializes the server address structure.
ChatClient::ChatClient(const std::string &hostname, uint16_t port)
    : hostname_(hostname), port_(port), sockfd_(-1), state_("start"), messageID_(0)
{
    // Setup hints for getaddrinfo.
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // IPv4

    // Resolve the hostname.
    if(getaddrinfo(hostname_.c_str(), nullptr, &hints, &res) != 0) {
        std::cerr << "Error: Unable to resolve hostname " << hostname_ << std::endl;
        exit(1);
    }
    
    // Copy the resolved IP address into serverAddr_.
    struct sockaddr_in* addr = reinterpret_cast<struct sockaddr_in*>(res->ai_addr);
    serverAddr_.sin_family = AF_INET;
    serverAddr_.sin_addr   = addr->sin_addr;
    serverAddr_.sin_port   = htons(port_);
    
    freeaddrinfo(res);
}

// Virtual destructor: cleans up resources.
ChatClient::~ChatClient() {
    if(sockfd_ != -1) {
        close(sockfd_);
    }
}

// Returns the current connection state.
std::string ChatClient::getState() const {
    return state_;
}

// Sets the current connection state.
void ChatClient::setState(const std::string &newState) {
    state_ = newState;
}

// Returns the next unique message ID (pre-increment).
uint16_t ChatClient::getNextMessageID() {
    return messageID_++;
}

void ChatClient::rename(const std::string &displayName) {
    displayName_ = displayName;
}

// Starts the listener in a new thread.
// The actual implementation of listen() is provided in the subclass.
void ChatClient::startListener() {
    std::thread listener([this]() {
        this->listen();
    });
    listener.detach();
}
