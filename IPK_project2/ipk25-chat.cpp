#include "ipk25-chat.h"
#include "ipk25-chat-udp.h"
#include "ipk25-chat-tcp.h"

void help() {
    std::cout << "Usage: program -t [protocol] -s [hostname] -p [port] -d [timeout] -r [retry_count]\n";
    exit(0);
}

void parse_arguments(int argc, char *argv[], ProgramArgs &args) {
    int c;
    while ((c = getopt(argc, argv, "t:s:p:d:r:")) != -1) {
        switch (c) {
            case 't':
                args.protocol = optarg;
                break;
            case 's':
                args.hostname = optarg;
                break;
            case 'p':
                args.port = static_cast<uint16_t>(std::stoi(optarg));
                break;
            case 'd':
                args.timeout = static_cast<uint16_t>(std::stoi(optarg));
                break;
            case 'r':
                args.retry_count = static_cast<uint8_t>(std::stoi(optarg));
                break;
            default:
                std::cerr << "Usage: " << argv[0] << " -t [protocol] -s [hostname] -p [port] -d [timeout] -r [retry_count]\n";
                exit(EXIT_FAILURE);
        }
    }
}

std::vector<std::string> split(const std::string &str) {
    std::vector<std::string> tokens;
    std::istringstream iss(str);
    std::string token;
    
    while (iss >> token) {
        tokens.push_back(token);
    }

    return tokens;
}

bool isValidString(const std::string &str) {
    for (char c : str) {
        if (!(std::isalnum(c) || c == '-' || c == '_')) {
            return false;
        }
    }
    return true;
}

bool isPrintableChar(const std::string &str) {
    for (char c : str) {
        if(c < 0x21 || c > 0x7E) {
            return false;
        }
    }
    return true;
}

bool isValidMessage(const std::string &message) {
    for (char c : message) {
        if(c != 0x0A || c < 0x20 || c > 0x7E) {
            return false;
        }
    }
    return true;
}

void resolve_hostname(ProgramArgs *args) {
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if (getaddrinfo(args->hostname.c_str(), nullptr, &hints, &res) != 0) {
        std::cerr << "Error: No such host found" << std::endl;
        exit(1); 
    }

    struct sockaddr_in *ipv4 = (struct sockaddr_in *)res->ai_addr;
    args->serverAddr.sin_family = AF_INET;
    args->serverAddr.sin_addr = ipv4->sin_addr;
    args->serverAddr.sin_port = htons(args->port);

    freeaddrinfo(res);
}

void signal_handler(int) {
    exit(0); //TODO gracefully terminate connection
}

int main(int argc, char *argv[]) {
    std::signal(SIGINT, signal_handler);

    if (argc == 2 && std::string(argv[1]) == "-h") {
        help();
    }

    ProgramArgs args;
    parse_arguments(argc, argv, args);

    if((args.hostname).empty()) {  
        std::cerr << "Error: Hostname is missing!" << std::endl;
        exit(1);
    }

    resolve_hostname(&args);

    if((args.protocol).empty()) {
        std::cerr << "Error: Protocol is missing!" << std::endl;
        exit(1);
    }

    if (args.protocol == "udp") {
        args.sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    } else if (args.protocol == "tcp") {
        args.sockfd = socket(AF_INET, SOCK_STREAM, 0);
    } else {
        std::cerr << "Invalid protocol!" << std::endl;
        return 1;
    }

    if (args.sockfd == -1) {
        std::cerr << "Socket creation failed!" << std::endl;
        return 1;
    }

    std::string MessageContent;

    if(args.protocol == "tcp") {
        if (connect(args.sockfd, (struct sockaddr*)&args.serverAddr, sizeof(args.serverAddr)) < 0) {
            std::cerr << "Connection failed!" << std::endl;
            return 1;
        }
    }

    pthread_t thread;
    if(args.protocol == "tcp") {
        if (pthread_create(&thread, nullptr, tcp_listen, (void*)&args) != 0) {
            std::cerr << "Failed to create tcp listener thread!" << std::endl;
            return 1;
        }
    } else {
        if (pthread_create(&thread, nullptr, udp_listen, (void*)&args) != 0) {
            std::cerr << "Failed to create UDP listener thread!" << std::endl;
            return 1;
        }
    }

    while (args.state != "end") { //TODO individual args.states so the commands are available only when should be!
        std::string input;
        std::getline(std::cin, input);

        auto tokens = split(input);

        if(tokens[0] == "/join" && tokens.size() == 2) {
            if(args.state == "open") {
                MessageContent = tokens[1];
                //if(isValidString(MessageContent) && MessageContent.size() <= 20) { //TODO doesnt work for the discord.verified-1
                    if(args.protocol == "udp")
                        udp_join(&args, MessageContent);
                    else
                        tcp_join(&args, MessageContent);
                //}
            }
            else {
                std::cerr << "Error: Trying to send an error in non-open state!" << std::endl;
            }
        }
        else if(tokens[0] == "/err" &&  tokens.size() == 3) {
            if(args.state == "auth" || args.state == "open") {
                MessageContent = tokens[2];
                if(MessageContent.size() <= 60000) {
                    if(args.protocol == "udp")
                        udp_err(&args, MessageContent);
                    else
                        tcp_err(&args, MessageContent);
                }
            }
            else {
                std::cerr << "Error: Trying to send an error in non-open or non-auth state!" << std::endl;
            }
        }
        else if(tokens[0] == "/bye" && tokens.size() == 1) {
            if(args.protocol == "udp")
                udp_bye(&args);
            else
                tcp_bye(&args);
        }
        else if(tokens[0] == "/ping" && tokens.size() == 1) {
            if(args.protocol == "udp")
                udp_ping(&args);
            else
                // tcp_ping(&args);
                std::cerr << "Error: Ping is not a valid command in TCP variant!" << std::endl;
        }
        else if(tokens[0] == "/rename" && tokens.size() == 2) {
            if(isPrintableChar(args.displayName) && tokens[1].size() <= 20) {
                args.displayName = tokens[1];
            }
            else {
                std::cerr << "Error: Name is too long or containing illegal characters!" << std::endl;
            }
        }
        else if(tokens[0] == "/auth" && tokens.size() == 4) {
            if(args.state == "start" || args.state == "auth")
            {
                if(tokens[1].size() <= 20 && tokens[2].size() <= 128 && tokens[3].size() <= 20) {
                    args.username = tokens[1];
                    args.secret = tokens[2];
                    args.displayName = tokens[3];
                    if (isValidString(args.username) && isValidString(args.secret) && isPrintableChar(args.displayName)) { 
                        if(args.protocol == "udp")
                            udp_auth(&args);
                        else
                            tcp_auth(&args);
                    }
                }
            }
            else {
                std::cerr << "Error: Trying to auth when already authenticated!" << std::endl;
            }
        }
        else if(tokens[0][0] != '/') {
            if(args.state == "open") {
                MessageContent = input;
                if(MessageContent.size() <= 60000 && isValidMessage(MessageContent)) {
                    if(args.protocol == "udp")
                        udp_msg(&args, MessageContent);
                    else
                        tcp_msg(&args, MessageContent);
                }
            }
            else {
                std::cerr << "Error: Trying to send a message in a non-open state!" << std::endl;
            }
        }
        else {
            std::cerr << "Error: Trying to process an unknown or otherwise malformed command or user input in general!" << std::endl;
        }
    }

    pthread_join(thread, nullptr);
    return 0;
}
