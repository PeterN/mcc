#include <stdio.h>
#include <math.h>
#include "client.h"
#include "player.h"
#include "level.h"
#include "astar.h"
#include "astar_worker.h"

#define NPC 64

#define RADIUS (7.0f / 32.0f)
#define HEIGHT (51.0f / 32.0f)
#define HEADROOM RADIUS
#define GRAVITY (1.0f / 32.0f)
#define MAXSPEED 0.20f
#define ACCEL 0.05f

enum {
	FM_RANDOM_PLAYER,
	FM_RANDOM_NPC,
	FM_RANDOM_ANY,
	FM_FIXED_PLAYER,
	FM_FIXED_NPC,
};

static enum blocktype_t s_door;

struct npctemp
{
	struct npc *npc[NPC];
	struct point *path[NPC];
	int step[NPC];
	float sx[NPC];
	float sy[NPC];
	float sz[NPC];
	float hs[NPC];
	float vs[NPC];
	bool iw[NPC];
	bool pf[NPC];
};

struct npctest
{
	int i;
	struct {
		char name[64];
		struct position_t pos;
		int8_t followid;
		int8_t stareid;
		int8_t fm;
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

static void npctest_find_nearest(struct level_t *l, int i, struct npctest *arg, int fm, int8_t *out)
{
	long mindist = INT_MAX;
	int nearest = -1;
	int j;

	if (fm == FM_FIXED_PLAYER || fm == FM_FIXED_NPC) return;

	const struct position_t us = arg->temp->npc[i]->pos;

	if (fm == FM_RANDOM_PLAYER || fm == FM_RANDOM_ANY)
	{
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
	}

	if (fm == FM_RANDOM_NPC || fm == FM_RANDOM_ANY)
	{
		for (j = 0; j < MAX_NPCS_PER_LEVEL; j++)
		{
			if (l->npcs[j] != NULL && l->npcs[j] != arg->temp->npc[i])
			{
				const struct position_t them = l->npcs[j]->pos;
				int dx = us.x - them.x;
				int dy = us.y - them.y;
				int dz = us.z - them.z;
				long dist = dx * dx + dy * dy + dz * dz;
				if (dist < mindist)
				{
					mindist = dist;
					nearest = j + MAX_CLIENTS_PER_LEVEL;
				}
			}
		}
	}

//	printf("Nearest is %d (%ld)\n", nearest, mindist);

	if (nearest != -1)
	{
		*out = nearest + 1;
	}
}

static void npctest_astar_cb(struct level_t *l, struct point *path, void *data)
{
	struct pftemp *temp = data;

	void *oldpath = temp->arg->temp->path[temp->i];

	temp->arg->temp->path[temp->i] = path;
	temp->arg->temp->step[temp->i] = 0;
	temp->arg->temp->pf[temp->i] = false;

	free(temp);
	free(oldpath);
}

static const struct position_t *npctest_getpos(struct level_t *l, int nearest)
{
	nearest -= 1;

	if (nearest < MAX_CLIENTS_PER_LEVEL)
	{
		/* Following player */
		if (l->clients[nearest] == NULL) return NULL;
		return &l->clients[nearest]->player->pos;
	}

	/* Following NPC */
	nearest -= MAX_CLIENTS_PER_LEVEL;
	if (l->npcs[nearest] == NULL) return NULL;
	return &l->npcs[nearest]->pos;
}

static void npctest_replacepath(struct level_t *l, struct npctest *arg, int i)
{
	/* Already getting a path... */
	if (arg->temp->pf[i]) return;

	const struct position_t *them = npctest_getpos(l, arg->n[i].followid);

	if (them == NULL || position_match(&arg->temp->npc[i]->pos, them, 48)) {
		/* Already there, no need to pf */
		return;
	}

	struct point a;
	struct point b;

	a.x = arg->temp->sx[i];
	a.y = arg->temp->sz[i];
	a.z = arg->temp->sy[i] - HEIGHT;

	b.x = them->x / 32;
	b.y = them->z / 32;
	b.z = them->y / 32;

	//for (; a.z > 0 && blocktype_passable(level_get_blocktype(l, a.x, a.z, a.y)); a.z--);
	for (; b.z > 0 && blocktype_passable(level_get_blocktype(l, b.x, b.z, b.y)); b.z--);

	b.z++;

	//printf("Pathfinding from %d %d %d to %d %d %d\n", a.x, a.y, a.z, b.x, b.y, b.z);

	struct pftemp *temp = malloc(sizeof *temp);
	temp->arg = arg;
	temp->i = i;

	astar_queue(l, &a, &b, &npctest_astar_cb, temp);

//	arg->temp->path[i] = as_find(l, &a, &b);
	arg->temp->pf[i] = true;
}

static inline float min(float a, float b)
{
	if (a < 0)
	{
		return a < b ? b : a;
	}
	return a < b ? a : b;
}

static bool npctest_4point(struct level_t *l, float x, float y, float z)
{
	int bx = x - RADIUS;
	int bz = z - RADIUS;
	int bx2 = x + RADIUS;
	int bz2 = z + RADIUS;
	int by = y;

	if (!blocktype_passable(level_get_blocktype(l, bx, by, bz))) return false;
	if (bx != bx2)
	{
		if (bz != bz2 && !blocktype_passable(level_get_blocktype(l, bx2, by, bz2))) return false;
		if (!blocktype_passable(level_get_blocktype(l, bx2, by, bz))) return false;
	}
	if (bz != bz2 && !blocktype_passable(level_get_blocktype(l, bx, by, bz2))) return false;

	return true;
}

static bool npctest_canmoveto(struct level_t *l, float x, float y, float z, float dx, float dz)
{
	if (!npctest_4point(l, x + dx, y - HEIGHT, z + dz)) return false;
	if (!npctest_4point(l, x + dx, y - HEIGHT + 1.0f, z + dz)) return false;
	return true;

	int bx = x;
	int bz = z;
	float fx = x - bx;
	float fz = z - bz;

	if (dx < 0 && fx + dx - RADIUS < 0) bx--;
	else if (dx > 0 && fx + dx + RADIUS > 32) bx++;
	if (dz < 0 && fz + dz - RADIUS < 0) bz--;
	else if (dz > 0 && fz + dz + RADIUS > 32) bz++;

	if (level_get_blocktype(l, bx, y - HEIGHT, bz) != AIR) return false;
	if (level_get_blocktype(l, bx, y - HEIGHT + 1.0f, bz) != AIR) return false;

	return true;
}

static void npctest_handle_tick(struct level_t *l, struct npctest *arg)
{
	arg->i++;

	int i = arg->i % 64;
	if (arg->n[i].followid != 0)
	{
		int8_t followid = arg->n[i].followid;
		npctest_find_nearest(l, i, arg, arg->n[i].fm, &arg->n[i].followid);
		if (followid != arg->n[i].followid)
		{
			npctest_replacepath(l, arg, i);
		}
		arg->n[i].stareid = arg->n[i].followid;
	}
	else if (arg->n[i].stareid != 0)
	{
		npctest_find_nearest(l, i, arg, FM_RANDOM_PLAYER, &arg->n[i].stareid);
	}

	for (i = 0; i < NPC; i++)
	{
		struct position_t *us = &arg->temp->npc[i]->pos;

		if (arg->n[i].followid == 0) continue;
		/* Waiting for path */
		if (arg->temp->path[i] == NULL)
		{
			npctest_replacepath(l, arg, i);
		}
		else
		{
			const struct position_t *them = npctest_getpos(l, arg->n[i].followid);
			if (them != NULL && position_match(&arg->temp->npc[i]->pos, them, 48)) continue;

			const struct point *p = arg->temp->path[i] + arg->temp->step[i];
			if (p->x == -1)
			{
				npctest_replacepath(l, arg, i);
			}
			else
			{
//				if ((arg->i % 10) != 0) continue;
				float dx = (p->x + 0.5f) - arg->temp->sx[i];
				float dy = (p->z + HEIGHT) - arg->temp->sy[i];
				float dz = (p->y + 0.5f) - arg->temp->sz[i];

				float azimuth = atan2(dz, dx);

				float ms = arg->temp->iw[i] ? MAXSPEED / 2.0f : MAXSPEED;
				if (arg->temp->hs[i] < ms)
				{
					arg->temp->hs[i] += ACCEL;
					if (arg->temp->hs[i] > ms)
					{
						arg->temp->hs[i] = ms;
					}
				}

				float sp = arg->temp->hs[i];
				float ddx = min(dx, cos(azimuth) * sp);
				float ddz = min(dz, sin(azimuth) * sp);

				//printf("Delta %f %f - %f %f (%f %f)\n", dx, dz, ddx, ddz, cos(azimuth) * sp, sin(azimuth) * sp);

				if (npctest_canmoveto(l, arg->temp->sx[i], arg->temp->sy[i], arg->temp->sz[i], ddx, ddz))
				{
					arg->temp->sx[i] += ddx;
					arg->temp->sz[i] += ddz;

					us->x = arg->temp->sx[i] * 32.0f;
					us->z = arg->temp->sz[i] * 32.0f;

					enum blocktype_t bt1 = level_get_blocktype(l, arg->temp->sx[i], arg->temp->sy[i] - 1.0f, arg->temp->sz[i]);
					enum blocktype_t bt2 = level_get_blocktype(l, arg->temp->sx[i], arg->temp->sy[i], arg->temp->sz[i]);
					arg->temp->iw[i] = blocktype_swim(bt1) | blocktype_swim(bt2);
				}
				else
				{
					arg->temp->hs[i] = 0.0f;
				}

//				if (dx < 0) us->x -= min(abs(dx), sp);
//				else if (dx > 0) us->x += min(dx, sp);

				//printf("Current height %f, target %f\n", arg->temp->sy[i], (p->z + 51.0f / 32.0f));

				if (arg->temp->iw[i])
				{
					arg->temp->vs[i] = dy < 0 ? -GRAVITY : GRAVITY;
				}
				else
				{
					arg->temp->vs[i] -= GRAVITY; //1.0f / 32.0f;
				}
				arg->temp->sy[i] += arg->temp->vs[i];
				if (arg->temp->vs[i] > 0.0f)
				{
					if (!npctest_4point(l, arg->temp->sx[i], arg->temp->sy[i] + HEADROOM, arg->temp->sz[i]))
					{
						arg->temp->sy[i] = ((int)(arg->temp->sy[i] - HEIGHT)) + HEIGHT + HEADROOM;
						arg->temp->vs[i] = 0.0f;
					}
				}
				if (arg->temp->vs[i] < 0.0f) // && arg->temp->sy[i] < (p->z + HEIGHT))
				{
					if (!npctest_4point(l, arg->temp->sx[i], arg->temp->sy[i] - HEIGHT, arg->temp->sz[i]))
					{
						arg->temp->sy[i] = ((int)(arg->temp->sy[i] - HEIGHT)) + HEIGHT + 1.0f;
						arg->temp->vs[i] = 0.0f;
						printf("Can't fall\n");

						if (dy > 0.0f)
						{
							arg->temp->vs[i] = 0.25f + GRAVITY;
							printf("Jumped\n");
						}
					}
					//printf("Landed\n");
				}

//				if (level_get_blocktype(l, arg->temp->sx[i], arg->temp->sy[i] - 51.0f / 32.0f, arg->temp->sz[i]) != AIR)
//				{

//					arg->temp->vs[i] = 0.0f;
//					printf("Landed\n");
//				}

//				printf("dy %f, vs %f\n", dy, arg->temp->vs[i]);

//				arg->temp->sy[i] += arg->temp->vs[i];
				us->y = arg->temp->sy[i] * 32.0f;

				//printf("Height %f\n", dy);

//				if (dy < 0) arg->temp->sy[i] -= min(abs(dy), sp);
//				else if (dy > 0) arg->temp->sy[i] += min(dy, sp);
//				if (dz < 0) us->z -= min(abs(dz), sp);
//				else if (dz > 0) us->z += min(dz, sp);

//				us->y = arg->temp->sy[i] * 32.0f;

				int bx = us->x / 32;
				int by = us->y / 32 - 1;
				int bz = us->z / 32;

				//printf("At %f %f %f (%d %d %d : %d %d %d)\n", arg->temp->sx[i], arg->temp->sy[i], arg->temp->sz[i], us->x, us->y, us->z, bx, by, bz);

				if (p->x == bx && p->z == by && p->y == bz)
				{
					arg->temp->step[i]++;
					//printf("Moved to step %d\n", arg->temp->step[i]);
				}
			}
		}

		if ((arg->i % 25) == 0)
			npctest_replacepath(l, arg, i);
	}

	const int step = 8;
	for (i = arg->i % step; i < NPC; i += step)
	{
		struct position_t *us = &arg->temp->npc[i]->pos;

		if (arg->n[i].stareid == 0) continue;

		const struct position_t *them = npctest_getpos(l, arg->n[i].stareid);
		if (them == NULL) continue;

		int dx = us->x - them->x;
		int dy = us->y - them->y;
		int dz = us->z - them->z;

		float azimuth = atan2(dz, dx);
		float zenith = atan2(dy, sqrtf(dx * dx + dz * dz));

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
			arg->temp->sx[i] = c->player->pos.x / 32.0f;
			arg->temp->sy[i] = c->player->pos.y / 32.0f;
			arg->temp->sz[i] = c->player->pos.z / 32.0f;
			if (arg->n[i].followid != 0)
			{
				npctest_replacepath(l, arg, i);
			}

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
		char *buf2 = strdup(data + 11);
		char *name = strchr(buf2, ' ');
		if (name == NULL)
		{
			client_notify(c, TAG_YELLOW "Missing name/id");
			free(buf2);
			return true;
		}

		*name++ = '\0';

		int i = npctest_get_by_name(arg, name);

		if (i >= 0)
		{
			arg->n[i].followid = c->player->levelid + 1;

			if (strcasecmp(buf2, "off") == 0)
			{
				arg->n[i].followid = 0;
			}
			else if (strcasecmp(buf2, "player") == 0)
			{
				arg->n[i].fm = FM_RANDOM_PLAYER;
			}
			else if (strcasecmp(buf2, "npc") == 0)
			{
				arg->n[i].fm = FM_RANDOM_NPC;
			}
			else if (strcasecmp(buf2, "any") == 0)
			{
				arg->n[i].fm = FM_RANDOM_ANY;
			}
			else if (strcasecmp(buf2, "me") == 0)
			{
				arg->n[i].fm = FM_FIXED_PLAYER;
			}
			else
			{
				arg->n[i].followid = npctest_get_by_name(arg, buf2) + MAX_CLIENTS_PER_LEVEL + 1;
				arg->n[i].fm = FM_FIXED_NPC;
			}

			snprintf(buf, sizeof buf, TAG_YELLOW "%s now %sfollowing", arg->n[i].name, arg->n[i].followid ? "" : "not ");
		}
		else
		{
			snprintf(buf, sizeof buf, TAG_YELLOW "%s not found", data + 10);
		}

		free(buf2);
	}
	else if (strncasecmp(data, "npc stare ", 10) == 0)
	{
		int i = npctest_get_by_name(arg, data + 10);

		if (i >= 0)
		{
			if (arg->n[i].stareid == 0)
			{
				arg->n[i].stareid = c->player->levelid + 1;
			}
			else
			{
				arg->n[i].stareid = 0;
			}
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
	s_door = blocktype_get_by_name("door");
}

void module_deinit(void *data)
{
	deregister_level_hook_func("npctest");
}
