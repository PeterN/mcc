#ifndef MCC_H
#define MCC_H

#include <stdbool.h>

struct server_t
{
    char *name;
    char *motd;
    int max_players;
    int players;
    bool public;
};

extern struct server_t g_server;

#endif /* MCC_H */
