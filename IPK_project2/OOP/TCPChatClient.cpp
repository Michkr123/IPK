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
    : ChatClient(hostname, port)
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
        std::cerr << "ERROR: Connection failed!" << std::endl;
        gracefullEnd();
        exit(1);
    }
}

// Helper: Sends a TCP message.
void TCPChatClient::sendTCP(const std::string &message) {
    ssize_t bytes_sent = send(sockfd_, message.c_str(), message.length(), 0);
    if (bytes_sent < 0) {
        std::cerr << "ERROR: Could not send message!" << std::endl;
        gracefullEnd();
        exit(1);
    }
}

// Helper: Forks a process to check for a reply within 5 seconds.
void TCPChatClient::checkReply() {
    replyReceived = false;

    // In the child process, wait 5 seconds.
    std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    if (!replyReceived) {
        std::cerr << "ERROR: no reply was received!" << std::endl;
        sendError("Didn't receive a reply message!");
        setState("end");
    }
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
    replyReceived = false;
    checkReply();
}

// Join channel command.
void TCPChatClient::joinChannel(const std::string &channel) {
    setState("join");
    // Build the JOIN message: "JOIN channel AS displayName\r\n"
    // For simplicity, we assume displayName is stored in ChatClient::state_
    std::string message = "JOIN " + channel + " AS " + displayName_ + "\r\n"; // Adjust displayName as needed.
    sendTCP(message);
    replyReceived = false;
    checkReply();
}

// Send a regular message.
void TCPChatClient::sendMessage(const std::string &messageContent) {
    // Replace "displayName_" with the actual member variable holding the user display name.
    std::string message = "MSG FROM " + displayName_ + " IS " + messageContent + "\r\n";
    sendTCP(message);
}

// Send an error message.
void TCPChatClient::sendError(const std::string &errorContent) {
    setState("end");
    std::string message = std::string("ERR FROM ") + displayName_ + " IS " + errorContent + "\r\n";
    sendTCP(message);
}

// Send BYE to terminate the session.
void TCPChatClient::bye() {
    setState("end");
    std::string message = std::string("BYE FROM ") + displayName_ + "\r\n";
    sendTCP(message);
}

// Listens for incoming TCP messages.
// It gathers data until it finds "\r\n", then processes complete lines.
void TCPChatClient::listen() {
    char buffer[1024];
    std::string messageBuffer;

    while (getState() != "end") {
        ssize_t bytes_received = recv(sockfd_, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received < 0) {
            std::cerr << "ERROR: Could not receive message" << std::endl;
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
                std::cout << "Action " << (result == "OK" ? "Succes: " : "Failure: ") << content << std::endl;
                // if (result == "OK") {
                //     std::cout << "Success: " << content << std::endl;
                // } else {
                //     std::cout << "Failure: " << content << std::endl;
                // }

                // Update state based on command
                if (getState() == "join") {
                    setState("open");
                } else if (getState() == "open") {
                    setState("end");
                } else if (getState() == "auth" && result == "OK") {
                    setState("open");
                }
                // Mark that a reply was received.
                replyReceived = true;
            }
            else if (command == "MSG") {
                if (getState() != "auth") {
                    std::string from, displayName, is;
                    std::string content;
                    iss >> from >> displayName >> is;
                    std::getline(iss, content);
                    std::cout << displayName << ": " << content << std::endl;
                }
                else {
                    sendError("Received message in auth state!");
                    setState("end");
                }
            }
            else if (command == "ERR") {
                std::string from, displayName, is;
                std::string content;
                iss >> from >> displayName >> is;
                std::getline(iss, content);
                std::cerr << "ERROR FROM " << displayName << ": " << content << std::endl;
                setState("end");
            }
            else if (command == "BYE") {
                setState("end");
            }
            else {
                std::cout << "ERROR: malformed message received." << std::endl;
                sendError("Received malformed message!");
                gracefullEnd();
                exit(1);            
            }
        }
    }
    gracefullEnd();
}   