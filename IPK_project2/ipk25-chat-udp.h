#ifndef UDP_H
#define UDP_H

#include <string>
#include <stdint.h>
#include "ipk25-chat.h"

void udp_join(ProgramArgs *args);
void udp_reply(ProgramArgs *args);
void udp_msg(ProgramArgs *args);
void udp_err(ProgramArgs *args);
void udp_bye(ProgramArgs *args);
void udp_ping(ProgramArgs *args);

#endif // UDP_H
