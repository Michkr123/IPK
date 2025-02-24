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

int auth(ProgramArgs &args) {
    while (true) {
        std::string input;
        std::getline(std::cin, input);

        auto tokens = split(input);

        if (tokens.size() == 4) {
            if(tokens[0] == "/auth") {
                args.command = tokens[0];
                args.username = tokens[1];
                args.secret = tokens[2];
                args.displayName = tokens[3];
                if (isValidString(args.username) && isValidString(args.secret) && isPrintableChar(args.displayName)) //TODO check if the checks are correct
                return 0;
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

    //TODO resolve hostname

    if((args.protocol).empty()) {  // missing protocol
        exit(1); //TODO exit message + code
    }

    if((args.hostname).empty()) {  // missing hostname / IP
        exit(1); //TODO exit message + code
    }

    //TODO check if prot is TCP or UDP and other things!

    auth(args); // authentication
    std::cout << "auth succesfull." << std::endl;


    bool exit_flag = false;
    while (!exit_flag) {
        std::string input;
        std::getline(std::cin, input);

        auto tokens = split(input);

        if(tokens[0] == "/join") {
            if(args.protocol == "udp")
                udp_join(&args);
            else
                tcp_join(&args);
        }
        else if(tokens[0] == "/reply") {
            if(args.protocol == "udp")
                udp_reply(&args);
            else
                tcp_reply(&args);
        }
        else if(tokens[0] == "/msg") {
            if(args.protocol == "udp")
                udp_msg(&args);
            else
                tcp_msg(&args);
        }
        else if(tokens[0] == "/err") {
            if(args.protocol == "udp")
                udp_err(&args);
            else
                tcp_err(&args);
        }
        else if(tokens[0] == "/bye") {
            if(args.protocol == "udp")
                udp_bye(&args);\
            else
                tcp_bye(&args);
        }
        else if(tokens[0] == "/ping") {
            if(args.protocol == "udp")
                udp_ping(&args);
            else
                tcp_ping(&args);
        }
        else {
            printf("non-existent command");//TODO chybny command
        }
    }

    return 0;
}
