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

static const char s_zombie_name[] = TAG_RED "Undead_zombie";

struct zombies_t
{
	int ticksremaining;
};

static void zombie_handle_chat(struct level_t *l, struct client_t *c, char *data, struct zombies_t *arg)
{
	if (!level_user_can_build(l, c->player)) return;

	if (strcasecmp(data, "zombies start") == 0)
	{
		arg->ticksremaining = 300;
		level_notify_all(l, TAG_GREEN "Game started!");

		int max = 1000;
		while (--max > 0)
		{
			int i = rand() % MAX_CLIENTS_PER_LEVEL;
			if (l->clients[i] != NULL)
			{
				char buf[128];
				snprintf(buf, sizeof buf, TAG_RED "%s has turned into a zombie!", l->clients[i]->player->username);
				level_notify_all(l, buf);

				player_set_alias(l->clients[i]->player, s_zombie_name);
				break;
			}
		}

		if (max == 0)
		{
			level_notify_all(l, TAG_YELLOW "No zombie found :(");
			arg->ticksremaining = 1;
		}
	}
	else if (strcasecmp(data, "zombies stop") == 0)
	{
		/* Let game finish in the next tick */
		arg->ticksremaining = 1;
	}
}

static void zombie_handle_move(struct level_t *l, struct client_t *c, int index, struct zombies_t *arg)
{
	/* Changing levels, don't handle teleports */
	if (c->player->level != c->player->new_level) return;

	/* Player isn't a zombie */
	if (strncmp(c->player->alias, TAG_RED, 2) != 0) return;

	int i;
	for (i = 0; i < MAX_CLIENTS_PER_LEVEL; i++)
	{
		struct client_t *cl = l->clients[i];
		/* cl is self, or already a zombie? */
		if (cl == c) continue;
		if (strncmp(cl->player->alias, TAG_RED, 2) == 0) continue;

		if (position_match(&c->player->pos, &cl->player->pos, 32))
		{
			char buf[128];
			snprintf(buf, sizeof buf, "%s has infected %s!", c->player->alias, cl->player->alias);
			level_notify_all(l, buf);

			player_set_alias(cl->player, s_zombie_name);
		}
	}
}

static void zombie_handle_tick(struct level_t *l, struct client_t *c, char *data, struct zombies_t *arg)
{
	if (arg->ticksremaining > 0)
	{
		arg->ticksremaining--;
		if (arg->ticksremaining == 0)
		{
			level_notify_all(l, TAG_YELLOW "Game Over!");
		}
	}
}

static void zombie_handle_spawn(struct level_t *l, struct client_t *c, char *data, struct zombies_t *arg)
{
	if (arg->ticksremaining > 0)
	{
		client_notify(c, TAG_YELLOW "Game in progress, spawning you as a zombie...");
		player_set_alias(c->player, s_zombie_name);
	}
}

static void zombie_handle_despawn(struct level_t *l, struct client_t *c, char *data, struct zombies_t *arg)
{
	player_set_alias(c->player, NULL);
}

static void zombie_level_hook(int event, struct level_t *l, struct client_t *c, void *data, struct level_hook_data_t *arg)
{
	switch (event)
	{
		case EVENT_TICK: zombie_handle_tick(l, c, data, arg->data); break;
		case EVENT_CHAT: zombie_handle_chat(l, c, data, arg->data); break;
		case EVENT_MOVE: zombie_handle_move(l, c, *(int *)data, arg->data); break;
		case EVENT_SPAWN: zombie_handle_spawn(l, c, data, arg->data); break;
		case EVENT_DESPAWN: zombie_handle_despawn(l, c, data, arg->data); break;
		case EVENT_INIT:
		{
			if (arg->size == 0)
			{
				LOG("Allocating new zombie data on %s\n", l->name);
			}
			else
			{
				if (arg->size == sizeof (struct zombies_t))
				{
					break;
				}

				LOG("Found invalid zombie data on %s, erasing\n", l->name);
				free(arg->data);
			}

			arg->size = sizeof (struct zombies_t);
			arg->data = calloc(1, arg->size);
			break;
		}
	}
}

void module_init(void **data)
{
	register_level_hook_func("zomibes", &zombie_level_hook);
}

void module_deinit(void *data)
{
	deregister_level_hook_func("zombies");
}
