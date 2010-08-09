#include <stdlib.h>
#include <math.h>
#include "perlin.h"
#include "mcc.h"

struct perlin_t
{
	int x, y;
	int offset_x, offset_y;
	int seed;
	float persistence;
	int octaves;
	float map[];
};

static float noise(const struct perlin_t *pp, int x, int y)
{
	int n = x + y * 57 + pp->seed;
	n = (n << 13) ^ n;
	return 1.0 - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7FFFFFFF) / 1073741824.0;
}

static float perlin_smoothed_noise(const struct perlin_t *pp, int x, int y)
{
	float corners = (noise(pp, x - 1, y - 1) + noise(pp, x + 1, y - 1) + noise(pp, x - 1, y + 1) + noise(pp, x + 1, y + 1)) / 16.0;
	float sides   = (noise(pp, x - 1, y) + noise(pp, x + 1, y) + noise(pp, x, y - 1) + noise(pp, x, y + 1)) / 8.0;
	float centre  = noise(pp, x, y) / 4.0;

	return corners + sides + centre;
}

static float perlin_interpolate(float a, float b, float x)
{
	float f = (1 - cos(x * M_PI)) * 0.5;

	return a * (1 - f) + b * f;
}

static float perlin_interpolated_noise(const struct perlin_t *pp, float x, float y)
{
	int ix = floor(x);
	float fx = x - ix;

	int iy = floor(y);
	float fy = y - iy;

	float v1 = perlin_smoothed_noise(pp, ix    , iy);
	float v2 = perlin_smoothed_noise(pp, ix + 1, iy);
	float v3 = perlin_smoothed_noise(pp, ix    , iy + 1);
	float v4 = perlin_smoothed_noise(pp, ix + 1, iy + 1);

	float i1 = perlin_interpolate(v1, v2, fx);
	float i2 = perlin_interpolate(v3, v4, fx);

	return perlin_interpolate(i1, i2, fy);
}

static float perlin_noise_2d(const struct perlin_t *pp, float x, float y)
{
	float total = 0;
	float frequency = 1.0 / (1 << pp->octaves);
	float amplitude = 1.0;
	int i;

	for (i = 1; i < pp->octaves; i++)
	{
		total += perlin_interpolated_noise(pp, x * frequency, y * frequency) * amplitude;
		frequency *= 2;
		amplitude *= pp->persistence;
	}

	return total;
}

void perlin_noise(struct perlin_t *pp)
{
	float min = +INFINITY;
	float max = -INFINITY;
	int x, y;

//	LOG("perlin: generating %d by %d map\n", pp->x, pp->y);

	for (x = 0; x < pp->x; x++)
	{
		for (y = 0; y < pp->y; y++)
		{
			float f = perlin_noise_2d(pp, x + pp->offset_x, y + pp->offset_y);
			pp->map[x + y * pp->x] = f;

			if (f < min) min = f;
			if (f > max) max = f;
		}
	}

//	LOG("perlin: range %f to %f\n", min, max);

//	LOG("perlin: normalizing\n");

//	for (x = 0; x < pp->x * pp->y; x++)
//	{
//		pp->map[x] = (pp->map[x] - min) / (max - min);
//	}

//	LOG("perlin: complete\n");
}

struct perlin_t *perlin_init(int x, int y, int seed, float persistence, int octaves)
{
	struct perlin_t *pp = malloc(sizeof *pp + sizeof *pp->map * x * y);
	if (pp == NULL)
	{
		LOG("[perlin] perlin_init(): couldn't allocate %lu bytes\n", sizeof *pp + sizeof *pp->map * x * y);
		return NULL;
	}

	pp->x = x;
	pp->y = y;
	pp->offset_x = 0;
	pp->offset_y = 0;
	pp->seed = seed;
	pp->persistence = persistence;
	pp->octaves = octaves;

	return pp;
}

void perlin_set_offset(struct perlin_t *pp, int x, int y)
{
	pp->offset_x = x;
	pp->offset_y = y;
}

void perlin_deinit(struct perlin_t *pp)
{
	free(pp);
}

const float *perlin_map(struct perlin_t *pp)
{
	return pp->map;
}
