#include <stdlib.h>
#include <math.h>
#include "filter.h"
#include "mcc.h"

struct filter_t
{
	int x, y;
	float map[];
};

struct filter_t *filter_init(int x, int y)
{
	struct filter_t *f = malloc(sizeof *f + sizeof *f->map * x * y);
	if (f == NULL)
	{
		LOG("[filter] filter_init(): couldn't allocate %zu bytes\n", sizeof *f + sizeof *f->map * x * y);
		return NULL;
	}

	f->x = x;
	f->y = y;
	return f;
}

void filter_deinit(struct filter_t *f)
{
	free(f);
}

void filter_process(struct filter_t *f, const float *map)
{
	int x, y, dx, dy;

	LOG("filter: processing\n");

	for (x = 0; x < f->x; x++)
	{
		for (y = 0; y < f->y; y++)
		{
			float *h = &f->map[x + y * f->x];
			int divide = 0;

			*h = 0;

			for (dx = -1; dx <= 1; dx++)
			{
				for (dy = -1; dy <= 1; dy++)
				{
					int ax = x + dx;
					int ay = y + dy;

					if (ax < 0 || ay < 0 || ax >= f->x || ay >= f->y) continue;

					*h += map[ax + ay * f->x];
					divide++;
				}
			}

			*h /= divide;
		}
	}

	LOG("filter: complete\n");
}

const float *filter_map(struct filter_t *f)
{
	return f->map;
}
