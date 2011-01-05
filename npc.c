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
#include "list.h"

static struct npc_list_t s_npcs;

bool npc_compare(struct npc **a, struct npc **b)
{
	return *a == *b;
}

LIST(npc, struct npc *, npc_compare);

/*struct player_t *player_add(const char *username, struct client_t *c, bool *newuser, int *identified)
{
	struct player_t *p = malloc(sizeof *p);
	memset(p, 0, sizeof *p);
	p->username = p->colourusername + 2;
	p->globalid = globalid;

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
}*/

#if 0
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

static unsigned gettime(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);

	return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
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
#endif

void npc_send_position(struct npc *npc)
{
	int changed = 0;
	int dx = 0, dy = 0, dz = 0;
	if (npc->pos.x != npc->oldpos.x || npc->pos.y != npc->oldpos.y || npc->pos.z != npc->oldpos.z)
	{
		changed = 1;
		dx = npc->pos.x - npc->oldpos.x;
		dy = npc->pos.y - npc->oldpos.y;
		dz = npc->pos.z - npc->oldpos.z;

		if (abs(dx) > 32 || abs(dy) > 32 || abs(dz) > 32)
		{
			changed = 4;
		}
	}
	if (npc->pos.h != npc->oldpos.h || npc->pos.p != npc->oldpos.p)
	{
		changed |= 2;
	}

	if (changed == 0) return;

	npc->oldpos = npc->pos;


	unsigned i;
	for (i = 0; i < MAX_CLIENTS_PER_LEVEL; i++)
	{
		struct client_t *c = npc->level->clients[i];
		if (c != NULL)
		{
			switch (changed)
			{
				case 1:
					client_add_packet(c, packet_send_position_update(npc->levelid, dx, dy, dz));
					break;

				case 2:
					client_add_packet(c, packet_send_orientation_update(npc->levelid, &npc->pos));
					break;

				case 3:
					client_add_packet(c, packet_send_full_position_update(npc->levelid, dx, dy, dz, &npc->pos));
					break;

				default:
					client_add_packet(c, packet_send_teleport_player(npc->levelid, &npc->pos));
					break;
			}
		}
	}
}

void npc_send_positions(void)
{
	unsigned i;
	for (i = 0; i < s_npcs.used; i++)
	{
		struct npc *npc = s_npcs.items[i];
		npc_send_position(npc);
	}
}
