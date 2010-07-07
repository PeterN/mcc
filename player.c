#include <stdio.h>
#include <string.h>
#include "player.h"
#include "mcc.h"

static struct player_list_t s_players;

bool player_t_compare(struct player_t **a, struct player_t **b)
{
    return *a == *b;
}

struct player_t *player_get_by_name(const char *username)
{
    int i;
    for (i = 0; i < s_players.used; i++)
	{
		if (strcmp(s_players.items[i]->username, username) == 0)
		{
		    return s_players.items[i];
		}
	}

	return NULL;
}

struct player_t *player_add(const char *username)
{
    struct player_t *p = malloc(sizeof *p);
    memset(p, 0, sizeof *p);
    p->username = strdup(username);

    player_list_add(&s_players, p);
    g_server.players++;

    return p;
}

void player_del(struct player_t *player)
{
    if (player == NULL) return;
    player_list_del(&s_players, player);
    g_server.players--;

    free(player);
}

void player_info()
{
    int i;
    for (i = 0; i < s_players.used; i++)
	{
	    printf("Player %d = %s\n", i, s_players.items[i]->username);
	}
}
