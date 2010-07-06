#include "client.h"

struct client_list_t s_clients;

struct client_t *client_get_by_player(struct player_t *p)
{
    int i;
    for (i = 0; i < s_clients.used; i++)
    {
        if (s_clients.items[i].player == p) return &s_clients.items[i];
    }

    return NULL;
}
