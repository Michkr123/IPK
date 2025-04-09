// TCPChatClient.cpp
#include "TCPChatClient.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <chrono>
#include <thread>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstdlib>

// Constructor
TCPChatClient::TCPChatClient(const std::string &hostname, uint16_t port)
    : ChatClient(hostname, port), replyReceived_(false)
{
    // Create a TCP socket.
    sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd_ < 0) {
        perror("TCP socket creation failed");
        exit(1);
    }
}

// Destructor: socket is closed in the base class destructor.
TCPChatClient::~TCPChatClient() {
    // Nothing additional required.
}

// Connect to the server using TCP.
void TCPChatClient::connectToServer() {
    if (connect(sockfd_, (struct sockaddr*)&serverAddr_, sizeof(serverAddr_)) < 0) {
        std::cerr << "Connection failed!" << std::endl;
        exit(1);
    }
}

// Helper: Sends a TCP message.
void TCPChatClient::sendTCP(const std::string &message) {
    ssize_t bytes_sent = send(sockfd_, message.c_str(), message.length(), 0);
    if (bytes_sent < 0) {
        std::cerr << "Error: Could not send message" << std::endl;
        close(sockfd_);
        exit(1);
    }
}

// Helper: Forks a process to check for a reply within 5 seconds.
void TCPChatClient::checkReply() {
    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "Failed to create child process in checkReply!" << std::endl;
        exit(1);
    } else if (pid == 0) {
        // In the child process, wait 5 seconds.
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        if (!replyReceived_) {
            std::cerr << "Error: no reply was received!" << std::endl;
            sendError("Didn't receive a reply message!");
            setState("end");
        }
        _exit(0);
    }
    // Parent process continues without waiting.
}

// Listens for incoming TCP messages.
// It gathers data until it finds "\r\n", then processes complete lines.
void TCPChatClient::listen() {
    char buffer[1024];
    std::string messageBuffer;

    while (getState() != "end") {
        ssize_t bytes_received = recv(sockfd_, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received < 0) {
            std::cerr << "Error: Could not receive message" << std::endl;
            break;
        } else if (bytes_received == 0) {
            std::cerr << "Server closed the connection" << std::endl;
            break;
        }

        // Null-terminate the incoming data and append to messageBuffer.
        buffer[bytes_received] = '\0';
        messageBuffer += buffer;

        // Process complete messages separated by "\r\n".
        size_t pos;
        while ((pos = messageBuffer.find("\r\n")) != std::string::npos && getState() != "end") {
            std::string message = messageBuffer.substr(0, pos);
            messageBuffer.erase(0, pos + 2);

            std::istringstream iss(message);
            std::string command;
            iss >> command;

            if (command == "REPLY") {
                std::string result, is;
                std::string content;
                iss >> result >> is;
                std::getline(iss, content);
                if (result == "OK") {
                    std::cout << "Success: " << content << std::endl;
                } else {
                    std::cout << "Failure: " << content << std::endl;
                }

                // Update state based on command
                if (getState() == "join") {
                    setState("open");
                } else if (getState() == "open") {
                    setState("end");
                } else if (getState() == "auth" && result == "OK") {
                    setState("open");
                }
                // Mark that a reply was received.
                replyReceived_ = true;
            }
            else if (command == "MSG") {
                std::string from, displayName, is;
                std::string content;
                iss >> from >> displayName >> is;
                std::getline(iss, content);
                std::cout << displayName << ": " << content << std::endl;
                if (getState() == "auth") {
                    setState("end");
                }
            }
            else if (command == "ERR") {
                std::string from, displayName, is;
                std::string content;
                iss >> from >> displayName >> is;
                std::getline(iss, content);
                std::cerr << "Error from " << displayName << ": " << content << std::endl;
                setState("end");
            }
            else if (command == "BYE") {
                std::string from, displayName;
                iss >> from >> displayName;
                std::cout << "Server ended the session from " << displayName << "." << std::endl;
                setState("end");
            }
            else {
                std::cout << "Unknown message: " << message << std::endl;
            }
        }
    }
    close(sockfd_);
}

// Authenticate command.
void TCPChatClient::auth(const std::string &username,
                           const std::string &secret,
                           const std::string &displayName) 
{
    setState("auth");
    displayName_ = displayName;
    // Build the AUTH message in the form: "AUTH username AS displayName USING secret\r\n"
    std::string message = "AUTH " + username + " AS " + displayName + " USING " + secret + "\r\n";
    sendTCP(message);
    // Initialize replyReceived_ flag to false before checking for reply.
    replyReceived_ = false;
    checkReply();
}

// Join channel command.
void TCPChatClient::joinChannel(const std::string &channel) {
    setState("join");
    // Build the JOIN message: "JOIN channel AS displayName\r\n"
    // For simplicity, we assume displayName is stored in ChatClient::state_
    std::string message = "JOIN " + channel + " AS " + "User" + "\r\n"; // Adjust displayName as needed.
    sendTCP(message);
    replyReceived_ = false;
    checkReply();
}

// Send a regular message.
void TCPChatClient::sendMessage(const std::string &messageContent) {
    // Replace "displayName_" with the actual member variable holding the user display name.
    std::string message = std::string("MSG FROM ") + displayName_ + " IS " + messageContent + "\r\n";
    sendTCP(message);
}

// Send an error message.
void TCPChatClient::sendError(const std::string &error) {
    setState("end");
    std::string message = std::string("ERR FROM ") + displayName_ + " IS " + error + "\r\n";
    sendTCP(message);
}

// Send BYE to terminate the session.
void TCPChatClient::bye() {
    setState("end");
    std::string message = std::string("BYE FROM ") + displayName_ + "\r\n";
    sendTCP(message);
}


// Send a ping message.
void TCPChatClient::ping() {
    // Build a PING message if needed (you can customize this according to protocol).
    std::string message = "PING\r\n";
    sendTCP(message);
}
