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
#include "playerdb.h"

static const char s_mod_name[] = TAG_YELLOW "Moderator";
static const int s_interval = 25 * 30;

static enum blocktype_t s_explosion;
static enum blocktype_t s_fire;
static enum blocktype_t s_active_tnt;
static enum blocktype_t s_fuse;

enum
{
	WAR_SPEC,
	WAR_RED,
	WAR_BLUE,
	WAR_MOD,
};

enum
{
	PAIN_HIT,
	PAIN_FIRE,
	PAIN_FALL,
	PAIN_DROWN,
	PAIN_BURIED,
};

const char *s_death_text[] =
{
	TAG_RED "%s was hit by %s's explosion",
	TAG_RED "%s was set fire by %s",
	TAG_RED "%s has died by falling",
	TAG_RED "%s was drowned by %s",
	TAG_RED "%s was buried by %s",
};

const char *s_suicide_text[] =
{
	TAG_RED "%s was hit by an explosion",
	TAG_RED "%s caught fire and died",
	TAG_RED "%s has died by falling",
	TAG_RED "%s has died by drowning",
	TAG_RED "%s was buried alive",
};

struct wartemp
{
	int8_t team;
	int8_t air;
	int8_t life;
	int8_t maxdy;
	int8_t lastpain;
	unsigned killer;
};

struct war
{
	int ticksremaining;
	int intervalticks;
	int redwins;
	int bluewins;
	int king;
	struct position_t redarea;
	struct position_t bluearea;
	struct wartemp *t;
};

static void set_alias(struct player_t *p, const char *colour)
{
	char buf[128];
	snprintf(buf, sizeof buf, "%s%s", colour, p->username);
	player_set_alias(p, buf, true);
}

static void set_colour(struct player_t *p, int8_t team, bool king)
{
	switch (team)
	{
		case WAR_RED:  set_alias(p, king ? TAG_RED : TAG_MAROON); break;
		case WAR_BLUE: set_alias(p, TAG_NAVY); break;
		case WAR_SPEC: set_alias(p, TAG_SILVER); break;
		case WAR_MOD:  set_alias(p, TAG_YELLOW); break;
	}
}

static void war_start(struct level_t *l, struct war *war)
{
	int i;
	int players = 0;
	for (i = 0; i < MAX_CLIENTS_PER_LEVEL; i++)
	{
		if (l->clients[i] != NULL && war->t[i].team != WAR_MOD) players++;
	}

	if (players < 2)
	{
		level_notify_all(l, TAG_YELLOW "Not enough players :(");
		war->intervalticks = s_interval;
		return;
	}

	int king = -1;
	int max = 1000;
	while (--max > 0)
	{
		king = rand() % MAX_CLIENTS_PER_LEVEL;
		if (l->clients[king] != NULL && war->t[king].team != WAR_MOD) break;
	}

	if (king == -1)
	{
		level_notify_all(l, TAG_YELLOW "No king found :(");
		war->intervalticks = s_interval;
		return;
	}

	level_notify_all(l, TAG_GREEN "Game started!");

	war->ticksremaining = 7500;

	for (i = 0; i < MAX_CLIENTS_PER_LEVEL; i++)
	{
		if (l->clients[i] == NULL || war->t[i].team == WAR_MOD) continue;

		set_colour(l->clients[i]->player, WAR_RED, i == king);
	}
}

static bool war_handle_chat(struct level_t *l, struct client_t *c, char *data, struct war *war)
{
	if (c->player->rank < RANK_OP) return false;

	if (strcasecmp(data, "war start") == 0)
	{
		war->intervalticks = 5 * 25 + 1;
	}
	else if (strcasecmp(data, "war stop") == 0)
	{
		/* Let game finish in the next tick */
		war->ticksremaining = 0;
		war->intervalticks = 0;
	}
	else if (strcasecmp(data, "war scores") == 0)
	{
		char buf[128];
		snprintf(buf, sizeof buf, TAG_LIME "The scores are: Attackers %d - %d Defenders", war->redwins, war->bluewins);
		level_notify_all(l, buf);
	}
	else if (strcasecmp(data, "war reset scores") == 0)
	{
		war->redwins = 0;
		war->bluewins = 0;
		level_notify_all(l, TAG_LIME "Scores reset!");
	}
	else if (strcasecmp(data, "war set attackers") == 0)
	{
		war->redarea = c->player->pos;
		client_notify(c, TAG_YELLOW "Attackers area set");
	}
	else if (strcasecmp(data, "war set defenders") == 0)
	{
		war->bluearea = c->player->pos;
		client_notify(c, TAG_YELLOW "Defenders area set");
	}
	else if (strcasecmp(data, "war moderate") == 0)
	{
		war->t[c->player->levelid].team = WAR_MOD;
		set_colour(c->player, WAR_MOD, false);

		char buf[128];
		snprintf(buf, sizeof buf, "%s " TAG_YELLOW "has become a moderator", c->player->colourusername);
		level_notify_all(l, buf);

		ClrBit(c->player->flags, FLAG_GAMES);
	}
	else if (strcasecmp(data, "war play") == 0)
	{
		war->t[c->player->levelid].team = WAR_SPEC;
		set_colour(c->player, WAR_SPEC, false);

		char buf[128];
		snprintf(buf, sizeof buf, "%s " TAG_YELLOW "is back in the game", c->player->colourusername);
		level_notify_all(l, buf);

		SetBit(c->player->flags, FLAG_GAMES);
	}
	else
	{
		return false;
	}

	return true;
}

static int war_4point(struct level_t *l, int x, int y, int z)
{
	int bx1 = (x - 9) / 32;
	int bz1 = (z - 9) / 32;
	int bx2 = (x + 9) / 32;
	int bz2 = (z + 9) / 32;
	int by = (y - 52) / 32;

	if (!blocktype_passable(level_get_blocktype(l, bx1, by, bz1))) return 1;
	if (bx1 != bx2)
	{
		if (bz1 != bz2 && !blocktype_passable(level_get_blocktype(l, bx2, by, bz2))) return 1;
		if (!blocktype_passable(level_get_blocktype(l, bx2, by, bz1))) return 1;
	}
	if (bz1 != bz2 && !blocktype_passable(level_get_blocktype(l, bx1, by, bz2))) return 1;

	if (blocktype_swim(level_get_blocktype(l, bx1, by, bz1))) return 2;
	if (bx1 != bx2)
	{
		if (bz1 != bz2 && blocktype_swim(level_get_blocktype(l, bx2, y, bz2))) return 2;
		if (blocktype_swim(level_get_blocktype(l, bx2, by, bz1))) return 2;
	}
	if (bz1 != bz2 && blocktype_swim(level_get_blocktype(l, bx1, by, bz2))) return 2;

	return 0;
}

static void war_handle_move(struct level_t *l, struct client_t *c, struct war *war)
{
	char buf[128];

	/* Changing levels, don't handle teleports */
	if (c->player->level != c->player->new_level) return;

//	if (war->ticksremaining == 0) return;

	struct wartemp *wt = war->t + c->player->levelid;
	if (wt->team == WAR_MOD) return;

	if (wt->team != WAR_SPEC && !c->player->teleport)
	{
		struct player_t *player = c->player;
		int dx = player->pos.x - player->lastpos.x;
		int dy = player->pos.y - player->lastpos.y;
		int dz = player->pos.z - player->lastpos.z;
		int dp = dx * dx + dz * dz;

		if (dy < 0)
		{
			if (dy < wt->maxdy) wt->maxdy = dy;

			if (wt->maxdy < -20)
			{
				int t = war_4point(l, player->pos.x, player->pos.y, player->pos.z);
				if (t == 1)
				{
//					snprintf(buf, sizeof buf, "Landed, max dy %d", war->t[i].maxdy);
//					client_notify(c, buf);
					/* maxdy is negative, so we need to add it... */
					wt->life += (wt->maxdy * 3) / 2;
					snprintf(buf, sizeof buf, TAG_RED "Life: %d%%", wt->life);
					client_notify(c, buf);
					wt->maxdy = 0;
					wt->lastpain = PAIN_FALL;
					wt->killer = 0;
				}
				else if (t == 2)
				{
//					client_notify(c, "Landed on liquid");
					wt->maxdy = 0;
				}
			}
		}

		if (dp > 100 || dy > 32 || dy < -120)
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

	if (position_match(&c->player->pos, &war->redarea, 96) && wt->team != WAR_RED)
	{
		wt->team = WAR_RED;
		wt->life = 100;
		wt->air  = 100;
		set_colour(c->player, WAR_RED, false);

		snprintf(buf, sizeof buf, TAG_YELLOW "%s joined the attackers", c->player->username);
		level_notify_all(l, buf);
	}
	else if (position_match(&c->player->pos, &war->bluearea, 96) && wt->team != WAR_BLUE)
	{
		wt->team = WAR_BLUE;
		wt->life = 100;
		wt->air  = 100;
		set_colour(c->player, WAR_BLUE, false);

		snprintf(buf, sizeof buf, TAG_YELLOW "%s joined the defenders", c->player->username);
		level_notify_all(l, buf);
	}
}

static void war_totals(struct level_t *l, struct war *war)
{
	char buf[128];
	snprintf(buf, sizeof buf, TAG_LIME "And the scores are: Attackers %d - %d Defenders", war->redwins, war->bluewins);
	level_notify_all(l, buf);
}

static void war_handle_tick(struct level_t *l, struct war *war)
{
	char buf[128];

	int i;
	for (i = 0; i < MAX_CLIENTS_PER_LEVEL; i++)
	{
		struct client_t *c = l->clients[i];
		if (c == NULL || c->sending_level) continue;

		struct wartemp *wt = war->t + i;

		if (wt->team == WAR_MOD || wt->team == WAR_SPEC) continue;

		if (war->ticksremaining % 5 == 0)
		{
			int bx = c->player->pos.x / 32;
			int by = c->player->pos.y / 32;
			int bz = c->player->pos.z / 32;

			enum blocktype_t b1 = level_get_blocktype(l, bx, by, bz);
			enum blocktype_t b2 = level_get_blocktype(l, bx, by - 1, bz);

			int oldair  = wt->air;
			int oldlife = wt->life;

			if (b1 == WATER || b1 == WATERSTILL)
			{
				if (wt->air > 0)
				{
					wt->air -= 2;
				}
				else
				{
					wt->life -= 10;
					wt->lastpain = PAIN_DROWN;
					wt->killer = level_get_blockowner(l, bx, by, bz);
				}
			}
			else
			{
				wt->air = 100;
				if (!blocktype_passable(b1))
				{
					wt->life -= 10;
					wt->lastpain = PAIN_BURIED;
					wt->killer = level_get_blockowner(l, bx, by, bz);
				}
				else if (b1 == s_explosion || b2 == s_explosion)
				{
					wt->life -= 20;
					wt->lastpain = PAIN_HIT;
					wt->killer = (b1 == s_explosion) ? level_get_blockowner(l, bx, by, bz) : level_get_blockowner(l, bx, by - 1, bz);
				}
				else if (b1 == s_fire || b2 == s_fire)
				{
					wt->life -= 20;
					wt->lastpain = PAIN_FIRE;
					wt->killer = (b1 == s_fire) ? level_get_blockowner(l, bx, by, bz) : level_get_blockowner(l, bx, by - 1, bz);
				}
				else if (b1 == LAVA || b1 == LAVASTILL || b2 == LAVA || b2 == LAVASTILL)
				{
					wt->life -= 10;
					wt->lastpain = PAIN_FIRE;
					wt->killer = (b1 == LAVA || b1 == LAVASTILL) ? level_get_blockowner(l, bx, by, bz) : level_get_blockowner(l, bx, by - 1, bz);
				}
			}

			if (wt->life <= 0)
			{
				char killer[64];

				/* DEAD */
				if (wt->killer != 0 && wt->killer != l->clients[i]->player->globalid)
				{
					int j;
					for (j = 0; j < MAX_CLIENTS_PER_LEVEL; j++)
					{
						if (l->clients[j] != NULL && l->clients[j]->player->globalid == wt->killer)
						{
							strncpy(killer, l->clients[j]->player->username, sizeof killer);
							break;
						}
					}
					if (j == MAX_CLIENTS_PER_LEVEL)
					{
						/* Player offline, consult DB */
						strncpy(killer, playerdb_get_username(wt->killer), sizeof killer);
					}

					snprintf(buf, sizeof buf, s_death_text[wt->lastpain], c->player->username, killer);
				}
				else
				{
					snprintf(buf, sizeof buf, s_suicide_text[wt->lastpain], c->player->username);
				}

				level_notify_all(l, buf);

				wt->team = WAR_SPEC;

				player_teleport(c->player, &l->spawn, true);
				set_colour(c->player, wt->team, i == war->king);
				continue;
			}

			if (oldlife > wt->life && wt->life < 50)
			{
				snprintf(buf, sizeof buf, TAG_RED "Life: %d%%", wt->life);
				client_notify(c, buf);
			}
			else if (oldair > wt->air && wt->air < 50)
			{
				snprintf(buf, sizeof buf, TAG_RED "Air: %d%%", wt->air);
				client_notify(c, buf);
			}
		}
	}
}

static void war_handle_block(struct level_t *l, struct client_t *c, struct block_event *be, struct war *war)
{
	int i = c->player->levelid;

	/* Nothing special for moderators */
	if (war->t[i].team == WAR_MOD) return;

	/* Cancel block change for spectators and dead players */
	if (war->t[i].team == WAR_SPEC || war->t[i].life <= 0)
	{
		be->nt = be->bt;
		return;
	}

	/* Replace TNT with its active counterpart. */
	if (be->bt == AIR)
	{
		if (be->nt == TNT)
		{
			be->nt = s_active_tnt;
		}
		else if (be->nt == DARKGREY)
		{
			be->nt = s_fuse;
		}
	}
}

static void war_handle_spawn(struct level_t *l, struct client_t *c, char *data, struct war *war)
{
	char buf[512];
	snprintf(buf, sizeof buf,
		TAG_YELLOW "Welcome to %s. This level is running WAR. "
		TAG_WHITE "Rules of the game: Attackers must kill the defender's king within %d minutes. "
		TAG_YELLOW "Placed TNT will become ACTIVE. "
		TAG_RED "ALL hacks must be disabled. Do NOT build things here that you'd like to keep, use your /home for that.",
		l->name, 7500 / 1500);
	client_notify(c, buf);

	if (war->ticksremaining > 0)
	{
		client_notify(c, TAG_RED "Game in progress! Wait for next round!");

		war->t[c->player->levelid].team = WAR_SPEC;
		set_colour(c->player, WAR_SPEC, false);
	}

	SetBit(c->player->flags, FLAG_GAMES);
}

static void war_handle_despawn(struct level_t *l, struct client_t *c, char *data, struct war *war)
{
	player_set_alias(c->player, NULL, false);
	ClrBit(c->player->flags, FLAG_GAMES);
}

static bool war_level_hook(int event, struct level_t *l, struct client_t *c, void *data, struct level_hook_data_t *arg)
{
	switch (event)
	{
		case EVENT_TICK: war_handle_tick(l, arg->data); break;
		case EVENT_CHAT: return war_handle_chat(l, c, data, arg->data);
		case EVENT_MOVE: war_handle_move(l, c, arg->data); break;
		case EVENT_BLOCK: war_handle_block(l, c, data, arg->data); break;
		case EVENT_SPAWN: war_handle_spawn(l, c, data, arg->data); break;
		case EVENT_DESPAWN: war_handle_despawn(l, c, data, arg->data); break;
		case EVENT_INIT:
		{
			if (arg->size == 0)
			{
				LOG("Allocating new war data on %s\n", l->name);
			}
			else
			{
				if (arg->size == sizeof (struct war))
				{
					struct war *war = arg->data;
					war->t = calloc(MAX_CLIENTS_PER_LEVEL, sizeof *war->t);
					break;
				}

				LOG("Found invalid war data on %s, erasing\n", l->name);
				free(arg->data);
			}

			arg->size = sizeof (struct war);
			arg->data = calloc(1, arg->size);

			struct war *war = arg->data;
			war->t = calloc(MAX_CLIENTS_PER_LEVEL, sizeof *war->t);
			break;
		}
		case EVENT_DEINIT:
		{
			if (l == NULL) break;

			int i;
			for (i = 0; i < MAX_CLIENTS_PER_LEVEL; i++)
			{
				struct client_t *cl = l->clients[i];
				if (cl == NULL) continue;

				player_set_alias(cl->player, NULL, true);
				ClrBit(cl->player->flags, FLAG_GAMES);
			}

			struct war *war = arg->data;
			free(war->t);
		}
	}

	return false;
}

void module_init(void **data)
{
	s_explosion  = blocktype_get_by_name("explosion");
	s_fire       = blocktype_get_by_name("fire");
	s_active_tnt = blocktype_get_by_name("active_tnt");
	s_fuse       = blocktype_get_by_name("fuse");

	register_level_hook_func("war", &war_level_hook);
}

void module_deinit(void *data)
{
	deregister_level_hook_func("war");
}
