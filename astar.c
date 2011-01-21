#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "astar.h"
#include "block.h"
#include "level.h"
#include "client.h"
#include "packet.h"
#include "hash.h"

#define RADIUS (9.0f / 32.0f)

static bool point_testradius(const struct level_t *l, float x, float y, int bz)
{
	int bx1 = x - RADIUS;
	int by1 = y - RADIUS;
	int bx2 = x + RADIUS;
	int by2 = y + RADIUS;

	if (!blocktype_passable(level_get_blocktype(l, bx1, bz, by1))) return false;
	if (bx1 != bx2)
	{
		if (by1 != by2 && !blocktype_passable(level_get_blocktype(l, bx2, bz, by2))) return false;
		if (!blocktype_passable(level_get_blocktype(l, bx2, bz, by1))) return false;
	}
	if (by1 != by2 && !blocktype_passable(level_get_blocktype(l, bx1, bz, by2))) return false;

	return true;
}

static bool path_iswalkable(const struct level_t *l, const struct point *a, const struct point *b)
{
	/* Cannot skip point if height changes */
	if (fabsf(a->z - b->z) >= 0.1f) return false;

	float dx = b->x - a->x;
	float dy = b->y - a->y;
	float angle = atan2(dy, dx);
	float length = sqrtf(dx * dx + dy * dy);
	float pos = 0.0f;

	float ddx = cos(angle) * 0.25f;
	float ddy = sin(angle) * 0.25f;

	float x = a->x + 0.5f;
	float y = a->y + 0.5f;

	while (pos < length)
	{
		if (!point_testradius(l, x, y, a->z) || !point_testradius(l, x, y, a->z + 1)) return false;
		if (point_testradius(l, x, y, a->z - 1)) return false;
		pos += 0.25f;
		x += ddx;
		y += ddy;
	}

	return true;
}

int point_add(const struct level_t *l, const struct point *p, int d, struct point *q)
{
	struct point n = *p;
	switch (d)
	{
		case 0: n.x--; break;
		case 1: n.x--; n.y--; break;
		case 2: n.y--; break;
		case 3: n.x++; n.y--; break;
		case 4: n.x++; break;
		case 5: n.x++; n.y++; break;
		case 6: n.y++; break;
		case 7: n.x--; n.y++; break;
	}

	/* Blocked at head-height? */
	if (!blocktype_passable(level_get_blocktype(l, n.x, n.z + 1, n.y)))
	{
		*q = n;
		return 0;
	}
	if (blocktype_passable(level_get_blocktype(l, n.x, n.z, n.y)))
	{
		/* Check diagonals for height */
		do
		{
			n.z--;
		}
		while (blocktype_passable(level_get_blocktype(l, n.x, n.z, n.y)));

		n.z++;
		/* else stay at same height */
	}
	else
	{
		/* Can jump? */
		if (blocktype_passable(level_get_blocktype(l, n.x, n.z + 2, n.y)))
		{
			/* Check diagonals for jump height */
			n.z++;
			*q = n;
			return 2;
		}
		else
		{
			*q = n;
			return 0;
		}
	}

	*q = n;
	return 1;
}

float point_dist(const struct point *a, const struct point *b, bool guess)
{
	int dx = a->x - b->x;
	int dy = a->y - b->y;
	int dz = a->z - b->z;
	return abs(dx) + abs(dy) + abs(dz);
//	return sqrtf(dx * dx + dy * dy + dz * dz);
}

struct node
{
	struct node *parent;
	struct node *prev;
	struct node *next;
	float f, g, h;
	struct point point;
};

static bool node_match(const struct node *a, const struct node *b)
{
	return a->point.x == b->point.x && a->point.y == b->point.y && a->point.z == b->point.z;
}

struct as
{
	const struct level_t *level;
	struct node *openlist;
	struct hash closedhash;
};

static void openlist_add(struct node **headref, struct node *node)
{
	struct node *prev = NULL;
	struct node *curr = *headref;

	assert(node->prev == NULL);
	assert(node->next == NULL);

	if (*headref == NULL || node->f <= (*headref)->f)
	{
		node->next = *headref;
		*headref = node;
	}
	else
	{
		while (curr != NULL && curr->f <= node->f)
		{
			prev = curr;
			curr = curr->next;
		}

		assert(prev != NULL);
		prev->next = node;
		node->prev = prev;
		node->next = curr;
		if (curr != NULL) curr->prev = node;
	}
}

static struct node *openlist_pop(struct node **headref)
{
	struct node *top = *headref;
	*headref = top->next;
	top->next = NULL;
	if (*headref != NULL) (*headref)->prev = NULL;

	return top;
}

static void openlist_del(struct node **headref, struct node *node)
{
	if (node->prev == NULL)
	{
		*headref = node->next;
	}
	else
	{
		node->prev->next = node->next;
	}
	if (node->next != NULL)
	{
		node->next->prev = node->prev;
	}
	free(node);
}

static void openlist_wipe(struct node **headref)
{
	struct node *prev;
	struct node *curr = *headref;

	while (curr != NULL)
	{
		prev = curr;
		curr = curr->next;
		free(prev);
	}

	*headref = NULL;
}

static struct node *as_isinlist(struct node **headref, struct node *node)
{
	struct node *curr = *headref;

	while (curr != NULL && !node_match(curr, node))
	{
		curr = curr->next;
	}

	return curr;
}

static void as_maybe(struct as *as, struct node *curr, const struct node *end, struct point n)
{
	if (hashnode_get(&as->closedhash, level_get_index(as->level, n.x, n.z, n.y), 0) != NULL)
	{
		return;
	}

	struct node *tentative = calloc(1, sizeof *tentative);
	tentative->point = n;

	float g = curr->g + point_dist(&curr->point, &n, false);

	struct node *maybe = as_isinlist(&as->openlist, tentative);
	if (maybe != NULL)
	{
		if (g < maybe->g)
		{
			openlist_del(&as->openlist, maybe);
		}
		else
		{
			free(tentative);
			return;
		}
	}

	tentative->parent = curr;
	tentative->h = point_dist(&n, &end->point, true);
	tentative->g = g;
	tentative->f = tentative->g + tentative->h;

	openlist_add(&as->openlist, tentative);
}

#define HASH_BITS 12
#define HASH_SIZE (1 << (HASH_BITS))

#define HASH_HALFBITS ((HASH_BITS) / 2)
#define HASH_HALFMASK ((1 << (HASH_HALFBITS)) - 1)

static unsigned as_hash(int key1, int key2)
{
	int part1 = key1 & HASH_HALFMASK;
	int part2 = key2 & HASH_HALFMASK;

	return ((part1 << HASH_HALFBITS) | part2) % HASH_SIZE;
}

struct point *as_find(const struct level_t *level, const struct point *a, const struct point *b)
{
	int i;

	struct as as;
	memset(&as, 0, sizeof as);
	as.level = level;

	struct node *start = calloc(1, sizeof *start);
	start->point = *a;

	struct node end;
	end.point = *b;

//	printf("Pathfinding from %d %d %d to %d %d %d\n", a->x, a->y, a->z, b->x, b->y, b->z);

	hash_init(&as.closedhash, &as_hash, HASH_SIZE);

	start->h = point_dist(&start->point, &end.point, true);
	start->f = start->h;

	openlist_add(&as.openlist, start);

	while (as.openlist != NULL)
	{
		struct node *curr = openlist_pop(&as.openlist);
		if (node_match(curr, &end))
		{
			struct node *n;

			int steps = 1;
			for (n = curr; n != NULL; n = n->parent)
			{
				steps++;
			}

			//printf("Found path, %d steps\n", steps - 1);

			struct point *s = malloc(sizeof *s * steps);
			struct point *ps = s + steps - 1;

			ps->x = -1;
			ps->y = -1;
			ps->z = -1;
			ps--;

			for (n = curr; n != NULL; n = n->parent, ps--)
			{
				ps->x = n->point.x;
				ps->y = n->point.y;
				ps->z = n->point.z;
			}

			if (steps > 2)
			{
				/* Optimise path */
				ps = s;
				struct point *ps2 = ps + 1;
				do {
					ps2++;
					if (path_iswalkable(as.level, ps, ps2))
					{
						*(ps + 1) = *ps2;
					}
					else
					{
						*(ps + 1) = *(ps2 - 1);
						ps++;
					}
				} while (ps2->x != -1);

				ps++;
				ps->x = -1;
			}

			hash_delete(&as.closedhash, true);
			openlist_wipe(&as.openlist);

			return s;
		}

		hashnode_set(&as.closedhash, level_get_index(level, curr->point.x, curr->point.z, curr->point.y), 0, curr);

		int c[8];
		struct point p[8];

		for (i = 0; i < 8; i += 2)
		{
			c[i] = point_add(as.level, &curr->point, i, &p[i]);

			if (c[i] > 0) as_maybe(&as, curr, &end, p[i]);
		}

		for (i = 1; i < 8; i += 2)
		{
			if (c[i - 1] == 1 && c[(i + 1) % 8] == 1)
			{
				c[i] = point_add(as.level, &curr->point, i, &p[i]);
				if (c[i] > 0) as_maybe(&as, curr, &end, p[i]);
			}
		}
	}

	hash_delete(&as.closedhash, true);
	openlist_wipe(&as.openlist);

//	printf("Path not found\n");

	return NULL;
}
