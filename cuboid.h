#ifndef CUBOID_H
#define CUBOID_H

#include <stdint.h>
#include <string.h>
#include "block.h"
#include "list.h"

struct level_t;

struct cuboid_t
{
	int16_t sx, sy, sz;
	int16_t ex, ey, ez;
	int16_t cx, cy, cz;
	enum blocktype_t old_type;
	enum blocktype_t new_type;
	struct level_t *level;
	unsigned owner;
	bool fixed;
	int count;
};

static inline bool cuboid_t_compare(struct cuboid_t *a, struct cuboid_t *b)
{
	return memcmp(a, b, sizeof *a) == 0;
}
LIST(cuboid, struct cuboid_t, cuboid_t_compare)

extern struct cuboid_list_t s_cuboids;

#endif /* CUBOID_H */
