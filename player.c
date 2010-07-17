#include <stdio.h>
#include <string.h>
#include <time.h>
#include "bitstuff.h"
#include "client.h"
#include "level.h"
#include "packet.h"
#include "player.h"
#include "playerdb.h"
#include "mcc.h"
#include "network.h"
#include "util.h"

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
		if (strcasecmp(s_players.items[i]->username, username) == 0)
		{
		    return s_players.items[i];
		}
	}

	return NULL;
}

struct player_t *player_add(const char *username)
{
    int globalid = playerdb_get_globalid(username, true);
    if (globalid == -1) return NULL;

    struct player_t *p = malloc(sizeof *p);
    memset(p, 0, sizeof *p);
    p->colourusername = malloc(strlen(username + 3));
    p->username = p->colourusername + 2;
    p->rank = playerdb_get_rank(username);
    sprintf(p->colourusername, "&%x%s", rank_get_colour(p->rank), username);
    p->globalid = globalid;

    int i;
    for (i = 0; i < BLOCK_END; i++)
    {
        p->bindings[i] = i;
    }

    player_list_add(&s_players, p);
    g_server.players++;

    return p;
}

void player_del(struct player_t *player)
{
    if (player == NULL) return;
    player_list_del_item(&s_players, player);
    g_server.players--;

    if (player->undo_log != NULL)
    {
        fclose(player->undo_log);
    }

    free(player);
}

static void player_change_undo_log(struct player_t *player, struct level_t *level)
{
    if (player->undo_log != NULL)
    {
        fclose(player->undo_log);
    }

    player->undo_log = NULL;
}

bool player_change_level(struct player_t *player, struct level_t *level)
{
    if (player->level == level) return false;

    player->new_level = level;

    player_change_undo_log(player, level);

    return true;
}

void player_move(struct player_t *player, struct position_t *pos)
{
    player->pos = *pos;
}

void player_send_position(struct player_t *player)
{
	if (player->client->hidden) return;

    int changed = 0;
    int dx = 0, dy = 0, dz = 0;
    if (player->pos.x != player->oldpos.x || player->pos.y != player->oldpos.y || player->pos.z != player->oldpos.z)
    {
        changed = 1;
        dx = player->pos.x - player->oldpos.x;
        dy = player->pos.y - player->oldpos.y;
        dz = player->pos.z - player->oldpos.z;

        if (abs(dx) > 32 || abs(dy) > 32 || abs(dz) > 32)
        {
            changed = 4;
        }
    }
    if (player->pos.h != player->oldpos.h || player->pos.p != player->oldpos.p)
    {
        changed |= 2;
    }

    if (changed == 0) return;

    //printf("%s changed: %dx%dx%d (%d %d)\n", player->username, player->pos.x, player->pos.y, player->pos.z, player->pos.h, player->pos.p);

    //changed = 4;

    int i;
    for (i = 0; i < s_clients.used; i++)
    {
        struct client_t *c = s_clients.items[i];
        if (c->player != NULL && c->player != player && c->player->level == player->level)
        {
            switch (changed)
            {
                case 1:
                    client_add_packet(c, packet_send_position_update(player->levelid, dx, dy, dz));
                    break;

                case 2:
                    client_add_packet(c, packet_send_orientation_update(player->levelid, &player->pos));
                    break;

                case 3:
                    client_add_packet(c, packet_send_full_position_update(player->levelid, dx, dy, dz, &player->pos));
                    break;

                default:
                    client_add_packet(c, packet_send_teleport_player(player->levelid, &player->pos));
                    break;
            }
        }
    }

    player->oldpos = player->pos;
}

void player_send_positions()
{
    int i;
    for (i = 0; i < s_players.used; i++)
    {
    	struct player_t *player = s_players.items[i];
    	if (player->following != NULL)
    	{
    		player->pos = player->following->pos;
    		client_add_packet(player->client, packet_send_teleport_player(0xFF, &player->pos));
    		continue;
    	}

        player_send_position(player);
    }
}

void player_info()
{
    int i;
    for (i = 0; i < s_players.used; i++)
	{
	    printf("Player %d = %s\n", i, s_players.items[i]->username);
	}
}

void player_undo_log(struct player_t *player, unsigned index)
{
    /* Don't store undo logs for privileged users */
    if (player->rank >= RANK_ADV_BUILDER) return;

    if (player->undo_log == NULL)
    {
        time_t t = time(NULL);
        snprintf(player->undo_log_name, sizeof player->undo_log_name, "undo/%s_%s_%lu.bin", player->level->name, player->username, t);
        lcase(player->undo_log_name);

        player->undo_log = fopen(player->undo_log_name, "wb");
        if (player->undo_log == NULL)
        {
            LOG("Unable to open undo log %s", player->undo_log_name);
            return;
        }
    }

    fwrite(&index, sizeof index, 1, player->undo_log);
    fwrite(&player->level->blocks[index], sizeof player->level->blocks[index], 1, player->undo_log);
}

void player_undo(struct client_t *c, const char *username, const char *levelname, const char *timestamp)
{
    struct level_t *level;
    if (!level_get_by_name(levelname, &level))
    {
        return;
    }

    char buf[256];
    snprintf(buf, sizeof buf, "undo/%s_%s_%s.bin", levelname, username, timestamp);
    lcase(buf);

    struct player_t *player = player_get_by_name(username);
    if (player != NULL && strcasecmp(player->undo_log_name, buf) == 0)
    {
        /* Can't playback existing undo log, so... make a new one */
        player_change_undo_log(player, level);
    }

    FILE *f = fopen(buf, "rb");

    if (f == NULL)
    {
        client_notify(c, "No actions to undo");
        return;
    }

    /* Get length */
    fseek(f, 0, SEEK_END);
    size_t total_len = ftell(f);
    fseek(f, 0, SEEK_SET);

    unsigned index;
    struct block_t block;

    size_t len = sizeof index + sizeof block;
    int nmemb = total_len / len;
    int pos;

    for (pos = nmemb - 1; pos >= 0; pos--)
    {
        fseek(f, pos * len, SEEK_SET);
        fread(&index, sizeof index, 1, f);
        fread(&block, sizeof block, 1, f);

        level_change_block_force(level, &block, index);
    }

    fclose(f);

    snprintf(buf, sizeof buf, "Undone %d actions by %s", nmemb, username);
    net_notify_all(buf);
}

enum rank_t rank_get_by_name(const char *rank)
{
    if (!strcasecmp(rank, "banned")) return RANK_BANNED;
    if (!strcasecmp(rank, "guest")) return RANK_GUEST;
    if (!strcasecmp(rank, "builder")) return RANK_BUILDER;
    if (!strcasecmp(rank, "advbuilder")) return RANK_ADV_BUILDER;
    if (!strcasecmp(rank, "op")) return RANK_OP;
    if (!strcasecmp(rank, "admin")) return RANK_ADMIN;
    return -1;
}

static const char *s_ranks[] = {
	"banned",
	"guest",
	"builder",
	"advbuilder",
	"op",
	"admin",
};

const char *rank_get_name(enum rank_t rank)
{
	return s_ranks[rank];
}

enum colour_t rank_get_colour(enum rank_t rank)
{
    switch (rank)
    {
        case RANK_BANNED: return COLOUR_SILVER;
        case RANK_GUEST: return COLOUR_SILVER;
        case RANK_BUILDER: return COLOUR_LIME;
        case RANK_ADV_BUILDER: return COLOUR_GREEN;
        case RANK_OP: return COLOUR_TEAL;
        case RANK_ADMIN: return COLOUR_MAROON;
    }

    return COLOUR_YELLOW;
}