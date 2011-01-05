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

static void npc_send_spawn(struct npc *npc)
{
	struct level_t *level = npc->level;

	unsigned i;
	for (i = 0; i < MAX_CLIENTS_PER_LEVEL; i++)
	{
		if (level->clients[i] != NULL)
		{
			client_add_packet(level->clients[i], packet_send_spawn_player(npc->levelid, npc->name, &npc->pos));
		}
	}
}

static void npc_send_despawn(struct npc *npc)
{
	struct level_t *level = npc->level;

	unsigned i;
	for (i = 0; i < MAX_CLIENTS_PER_LEVEL; i++)
	{
		if (level->clients[i] != NULL)
		{
			client_add_packet(level->clients[i], packet_send_despawn_player(npc->levelid));
		}
	}
}

struct npc *npc_add(struct level_t *level, const char *name, struct position_t position)
{
	struct npc *npc = malloc(sizeof *npc);
	memset(npc, 0, sizeof *npc);

	npc->levelid = level_get_new_npc_id(level, npc);
	if (npc->levelid == -1)
	{
		free(npc);
		return NULL;
	}

	npc->level = level;
	npc->levelid += MAX_CLIENTS_PER_LEVEL;

	snprintf(npc->name, sizeof npc->name, TAG_BLUE "%s", name);
	npc->pos  = position;
	npc_list_add(&s_npcs, npc);

	npc_send_spawn(npc);

	return npc;
}

void npc_del(struct npc *npc)
{
	if (npc == NULL) return;

	npc_send_despawn(npc);

	/* Remove npc from level's list */
	if (npc->level != NULL)
	{
		npc->level->npcs[npc->levelid - MAX_CLIENTS_PER_LEVEL] = NULL;
	}

	npc_list_del_item(&s_npcs, npc);

	free(npc);
}

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



