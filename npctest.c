#include <stdio.h>
#include <math.h>
#include "client.h"
#include "player.h"
#include "level.h"
#include "astar.h"
#include "astar_thread.h"

#define NPC 64

struct npctemp
{
	struct npc *npc[NPC];
	struct point *path[NPC];
	int step[NPC];
	float sx[NPC];
	float sy[NPC];
	float sz[NPC];
};

struct npctest
{
	int i;
	struct {
		char name[64];
		struct position_t pos;
		int8_t followid;
		int8_t stareid;
	} n[NPC];
	struct npctemp *temp;
};

struct pftemp
{
	struct npctest *arg;
	int i;
};

void npctest_save(struct level_t *l, struct npctest *arg)
{
	int i;
	for (i = 0; i < NPC; i++)
	{
		if (arg->temp->npc[i] != NULL)
		{
			arg->n[i].pos = arg->temp->npc[i]->pos;
		}
	}
}

void npctest_init(struct level_t *l, struct npctest *arg)
{
	arg->temp = malloc(sizeof *arg->temp);
	memset(arg->temp, 0, sizeof *arg->temp);

	int i;
	for (i = 0; i < NPC; i++)
	{
		if (arg->n[i].name[0] != '\0')
		{
			arg->temp->npc[i] = npc_add(l, arg->n[i].name, arg->n[i].pos);
			arg->temp->sx[i] = arg->n[i].pos.x / 32.0f;
			arg->temp->sy[i] = arg->n[i].pos.y / 32.0f;
			arg->temp->sz[i] = arg->n[i].pos.z / 32.0f;
		}
		else
		{
			arg->temp->npc[i] = NULL;
		}
	}
}

void npctest_deinit(struct level_t *l, struct npctest *arg)
{
	npctest_save(l, arg);

	int i;
	for (i = 0; i < NPC; i++)
	{
		if (arg->temp->npc[i] != NULL)
		{
			npc_del(arg->temp->npc[i]);
		}

		free(arg->temp->path[i]);
	}

	free(arg->temp);
}

static int npctest_get_by_name(struct npctest *arg, const char *name)
{
	int i;

	char *ep;
	i = strtol(name, &ep, 10);
	if (*ep == '\0' && i >= 0 && i < NPC && arg->n[i].name[0] != '\0') return i;

	for (i = 0; i < NPC; i++)
	{
		if (strcasecmp(arg->n[i].name, name) == 0)
		{
			return i;
		}
	}

	int n = strlen(name);
	for (i = 0; i < NPC; i++)
	{
		if (strncasecmp(arg->n[i].name, name, n) == 0)
		{
			return i;
		}
	}

	return -1;
}

static void npctest_find_nearest(struct level_t *l, int i, struct npctest *arg, int8_t *out)
{
	long mindist = INT_MAX;
	int nearest = -1;
	int j;

	const struct position_t us = arg->temp->npc[i]->pos;

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
		*out = nearest + 1;
	}
}

static void npctest_astar_cb(struct level_t *l, struct point *path, void *data)
{
	printf("Got PF response\n");
	struct pftemp *temp = data;
	temp->arg->temp->path[temp->i] = path;
	temp->arg->temp->step[temp->i] = 0;
	free(temp);
}

static void npctest_replacepath(struct level_t *l, struct npctest *arg, int i)
{
	free(arg->temp->path[i]);
	arg->temp->path[i] = NULL;

	int nearest = arg->n[i].followid - 1;
	if (l->clients[nearest] == NULL) return;

	//const struct position_t us = arg->temp->npc[i]->pos;
	const struct position_t them = l->clients[nearest]->player->pos;

	struct point a;
	struct point b;

	a.x = arg->temp->sx[i];
	a.y = arg->temp->sz[i];
	a.z = arg->temp->sy[i];

	b.x = them.x / 32;
	b.y = them.z / 32;
	b.z = them.y / 32;

	for (; a.z > 0 && level_get_blocktype(l, a.x, a.z, a.y) == AIR; a.z--);
	for (; b.z > 0 && level_get_blocktype(l, b.x, b.z, b.y) == AIR; b.z--);

	a.z++;
	b.z++;

	printf("Pathfinding from %d %d %d to %d %d %d\n", a.x, a.y, a.z, b.x, b.y, b.z);

	struct pftemp *temp = malloc(sizeof *temp);
	temp->arg = arg;
	temp->i = i;

	astar_queue(l, a, b, &npctest_astar_cb, temp);

//	arg->temp->path[i] = as_find(l, &a, &b);
	arg->temp->step[i] = -1;
}

static inline float min(float a, float b)
{
	if (a < 0)
	{
		return a < b ? b : a;
	}
	return a < b ? a : b;
}

static void npctest_handle_tick(struct level_t *l, struct npctest *arg)
{
	arg->i++;

	int i = arg->i % 64;
	if (arg->n[i].stareid != 0) npctest_find_nearest(l, i, arg, &arg->n[i].stareid);
	if (arg->n[i].followid != 0)
	{
		int8_t followid = arg->n[i].followid;
		npctest_find_nearest(l, i, arg, &arg->n[i].followid);
		if (followid != arg->n[i].followid)
		{
			npctest_replacepath(l, arg, i);
		}
		arg->n[i].stareid = arg->n[i].followid;
	}

	for (i = 0; i < NPC; i++)
	{
		struct position_t *us = &arg->temp->npc[i]->pos;

		if (arg->n[i].followid == 0) continue;
		/* Waiting for path */
		if (arg->temp->step[i] == -1) continue;
		if (arg->temp->path[i] == NULL)
		{
			npctest_replacepath(l, arg, i);
		}
		else
		{
			const struct point *p = arg->temp->path[i] + arg->temp->step[i];
			if (p->x == -1)
			{
				npctest_replacepath(l, arg, i);
			}
			else
			{
				float dx = (p->x + 0.5f) - arg->temp->sx[i];
				float dy = (p->z + 1.5f) - arg->temp->sy[i];
				float dz = (p->y + 0.5f) - arg->temp->sz[i];

				float azimuth = atan2(dz, dx);

				float sp = 0.25f;
				float ddx = cos(azimuth) * sp;
				float ddz = sin(azimuth) * sp;

				printf("Delta %f %f - %f %f (%f %f)\n", dx, dz, ddx, ddz, cos(azimuth) * sp, sin(azimuth) * sp);

				arg->temp->sx[i] += min(dx, ddx);
				arg->temp->sz[i] += min(dz, ddz);

				us->x = arg->temp->sx[i] * 32.0f;
				us->z = arg->temp->sz[i] * 32.0f;

//				if (dx < 0) us->x -= min(abs(dx), sp);
//				else if (dx > 0) us->x += min(dx, sp);
				if (dy < 0) arg->temp->sy[i] -= min(abs(dy), sp);
				else if (dy > 0) arg->temp->sy[i] += min(dy, sp);
//				if (dz < 0) us->z -= min(abs(dz), sp);
//				else if (dz > 0) us->z += min(dz, sp);

				us->y = arg->temp->sy[i] * 32.0f;

				int bx = us->x / 32;
				int by = us->y / 32 - 1;
				int bz = us->z / 32;

				if (p->x == bx && p->z == by && p->y == bz)
				{
					arg->temp->step[i]++;
					printf("Moved to step %d\n", arg->temp->step[i]);
				}
			}
		}
	}

	const int step = 8;
	for (i = arg->i % step; i < NPC; i += step)
	{
		struct position_t *us = &arg->temp->npc[i]->pos;

		if (arg->n[i].stareid == 0) continue;

		int nearest = arg->n[i].stareid - 1;
		if (l->clients[nearest] == NULL) continue;

		//const struct position_t us = arg->temp->npc[i]->pos;
		const struct position_t *them = &l->clients[nearest]->player->pos;
		int dx = us->x - them->x;
		int dy = us->y - them->y;
		int dz = us->z - them->z;

		//float r = sqrtf(dx * dx + dy * dy + dz * dz);
		float azimuth = atan2(dz, dx);
		float zenith = atan2(dy, sqrtf(dx * dx + dz * dz));

		//LOG("For %s, nearest is %s distance %f azimuth %f zenith %f\n",
		//	arg->n[i].name, l->clients[nearest]->player->username,
		//	r, azimuth * 360 / (2 * M_PI), zenith * 360 / (2 * M_PI));

		us->h = azimuth * 256 / (2 * M_PI) - 64;
		us->p = zenith * 256 / (2 * M_PI);
	}
}

static bool npctest_handle_chat(struct level_t *l, struct client_t *c, char *data, struct npctest *arg)
{
	char buf[65];

	if (c->player->rank < RANK_OP) return false;

	if (strncasecmp(data, "npc add ", 8) == 0)
	{
		int i;
		for (i = 0; i < NPC; i++)
		{
			if (arg->n[i].name[0] == '\0')
			{
				snprintf(arg->n[i].name, sizeof arg->n[i].name, data + 8);
				arg->temp->npc[i] = npc_add(l, arg->n[i].name, c->player->pos);
				arg->temp->sx[i] = c->player->pos.x / 32.0f;
				arg->temp->sy[i] = c->player->pos.y / 32.0f;
				arg->temp->sz[i] = c->player->pos.z / 32.0f;

				arg->n[i].followid = 0;
				arg->n[i].stareid = 0;

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

			npc_del(arg->temp->npc[i]);
			free(arg->temp->path[i]);
			arg->n[i].name[0] = '\0';
			arg->n[i].followid = 0;
			arg->n[i].stareid = 0;
			arg->temp->npc[i] = NULL;
			arg->temp->path[i] = NULL;
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
			arg->temp->npc[i]->pos = c->player->pos;
			snprintf(buf, sizeof buf, TAG_YELLOW "Summoned %s", arg->n[i].name);
		}
		else
		{
			snprintf(buf, sizeof buf, TAG_YELLOW "%s not found", data + 8);
		}
	}
	else if (strncasecmp(data, "npc tp ", 7) == 0)
	{
		int i = npctest_get_by_name(arg, data + 7);
		if (i >= 0)
		{
			player_teleport(c->player, &arg->temp->npc[i]->pos, true);
			return true;
		}
		else
		{
			snprintf(buf, sizeof buf, TAG_YELLOW "%s not found", data + 7);
		}
	}
	else if (strncasecmp(data, "npc follow ", 11) == 0)
	{
		int i = npctest_get_by_name(arg, data + 11);

		if (i >= 0)
		{
			arg->n[i].followid = !arg->n[i].followid;
			snprintf(buf, sizeof buf, TAG_YELLOW "%s now %sfollowing", arg->n[i].name, arg->n[i].followid ? "" : "not ");
		}
		else
		{
			snprintf(buf, sizeof buf, TAG_YELLOW "%s not found", data + 10);
		}
	}
	else if (strncasecmp(data, "npc stare ", 10) == 0)
	{
		int i = npctest_get_by_name(arg, data + 10);

		if (i >= 0)
		{
			arg->n[i].stareid = !arg->n[i].stareid;
			snprintf(buf, sizeof buf, TAG_YELLOW "%s now %sstaring", arg->n[i].name, arg->n[i].stareid ? "" : "not ");
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
				npc_del(arg->temp->npc[i]);
				free(arg->temp->path[i]);
				arg->n[i].name[0] = '\0';
				arg->n[i].followid = 0;
				arg->n[i].stareid = 0;
				arg->temp->npc[i] = NULL;
				arg->temp->path[i] = NULL;
			}
		}
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

				LOG("Found invalid npctest data on %s, erasing\n", l->name);
				free(arg->data);
			}

			arg->size = sizeof (struct npctest);
			arg->data = calloc(1, arg->size);

			struct npctest *npc = arg->data;
			npc->temp = malloc(sizeof *npc->temp);
			memset(npc->temp, 0, sizeof *npc->temp);
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
