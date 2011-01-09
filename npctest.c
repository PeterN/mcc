#include <stdio.h>
#include <math.h>
#include "client.h"
#include "player.h"
#include "level.h"

#define NPC 64

struct npctesto
{
	int i;
	struct {
		char name[64];
		struct position_t pos;
		struct npc *npc;
	} n[32];
};

struct npctest
{
	int i;
	struct {
		char name[64];
		struct position_t pos;
		struct npc *npc;
	} n[NPC];
	struct {
		int8_t followid;
		int8_t stareid;
	} f[NPC];
};

void npctest_save(struct level_t *l, struct npctest *arg)
{
	int i;
	for (i = 0; i < NPC; i++)
	{
		if (arg->n[i].npc != NULL)
		{
			arg->n[i].pos = arg->n[i].npc->pos;
		}
	}
}

void npctest_init(struct level_t *l, struct npctest *arg)
{
	int i;
	for (i = 0; i < NPC; i++)
	{
		if (arg->n[i].name[0] != '\0')
		{
			arg->n[i].npc = npc_add(l, arg->n[i].name, arg->n[i].pos);
		}
		else
		{
			arg->n[i].npc = NULL;
		}
	}
}

void npctest_init_old(struct level_t *l, struct npctest *arg)
{
	memset(arg->f, 0, sizeof arg->f);

	npctest_init(l, arg);
}

void npctest_deinit(struct level_t *l, struct npctest *arg)
{
	npctest_save(l, arg);

	int i;
	for (i = 0; i < NPC; i++)
	{
		if (arg->n[i].npc != NULL)
		{
			npc_del(arg->n[i].npc);
		}
	}
}

static int npctest_get_by_name(struct npctest *arg, const char *name)
{
	int i;

	char *ep;
	i = strtol(name, &ep, 10);
	if (*ep == '\0') return i;

	for (i = 0; i < NPC; i++)
	{
		if (strcasecmp(arg->n[i].name, name) == 0)
		{
			return i;
		}
	}

	return -1;
}

static void npctest_find_nearest(struct level_t *l, int i, struct npctest *arg)
{
	long mindist = INT_MAX;
	int nearest = -1;
	int j;

	const struct position_t us = arg->n[i].npc->pos;

	for (j = 0; j < MAX_CLIENTS_PER_LEVEL; j++)
	{
		if (l->clients[j] != NULL)
		{
			const struct position_t them = l->clients[j]->player->pos;
			int dx = us.x - them.x;
			int dy = us.y - them.y;
			int dz = us.z - them.z;
			long dist = dx * dx + dy * dy + dz * dz;
			if (dist < mindist)
			{
				mindist = dist;
				nearest = j;
			}
		}
	}

	if (nearest != -1)
	{
		arg->f[i].stareid = nearest + 1;
	}
}

static void npctest_handle_tick(struct level_t *l, struct npctest *arg)
{
	arg->i++;

	int i = arg->i % 64;
	if (arg->f[i].stareid != 0) npctest_find_nearest(l, i, arg);

	const int step = 8;
	for (i = arg->i % step; i < NPC; i += step)
	{
		if (arg->f[i].stareid == 0) continue;

		int nearest = arg->f[i].stareid - 1;
		if (l->clients[nearest] == NULL) continue;

		const struct position_t us = arg->n[i].npc->pos;
		const struct position_t them = l->clients[nearest]->player->pos;
		int dx = us.x - them.x;
		int dy = us.y - them.y;
		int dz = us.z - them.z;

		//float r = sqrtf(dx * dx + dy * dy + dz * dz);
		float azimuth = atan2(dz, dx);
		float zenith = atan2(dy, sqrtf(dx * dx + dz * dz));

		//LOG("For %s, nearest is %s distance %f azimuth %f zenith %f\n",
		//	arg->n[i].name, l->clients[nearest]->player->username,
		//	r, azimuth * 360 / (2 * M_PI), zenith * 360 / (2 * M_PI));

		arg->n[i].npc->pos.h = azimuth * 256 / (2 * M_PI) - 64;
		arg->n[i].npc->pos.p = zenith * 256 / (2 * M_PI);
	}
}

static bool npctest_handle_chat(struct level_t *l, struct client_t *c, char *data, struct npctest *arg)
{
	char buf[128];

	if (c->player->rank < RANK_OP) return false;

	if (strncasecmp(data, "npc add ", 8) == 0)
	{
		int i;
		for (i = 0; i < NPC; i++)
		{
			if (arg->n[i].name[0] == '\0')
			{
				snprintf(arg->n[i].name, sizeof arg->n[i].name, data + 8);
				arg->n[i].npc = npc_add(l, arg->n[i].name, c->player->pos);

				snprintf(buf, sizeof buf, TAG_YELLOW "%s created", data + 8);
				break;
			}
		}
		if (i == NPC) snprintf(buf, sizeof buf, TAG_YELLOW "No free NPC slots");
	}
	else if (strncasecmp(data, "npc del ", 8) == 0)
	{
		int i = npctest_get_by_name(arg, data + 8);

		if (i >= 0)
		{
			snprintf(buf, sizeof buf, TAG_YELLOW "%s deleted", arg->n[i].name);

			npc_del(arg->n[i].npc);
			arg->n[i].name[0] = '\0';
		}
		else
		{
			snprintf(buf, sizeof buf, TAG_YELLOW "%s not found", data + 7);
		}
	}
	else if (strncasecmp(data, "npc get ", 8) == 0)
	{
		int i = npctest_get_by_name(arg, data + 8);

		if (i >= 0)
		{
			arg->n[i].npc->pos = c->player->pos;
			snprintf(buf, sizeof buf, TAG_YELLOW "Summoned %s", arg->n[i].name);
		}
		else
		{
			snprintf(buf, sizeof buf, TAG_YELLOW "%s not found", data + 8);
		}
	}
	else if (strncasecmp(data, "npc stare ", 10) == 0)
	{
		int i = npctest_get_by_name(arg, data + 10);

		if (i >= 0)
		{
			arg->f[i].stareid = !arg->f[i].stareid;
			snprintf(buf, sizeof buf, TAG_YELLOW "%s now %sstaring", arg->n[i].name, arg->f[i].stareid ? "" : "not ");
		}
		else
		{
			snprintf(buf, sizeof buf, TAG_YELLOW "%s not found", data + 10);
		}
	}
	else if (strcasecmp(data, "npc wipe") == 0)
	{
		int i;
		for (i = 0; i < NPC; i++)
		{
			if (arg->n[i].name[0] != '\0')
			{
				npc_del(arg->n[i].npc);
				arg->n[i].name[0] = '\0';
			}
		}
		memset(arg, 0, sizeof (struct npctest));
		snprintf(buf, sizeof buf, TAG_YELLOW "NPCs wiped");
	}
	else if (strcasecmp(data, "npc list") == 0)
	{
		char *bufp;

		strcpy(buf, TAG_YELLOW "NPCs:");
		bufp = buf + strlen(buf);

		int i;
		for (i = 0; i < NPC; i++)
		{
			if (arg->n[i].name[0] == '\0') continue;

			char buf2[64];
			snprintf(buf2, sizeof buf2, " %d:%s", i, arg->n[i].name);

			size_t len = strlen(buf2);
			if (len >= sizeof buf - (bufp - buf))
			{
				client_notify(c, buf);

				strcpy(buf, TAG_YELLOW);
				bufp = buf + strlen(buf);
			}

			strcpy(bufp, buf2);
			bufp += len;
		}
	}
	else
	{
		return false;
	}

	client_notify(c, buf);

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
		case EVENT_TICK: npctest_handle_tick(l, arg->data); break;
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
				else if (arg->size == sizeof (struct npctesto))
				{
					arg->size = sizeof (struct npctest);
					arg->data = realloc(arg->data, arg->size);
					npctest_init_old(l, arg->data);
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
