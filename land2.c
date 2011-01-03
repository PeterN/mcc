#include <stdbool.h>
#include <math.h>
#include "block.h"
#include "level.h"
#include "perlin.h"
#include "faultgen.h"
#include "filter.h"

static float range(float i, float l, float h)
{
	if (h <= l) return l;
	return l + (i * (h - l));
}

static float negate_edge(int x, int y, int mx, int my)
{
	float tx = ((float)x / (mx - 1)) * 0.5f;
	float ty = ((float)y / (my - 1)) * 0.5f;
	tx = fabsf(tx - 0.25f);
	ty = fabsf(ty - 0.25f);
	float t = (tx > ty ? tx : ty) - 0.15f;
	return t > 0.0f ? t : 0.0f;
}

static bool tree_check(const struct level_t *level, int x, int y, int z, int dist)
{
	int xx, yy, zz;

	for (xx = -dist; xx <= dist; xx++)
	{
		for (yy = -dist; yy <= dist; yy++)
		{
			for (zz = -dist; zz <= dist; zz++)
			{
				if (!level_valid_xyz(level, x + xx, y + yy, z + zz)) continue;

				if (level->blocks[level_get_index(level, x + xx, y + yy, z + zz)].type == TRUNK) return true;
			}
		}
	}
	return false;
}

static void add_tree(struct level_t *level, int x, int y, int z)
{
	int xx, yy, zz;

	struct block_t block;
	memset(&block, 0, sizeof block);

	int h = rand() % 4 + 4;

	block.type = TRUNK;
	for (yy = 0; yy < h; yy++)
	{
		unsigned index = level_get_index(level, x, y + yy, z);
		if (level->blocks[index].type == AIR)
		{
			level_set_block(level, &block, index);
		}
		else
		{
			h = yy;
		}
	}

	int t = h - 3;

	block.type = LEAF;
	for (xx = -t; xx <= t; xx++)
	{
		for (yy = -t; yy <= t; yy++)
		{
			for (zz = -t; zz <= t; zz++)
			{
				if (!level_valid_xyz(level, x + xx, y + yy + h, z + zz)) continue;

				unsigned index = level_get_index(level, x + xx, y + yy + h, z + zz);
				if (level->blocks[index].type == AIR)
				{
					int dist = xx * xx + yy * yy + zz * zz;
					if (dist < (t + 1) * (t + 1))
					{
						level_set_block(level, &block, index);
					}
				}
			}
		}
	}
}

void level_gen_mcsharp(struct level_t *level, const char *type)
{
	bool island = !strcmp(type, "island");
	bool forest = !strcmp(type, "forest");
	bool ocean = !strcmp(type, "ocean");
	bool mountains = !strcmp(type, "mountains");

	int mx = level->x;
	int my = level->y;
	int mz = level->z;

	struct block_t block;
	memset(&block, 0, sizeof block);

	memset(level->blocks, 0, sizeof *level->blocks * mx * my * mz);

	const float *terrain;
	const float *overlay;
	const float *overlay2;

	struct faultgen_t *fg;
	struct filter_t *ft;
	struct perlin_t *pp1;
	struct perlin_t *pp2;

	int waterlevel = ocean ? my * 0.85f : my / 2 + 2;

	/* Generate the level */
	fg = faultgen_init(mx, mz);
	faultgen_create(fg, mountains);

	/* Filter level */
	ft = filter_init(mx, mz);
	filter_process(ft, faultgen_map(fg));

	terrain = filter_map(ft);

	/* Overlay */
	pp1 = perlin_init(mx, mz, rand(), 0.7f, 8);
	perlin_noise(pp1);
	overlay = perlin_map(pp1);

	if (!ocean)
	{
		/* Trees */
		pp2 = perlin_init(mx, mz, rand(), 0.7f, 8);
		perlin_noise(pp2);
		overlay2 = perlin_map(pp2);
	}

	float rangelow  = 0.2f;
	float rangehigh = 0.8f;
	float treedens  = 0.35f;
	int treedist    = 3;

	if (island)
	{
		rangelow  = 0.4f;
		rangehigh = 0.75f;
	}
	else if (forest)
	{
		rangelow  = 0.45f;
		rangehigh = 0.8f;
		treedens  = 0.7f;
		treedist  = 2;
	}
	else if (mountains)
	{
		rangelow  = 0.3f;
		rangehigh = 0.9f;
		treedist  = 4;
	}
	else if (ocean)
	{
		rangelow  = 0.1f;
		rangehigh = 0.6f;
	}

	int bb;
	for (bb = 0; bb < mx * mz; bb++)
	{
		int x = bb % mx;
		int z = bb / mx;
		float fy;

		if (island)
		{
			fy = range(terrain[bb], rangelow - negate_edge(x, z, mx, mz), rangehigh - negate_edge(x, z, mx, mz));
		}
		else
		{
			fy = range(terrain[bb], rangelow, rangehigh);
		}

		/* Clamp fy to map height */
		int y = fy * my;
		if (y < 0) y = 0;
		if (y > my - 1) y = my - 1;

		if (y > waterlevel)
		{
			int yy;
			for (yy = 0; y - yy >= 0; yy++)
			{
				if (overlay[bb] < 0.72f)
				{
					/* Not zoned for rocks/gravel */
					if (island)
					{
						/* Increase sand height for islands */
						if (y > waterlevel + 2)
						{
							if (yy == 0)
							{
								block.type = GRASS;
							}
							else if (yy < 3)
							{
								block.type = DIRT;
							}
							else
							{
								block.type = ROCK;
							}
						}
						else
						{
							block.type = SAND;
						}
					}
					else
					{
						if (yy == 0)
						{
							block.type = GRASS;
						}
						else if (yy < 3)
						{
							block.type = DIRT;
						}
						else
						{
							block.type = ROCK;
						}
					}
				}
				else
				{
					block.type = ROCK;
				}

				level_set_block(level, &block, level_get_index(level, x, y - yy, z));
			}

			if (overlay[bb] < 0.25f)
			{
				/* Flowers */
				int t = rand() % 12;

				if (t == 10)
				{
					block.type = REDFLOWER;
					level_set_block(level, &block, level_get_index(level, x, y + 1, z));
				}
				else if (t == 11)
				{
					block.type = YELLOWFLOWER;
					level_set_block(level, &block, level_get_index(level, x, y + 1, z));
				}
			}

			if (!ocean)
			{
				/* Trees */
				if (overlay[bb] < 0.65f && overlay2[bb] < treedens)
				{
					if (level->blocks[level_get_index(level, x, y + 1, z)].type == AIR)
					{
						if (level->blocks[level_get_index(level, x, y, z)].type == GRASS)
						{
							if (rand() % 13 == 0)
							{
								if (!tree_check(level, x, y, z, treedist))
								{
									add_tree(level, x, y + 1, z);
								}
							}
						}
					}
				}
			}
		}
		else
		{
			/* On/under water line */
			int yy;
			for (yy = 0; waterlevel - yy >= 0; yy++)
			{
				if (waterlevel - yy > y)
				{
					block.type = WATER;
				}
				else if (waterlevel - yy > y - 3)
				{
					if (overlay[bb] < 0.75f)
					{
						block.type = SAND;
					}
					else
					{
						block.type = GRAVEL;
					}
				}
				else
				{
					block.type = ROCK;
				}
				level_set_block(level, &block, level_get_index(level, x, waterlevel - yy, z));
			}
		}
	}

	faultgen_deinit(fg);
	filter_deinit(ft);
	perlin_deinit(pp1);
	if (!ocean) perlin_deinit(pp2);
}
