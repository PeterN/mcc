#include "level.h"
#include "block.h"

static struct
{
	enum blocktype_t air_layer;
} s;

enum blocktype_t convert_air_layer(struct level_t *level, unsigned index, const struct block_t *block)
{
	return AIR;
}

void physics_air_layer_sub(struct level_t *l, int16_t x, int16_t y, int16_t z, enum blocktype_t type)
{
	// Test x,y,z are valid!
	if (x < 0 || y < 0 || z < 0 || x >= l->x || y >= l->y || z >= l->z) return;

	unsigned index = level_get_index(l, x, y, z);
	switch (l->blocks[index].type)
	{
		case WATER:
		case WATERSTILL:
		case LAVA:
		case LAVASTILL:
			level_addupdate(l, index, type, 0);
			break;
	}
}

void physics_air_layer(struct level_t *l, unsigned index, const struct block_t *block)
{
	int16_t x, y, z;
	level_get_xyz(l, index, &x, &y, &z);

	physics_air_layer_sub(l, x - 1, y, z, block->type);
	physics_air_layer_sub(l, x + 1, y, z, block->type);
	physics_air_layer_sub(l, x, y, z - 1, block->type);
	physics_air_layer_sub(l, x, y, z + 1, block->type);
	level_addupdate(l, index, AIR, 0);
}

void module_init(void **data)
{
	s.air_layer = register_blocktype(-1, "air_layer", &convert_air_layer, NULL, &physics_air_layer);
}

void module_deinit(void *data)
{
	deregister_blocktype(s.air_layer);
}
