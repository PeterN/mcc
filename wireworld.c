#include "level.h"
#include "block.h"

static struct
{
	enum blocktype_t wire;
	enum blocktype_t wire3d;
} s;

enum blocktype_t convert_wire(struct level_t *level, unsigned index, const struct block_t *block)
{
	switch (block->data)
	{
		default: return GOLDSOLID;
		case 1: return RED;
		case 2: return BLUE;
	}
}

int trigger_wire(struct level_t *l, unsigned index, const struct block_t *block, struct client_t *c)
{
	if (block->data == 0)
	{
		level_addupdate(l, index, -1, 1);
	}

	return TRIG_FILL;
}

int physics_wire_sub(struct level_t *l, int16_t x, int16_t y, int16_t z, enum blocktype_t type)
{
	// Test x,y,z are valid!
	if (x < 0 || y < 0 || z < 0 || x >= l->x || y >= l->y || z >= l->z) return 0;

	unsigned index = level_get_index(l, x, y, z);
	if (l->blocks[index].type == type && l->blocks[index].data == 1)
	{
		return 1;
	}

	return 0;
}

void physics_wire(struct level_t *l, unsigned index, const struct block_t *block)
{
	if (block->data == 2)
	{
		level_addupdate(l, index, -1, 0);
	}
	else if (block->data == 1)
	{
		level_addupdate(l, index, -1, 2);
	}
	else
	{
		int n = 0;

		int16_t x, y, z;
		level_get_xyz(l, index, &x, &y, &z);

		n += physics_wire_sub(l, x - 1, y, z - 1, block->type);
		n += physics_wire_sub(l, x    , y, z - 1, block->type);
		n += physics_wire_sub(l, x + 1, y, z - 1, block->type);
		n += physics_wire_sub(l, x - 1, y, z    , block->type);
		n += physics_wire_sub(l, x + 1, y, z    , block->type);
		n += physics_wire_sub(l, x - 1, y, z + 1, block->type);
		n += physics_wire_sub(l, x    , y, z + 1, block->type);
		n += physics_wire_sub(l, x + 1, y, z + 1, block->type);

		if (n == 1 || n == 2)
		{
			level_addupdate(l, index, -1, 1);
		}
		else
		{
			level_addupdate(l, index, -1, 0);
		}
	}
}

void physics_wire3d(struct level_t *l, unsigned index, const struct block_t *block)
{
	if (block->data == 2)
	{
		level_addupdate(l, index, -1, 0);
	}
	else if (block->data == 1)
	{
		level_addupdate(l, index, -1, 2);
	}
	else
	{
		int n = 0;

		int16_t x, y, z, dx, dy, dz;
		level_get_xyz(l, index, &x, &y, &z);

		for (dx = -1; dx <= 1; dx++)
		{
			for (dy = -1; dy <= 1; dy++)
			{
				for (dz = -1; dz <= 1; dz++)
				{
					if (dx == 0 && dy == 0 && dz == 0) continue;
					n += physics_wire_sub(l, x + dx, y + dy, z + dz, block->type);
					if (n > 2)
					{
						level_addupdate(l, index, -1, 0);
						return;
					}
				}
			}
		}

		if (n == 1 || n == 2)
		{
			level_addupdate(l, index, -1, 1);
		}
		else
		{
			level_addupdate(l, index, -1, 0);
		}
	}
}

void module_init(void **data)
{
	s.wire = register_blocktype(BLOCK_INVALID, "wire", RANK_BUILDER, &convert_wire, &trigger_wire, NULL, &physics_wire, false);
	s.wire3d = register_blocktype(BLOCK_INVALID, "wire3d", RANK_BUILDER, &convert_wire, &trigger_wire, NULL, &physics_wire3d, false);
}

void module_deinit(void *data)
{
	deregister_blocktype(s.wire);
	deregister_blocktype(s.wire3d);
}
