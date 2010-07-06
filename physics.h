#ifndef PHYSICS_H
#define PHYSICS_H

#include "list.h"

static inline bool unsigned_compare(unsigned *a, unsigned *b)
{
	return *a == *b;
}
LIST(physics, unsigned, unsigned_compare)

/* The physics object maintains a list of block indices which have physics.
 * This wastes some memory, but prevents having to scan x * y *z
 * blocks (which could reach a hundred million) every physics tick.
 *
 * This does not need to be saved.
 */

#endif /* PHYSICS_H */
