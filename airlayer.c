#include "level.h"
#include "block.h"

static struct
{
	enum blocktype_t air_layer;
} s;

static enum blocktype_t convert_air_layer(struct level_t *level, unsigned index, const struct block_t *block)
{
	return AIR;
}

static void physics_air_layer_sub(struct level_t *l, int16_t x, int16_t y, int16_t z, enum blocktype_t type)
{
	if (!level_valid_xyz(l, x, y, z)) return;

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

static void physics_air_layer(struct level_t *l, unsigned index, const struct block_t *block)
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
	s.air_layer = register_blocktype(-1, "air_layer", RANK_ADV_BUILDER, &convert_air_layer, NULL, NULL, &physics_air_layer, true, true, false);
}

void module_deinit(void *data)
{
	deregister_blocktype(s.air_layer);
}
