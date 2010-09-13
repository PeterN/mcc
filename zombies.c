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

static const char s_zombie_name[] = TAG_RED "Undead_zombie";
static const char s_mod_name[] = TAG_YELLOW "Moderator";
static const int s_interval = 25 * 30;

struct zombies_t
{
	int ticksremaining;
	int intervalticks;
	int zombieticks;
	int zombiewins;
	int humanwins;
};

static bool is_zombie(const struct player_t *p)
{
	return p->alias[1] == 'c';
}

static bool is_mod(const struct player_t *p)
{
	return p->alias[1] == 'e';
}

static void zombie_start(struct level_t *l, struct zombies_t *arg)
{
	arg->ticksremaining = 7500;
	arg->intervalticks = 0;
	arg->zombieticks = 250;

	level_notify_all(l, TAG_GREEN "Game started!");

	int zombie = -1;
	int max = 1000;
	while (--max > 0)
	{
		zombie = rand() % MAX_CLIENTS_PER_LEVEL;
		if (l->clients[zombie] != NULL && !is_mod(l->clients[zombie]->player)) break;
	}

	if (zombie == -1)
	{
		level_notify_all(l, TAG_YELLOW "No zombie found :(");
		arg->ticksremaining = 0;
		arg->intervalticks = s_interval;
		return;
	}

	int i;
	int players = 0;
	for (i = 0; i < MAX_CLIENTS_PER_LEVEL; i++)
	{
		if (l->clients[i] != NULL && !is_mod(l->clients[i]->player)) players++;
	}

	if (players < 2)
	{
		level_notify_all(l, TAG_YELLOW "Not enough players :(");
		arg->ticksremaining = 0;
		arg->intervalticks = s_interval;
		return;
	}

	for (i = 0; i < MAX_CLIENTS_PER_LEVEL; i++)
	{
		if (l->clients[i] != NULL && !is_mod(l->clients[i]->player))
		{
			if (i != zombie)
			{
				player_teleport(l->clients[i]->player, &l->spawn, true);
				player_set_alias(l->clients[i]->player, NULL, true);
			}
			else
			{
				char buf[128];
				snprintf(buf, sizeof buf, TAG_RED "%s has turned into a zombie!", l->clients[i]->player->username);
				level_notify_all(l, buf);

				player_teleport(l->clients[i]->player, &l->spawn, true);
				player_set_alias(l->clients[i]->player, s_zombie_name, true);
			}
		}
	}
}

static void zombie_handle_chat(struct level_t *l, struct client_t *c, char *data, struct zombies_t *arg)
{
	if (c->player->rank < RANK_OP) return;

	if (strcasecmp(data, "zombies start") == 0)
	{
		arg->intervalticks = 5 * 25 + 1;
	}
	else if (strcasecmp(data, "zombies stop") == 0)
	{
		/* Let game finish in the next tick */
		arg->ticksremaining = 0;
		arg->intervalticks = 0;
	}
	else if (strcasecmp(data, "zombies scores") == 0)
	{
		char buf[128];
		snprintf(buf, sizeof buf, TAG_LIME "The scores are: Zombies %d - %d Humans", arg->zombiewins, arg->humanwins);
		level_notify_all(l, buf);
	}
	else if (strcasecmp(data, "zombies reset scores") == 0)
	{
		arg->zombiewins = 0;
		arg->humanwins = 0;
		level_notify_all(l, TAG_LIME "Scores reset!");
	}
	else if (strcasecmp(data, "zombies moderate") == 0)
	{
		player_set_alias(c->player, s_mod_name, true);
		char buf[128];
		snprintf(buf, sizeof buf, "%s " TAG_YELLOW "has become a moderator", c->player->colourusername);
		level_notify_all(l, buf);

		ClrBit(c->player->flags, FLAG_GAMES);
	}
	else if (strcasecmp(data, "zombies play") == 0)
	{
		player_set_alias(c->player, s_zombie_name, true);
		char buf[128];
		snprintf(buf, sizeof buf, "%s " TAG_YELLOW "is back in the game", c->player->colourusername);
		level_notify_all(l, buf);

		SetBit(c->player->flags, FLAG_GAMES);
	}
}

static void zombie_handle_move(struct level_t *l, struct client_t *c, int index, struct zombies_t *arg)
{
	/* Changing levels, don't handle teleports */
	if (c->player->level != c->player->new_level) return;

	if (arg->ticksremaining == 0) return;

	if (is_mod(c->player)) return;

	if (!c->player->teleport)
	{
		if (arg->zombieticks > 0 && is_zombie(c->player))
		{
			/* Zombie can't move yet */
			player_teleport(c->player, &c->player->oldpos, true);
			return;
		}

		struct player_t *player = c->player;
		int dx = player->pos.x - player->lastpos.x;
		int dy = player->pos.y - player->lastpos.y;
		int dz = player->pos.z - player->lastpos.z;

		if (abs(dx) > 32 || dy > 32 || dy < -96 || abs(dz) > 32)
		{
			/* Player moved too far! */
			player_teleport(player, &player->oldpos, true);
			client_notify(c, TAG_RED "Teleporting forbidden during game");
		}
	}

	if (arg->zombieticks > 0) return;

	/* Player isn't a zombie */
	if (!is_zombie(c->player)) return;

	int i;
	for (i = 0; i < MAX_CLIENTS_PER_LEVEL; i++)
	{
		struct client_t *cl = l->clients[i];
		/* cl is self, or already a zombie? */
		if (cl == NULL || cl == c) continue;
		if (is_zombie(cl->player) || is_mod(cl->player)) continue;

		if (position_match(&c->player->pos, &cl->player->pos, 32))
		{
			char buf[128];
			snprintf(buf, sizeof buf, TAG_RED "%s has infected %s!", c->player->username, cl->player->username);
			level_notify_all(l, buf);

			client_notify(cl, TAG_RED "You have been infected!");

			player_set_alias(cl->player, s_zombie_name, true);
		}
	}
}

static void zombie_totals(struct level_t *l, struct zombies_t *arg)
{
	char buf[128];
	snprintf(buf, sizeof buf, TAG_LIME "And the scores are: Zombies %d - %d Humans", arg->zombiewins, arg->humanwins);
	level_notify_all(l, buf);
}

static void zombie_handle_tick(struct level_t *l, struct client_t *c, char *data, struct zombies_t *arg)
{
	if (arg->ticksremaining == 0)
	{
		if (arg->intervalticks > 0)
		{
			
			if (arg->intervalticks == s_interval)
			{
				char buf[128];
				//snprintf(buf, sizeof buf, TAG_YELLOW "Game will restart in %d seconds", s_interval / 25);
				//level_notify_all(l, buf);
				snprintf(buf, sizeof buf, TAG_YELLOW "! New game in " TAG_WHITE "%s " TAG_YELLOW "will start in %d seconds", l->name, s_interval / 25);
				net_notify_all(buf);
			}
			arg->intervalticks--;
			if (arg->intervalticks == 5 * 25)
			{
				level_notify_all(l, TAG_YELLOW "New game will start in 5 seconds!");
			}
			else if (arg->intervalticks == 0)
			{
				zombie_start(l, arg);
			}
		}
		return;
	}

	if (arg->zombieticks > 0)
	{
		arg->zombieticks--;
		if (arg->zombieticks == 0)
		{
			level_notify_all(l, TAG_RED "The zombie is on the move!");
		}
	}

	int alive = 0;
	int dead = 0;

	if (arg->ticksremaining > 250)
	{
		if (arg->ticksremaining % 1500 == 0)
		{
			char buf[64];
			snprintf(buf, sizeof buf, TAG_YELLOW "%d minute%s remaining", arg->ticksremaining / 1500, arg->ticksremaining > 1500 ? "s" : "");
			level_notify_all(l, buf);
		}
	}
	else
	{
		if (arg->ticksremaining % 25 == 0)
		{
			char buf[64];
			snprintf(buf, sizeof buf, TAG_YELLOW "%d second%s remaining", arg->ticksremaining / 25, arg->ticksremaining > 25 ? "s" : "");
			level_notify_all(l, buf);
		}
	}

	int i;

	arg->ticksremaining--;
	if (arg->ticksremaining == 0)
	{
		level_notify_all(l, TAG_YELLOW "Game Over!");
	}
	else if (arg->ticksremaining % 375 == 0)
	{
		for (i = 0; i < MAX_CLIENTS_PER_LEVEL; i++)
		{
			struct client_t *cl = l->clients[i];
			if (cl == NULL) continue;

			int j = rand() % 3;
			if (is_zombie(cl->player))
			{
				static const char *str[] = {
					TAG_RED "Eat the humans!",
					TAG_RED "Infect the humans!",
					TAG_RED "Eat their braaaaiiiinnnnzzzz...",
				};
				client_notify(cl, str[j]);
			}
			else if (!is_mod(cl->player))
			{
				static const char *str[] = {
					TAG_GREEN "Hide from the zombies!",
					TAG_GREEN "The zombies are coming...",
					TAG_GREEN "The zombies want your braaaaiiiinnnnzzzz...",
				};
				client_notify(cl, str[j]);
			}
		}
	}

	for (i = 0; i < MAX_CLIENTS_PER_LEVEL; i++)
	{
		struct client_t *cl = l->clients[i];
		if (cl == NULL) continue;

		if (is_zombie(cl->player))
		{
			dead++;
		}
		else if (!is_mod(cl->player))
		{
			alive++;
		}
	}

	if (arg->ticksremaining > 0 && alive == 0)
	{
		arg->ticksremaining = 1;
		return;
	}

	if (alive == 0)
	{
		level_notify_all(l, TAG_RED "Zombies win!");
		arg->zombiewins++;
		arg->intervalticks = s_interval;
		zombie_totals(l, arg);
	}
	else if (arg->ticksremaining == 0)
	{
		level_notify_all(l, TAG_RED "Zombies fail, humans are alive...");
		arg->humanwins++;
		arg->intervalticks = s_interval;
		zombie_totals(l, arg);
	}
	else if (arg->ticksremaining % 750 == 0)
	{
		char buf[128];
		snprintf(buf, sizeof buf, TAG_YELLOW "%d human%s alive...", alive, alive == 1 ? "" : "s");
		level_notify_all(l, buf);
	}
}

static void zombie_handle_spawn(struct level_t *l, struct client_t *c, char *data, struct zombies_t *arg)
{
	char buf[512];
	snprintf(buf, sizeof buf,
		TAG_YELLOW "Welcome to %s. This level is running zombies. "
		TAG_WHITE "Rules of the game: Humans must stay alive for %d minutes by running away. "
		"Zombies must infect the humans by walking into them. "
		"Zombies win if all the humans are infected by the end of the game. "
		TAG_RED "ALL hacks must be disabled. Do NOT build things here that you'd like to keep, use your /home for that.",
		l->name, 7500 / 1500);
	client_notify(c, buf);

	if (arg->ticksremaining > 0)
	{
		client_notify(c, TAG_RED "Game in progress! You will start as a zombie!");
		player_set_alias(c->player, s_zombie_name, false);
	}

	SetBit(c->player->flags, FLAG_GAMES);
}

static void zombie_handle_despawn(struct level_t *l, struct client_t *c, char *data, struct zombies_t *arg)
{
	player_set_alias(c->player, NULL, false);
	ClrBit(c->player->flags, FLAG_GAMES);
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
	register_level_hook_func("zombies", &zombie_level_hook);
}

void module_deinit(void *data)
{
	deregister_level_hook_func("zombies");
}
