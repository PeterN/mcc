#ifndef MCC_H
#define MCC_H

#include <stdbool.h>
#include <time.h>

struct server_t
{
    char *name;
    char *motd;
    int max_players;
    int players;
    bool public;
    bool exit;
    time_t start_time;
    clock_t cpu_start;
    double cpu_time;

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
