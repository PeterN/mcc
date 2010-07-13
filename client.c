#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include "client.h"
#include "commands.h"
#include "player.h"
#include "packet.h"
#include "level.h"
#include "network.h"
#include "mcc.h"

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

bool client_notify_by_username(const char *username, const char *message)
{
    int i;
    for (i = 0; i < s_clients.used; i++)
    {
        struct client_t *c = &s_clients.items[i];
        if (strcasecmp(c->player->username, username) == 0)
        {
            client_notify(c, message);
            return true;
        }
    }

    return false;
}

void client_process(struct client_t *c, char *message)
{
    char buf[64];

    if (message[0] == '!' || message[0] == '/')
	{
	    char *bufp = message + 1;
	    char *param[10];
	    int params = 0;

	    memset(param, 0, sizeof param);

	    for (;;)
	    {
	        size_t l = strcspn(bufp, " ,");
	        bool end = false;

	        if (bufp[l] == '\0') end = true;
	        bufp[l] = '\0';
	        param[params++] = bufp;
	        bufp += l + 1;

	        if (end) break;
	    }

	    if (!command_process(c, params, (const char **)param))
        {
            client_notify(c, "Unknown command");
	        return;
	    }
	}
	else
	{
	    switch (message[0])
	    {
	        case '@': // Private message
	        {
                size_t l = strcspn(message, " ,");
                if (l != strlen(message))
                {
                    message[l] = '\0';
                    snprintf(buf, sizeof buf, "(%s: %s)", c->player->username, message + l + 1);
                    if (!client_notify_by_username(message + 1, buf))
                    {
                        client_notify(c, "User is offline");
                    }
                }
                return;
	        }

	        case ';':
                snprintf(buf, sizeof buf, "* %s %s", c->player->username, message + 1);
                break;

	        case '\'':
                snprintf(buf, sizeof buf, "%s: %s", c->player->username, message + 1);
                break;

	        default:
                snprintf(buf, sizeof buf, "%s: %s", c->player->username, message);
                break;
	    }

	    net_notify_all(buf);
	}
}

void client_send_spawn(const struct client_t *c, bool hiding)
{
    const struct level_t *level = c->player->level;

    int i;
    for (i = 0; i < MAX_CLIENTS_PER_LEVEL; i++)
    {
        if (level->clients[i] != NULL && level->clients[i] != c)
        {
            client_add_packet(level->clients[i], packet_send_spawn_player(c->player->levelid, c->player->username, &level->spawn));
        }
    }
}

void client_send_despawn(const struct client_t *c, bool hiding)
{
    const struct level_t *level = c->player->level;

    int i;
    for (i = 0; i < MAX_CLIENTS_PER_LEVEL; i++)
    {
        if (level->clients[i] != NULL && level->clients[i] != c)
        {
            client_add_packet(level->clients[i], packet_send_despawn_player(c->player->levelid));
        }
    }
}
