#include <stdlib.h>
#include "level.h"

void level_init(struct level_t *level, unsigned x, unsigned y, unsigned z)
{
	level->x = x;
	level->y = y;
	level->z = z;

	level->blocks = calloc(x * y * z, sizeof *level->blocks);

	physics_init(&level->physics);
}

void level_set_block(struct level_t *level, struct block_t *block, unsigned index)
{
	bool old_phys = block_has_physics(&level->blocks[index]);
	bool new_phys = block_has_physics(block);

	level->blocks[index] = *block;

	if (new_phys != old_phys)
	{
		if (new_phys)
		{
			physics_add(&level->physics, index);
		}
		else
		{
			physics_del(&level->physics, index);
		}
	}
}

void level_clear_block(struct level_t *level, unsigned index)
{
	static struct block_t empty = { AIR, 0 };

	level_set_block(level, &empty, index);
}
