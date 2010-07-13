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
    p->rank = playerdb_get_rank(username);
    p->globalid = playerdb_get_globalid(username);

    player_list_add(&s_players, p);
    g_server.players++;

    return p;
}

void player_del(struct player_t *player)
{
    if (player == NULL) return;
    player_list_del(&s_players, player);
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

    player->level = level;

    player_change_undo_log(player, level);

    char buf[64];
    snprintf(buf, sizeof buf, "%s moved to '%s'", player->username, level->name);
    net_notify_all(buf);

    return true;
}

void player_move(struct player_t *player, struct position_t *pos)
{
    player->pos = *pos;
}

void player_send_position(struct player_t *player)
{
    int changed = 0;
    if (player->pos.x != player->oldpos.x || player->pos.y != player->oldpos.y || player->pos.z != player->oldpos.z)
    {
        changed = 1;
    }
    if (player->pos.h != player->oldpos.h || player->pos.p != player->oldpos.p)
    {
        changed |= 2;
    }

    int i;
    for (i = 0; i < s_clients.used; i++)
    {
        struct client_t *c = &s_clients.items[i];
        if (c->player != NULL && c->player != player && c->player->level == player->level)
        {
            client_add_packet(c, packet_send_teleport_player(c->player->levelid, &player->pos));
        }
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
        player->undo_log = fopen(player->undo_log_name, "wb");
        if (player->undo_log == NULL)
        {
            char buf[64];
            snprintf(buf, sizeof buf, "Unable to open undo log %s", player->undo_log_name);
            net_notify_all(buf);
            return;
        }
    }

    fwrite(&index, sizeof index, 1, player->undo_log);
    fwrite(&player->level->blocks[index], sizeof player->level->blocks[index], 1, player->undo_log);
}

void player_undo(const char *username, const char *levelname, const char *timestamp)
{
    struct level_t *level;
    if (!level_get_by_name(levelname, &level))
    {
        return;
    }

    char buf[256];
    snprintf(buf, sizeof buf, "undo/%s_%s_%s.bin", levelname, username, timestamp);

    struct player_t *player = player_get_by_name(username);
    if (player != NULL && strcmp(player->undo_log_name, buf) == 0)
    {
        /* Can't playback existing undo log, so... make a new one */
        player_change_undo_log(player, level);
    }

    FILE *f = fopen(buf, "rb");

    if (f == NULL)
    {
        net_notify_all("No actions to undo");
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
