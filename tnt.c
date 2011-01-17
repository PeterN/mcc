#include <math.h>
#include "level.h"
#include "block.h"

static struct
{
	enum blocktype_t active_tnt;
	enum blocktype_t explosion;
	enum blocktype_t fuse;
} s;

static enum blocktype_t convert_active_tnt(struct level_t *level, unsigned index, const struct block_t *block)
{
	return TNT;
}

static int trigger_active_tnt(struct level_t *l, unsigned index, const struct block_t *block, struct client_t *c)
{
	level_addupdate(l, index, BLOCK_INVALID, ((rand() % 3 + 3) << 8) | 1);

	return TRIG_FILL;
}

static void physics_active_tnt(struct level_t *l, unsigned index, const struct block_t *block)
{
	int r = GetBits(block->data, 0, 8);
	if (r == 0) return;

	int mr = GetBits(block->data, 8, 8);

	int nr = r + 1;
	if (nr > mr)
	{
		level_addupdate(l, index, AIR, 0);
	}
	else
	{
		level_addupdate(l, index, BLOCK_INVALID, (mr << 8) | nr);
	}

	int16_t x, y, z;
	level_get_xyz(l, index, &x, &y, &z);

	int dx, dy, dz, bx, by, bz;
	int r2 = r * r;
	for (dx = -r; dx <= r; dx++)
	{
		for (dy = -r; dy <= r; dy++)
		{
			for (dz = -r; dz <= r; dz++)
			{
				if (dx == 0 && dy == 0 && dz == 0) continue;

				bx = x + dx; by = y + dy; bz = z + dz;
				if (!level_valid_xyz(l, bx, by, bz)) continue;

				index = level_get_index(l, bx, by, bz);
				if (l->blocks[index].fixed || l->blocks[index].type == ADMINIUM || l->blocks[index].type == s.explosion) continue;

				if (l->blocks[index].type == s.active_tnt)
				{
					if (l->blocks[index].data == 0) level_addupdate(l, index, BLOCK_INVALID, ((rand() % 5 + 5) << 8) | 1);
				}
				else if (l->blocks[index].type == s.fuse)
				{
					/* Trigger fuse early */
					if (l->blocks[index].data == 0) level_addupdate(l, index, BLOCK_INVALID, 2);
				}
				else if (rand() % 4 < 3)
				{
					int d = dx * dx + dy * dy + dz * dz;
					if (d < r2)
					{
						level_addupdate(l, index, s.explosion, 8);
					}
				}
			}
		}
	}
}

static enum blocktype_t convert_explosion(struct level_t *level, unsigned index, const struct block_t *block)
{
//	return LAVASTILL;
	/* Flicker */
	return HasBit(block->data, 12) ? AIR : LAVASTILL;
}

#if 0
static void physics_explosion_sub(struct level_t *l, int16_t x, int16_t y, int16_t z, enum blocktype_t type, int magnitude)
{
	if (!level_valid_xyz(l, x, y, z)) return;

	unsigned index = level_get_index(l, x, y, z);
	if (l->blocks[index].fixed || l->blocks[index].type == ADMINIUM) return;
	//if (l->blocks[index].owner != 0 && l->blocks[index].owner != owner) return;

	if (l->blocks[index].type == s.active_tnt)
	{
		magnitude = 0x305;
	}
	else if (l->blocks[index].type == s.fuse)
	{
		/* Trigger fuse early */
		level_addupdate(l, index, -1, 2);
		return;
	}

	level_addupdate(l, index, type, magnitude);
}
#endif

static void physics_explosion(struct level_t *l, unsigned index, const struct block_t *block)
{
	int tr = GetBits(block->data, 0, 8);

	if (tr == 0)
	{
		level_addupdate(l, index, AIR, 0);
	}
	else
	{
		bool invis = HasBit(block->data, 12);

		if (!invis && tr <= rand() % 5)
		{
			invis = true;
		}

		level_addupdate(l, index, BLOCK_INVALID, (invis << 12) | (tr - 1));
	}

	/*
	int iter = GetBits(block->data, 8, 4);

	if (iter > 0)
	{
		int16_t x, y, z, dx, dy, dz;
		level_get_xyz(l, index, &x, &y, &z);

		for (dx = -1; dx <= 1; dx++)
		{
			for (dy = -1; dy <= 1; dy++)
			{
				for (dz = -1; dz <= 1; dz++)
				{
					if (dx == 0 && dy == 0 && dz == 0) continue;
					int r = (block->data & 0xFF) - rand() % 3;
					if (r <= 1) continue;
					int sub_iter = iter - (dx == 0 || dy == 0 || dz == 0 ? 1 : 2);
					if (sub_iter < 0) continue;
					physics_explosion_sub(l, x + dx, y + dy, z + dz, block->type, (sub_iter << 8) + r);
				}
			}
		}
	}

	if ((block->data & 0xFF) == 0)
	{
		level_addupdate(l, index, AIR, 0);
	}
	else
	{
		int vis = (rand() % 10 < 3) << 12;
		level_addupdate(l, index, -1, vis | ((block->data & 0xFF) - 1));
	}
	*/
}

static enum blocktype_t convert_fuse(struct level_t *level, unsigned index, const struct block_t *block)
{
	if (block->data == 0) return DARKGREY;

	switch (GetBits(block->data, 8, 2))
	{
		case 0: return RED;
		case 1: return ORANGE;
		case 2: return YELLOW;
		default: return LIGHTGREY;
	}
}

static int trigger_fuse(struct level_t *l, unsigned index, const struct block_t *block, struct client_t *c)
{
	level_addupdate(l, index, -1, 5);

	return TRIG_FILL;
}

static void physics_fuse_sub(struct level_t *l, int16_t x, int16_t y, int16_t z)
{
	if (!level_valid_xyz(l, x, y, z)) return;

	unsigned index = level_get_index(l, x, y, z);

	/* Don't mess with fixed blocks */
	if (l->blocks[index].fixed) return;

	if (l->blocks[index].type == s.fuse)
	{
		if (l->blocks[index].data == 0)
		{
			level_addupdate(l, index, BLOCK_INVALID, 5);
		}
	}
	else if (l->blocks[index].type == s.active_tnt)
	{
		level_addupdate(l, index, BLOCK_INVALID, ((rand() % 3 + 3) << 8) | 1);
	}
}

static void physics_fuse(struct level_t *l, unsigned index, const struct block_t *block)
{
	int stage = GetBits(block->data, 0, 8);
	if (stage == 0) return;
	if (stage == 1)
	{
		int16_t x, y, z, dx, dy, dz;
		level_get_xyz(l, index, &x, &y, &z);

		for (dx = -1; dx <= 1; dx++)
		{
			for (dy = -1; dy <= 1; dy++)
			{
				for (dz = -1; dz <= 1; dz++)
				{
					if (dx == 0 && dy == 0 && dz == 0) continue;
					physics_fuse_sub(l, x + dx, y + dy, z + dz);
				}
			}
		}

		if (!block->fixed)
		{
			level_addupdate(l, index, AIR, 0);
		}
	}
	else
	{
		level_addupdate(l, index, BLOCK_INVALID, (stage - 1) | ((rand() % 4) << 8));
	}
}

void module_init(void **data)
{
	s.active_tnt = register_blocktype(BLOCK_INVALID, "active_tnt", RANK_ADV_BUILDER, &convert_active_tnt, &trigger_active_tnt, NULL, &physics_active_tnt, false, false, false);
	s.explosion = register_blocktype(BLOCK_INVALID, "explosion", RANK_ADV_BUILDER, &convert_explosion, NULL, NULL, &physics_explosion, false, true, false);
	s.fuse = register_blocktype(BLOCK_INVALID, "fuse", RANK_ADV_BUILDER, &convert_fuse, &trigger_fuse, NULL, &physics_fuse, false, false, false);
}

void module_deinit(void *data)
{
	deregister_blocktype(s.active_tnt);
	deregister_blocktype(s.explosion);
	deregister_blocktype(s.fuse);
}
