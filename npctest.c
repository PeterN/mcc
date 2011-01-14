#include <stdio.h>
#include <math.h>
#include "client.h"
#include "player.h"
#include "level.h"
#include "astar.h"
#include "astar_worker.h"

#define NPC 128

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

static enum blocktype_t s_npcwall;
static enum blocktype_t s_door;

struct npcinfo
{
	struct npc *npc;
	struct point *path; /* Current path */
	int step;           /* Step within the path */
	float sx, sy, sz;   /* FP position */
	float hs, vs;       /* Horizontal / vertical speed */
	bool iw;            /* In-water? */
	bool pf;            /* Pathfinding? */
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
	struct npcinfo *t;
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
		if (arg->t[i].npc != NULL)
		{
			arg->n[i].pos = arg->t[i].npc->pos;
		}
	}
}

void npctest_init(struct level_t *l, struct npctest *arg)
{
	arg->t = calloc(NPC, sizeof *arg->t);

	int i;
	for (i = 0; i < NPC; i++)
	{
		if (arg->n[i].name[0] != '\0')
		{
			arg->t[i].npc = npc_add(l, arg->n[i].name, arg->n[i].pos);
			arg->t[i].sx = arg->n[i].pos.x / 32.0f;
			arg->t[i].sy = arg->n[i].pos.y / 32.0f;
			arg->t[i].sz = arg->n[i].pos.z / 32.0f;
		}
		else
		{
			arg->t[i].npc = NULL;
		}
	}
}

void npctest_deinit(struct level_t *l, struct npctest *arg)
{
	npctest_save(l, arg);

	int i;
	for (i = 0; i < NPC; i++)
	{
		if (arg->t[i].npc != NULL)
		{
			npc_del(arg->t[i].npc);
		}

		free(arg->t[i].path);
	}

	free(arg->t);
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

	const struct position_t us = arg->t[i].npc->pos;

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
			if (l->npcs[j] != NULL && l->npcs[j] != arg->t[i].npc)
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

	if (nearest != -1)
	{
		*out = nearest + 1;
	}
}

static void npctest_astar_cb(struct level_t *l, struct point *path, void *data)
{
	struct pftemp *temp = data;
	struct npcinfo *ni = &temp->arg->t[temp->i];

	void *oldpath = ni->path;
	ni->path = path;
	ni->step = 0;
	ni->pf = false;

	temp->arg->n[temp->i].stareid = 0;

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

static bool npctest_4point(const struct level_t *l, float x, float y, float z)
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

static inline bool fmatch(float a, float b)
{
	return fabsf(a - b) < 0.05f;
}

static bool npctest_iswalkable(const struct level_t *l, float ax, float ay, float az, float bx, float by, float bz)
{
	/* Cannot skip point if height changes */
	if (!fmatch(ay, by)) return false;

	float dx = bx - ax;
	float dz = bz - az;
	float angle = atan2(dz, dx);
	float length = sqrtf(dx * dx + dz * dz);
	float pos = 0.0f;

	float ddx = cos(angle) * 0.25f;
	float ddz = sin(angle) * 0.25f;

	float x = ax;
	float z = az;

	while (pos < length)
	{
		if (!npctest_4point(l, x, ay, z) || !npctest_4point(l, x, ay + 1.0f, z)) return false;
		if (npctest_4point(l, x, ay - 1.0f, z)) return false;
		pos += 0.25f;
		x += ddx;
		z += ddz;
	}

	return true;
}


static void npctest_replacepath(struct level_t *l, struct npctest *arg, int i)
{
	struct npcinfo *ni = &arg->t[i];

	/* Already getting a path... */
	if (ni->pf) return;

	const struct position_t *them = npctest_getpos(l, arg->n[i].followid);

	if (them == NULL) return;

	if (position_match(&ni->npc->pos, them, 48))
	{
		/* Already there, no need to pf */
		free(ni->path);
		ni->path = NULL;

		arg->n[i].stareid = arg->n[i].followid;
		return;
	}

	float ax = ni->sx;
	float az = ni->sz;
	float ay = ni->sy - HEIGHT;

	float bx = them->x / 32.0f;
	float bz = them->z / 32.0f;
	float by = them->y / 32.0f - HEIGHT;

	for (; by > 0 && npctest_4point(l, bx, by - 1, bz); by--);

	struct point a;
	a.x = ax; a.y = az; a.z = ay;

	struct point b;
	b.x = bx; b.y = bz; b.z = by;

	if (a.x == b.x && a.y == b.y && a.z == b.z)
	{
		free(ni->path);
		ni->path = NULL;

		arg->n[i].stareid = arg->n[i].followid;
		return;
	}

	if (npctest_iswalkable(l, ax, ay, az, bx, by, bz))
	{
		/* We can walk directly to our target, so don't bother pathfinding */
		free(ni->path);
		ni->path = malloc(3 * sizeof (struct point));
		ni->step = 0;
		ni->path[0] = a;
		ni->path[1] = b;
		ni->path[2].x = -1;
		return;
	}

	struct pftemp *temp = malloc(sizeof *temp);
	temp->arg = arg;
	temp->i = i;

	astar_queue(l, &a, &b, &npctest_astar_cb, temp);
	ni->pf = true;
}

static inline float min(float a, float b)
{
	if (a < 0)
	{
		return a < b ? b : a;
	}
	return a < b ? a : b;
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
	}
	else if (arg->n[i].stareid != 0)
	{
		npctest_find_nearest(l, i, arg, FM_RANDOM_PLAYER, &arg->n[i].stareid);
	}

	for (i = 0; i < NPC; i++)
	{
		struct npcinfo *ni = &arg->t[i];
		struct position_t *us = &ni->npc->pos;

		if (arg->n[i].followid == 0) continue;
		/* Waiting for path */
		if (ni->path == NULL)
		{
			npctest_replacepath(l, arg, i);
		}
		else
		{
			const struct position_t *them = npctest_getpos(l, arg->n[i].followid);
			if (them != NULL && position_match(&ni->npc->pos, them, 48)) continue;

			const struct point *p = ni->path + ni->step;
			if (p->x == -1)
			{
				npctest_replacepath(l, arg, i);
			}
			else
			{
				float dx = (p->x + 0.5f) - ni->sx;
				float dy = (p->z + HEIGHT) - ni->sy;
				float dz = (p->y + 0.5f) - ni->sz;

				float azimuth = atan2(dz, dx);

				if (arg->n[i].stareid == 0 && (arg->i % 8) == 0)
				{
					us->h = azimuth * 256 / (2 * M_PI) + 64;
					us->p = 0;
				}

				float ms = ni->iw ? MAXSPEED * 0.5f : MAXSPEED;
				
				ni->hs += ACCEL;
				if (ni->hs > ms) ni->hs = ms;

				float sp = ni->hs;
				float ddx = min(dx, cos(azimuth) * sp);
				float ddz = min(dz, sin(azimuth) * sp);

				if (npctest_canmoveto(l, ni->sx, ni->sy, ni->sz, ddx, ddz))
				{
					ni->sx += ddx;
					ni->sz += ddz;

					us->x = ni->sx * 32.0f;
					us->z = ni->sz * 32.0f;

					enum blocktype_t bt1 = level_get_blocktype(l, ni->sx, ni->sy - HEIGHT, ni->sz);
					enum blocktype_t bt2 = level_get_blocktype(l, ni->sx, ni->sy - HEIGHT + 1.0f, ni->sz);
					ni->iw = blocktype_swim(bt1) | blocktype_swim(bt2);
				}
				else
				{
					ni->hs = 0.0f;
				}

				if (ni->iw)
				{
					ni->vs = (dy < 0.0f ? -GRAVITY : GRAVITY) * 5.0f;
				}
				else
				{
					ni->vs -= GRAVITY;
				}

				ni->sy += ni->vs;
				if (ni->vs > 0.0f)
				{
					if (!npctest_4point(l, ni->sx, ni->sy + HEADROOM, ni->sz))
					{
						ni->sy = ((int)(ni->sy - HEIGHT)) + HEIGHT + HEADROOM;
						ni->vs = 0.0f;
					}
				}
				if (ni->vs < 0.0f) // && arg->temp->sy[i] < (p->z + HEIGHT))
				{
					if (!npctest_4point(l, ni->sx, ni->sy - HEIGHT, ni->sz))
					{
						ni->sy = ((int)(ni->sy - HEIGHT)) + HEIGHT + 1.0f;
						ni->vs = 0.0f;

						if (dy > 0.0f)
						{
							ni->vs = 0.25f + GRAVITY;
						}
					}
				}

				us->y = ni->sy * 32.0f;

				/* Check we're at approximately the centre of the target tile */
				if (fmatch(ni->sx, p->x + 0.5f) &&
					fmatch(ni->sz, p->y + 0.5f) &&
					fmatch(ni->sy - HEIGHT, p->z))
				{
					ni->step++;
				}
			}
		}

		if ((arg->i % 100) == 0) npctest_replacepath(l, arg, i);
	}

	const int step = 8;
	for (i = arg->i % step; i < NPC; i += step)
	{
		struct position_t *us = &arg->t[i].npc->pos;

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
				arg->t[i].npc = npc_add(l, arg->n[i].name, c->player->pos);
				arg->t[i].sx = c->player->pos.x / 32.0f;
				arg->t[i].sy = c->player->pos.y / 32.0f;
				arg->t[i].sz = c->player->pos.z / 32.0f;

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

			npc_del(arg->t[i].npc);
			free(arg->t[i].path);
			arg->n[i].name[0] = '\0';
			arg->n[i].followid = 0;
			arg->n[i].stareid = 0;
			arg->t[i].npc = NULL;
			arg->t[i].path = NULL;
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
			arg->t[i].npc->pos = c->player->pos;
			arg->t[i].sx = c->player->pos.x / 32.0f;
			arg->t[i].sy = c->player->pos.y / 32.0f;
			arg->t[i].sz = c->player->pos.z / 32.0f;
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
			player_teleport(c->player, &arg->t[i].npc->pos, true);
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
				npc_del(arg->t[i].npc);
				free(arg->t[i].path);
				arg->n[i].name[0] = '\0';
				arg->n[i].followid = 0;
				arg->n[i].stareid = 0;
				arg->t[i].npc = NULL;
				arg->t[i].path = NULL;
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
			npc->t = calloc(NPC, sizeof *npc->t);
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

static enum blocktype_t convert_npcwall(struct level_t *level, unsigned index, const struct block_t *block)
{
	return AIR;
}

void module_init(void **data)
{
	s_npcwall = register_blocktype(BLOCK_INVALID, "npcwall", RANK_MOD, &convert_npcwall, NULL, NULL, NULL, true, false, false);
	s_door = blocktype_get_by_name("door");

	register_level_hook_func("npctest", &npctest_level_hook);
}

void module_deinit(void *data)
{
	deregister_level_hook_func("npctest");

	deregister_blocktype(s_npcwall);
}
