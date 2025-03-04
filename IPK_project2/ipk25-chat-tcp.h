#ifndef TCP_H
#define TCP_H

#include <string>
#include <stdint.h>
#include <cstring>
#include <unistd.h>
#include "ipk25-chat.h"

void tcp_listen(ProgramArgs *args, bool *exit_flag);
void tcp_auth(ProgramArgs *args);
void tcp_join(ProgramArgs *args, std::string channelID);
// void tcp_reply(ProgramArgs *args);
void tcp_msg(ProgramArgs *args, std::string messageContent);
void tcp_err(ProgramArgs *args, std::string messageContent);
void tcp_bye(ProgramArgs *args);
// void tcp_ping(ProgramArgs *args);

#endif // TCP_H
