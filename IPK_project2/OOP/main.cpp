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

// Simple help message
void help() {
    std::cout << "Usage: ./chatClient -t <protocol> -s <hostname/IP>\n";
    std::cout << "       protocol: udp or tcp\n";
    std::cout << "       hostname: server hostname or ip\n";
    exit(0);
}

int main(int argc, char *argv[]) {
    // Set up signal handling (graceful termination)
    std::signal(SIGINT, [](int){ exit(0); });

    if (argc == 2 && std::string(argv[1]) == "-h") {
        help();
    }

    // Parse command-line options using Utils
    Options opts = parseArguments(argc, argv);
    // Ensure protocol and hostname are provided
    if (opts.protocol.empty() || opts.hostname.empty()) {
        std::cerr << "Error: Protocol and hostname must be provided.\n";
        help();
    }

    // Create the appropriate ChatClient instance based on the protocol.
    std::unique_ptr<ChatClient> client;
    if (opts.protocol == "udp") {
        client = std::make_unique<UDPChatClient>(opts.hostname, opts.port, opts.retry_count, opts.timeout);
    } else if (opts.protocol == "tcp") {
        client = std::make_unique<TCPChatClient>(opts.hostname, opts.port);
    } else {
        std::cerr << "Invalid protocol specified: " << opts.protocol << "\n";
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
            // /auth <username> <secret> <displayName>
            std::string username = tokens[1];
            std::string secret   = tokens[2];
            std::string disp     = tokens[3];
            // Validate lengths and characters.
            if (username.size() <= 20 && secret.size() <= 128 && disp.size() <= 20) {
                if (isValidString(username) && isValidString(secret) && isPrintableChar(disp)) {
                    client->auth(username, secret, disp);
                } else {
                    std::cerr << "Error: Invalid characters in username, secret, or display name.\n";
                }
            } else {
                std::cerr << "Error: Username, secret, or display name too long.\n";
            }
        }
        else if (tokens[0] == "/join" && tokens.size() == 2) {
            // /join <channel>
            if (client->getState() == "open") {
                client->joinChannel(tokens[1]);
            } else {
                std::cerr << "Error: Cannot join channel in current state.\n";
            }
        }
        else if (tokens[0] == "/err" && tokens.size() >= 2) {
            // /err <error message> (everything after /err is the error message)
            std::string errorMsg = input.substr(5); // Skip "/err " (5 characters)
            if (errorMsg.size() <= 60000) {
                client->sendError(errorMsg);
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
        else if (tokens[0] == "/bye") {
            client->bye();
        }
        else if (tokens[0][0] != '/') {
            // Treat input as a normal message if state is "open".
            if (client->getState() == "open") {
                if (input.size() <= 60000 && isValidMessage(input)) {
                    client->sendMessage(input);
                } else {
                    std::cerr << "Error: Invalid message content.\n";
                }
            }
            else {
                std::cerr << "Error: Cannot send message in current state.\n";
            }
        }
        else {
            std::cerr << "Error: Unknown or malformed command.\n";
        }
    }

    return 0;
}
