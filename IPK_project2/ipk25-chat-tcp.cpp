#include "ipk25-chat-tcp.h"
#include <iostream>

void tcp_join(ProgramArgs *args) {
    std::cout << "Join tcp" << std::endl;
}

void tcp_reply(ProgramArgs *args) {
    std::cout << "Reply tcp" << std::endl;
}

void tcp_msg(ProgramArgs *args) {
    std::cout << "Message tcp" << std::endl;
}

void tcp_err(ProgramArgs *args) {
    std::cout << "Error tcp" << std::endl;
}

void tcp_bye(ProgramArgs *args) {
    std::cout << "Bye tcp" << std::endl;
}

void tcp_ping(ProgramArgs *args) {
    std::cout << "Ping tcp" << std::endl;
}
