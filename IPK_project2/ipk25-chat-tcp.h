#ifndef TCP_H
#define TCP_H

#include <string>
#include <stdint.h>
#include "ipk25-chat.h"

void tcp_join(ProgramArgs *args);
void tcp_reply(ProgramArgs *args);
void tcp_msg(ProgramArgs *args);
void tcp_err(ProgramArgs *args);
void tcp_bye(ProgramArgs *args);
void tcp_ping(ProgramArgs *args);

#endif // TCP_H
