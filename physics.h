#ifndef PHYSICS_H
#define PHYSICS_H

#include <stdlib.h>

/* The physics object maintains a list of block indices which have physics.
 * This wastes some memory, but prevents having to scan x * y *z
 * blocks (which could reach a hundred million) every physics tick.
 *
 * This does not need to be saved.
 */
struct physics_t
{
	size_t used;
	size_t size;

	unsigned *indices;
};

static inline void physics_init(struct physics_t *physics)
{
	physics->used = 0;
	physics->size = 0;
	physics->indices = NULL;
}

static inline void physics_add(struct physics_t *physics, unsigned index)
{
	if (physics->used >= physics->size)
	{
		physics->size += sizeof *physics->indices * 64U;
		physics->indices = realloc(physics->indices, physics->size);
		/* No need to clear allocated memory */
	}

	physics->indices[physics->used++] = index;
}

static inline void physics_del(struct physics_t *physics, unsigned index)
{
	size_t i;

	for (i = 0; i < physics->used; i++)
	{
		if (physics->indices[i] == index) {
			physics->indices[i] = physics->indices[--physics->used];
			return;
		}
	}
}

#endif /* PHYSICS_H */
