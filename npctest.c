#include <stdio.h>
#include "client.h"
#include "player.h"
#include "level.h"

struct npctest
{
	int i;
	struct {
		char name[64];
		struct position_t pos;
		struct npc *npc;
	} n[4];
};

void npctest_save(struct level_t *l, struct npctest *arg)
{
	int i;
	for (i = 0; i < arg->i; i++)
	{
		arg->n[i].pos = arg->n[i].npc->pos;
	}
}

void npctest_init(struct level_t *l, struct npctest *arg)
{
	int i;
	for (i = 0; i < arg->i; i++)
	{
		arg->n[i].npc = npc_add(l, arg->n[i].name, arg->n[i].pos);
	}
}

void npctest_deinit(struct level_t *l, struct npctest *arg)
{
	int i;
	for (i = 0; i < arg->i; i++)
	{
		npc_del(arg->n[i].npc);
	}
}

static bool npctest_handle_chat(struct level_t *l, struct client_t *c, char *data, struct npctest *arg)
{
	if (c->player->rank < RANK_OP) return false;

	if (strncasecmp(data, "npc add ", 8) == 0)
	{
		if (arg->i < 3)
		{
			snprintf(arg->n[arg->i].name, sizeof arg->n[arg->i].name, data + 8);

			arg->n[arg->i].npc = npc_add(l, arg->n[arg->i].name, c->player->pos);
			arg->i++;
		}
	}
	else if (strcasecmp(data, "npc del") == 0)
	{
		if (arg->i > 0)
		{
			arg->i--;
			npc_del(arg->n[arg->i].npc);
		}
	}
	else
	{
		return false;
	}

	return true;
}

static void npctest_handle_move(struct level_t *l, struct client_t *c, int index, struct npctest *arg)
{
	/* Changing levels, don't handle teleports */
	if (c->player->level != c->player->new_level) return;
}

static bool npctest_level_hook(int event, struct level_t *l, struct client_t *c, void *data, struct level_hook_data_t *arg)
{
	switch (event)
	{
		case EVENT_TICK: break;
		case EVENT_CHAT: return npctest_handle_chat(l, c, data, arg->data);
		case EVENT_MOVE: npctest_handle_move(l, c, *(int *)data, arg->data); break;
		case EVENT_SAVE: npctest_save(l, arg->data); break;
		case EVENT_INIT:
		{
			if (arg->size == 0)
			{
				LOG("Allocating new npctest data on %s\n", l->name);
			}
			else
			{
				if (arg->size == sizeof (struct npctest))
				{
					npctest_init(l, arg->data);
					break;
				}

				LOG("Found invalid npctest data on %s, erasing\n", l->name);
				free(arg->data);
			}

			arg->size = sizeof (struct npctest);
			arg->data = calloc(1, arg->size);
			break;
		}
		case EVENT_DEINIT:
		{
			if (l == NULL) break;

			npctest_deinit(l, arg->data);
		}
	}

	return false;
}

void module_init(void **data)
{
	register_level_hook_func("npctest", &npctest_level_hook);
}

void module_deinit(void *data)
{
	deregister_level_hook_func("npctest");
}
