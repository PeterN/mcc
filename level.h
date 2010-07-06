#ifndef LEVEL_H
#define LEVEL_H

#include "block.h"
#include "physics.h"

struct level_t
{
	unsigned x;
	unsigned y;
	unsigned z;

	struct block_t *blocks;
	struct physics_list_t physics;
};

static inline unsigned level_get_index(struct level_t *level, unsigned x, unsigned y, unsigned z)
{
	return x + (z * level->y + y) * level->x;
}

void level_init(struct level_t *level, unsigned x, unsigned y, unsigned z);
void level_set_block(struct level_t *level, struct block_t *block, unsigned index);
void level_clear_block(struct level_t *level, unsigned index);

#endif /* LEVEL_H */
