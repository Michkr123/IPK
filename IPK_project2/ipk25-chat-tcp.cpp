#include "ipk25-chat-tcp.h"
#include <iostream>

void tcp_send(ProgramArgs *args, const char *message) {
    ssize_t bytes_sent = send(args->sockfd, message, strlen(message), 0);
    if (bytes_sent < 0) {
        std::cerr << "Error: Could not send message\n";
        close(args->sockfd);
        exit(1);
    }
}

void* tcp_listen(void* arg) {
    ProgramArgs* args = static_cast<ProgramArgs*>(arg);
    char buffer[1024];
    std::string message_buffer;

    while ((args->state != "end")) {
        ssize_t bytes_received = recv(args->sockfd, buffer, sizeof(buffer) - 1, 0);

        if (bytes_received < 0) {
            std::cerr << "Error: Could not receive message\n";
            break;
        } else if (bytes_received == 0) {
            std::cerr << "Server closed the connection\n";
            break;
        }

        buffer[bytes_received] = '\0';
        message_buffer += buffer;
        
        size_t pos;
        while ((pos = message_buffer.find("\r\n")) != std::string::npos) {
            std::string message = message_buffer.substr(0, pos);
            message_buffer.erase(0, pos + 2);

            std::istringstream iss(message);
            std::string command;
            iss >> command;

            if (command == "REPLY") {
                std::string result, is, content;
                iss >> result >> is;
                std::getline(iss, content);
                if (result == "OK") {
                    std::cout << "Success: " << content << std::endl;
                } else {
                    std::cout << "Failure: " << content << std::endl;
                }

                std::cout << "RESULT OF THE REPLY: " << result << std::endl;
                std::cout << "STATE: " << args->state << std::endl; 

                if(args->state == "join") {
                    args->state = "open";
                }
                else if(args->state == "open") {
                    args->state = "end";
                }
                else if(args->state == "auth" && result == "OK") {
                    args->state = "open";
                }
            } 
            else if (command == "MSG") {
                std::string from, displayName, is, content;
                iss >> from >> displayName >> is;
                std::getline(iss, content);
                std::cout << displayName << ": " << content << std::endl;

                if(args->state == "auth") {
                    args->state = "end";
                }
            } 
            else if (command == "ERR") {
                std::string from, displayName, is, content;
                iss >> from >> displayName >> is;
                std::getline(iss, content);
                std::cerr << "Error from " << displayName << ": " << content << std::endl;
                
                args->state = "end";
            } 
            else if (command == "BYE") {
                std::string from, displayName;
                iss >> from >> displayName;
                std::cout << "Server ended the session from " << displayName << ".\n";
                
                args->state = "end";
            } 
            else {
                std::cout << "Unknown message: " << message << std::endl;
            }
        }
    }

    close(args->sockfd);
    return nullptr;
}

void tcp_auth(ProgramArgs *args) {
    args->state = "auth";
    std::string message = "AUTH " + args->username + " AS " + args->displayName + " USING " + args->secret + "\r\n";
    tcp_send(args, message.c_str());
}

void tcp_join(ProgramArgs *args, std::string channelID) {
    args->state = "join";
    std::string message = "JOIN " + channelID + " AS " + args->displayName + "\r\n";   
    tcp_send(args, message.c_str());
}

void tcp_msg(ProgramArgs *args, std::string messageContent) {
    std::string message = "MSG FROM " + args->displayName + " IS " + messageContent + "\r\n";
    tcp_send(args, message.c_str());
}

void tcp_err(ProgramArgs *args, std::string messageContent) {
    args->state = "end";
    std::string message = "ERR FROM " + args->displayName + " IS " + messageContent + "\r\n";
    tcp_send(args, message.c_str());
}

void tcp_bye(ProgramArgs *args) {
    args->state = "end";
    std::string message = "BYE FROM " + args->displayName + "\r\n";
    tcp_send(args, message.c_str());
}
