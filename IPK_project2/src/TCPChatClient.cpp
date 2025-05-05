#include "TCPChatClient.h"


TCPChatClient::TCPChatClient(const std::string &hostname, uint16_t port)
    : ChatClient(hostname, port)
{
    sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd_ < 0) {
        perror("TCP socket creation failed");
        exit(1);
    }
}

TCPChatClient::~TCPChatClient() {
    if (sockfd_ != -1) {
        if (shutdown(sockfd_, SHUT_RDWR) < 0) {
            std::cerr << "ERROR: Shutdown failed!" << std::endl;
        }
        close(sockfd_);
        sockfd_ = -1;
    }
}

void TCPChatClient::connectToServer() {
    if (connect(sockfd_, (struct sockaddr*)&serverAddr_, sizeof(serverAddr_)) < 0) {
        std::cerr << "ERROR: Connection failed!" << std::endl;
        gracefullEnd();
    }
}

void TCPChatClient::sendTCP(const std::string &message) {
    ssize_t bytes_sent = send(sockfd_, message.c_str(), message.length(), 0);
    if (bytes_sent < 0) {
        std::cerr << "ERROR: Could not send message!" << std::endl;
        gracefullEnd();
    }
}

void TCPChatClient::checkReply() {
    replyReceived_ = false;
    auto startTime = std::chrono::steady_clock::now();
    const int timeoutMs = 5000;

    while (true) {
        if (replyReceived_) {
            break;
        }
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count();
        if (elapsed >= timeoutMs) {
            std::cout << "ERROR: no reply was received!" << std::endl;
            sendError("Didn't receive a reply message!");
            setState("end");
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void TCPChatClient::auth(const std::string &username,
                           const std::string &secret,
                           const std::string &displayName) 
{
    displayName_ = displayName;
    std::string message = "AUTH " + username + " AS " + displayName + " USING " + secret + "\r\n";

    setState("auth");
    sendTCP(message);
    replyReceived_ = false;
    checkReply();
}

void TCPChatClient::joinChannel(const std::string &channel) {
    std::string message = "JOIN " + channel + " AS " + displayName_ + "\r\n";
    
    setState("join");
    sendTCP(message);
    replyReceived_ = false;
    checkReply();
}

void TCPChatClient::sendMessage(const std::string &messageContent) {
    std::string message = "MSG FROM " + displayName_ + " IS " + messageContent + "\r\n";

    sendTCP(message);
}

void TCPChatClient::sendError(const std::string &errorContent) {
    std::string message = std::string("ERR FROM ") + displayName_ + " IS " + errorContent + "\r\n";

    sendTCP(message);
    setState("end");
}

void TCPChatClient::bye() {
    std::string message = std::string("BYE FROM ") + (displayName_ == "" ? "unknown" : displayName_) + "\r\n";

    sendTCP(message);
    setState("end");
}

void TCPChatClient::malformedMessage() {
    std::cout << "ERROR: malformed message received." << std::endl;

    sendError("Received malformed message!");
    setState("end");
}

void TCPChatClient::listen() {
    char buffer[1024];
    std::string messageBuffer;

    int flags = fcntl(sockfd_, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl(F_GETFL) failed");
    }

    if (fcntl(sockfd_, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl(F_SETFL) failed");
    }

    while (getState() != "end") {
        ssize_t bytes_received = recv(sockfd_, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            } else {
                std::cerr << "ERROR: receiving data failed!" << std::endl;
                break;
            }
        }

        buffer[bytes_received] = '\0';
        messageBuffer += buffer;        

        size_t pos;
        while ((pos = messageBuffer.find("\r\n")) != std::string::npos && getState() != "end") {
            std::string message = messageBuffer.substr(0, pos);
            messageBuffer.erase(0, pos + 2);
            std::istringstream iss(message);
            std::string command;
            iss >> command;
            std::transform(command.begin(), command.end(), command.begin(), [](unsigned char c) {
                return std::toupper(c);
            });
            std::string extra;

            if (command == "REPLY") {
                std::string result, is;
                if (!(iss >> result >> is)) {
                    malformedMessage();
                    continue;
                }
                std::string content;
                std::transform(is.begin(), is.end(), is.begin(), [](unsigned char c) {
                    return std::toupper(c);
                });
                
                std::getline(iss, content);
                if (is != "IS" || content.empty() || iss >> extra) {
                    malformedMessage();
                    continue;
                }
                std::cout << "Action " << (result == "OK" ? "Success:" : "Failure:") << content << std::endl;

                if (getState() == "join") {
                    setState("open");
                } else if (getState() == "open") {
                    setState("end");
                } else if (getState() == "auth" && result == "OK") {
                    setState("open");
                }
                replyReceived_ = true;
            }
            else if (command == "MSG") {
                if (getState() != "auth") {
                    std::string from, displayName, is;
                    if (!(iss >> from >> displayName >> is)) {
                        malformedMessage();
                        continue;
                    }
                    std::transform(from.begin(), from.end(), from.begin(), [](unsigned char c) {
                        return std::toupper(c);
                    });
                    std::transform(is.begin(), is.end(), is.begin(), [](unsigned char c) {
                        return std::toupper(c);
                    });
                    std::string content;
                    std::getline(iss, content);
                    if (content.empty() || from != "FROM" || is != "IS" || iss >> extra) {
                        malformedMessage();
                        continue;
                    }
                    std::cout << displayName << ":" << content << std::endl;
                }
                else {
                    sendError("Received message in auth state!");
                    setState("end");
                }
            }
            else if (command == "ERR") {
                std::string from, displayName, is;
                if (!(iss >> from >> displayName >> is)) {
                    malformedMessage();
                    continue;
                }
                std::transform(from.begin(), from.end(), from.begin(), [](unsigned char c) {
                    return std::toupper(c);
                });
                std::transform(is.begin(), is.end(), is.begin(), [](unsigned char c) {
                    return std::toupper(c);
                });
                std::string content;
                std::getline(iss, content);
                if (content.empty() || from != "FROM" || is != "IS" || iss >> extra) {
                    malformedMessage();
                    continue;
                }
                std::cout << "ERROR FROM " << displayName << ":" << content << std::endl;
                exit(1);
            }
            else if (command == "BYE") {
                setState("end");
            }
            else {
                std::cout << "ERROR: malformed message received." << std::endl;
                sendError("Received malformed message!");
                exit(1);            
            }
        }
    }
    exit(0);
}   