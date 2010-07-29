#ifndef POSITION_H
#define POSITION_H

#include <stdbool.h>

struct position_t
{
	int16_t x;
	int16_t y;
	int16_t z;
	uint8_t h;
	uint8_t p;
};

static inline bool position_match(const struct position_t *a, const struct position_t *b, int area)
{
	if (abs(a->x - b->x) >= area) return false;
	if (abs(a->y - b->y) >= area) return false;
	if (abs(a->z - b->z) >= area) return false;
	return true;
}

#endif /* POSITION */
