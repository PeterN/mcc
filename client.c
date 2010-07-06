#include "client.h"
#include "packet.h"

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

void client_add_packet(struct client_t *c, struct packet_t *p)
{
    struct packet_t **ip = &c->packet_send;
    while (*ip != NULL)
    {
        ip = &(*ip)->next;
    }

    *ip = p;
}
