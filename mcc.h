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
    bool exit;

    struct
    {
        char *hostname;
        int port;
        char *name;
        char *channel;
        char *pass;
    } irc;
};

extern struct server_t g_server;

#endif /* MCC_H */
