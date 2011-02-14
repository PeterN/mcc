#include <stdio.h>
#include <string.h>
#include "bitstuff.h"
#include "client.h"
#include "level.h"
#include "packet.h"
#include "player.h"
#include "playerdb.h"
#include "mcc.h"
#include "network.h"
#include "util.h"
#include "level_worker.h"
#include "gettime.h"

static struct player_list_t s_players;

bool player_t_compare(struct player_t **a, struct player_t **b)
{
	return *a == *b;
}

struct player_t *player_get_by_name(const char *username, bool partial)
{
	unsigned i;
	for (i = 0; i < s_players.used; i++)
	{
		if (strcasecmp(s_players.items[i]->username, username) == 0)
		{
			return s_players.items[i];
		}
	}

	if (partial)
	{
		int n = strlen(username);
		int c = 0;
		struct player_t *match = NULL;

		for (i = 0; i < s_players.used; i++)
		{
			struct player_t *p = s_players.items[i];
			if (!p->client->hidden && strncasecmp(p->username, username, n) == 0)
			{
				c++;
				match = p;
			}
		}

		if (c == 1) return match;
	}

	return NULL;
}

void player_set_alias(struct player_t *p, const char *alias, bool send_spawn)
{
	if (alias == NULL)
	{
		strncpy(p->gamealias, p->colourusername, sizeof p->gamealias);
		strncpy(p->gameusername, p->colourusername, sizeof p->gameusername);
	}
	else
	{
		strncpy(p->gamealias, alias, sizeof p->gamealias);
		strncpy(p->gameusername, p->colourusername, sizeof p->gameusername);
		p->gameusername[1] = p->gamealias[1];
	}

	if (p->client != NULL && !p->client->hidden && send_spawn)
	{
		// Renaming own client doesn't work
//		client_add_packet(p->client, packet_send_despawn_player(0xFF));
//		client_add_packet(p->client, packet_send_spawn_player(0xFF, p->alias, &p->pos));
		client_send_despawn(p->client, false);
		client_send_spawn(p->client, false);
	}
}

struct player_t *player_add(const char *username, struct client_t *c, bool *newuser, int *identified)
{
	int globalid = playerdb_get_globalid(username, true, newuser);
	if (globalid == -1) return NULL;

	struct player_t *p = malloc(sizeof *p);
	memset(p, 0, sizeof *p);
	p->username = p->colourusername + 2;
	p->rank = playerdb_get_rank(username);
	snprintf(p->colourusername, sizeof p->colourusername, "&%x%s", rank_get_colour(p->rank), username);
	p->globalid = globalid;
	player_set_alias(p, NULL, false);

	if (p->rank > RANK_BUILDER)
	{
		const char *last_ip = playerdb_get_last_ip(globalid, 1);
		if (last_ip != NULL && strcmp(c->ip, last_ip) == 0)
		{
			*identified = 2;
		}
		else
		{
			p->rank = RANK_BUILDER;
			*identified = 1;
		}
	}
	else
	{
		*identified = 0;
	}

	playerdb_log_visit(globalid, c->ip, (*identified) == 2);

	enum blocktype_t i;
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

	free(player);
}

bool player_change_level(struct player_t *player, struct level_t *level)
{
	/* Special case, when level is NULL, choose a starting level */
	if (level != NULL)
	{
		if (player->level == level) return false;
		if (level->delete) return false;
	}

	player->new_level = level;
	player->client->waiting_for_level = true;

	level_send_queue(player->client);

	return true;
}

void player_move(struct player_t *player, struct position_t *pos)
{
	/* If we've just teleported, don't allow a position change too far */
	if (player->teleport == true)
	{
		if (!position_match(&player->pos, pos, 64)) return;
		player->teleport = false;
	}

	int dx = abs(player->pos.x - pos->x);
	int dy = abs(player->pos.y - pos->y);
	int dz = abs(player->pos.z - pos->z);
	/* We only care that speed is above a threshold, therefore
	 * we don't need to find the square root. */

	unsigned i;
	player->speed = 0;
	for (i = 0; i < 10 - 1; i++)
	{
		player->speeds[i] = player->speeds[i + 1];
		player->speed += player->speeds[i];
	}
	player->speeds[i] = dx + dy + dz;
	player->speed += player->speeds[i];

	player->lastpos = player->pos;
	player->pos = *pos;

	if (player->level != NULL)
	{
		call_level_hook(EVENT_MOVE, player->level, player->client, &player->levelid);
	}
}

void player_teleport(struct player_t *player, const struct position_t *pos, bool instant)
{
	player->pos = *pos;
	player->lastpos = *pos;
	player->teleport = true;

	if (instant)
	{
		client_add_packet(player->client, packet_send_teleport_player(0xFF, &player->pos));
	}
}

bool player_check_spam(struct player_t *player)
{
	int len = 60;
	int ofs = player->spampos1 > player->spampos2 ? len : 0;
	int now = gettime();
	player->spam[player->spampos2] = now;

	while (player->spam[player->spampos1] < now - 2000 && player->spampos1 < player->spampos2 + ofs)
	{
		player->spampos1++;
		if (player->spampos1 >= len) { player->spampos1 = 0; ofs = 0; }
	}

	int blocks = player->spampos2 + ofs - player->spampos1;
	if (blocks > 20)
	{
		char buf[128];
		snprintf(buf, sizeof buf, "Anti-grief: %d blocks in %u ms", blocks, player->spam[player->spampos2] - player->spam[player->spampos1]);
		LOG("%s by %s\n", buf, player->username);

		if (blocks > 30)
		{
			net_close(player->client, buf);
			return true;
		}

		player->warnings++;
	}

	player->spampos2++;
	if (player->spampos2 >= len) player->spampos2 = 0;

	return false;
}

static void player_send_position(struct player_t *player, unsigned cur_tick)
{
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

	player->oldpos = player->pos;

	if (player->client->hidden) return;

	player->last_active = cur_tick;

	unsigned i;
	for (i = 0; i < MAX_CLIENTS_PER_LEVEL; i++)
	{
		struct client_t *c = player->level->clients[i];
		if (c == NULL || c->sending_level || c->player == player) continue;

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

void player_send_positions(unsigned cur_tick)
{
	unsigned i;
	for (i = 0; i < s_players.used; i++)
	{
		struct player_t *player = s_players.items[i];
		if (player->following != NULL)
		{
			player->pos = player->following->pos;
			player->pos.y -= 23;
			client_add_packet(player->client, packet_send_teleport_player(0xFF, &player->pos));
			continue;
		}

		player_send_position(player, cur_tick);
	}
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
	if (!strcasecmp(rank, "regular")) return RANK_REGULAR;
	if (!strcasecmp(rank, "builder")) return RANK_BUILDER;
	if (!strcasecmp(rank, "advbuilder")) return RANK_ADV_BUILDER;
	if (!strcasecmp(rank, "mod")) return RANK_MOD;
	if (!strcasecmp(rank, "op")) return RANK_OP;
	if (!strcasecmp(rank, "admin")) return RANK_ADMIN;
	return -1;
}

const char *rank_get_name(enum rank_t rank)
{
	static char numeric_rank[5];

	switch (rank)
	{
		case RANK_BANNED:      return "banned";
		case RANK_GUEST:       return "guest";
		case RANK_REGULAR:     return "regular";
		case RANK_BUILDER:     return "builder";
		case RANK_ADV_BUILDER: return "advbuilder";
		case RANK_MOD:         return "mod";
		case RANK_OP:          return "op";
		case RANK_ADMIN:       return "admin";
		default:
			snprintf(numeric_rank, sizeof numeric_rank, "%d", rank);
			return numeric_rank;
	}
}

enum colour_t rank_get_colour(enum rank_t rank)
{
	switch (rank)
	{
		case RANK_BANNED: return COLOUR_SILVER;
		case RANK_GUEST: return COLOUR_SILVER;
		case RANK_REGULAR: return COLOUR_GOLD;
		case RANK_BUILDER: return COLOUR_LIME;
		case RANK_ADV_BUILDER: return COLOUR_GREEN;
		case RANK_MOD: return COLOUR_TEAL;
		case RANK_OP: return COLOUR_AQUA;
		case RANK_ADMIN: return COLOUR_MAROON;
	}

	return COLOUR_YELLOW;
}

void player_list(struct client_t *c, const struct level_t *l)
{
	char buf[4096];
	unsigned i;
	for (i = 0; i < 5; i++)
	{
		bool added = false;
		char *bufp = buf;
		memset(buf, 0, sizeof buf);

		switch (i) {
			case 0: snprintf(buf, sizeof buf, TAG_TEAL "Ops: "); break;
			case 1: snprintf(buf, sizeof buf, TAG_TEAL "Mods: "); break;
			case 2: snprintf(buf, sizeof buf, TAG_GREEN "Builders: "); break;
			case 3: snprintf(buf, sizeof buf, TAG_GOLD "Regulars: "); break;
			case 4: snprintf(buf, sizeof buf, TAG_SILVER "Guests: "); break;
		}
		bufp += strlen(buf);

		unsigned j;
		for (j = 0; j < s_players.used; j++)
		{
			const struct player_t *p = s_players.items[j];
			if (p == NULL) continue;
			if (l != NULL && p->level != l) continue;

			if (i == 0 && p->rank < RANK_OP) continue;
			if (i == 1 && (p->rank < RANK_MOD || p->rank >= RANK_OP)) continue;
			if (i == 2 && (p->rank < RANK_BUILDER || p->rank >= RANK_MOD)) continue;
			if (i == 3 && (p->rank < RANK_REGULAR || p->rank >= RANK_BUILDER)) continue;
			if (i == 4 && p->rank >= RANK_REGULAR) continue;

			if (c->player->rank < RANK_OP && p->client->hidden) continue;

			size_t len = strlen(p->colourusername) + 1;
			if (len >= sizeof buf - (bufp - buf))
			{
				client_notify(c, buf);
				bufp = buf;
				memset(buf, 0, sizeof buf);
			}

			strcpy(bufp, p->colourusername);
			bufp += len - 1;
			*bufp = ' ';
			bufp++;

			added = true;
		}

		if (added) client_notify(c, buf);
	}
}
