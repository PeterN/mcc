#include "block.h"
#include "level.h"

static struct spleef_t
{
	enum blocktype_t air;
	enum blocktype_t floor1;
	enum blocktype_t floor2;
	enum blocktype_t green;

	enum blocktype_t spleef1;
	enum blocktype_t spleef2;
	enum blocktype_t spleeft;
} s;

static enum blocktype_t convert_spleef1(struct level_t *level, unsigned index, const struct block_t *block)
{
	switch (block->data)
	{
		case 0: return s.floor1;
		case 1: return s.air;
		case 2: return RED;
		case 3: return BLUE;
	}
//	return block->data ? s.air : s.floor1;
}

static enum blocktype_t convert_spleef2(struct level_t *level, unsigned index, const struct block_t *block)
{
	switch (block->data)
	{
		case 0: return s.floor2;
		case 1: return s.air;
		case 2: return RED;
		case 3: return BLUE;
	}
//
//	return block->data ? s.air : s.floor2;
}

static bool trigger_spleef(struct level_t *level, unsigned index, const struct block_t *block)
{
	level_addupdate(level, index, -1, 1);
	return false;
}

static void physics_spleef_sub(struct level_t *level, int16_t x, int16_t y, int16_t z)
{
	if (x < 0 || y < 0 || z < 0 || x >= level->x || y >= level->y || z >= level->z) return;

	unsigned index = level_get_index(level, x, y, z);
	if (level->blocks[index].type == s.spleef1 || level->blocks[index].type == s.spleef2)
	{
		if (level->blocks[index].data < 2)
			level_addupdate(level, index, -1, 2);
	}
}

static void physics_spleef(struct level_t *level, unsigned index, const struct block_t *block)
{
	int16_t x, y, z;
	level_get_xyz(level, index, &x, &y, &z);

	if (level->blocks[index].data < 2) return;
	if (level->blocks[index].data == 3)
	{
		level_addupdate(level, index, -1, 0);
		return;
	}

	physics_spleef_sub(level, x - 1, y, z);
	physics_spleef_sub(level, x + 1, y, z);
	physics_spleef_sub(level, x, y, z - 1);
	physics_spleef_sub(level, x, y, z + 1);

	level_addupdate(level, index, -1, 3);
}

static enum blocktype_t convert_spleeft(struct level_t *level, unsigned index, const struct block_t *block)
{
	return s.green;
}

static bool trigger_spleeft(struct level_t *level, unsigned index, const struct block_t *block)
{
	int16_t x, y, z;
	level_get_xyz(level, index, &x, &y, &z);

	physics_spleef_sub(level, x - 1, y, z);
	physics_spleef_sub(level, x + 1, y, z);
	physics_spleef_sub(level, x, y, z - 1);
	physics_spleef_sub(level, x, y, z + 1);
	return true;
}

void module_init(void **data)
{
	s.air = blocktype_get_by_name("air");
	s.floor1 = blocktype_get_by_name("iron");
	s.floor2 = blocktype_get_by_name("gold");
	s.green = blocktype_get_by_name("green");

	s.spleef1 = register_blocktype(-1, "spleef1", &convert_spleef1, &trigger_spleef, &physics_spleef);
	s.spleef2 = register_blocktype(-1, "spleef2", &convert_spleef2, &trigger_spleef, &physics_spleef);
	s.spleeft = register_blocktype(-1, "spleeft", &convert_spleeft, &trigger_spleeft, NULL);
}

void module_deinit(void *data)
{
}
