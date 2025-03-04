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

int auth(ProgramArgs &args) {
    while (true) {
        std::string input;
        std::getline(std::cin, input);

        auto tokens = split(input);

        if (tokens.size() == 4) {
            if(tokens[0] == "/auth") {
                args.username = tokens[1];
                args.secret = tokens[2];
                args.displayName = tokens[3];
                if (isValidString(args.username) && isValidString(args.secret) && isPrintableChar(args.displayName)) { 
                    if(args.protocol == "udp")
                        udp_auth(&args);
                    else
                        tcp_auth(&args);
                    return 0; 
                }
            }
        }
    }
}

int main(int argc, char *argv[]) {
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
    bool exit_flag = false;

    if(args.protocol == "tcp") {
        if (connect(args.sockfd, (struct sockaddr*)&args.serverAddr, sizeof(args.serverAddr)) < 0) {
            std::cerr << "Connection failed!" << std::endl;
            return 1;
        }
    }
    
    pid_t pid = fork();
    if(pid == 0) {
        if(args.protocol == "tcp")
            tcp_listen(&args, &exit_flag);
        else
            udp_listen(&args, &exit_flag);
        exit(0);
    }

    auth(args); // authentication

    while (!exit_flag) {
        std::string input;
        std::getline(std::cin, input);

        auto tokens = split(input);

        if(tokens[0] == "/join" && tokens.size() == 3) {
            if(args.protocol == "udp")
                udp_join(&args, MessageContent);
            else
                tcp_join(&args, MessageContent);
        }
        else if(tokens[0] == "/err" && tokens.size() == 3) {
            MessageContent = tokens[2];
            if(args.protocol == "udp")
                udp_err(&args, MessageContent);
            else
                tcp_err(&args, MessageContent);
        }
        else if(tokens[0] == "/bye" && tokens.size() == 1) {
            if(args.protocol == "udp")
                udp_bye(&args);\
            else
                tcp_bye(&args);
        }
        else if(tokens[0] == "/ping" && tokens.size() == 1) {
            if(args.protocol == "udp")
                udp_ping(&args);
            else
                // tcp_ping(&args);
                std::cout << "non-existent command" << std::endl;
        }
        else if(tokens[0] == "/rename" && tokens.size() == 2) {
            args.displayName = tokens[1];
            if(!isPrintableChar(args.displayName)) {
                exit(1); 
            }
        }
        else if(tokens[0] == "/auth" && tokens.size() == 3) {
            args.username = tokens[1];
            args.secret = tokens[2];
            args.displayName = tokens[3];
            if (isValidString(args.username) && isValidString(args.secret) && isPrintableChar(args.displayName)) { 
                if(args.protocol == "udp")
                    udp_auth(&args);
                else
                    tcp_auth(&args);
                return 0; 
            }
        }
        else if(tokens[0][0] != '/') {
            MessageContent = input;
            if(args.protocol == "udp")
                udp_msg(&args, MessageContent);
            else
                tcp_msg(&args, MessageContent);
        }
        else {
            std::cout << "non-existent command" << std::endl;
        }
    }

    return 0;
}
