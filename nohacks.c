#include <stdio.h>
#include <string.h>
#include "bitstuff.h"
#include "block.h"
#include "colour.h"
#include "client.h"
#include "level.h"
#include "player.h"
#include "position.h"
#include "mcc.h"
#include "network.h"

struct nohacks_t
{
	bool nohacks;
	bool game;
};

static void nohacks_handle_chat(struct level_t *l, struct client_t *c, char *data, struct nohacks_t *arg)
{
	if (c->player->rank < RANK_OP) return;

	if (strcasecmp(data, "hacks off") == 0)
	{
		arg->nohacks = true;
	}
	else if (strcasecmp(data, "hacks on") == 0)
	{
		arg->nohacks = false;
	}
	else if (strcasecmp(data, "game off") == 0)
	{
		arg->game = false;
		int i;
		for (i = 0; i < MAX_CLIENTS_PER_LEVEL; i++)
		{
			struct client_t *cl = l->clients[i];
			if (cl == NULL) continue;

			ClrBit(cl->player->flags, FLAG_GAMES);
		}
	}
	else if (strcasecmp(data, "game on") == 0)
	{
		arg->game = true;
		int i;
		for (i = 0; i < MAX_CLIENTS_PER_LEVEL; i++)
		{
			struct client_t *cl = l->clients[i];
			if (cl == NULL) continue;

			SetBit(cl->player->flags, FLAG_GAMES);
		}
	}
}

static void nohacks_handle_move(struct level_t *l, struct client_t *c, int index, struct nohacks_t *arg)
{
	/* Changing levels, don't handle teleports */
	if (c->player->level != c->player->new_level) return;

	if (!arg->nohacks) return;

	if (!c->player->teleport)
	{
		struct player_t *player = c->player;
		int dx = player->pos.x - player->lastpos.x;
		int dy = player->pos.y - player->lastpos.y;
		int dz = player->pos.z - player->lastpos.z;

		/* Calculate diagonal distance */
		int dp = dx * dx + dz * dz;

		if (dp > 100 || dy > 20 || dy < -120)
		{
			/* Player moved too far! */
			player_teleport(player, &player->oldpos, true);

			if (dp < 100000)
			{
				client_notify(c, TAG_RED "Client hacks are forbidden during game");
			}
			else
			{
				client_notify(c, TAG_RED "Teleporting forbidden during game");
			}
		}
	}
}

static void nohacks_handle_spawn(struct level_t *l, struct client_t *c, char *data, struct nohacks_t *arg)
{
	if (arg->nohacks)
	{
		char buf[512];
		snprintf(buf, sizeof buf,
			TAG_YELLOW "Welcome to %s. This level is running no hacks. "
			TAG_RED "ALL hacks must be disabled.",
			l->name);
		client_notify(c, buf);
	}

	if (arg->game)
	{
		SetBit(c->player->flags, FLAG_GAMES);
	}
}

static void nohacks_handle_despawn(struct level_t *l, struct client_t *c, char *data, struct nohacks_t *arg)
{
	if (arg->game)
	{
		ClrBit(c->player->flags, FLAG_GAMES);
	}
}

static void nohacks_level_hook(int event, struct level_t *l, struct client_t *c, void *data, struct level_hook_data_t *arg)
{
	switch (event)
	{
		case EVENT_CHAT: nohacks_handle_chat(l, c, data, arg->data); break;
		case EVENT_MOVE: nohacks_handle_move(l, c, *(int *)data, arg->data); break;
		case EVENT_SPAWN: nohacks_handle_spawn(l, c, data, arg->data); break;
		case EVENT_DESPAWN: nohacks_handle_despawn(l, c, data, arg->data); break;
		case EVENT_INIT:
		{
			if (arg->size == 0)
			{
				LOG("Allocating new nohacks data on %s\n", l->name);
			}
			else
			{
				if (arg->size == sizeof (struct nohacks_t))
				{
					break;
				}

				LOG("Found invalid nohacks data on %s, erasing\n", l->name);
				free(arg->data);
			}

			arg->size = sizeof (struct nohacks_t);
			arg->data = calloc(1, arg->size);
			break;
		}
		case EVENT_DEINIT:
		{
			if (l == NULL) return;

			int i;
			for (i = 0; i < MAX_CLIENTS_PER_LEVEL; i++)
			{
				struct client_t *cl = l->clients[i];
				if (cl == NULL) continue;

				player_set_alias(cl->player, NULL, true);
				ClrBit(cl->player->flags, FLAG_GAMES);
			}
		}
	}
}

void module_init(void **data)
{
	register_level_hook_func("nohacks", &nohacks_level_hook);
}

void module_deinit(void *data)
{
	deregister_level_hook_func("nohacks");
}
