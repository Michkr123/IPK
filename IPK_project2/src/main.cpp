#include "ChatClient.h"
#include "UDPChatClient.h"
#include "TCPChatClient.h"
#include "Utils.h"

int tcp_sockfd_global = -1;
std::unique_ptr<ChatClient> client;

int main(int argc, char *argv[]) {
    std::signal(SIGINT, [](int) {
        client->bye();
        if (tcp_sockfd_global != -1) {
            if (shutdown(tcp_sockfd_global, SHUT_RDWR) < 0) {
                std::cerr << "ERROR: Shutdown failed!" << std::endl;
            }
            close(tcp_sockfd_global);
            tcp_sockfd_global = -1;
        }
        exit(0);
    });
    
    if (argc == 2 && std::string(argv[1]) == "-h") {
        help();
    }

    Options opts = parseArguments(argc, argv);

    if (opts.protocol.empty() || opts.hostname.empty()) {
        std::cout << "ERROR: Protocol and hostname must be provided.\n";
        help();
    }

    if (opts.protocol == "udp") {
        client = std::make_unique<UDPChatClient>(opts.hostname, opts.port, opts.retry_count, opts.timeout);
    } else if (opts.protocol == "tcp") {
        client = std::make_unique<TCPChatClient>(opts.hostname, opts.port);
        tcp_sockfd_global = client->getSockfd();
    } else {
        std::cout << "ERROR: Invalid protocol specified: " << opts.protocol << "\n";
        return 1;
    }

    client->connectToServer();

    client->startListener();

    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    char buffer[1024];
    std::string input;

    while (client->getState() != "end") {
        ssize_t bytes = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            std::string input(buffer);
            boost::trim(input);
            if (input.empty()) { 
                continue;
            }
            std::vector<std::string> tokens = split(input);
            if (tokens.empty()) {
                continue;
            }

            if (tokens[0] == "/auth" && tokens.size() == 4) {
                if (client->getState() == "start" || client->getState() == "auth") { 
                    std::string username = tokens[1];
                    std::string secret = tokens[2];
                    std::string displayName = tokens[3];

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
                std::string channel = tokens[1];
                if (channel.size() <= 20 && isValidString(channel)) {
                    if (client->getState() == "open") {
                        client->joinChannel(channel);
                    } else {
                        std::cout << "ERROR: Cant join channel in current state!" << std::endl;
                    }
                }
                else {
                    std::cout << "ERROR: Invalid characters in channel name or too long!" << std::endl;
                }
            }
            else if (tokens[0] == "/rename" && tokens.size() == 2) {
                std::string displayName = tokens[1];
                if (displayName.size() <= 20 && isPrintableChar(displayName)) { 
                    client->rename(displayName);
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
                if (client->getState() == "open") {
                    if (isValidMessage(input)) {
                        if (input.size() <= 60000) {
                            client->sendMessage(input);
                        }
                        else {
                            client->sendMessage(input.substr(0, 60000));
                        }
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
        else if (bytes == 0) {
            client->bye();
            client->setState("end");
        }
    }
    return 0;
}