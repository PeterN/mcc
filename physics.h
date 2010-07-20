#ifndef PHYSICS_H
#define PHYSICS_H

#include "list.h"

static inline bool unsigned_compare(unsigned *a, unsigned *b)
{
	return *a == *b;
}
LIST(physics, unsigned, unsigned_compare)

struct block_update_t
{
	unsigned index;
	enum blocktype_t newtype;
	uint16_t newdata;
	//struct block_t block;
};

static inline bool block_update_t_compare(struct block_update_t *a, struct block_update_t *b)
{
	return a->index == b->index;
}
LIST(block_update, struct block_update_t, block_update_t_compare)

#endif /* PHYSICS_H */
