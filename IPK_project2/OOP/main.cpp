// main.cpp
#include "ChatClient.h"         // Abstract base class
#include "UDPChatClient.h"      // UDP implementation
#include "TCPChatClient.h"      // TCP implementation
#include "Utils.h"              // Contains Options, parseArguments, split, isValidString, isPrintableChar, isValidMessage
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <csignal>

int tcp_sockfd_global = -1;

// Simple help message
void help() {
    std::cout << std::endl;
    std::cout << "Usage: ./chatClient -t <protocol> -s <hostname/IP> -p <port> -d <timeout> -r <retransmissions>\n";
    std::cout << "       protocol:          udp or tcp. (required)" << std::endl;
    std::cout << "       hostname:          server hostname or ip. (required)" << std::endl;
    std::cout << "       port:              server port. (optional)" << std::endl;
    std::cout << "       timeout:           UDP confirmation timeout (in milliseconds). (optional)" << std::endl;
    std::cout << "       retransmissions:   maximum number of UDP retransmissions. (optional)" << std::endl;
    std::cout << std::endl;
    std::cout << "Default values for optional arguments:" << std::endl;
    std::cout << "       port:              4567" << std::endl;
    std::cout << "       timeout:           250ms" << std::endl;
    std::cout << "       retransmissions:   3" << std::endl;
    exit(0);
}

void commandHelp() {
    std::cout << std::endl;
    std::cout << "Supported client commands:" << std::endl;
    std::cout << "  /auth {Username} {Secret} {DisplayName} - Autorises to the server. (use once)" << std::endl;
    std::cout << "  /join {ChannelID}                       - Changes the channel." << std::endl;
    std::cout << "  /rename {DisplayName}                   - Changes the display name." << std::endl;
    std::cout << "  /err {MessageContent}                   - Sends error to the server." << std::endl;
    std::cout << "  /bye                                    - disconnects from the server." << std::endl;
    std::cout << "  {messageContent}                        - Sends message." << std::endl;
}

int main(int argc, char *argv[]) {
    // Set up signal handling (graceful termination)
    std::signal(SIGINT, [](int) {
        // Check if the TCP socket global variable is valid.
        if (tcp_sockfd_global != -1) {
            if (shutdown(tcp_sockfd_global, SHUT_RDWR) < 0) {
                std::cerr << "ERROR: Shutdown failed!" << std::endl;
                exit(1);
            }
            close(tcp_sockfd_global);
        }
        std::exit(0);
    });
    
    if (argc == 2 && std::string(argv[1]) == "-h") {
        help();
    }

    // Parse command-line options using Utils
    Options opts = parseArguments(argc, argv);
    // Ensure protocol and hostname are provided
    if (opts.protocol.empty() || opts.hostname.empty()) {
        std::cout << "ERROR: Protocol and hostname must be provided.\n";
        help();
    }

    // Create the appropriate ChatClient instance based on the protocol.
    std::unique_ptr<ChatClient> client;
    if (opts.protocol == "udp") {
        client = std::make_unique<UDPChatClient>(opts.hostname, opts.port, opts.retry_count, opts.timeout);
    } else if (opts.protocol == "tcp") {
        client = std::make_unique<TCPChatClient>(opts.hostname, opts.port);
    } else {
        std::cout << "ERROR: Invalid protocol specified: " << opts.protocol << "\n";
        return 1;
    }

    // Connect to the server (for TCP this opens a connection;
    // UDPChatClient::connectToServer() might be a no-op)
    client->connectToServer();

    // Start the listener in a separate thread.
    client->startListener();

    // Interactive loop:
    // Commands:
    //   /auth <username> <secret> <displayName>
    //   /join <channel>
    //   /err <error message>
    //   /bye
    //   /ping
    // If a line does not start with '/', it is considered a normal message.
    std::string input;
    while (client->getState() != "end") {
        std::getline(std::cin, input);
        if (input.empty()) continue;

        // Split the input using the utility split function.
        std::vector<std::string> tokens = split(input);
        if (tokens.empty()) continue;

        if (tokens[0] == "/auth" && tokens.size() == 4) {
            if(client->getState() == "start" || client->getState() == "auth") { 
                // /auth <username> <secret> <displayName>
                std::string username = tokens[1];
                std::string secret   = tokens[2];
                std::string displayName     = tokens[3];
                // Validate lengths and characters.
                if (username.size() <= 20 && secret.size() <= 128 && displayName.size() <= 20) {
                    if (isValidString(username) && isValidString(secret) && isPrintableChar(displayName)) {
                        client->auth(username, secret, displayName);
                    } else {
                        std::cout << "ERROR: Invalid characters in username, secret, or display name!" << std::endl;
                    }
                } else {
                    std::cout << "ERROR: Username, secret, or display name too long!" << std::endl;
                }
            }
            else {
                std::cout << "ERROR: Already authorized!" << std::endl;
            }
        }
        else if (tokens[0] == "/join" && tokens.size() == 2) {
            // /join <channel>
            if (client->getState() == "open") {
                client->joinChannel(tokens[1]);
            } else {
                std::cout << "ERROR: Cannot join channel in current state!" << std::endl;
            }
        }
        else if (tokens[0] == "/err" && tokens.size() >= 2) {
            // /err <error message> (everything after /err is the error message)
            if(client->getState() == "auth" || client->getState() == "open") {
                std::string errorMsg = input.substr(5); // Skip "/err " (5 characters)
                if (errorMsg.size() <= 60000) {
                    client->sendError(errorMsg);
                }
            }
            else {
                std::cout << "ERROR: Cannot send error message in current state!" << std::endl;
            }
        }
        else if (tokens[0] == "/rename" && tokens.size() == 2) {
            if (tokens[1].size() <= 20 && isPrintableChar(tokens[1])) { 
                client->rename(tokens[1]);
            }
            else {
                std::cout << "ERROR: Name is too long or containing invalid characters!" << std::endl;
            }
        }
        else if (tokens[0] == "/bye" && tokens.size() == 1) {
            client->bye();
        }
        else if (tokens[0] == "/help" && tokens.size() == 1) {
            commandHelp();
        }
        else if (tokens[0][0] != '/') {
            // Treat input as a normal message if state is "open".
            if (client->getState() == "open") {
                if (input.size() <= 60000 && isValidMessage(input)) {
                    client->sendMessage(input);
                } else {
                    std::cout << "ERROR: Invalid message content.\n";
                }
            }
            else {
                std::cout << "ERROR: Cannot send message in current state.\n";
            }
        }
        else {
            std::cout << "ERROR: Unknown or malformed command.\n";
        }
    }
    return 0;
}
