#include "level.h"
#include "block.h"

static struct
{
	enum blocktype_t tnt;
	
	enum blocktype_t active_tnt;
	enum blocktype_t explosion;
	enum blocktype_t fuse;
} s;

enum blocktype_t convert_active_tnt(struct level_t *level, unsigned index, const struct block_t *block)
{
	return TNT;
}

int trigger_active_tnt(struct level_t *l, unsigned index, const struct block_t *block, struct client_t *c)
{
	level_addupdate(l, index, s.explosion, 0x305);

	return TRIG_FILL;
}

enum blocktype_t convert_explosion(struct level_t *level, unsigned index, const struct block_t *block)
{
	/* Flicker */
	return HasBit(block->data, 12) ? AIR : LAVA;
}

void physics_explosion_sub(struct level_t *l, int16_t x, int16_t y, int16_t z, enum blocktype_t type, int magnitude)
{
	// Test x,y,z are valid!
	if (x < 0 || y < 0 || z < 0 || x >= l->x || y >= l->y || z >= l->z) return;

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

void physics_explosion(struct level_t *l, unsigned index, const struct block_t *block)
{
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
}

enum blocktype_t convert_fuse(struct level_t *level, unsigned index, const struct block_t *block)
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

int trigger_fuse(struct level_t *l, unsigned index, const struct block_t *block, struct client_t *c)
{
	level_addupdate(l, index, -1, 5);

	return TRIG_FILL;
}

void physics_fuse_sub(struct level_t *l, int16_t x, int16_t y, int16_t z, enum blocktype_t type, enum blocktype_t tnt)
{
	// Test x,y,z are valid!
	if (x < 0 || y < 0 || z < 0 || x >= l->x || y >= l->y || z >= l->z) return;

	unsigned index = level_get_index(l, x, y, z);

	/* Don't mess with fixed blocks */
	if (l->blocks[index].fixed) return;

	if (l->blocks[index].type == type)
	{
		if (l->blocks[index].data == 0)
		{
			level_addupdate(l, index, type, 5);
		}
	}
	else if (l->blocks[index].type == tnt)
	{
		level_addupdate(l, index, s.explosion, 0x305);
	}
}

void physics_fuse(struct level_t *l, unsigned index, const struct block_t *block)
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
					physics_fuse_sub(l, x + dx, y + dy, z + dz, block->type, s.active_tnt);
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
		level_addupdate(l, index, -1, (stage - 1) | ((rand() % 4) << 8));
	}
}

void module_init(void **data)
{
	s.tnt = blocktype_get_by_name("tnt");
	
	s.active_tnt = register_blocktype(BLOCK_INVALID, "active_tnt", RANK_ADV_BUILDER, &convert_active_tnt, &trigger_active_tnt, NULL, NULL, false);
	s.explosion = register_blocktype(BLOCK_INVALID, "explosion", RANK_ADV_BUILDER, &convert_explosion, NULL, NULL, &physics_explosion, false);
	s.fuse = register_blocktype(BLOCK_INVALID, "fuse", RANK_ADV_BUILDER, &convert_fuse, &trigger_fuse, NULL, &physics_fuse, false);
}
