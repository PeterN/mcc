#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include "client.h"
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

static int level_filename_filter(const struct dirent *d)
{
    return strstr(d->d_name, ".mcl") != NULL;
}

static char s_pattern[256];
static int undo_filename_filter(const struct dirent *d)
{
    return strncmp(d->d_name, s_pattern, strlen(s_pattern)) == 0;
}

void client_process(struct client_t *c, char *message)
{
    if (message[0] == '!' || message[0] == '/')
	{
	    char *bufp = message + 1;
	    char *param[10];
	    int params = 0;

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
	    if (strcasecmp("exit", param[0]) == 0)
	    {
	        if (c->player->rank != RANK_ADMIN) goto unknown;

	        g_server.exit = true;
	    }
        if (strcasecmp("fixed", param[0]) == 0)
        {
            if (c->player->rank < RANK_OP) goto unknown;

            ToggleBit(c->player->flags, PLAYER_PLACE_FIXED);

	        char buf[64];
            snprintf(buf, sizeof buf, "Fixed %s", HasBit(c->player->flags, PLAYER_PLACE_FIXED) ? "on" : "off");
            client_add_packet(c, packet_send_message(0xFF, buf));
        }
	    else if (strcasecmp("goto", param[0]) == 0)
	    {
	        if (params != 2)
	        {
                client_add_packet(c, packet_send_message(0xFF, "goto [name]"));
                return;
	        }

            struct level_t *l;
            if (level_get_by_name(param[1], &l))
            {
                if (player_change_level(c->player, l)) level_send(c);
            }
            else
            {
                char buf[64];
                snprintf(buf, sizeof buf, "Cannot go to level '%s'", param[1]);
                client_add_packet(c, packet_send_message(0xFF, buf));
            }
	    }
	    else if (strcasecmp("home", param[0]) == 0)
	    {
	        char name[64];
	        struct level_t *l;

	        snprintf(name, sizeof name, "%s_home", c->player->username);

	        if (!level_get_by_name(name, &l))
	        {
	            l = malloc(sizeof *l);
                level_init(l, 32, 32, 32, name);
                level_gen(l, 0);
                level_list_add(&s_levels, l);
	        }

            /* Don't resend the level if play is already on it */
            if (player_change_level(c->player, l)) level_send(c);
	    }
	    else if (strcasecmp("identify", param[0]) == 0)
	    {
	        if (params != 2)
	        {
	            client_add_packet(c, packet_send_message(0xFF, "identify [password]"));
                return;
	        }
	    }
	    else if (strcasecmp("levels", param[0]) == 0)
	    {
	        char buf[64], *bufp;
	        struct dirent **namelist;
	        int n, i;

	        strcpy(buf, "Levels: ");
	        bufp = buf + strlen(buf);

	        n = scandir("levels", &namelist, &level_filename_filter, alphasort);
	        if (n < 0)
	        {
	            client_add_packet(c, packet_send_message(0xFF, "Unable to get list of levels"));
	            return;
	        }

	        for (i = 0; i < n; i++)
	        {
	            size_t len = strlen(namelist[i]->d_name) + (i < n - 1 ? 2 : 0);
	            if (len >= sizeof buf - (bufp - buf))
	            {
	                client_add_packet(c, packet_send_message(0xFF, buf));
	                bufp = buf;
	            }

                strcpy(bufp, namelist[i]->d_name);
	            bufp += len;

	            free(namelist[i]);

	            if (i < n - 1) strcpy(bufp - 2, ", ");
	        }

	        client_add_packet(c, packet_send_message(0xFF, buf));
	    }
	    else if (strcasecmp("mapinfo", param[0]) == 0)
	    {
	        char buf[64];
	        snprintf(buf, sizeof buf, "Level '%s': %d x %d x %d", c->player->level->name, c->player->level->x, c->player->level->y, c->player->level->z);
	        client_add_packet(c, packet_send_message(0xFF, buf));
	    }
	    else if (strcasecmp("newlvl", param[0]) == 0)
	    {
	        if (c->player->rank != RANK_ADMIN) goto unknown;

	        if (params != 6)
	        {
                client_add_packet(c, packet_send_message(0xFF, "newlvl [name] [x] [y] [z] [type]"));
                client_add_packet(c, packet_send_message(0xFF, " type: 0=flat 1=flat/adminium 2=smooth 6=rough"));
                return;
	        }

	        const char *name = param[1];
	        int x = strtol(param[2], NULL, 10);
	        int y = strtol(param[3], NULL, 10);
	        int z = strtol(param[4], NULL, 10);
	        int t = strtol(param[5], NULL, 10);

            if (!level_get_by_name(name, NULL))
            {
                struct level_t *l = malloc(sizeof *l);
                level_init(l, x, y, z, name);
                level_gen(l, t);
                level_list_add(&s_levels, l);
            }
            else
            {
                char buf[64];
                snprintf(buf, sizeof buf, "Level '%s' already exists", name);
                client_add_packet(c, packet_send_message(0xFF, buf));
            }
	    }
	    else if (strcasecmp("setrank", param[0]) == 0)
	    {
	        int oldrank;
	        int newrank;
	        struct player_t *p;

	        if (c->player->rank < RANK_OP) goto unknown;

	        if (params != 3)
	        {
	            client_add_packet(c, packet_send_message(0xFF, "setrank [name] [rank]"));
	            return;
	        }

            newrank = rank_get_by_name(param[2]);
            if (newrank == -1 && c->player->rank == RANK_ADMIN)
            {
                client_add_packet(c, packet_send_message(0xFF, "Invalid rank: banned guest builder advbuilder op admin"));
                return;
            }
            if ((newrank == -1 || newrank >= RANK_OP) && c->player->rank == RANK_OP)
            {
                client_add_packet(c, packet_send_message(0xFF, "Invalid rank: banned guest builder advbuilder"));
                return;
            }

            oldrank = playerdb_get_rank(param[1]);
            if (oldrank == newrank)
            {
                client_add_packet(c, packet_send_message(0xFF, "User already at rank"));
                return;
            }
            if (c->player->rank != RANK_ADMIN && oldrank == RANK_ADMIN)
            {
                client_add_packet(c, packet_send_message(0xFF, "Cannot demote admin"));
                return;
            }

	        playerdb_set_rank(param[1], rank_get_by_name(param[2]));
	        p = player_get_by_name(param[1]);
	        if (p != NULL)
	        {
	            p->rank = newrank;
	        }
	    }
	    else if (strcasecmp("solid", param[0]) == 0)
	    {
	        if (c->player->rank < RANK_OP) goto unknown;

	        ToggleBit(c->player->flags, PLAYER_PLACE_SOLID);

	        char buf[64];
            snprintf(buf, sizeof buf, "Solid %s", HasBit(c->player->flags, PLAYER_PLACE_SOLID) ? "on" : "off");
            client_add_packet(c, packet_send_message(0xFF, buf));
	    }
	    else if (strcasecmp("undo", param[0]) == 0)
	    {
	        if (c->player->rank < RANK_OP) goto unknown;

	        if (params == 3)
	        {
                char buf[64], *bufp;
                struct dirent **namelist;
                int n, i;

                strcpy(buf, "Undo log: ");
                bufp = buf + strlen(buf);

                snprintf(s_pattern, sizeof s_pattern, "%s_%s_", param[2], param[1]);

                n = scandir("undo", &namelist, &undo_filename_filter, alphasort);
                if (n < 0)
                {
                    client_add_packet(c, packet_send_message(0xFF, "Unable to get list of undo logs"));
                    return;
                }

                for (i = 0; i < n; i++)
                {
                    size_t len = strlen(namelist[i]->d_name + strlen(s_pattern)) + (i < n - 1 ? 2 : 0);
                    if (len >= sizeof buf - (bufp - buf))
                    {
                        client_add_packet(c, packet_send_message(0xFF, buf));
                        bufp = buf;
                    }

                    strcpy(bufp, namelist[i]->d_name + strlen(s_pattern));
                    bufp += len;

                    free(namelist[i]);

                    if (i < n - 1) strcpy(bufp - 2, ", ");
                }

                client_add_packet(c, packet_send_message(0xFF, buf));
	        }
	        if (params != 4)
	        {
	            client_add_packet(c, packet_send_message(0xFF, "undo [user] [level]"));
	            client_add_packet(c, packet_send_message(0xFF, "undo [user] [level] [time]"));
	            return;
	        }

	        player_undo(param[1], param[2], param[3]);
	    }
	    else
	    {
unknown:
	        client_add_packet(c, packet_send_message(0xFF, "Unknown command"));
	        return;
	    }
	}
	else
	{
	    char buf[64];
	    snprintf(buf, sizeof buf, "%s: %s", c->player->username, message);
	    net_notify_all(buf);
	}
}
