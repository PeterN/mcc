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
	int dx = a->x - b->x;
	int dy = abs(a->y - b->y);
	int dz = a->z - b->z;

	/* Elongate the centre vertically */
	if (dy < area / 2)
	{
		dy = 0;
	}
	else
	{
		dy -= area / 2;
	}

	return (dx * dx + dy * dy + dz * dz < area * area);
}

#endif /* POSITION */
