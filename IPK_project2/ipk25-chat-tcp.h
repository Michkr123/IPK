#ifndef TCP_H
#define TCP_H

#include <string>
#include <stdint.h>
#include <cstring>
#include <unistd.h>
#include <chrono>
#include <thread>
#include "ipk25-chat.h"

void tcp_auth(ProgramArgs *args);
void tcp_join(ProgramArgs *args, std::string channelID);
void tcp_msg(ProgramArgs *args, std::string messageContent);
void tcp_err(ProgramArgs *args, std::string messageContent);
void tcp_bye(ProgramArgs *args);
void* tcp_listen(void* arg);

#endif // TCP_H
