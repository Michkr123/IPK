#include "ChatClient.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <cstring>
#include <iostream>
#include <cstdlib>
#include <unistd.h>

ChatClient::ChatClient(const std::string &hostname, uint16_t port)
    : hostname_(hostname), port_(port), sockfd_(-1), state_("start"), messageID_(0)
{
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;

    if(getaddrinfo(hostname_.c_str(), nullptr, &hints, &res) != 0) {
        std::cerr << "Error: Unable to resolve hostname " << hostname_ << std::endl;
        exit(1);
    }
    
    struct sockaddr_in* addr = reinterpret_cast<struct sockaddr_in*>(res->ai_addr);
    serverAddr_.sin_family = AF_INET;
    serverAddr_.sin_addr   = addr->sin_addr;
    serverAddr_.sin_port   = htons(port_);
    
    freeaddrinfo(res);
}

ChatClient::~ChatClient() {
    if(sockfd_ != -1) {
        close(sockfd_);
        sockfd_ = -1;
    }
}

void ChatClient::gracefullEnd() {
    if(sockfd_ != -1) {
        if (shutdown(sockfd_, SHUT_RDWR) < 0) {
            std::cout << "ERROR: Shutdown failed!" << std::endl;
            exit(1);
        }
        close(sockfd_);
        sockfd_ = -1;
    }
}

std::string ChatClient::getState() const {
    return state_;
}

void ChatClient::setState(const std::string &newState) {
    state_ = newState;
}

uint16_t ChatClient::getNextMessageID() {
    return messageID_++;
}

void ChatClient::rename(const std::string &displayName) {
    displayName_ = displayName;
}

void ChatClient::startListener() {
    std::thread listener([this]() {
        this->listen();
    });
    listener.detach();
}