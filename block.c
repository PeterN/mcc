#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "mcc.h"
#include "block.h"
#include "level.h"
#include "client.h"
#include "colour.h"

static struct blocktype_desc_list_t s_blocks;

int register_blocktype(enum blocktype_t type, const char *name, enum rank_t min_rank, convert_func_t convert_func, trigger_func_t trigger_func, delete_func_t delete_func, physics_func_t physics_func, bool clear)
{
	struct blocktype_desc_t desc;
	memset(&desc, 0, sizeof desc);

	if (type == BLOCK_INVALID)
	{
		/* Allocate a new block type, reusing the name if available */
		unsigned i;
		for (i = 0; i < s_blocks.used; i++)
		{
			if (strcasecmp(s_blocks.items[i].name, name) == 0)
			{
				type = i;
				break;
			}
		}

		if (type == BLOCK_INVALID)
		{
			blocktype_desc_list_add(&s_blocks, desc);
			type = s_blocks.used - 1;
		}
	}
	else
	{
		/* Add blank entries until we reach the block we want */
		while (s_blocks.used <= type)
		{
			blocktype_desc_list_add(&s_blocks, desc);
		}
	}

	struct blocktype_desc_t *descp = &s_blocks.items[type];

	if (descp->loaded)
	{
		LOG("Block type %s already registered\n", descp->name);
		return BLOCK_INVALID;
	}

	free(descp->name);
	descp->name = strdup(name);
	descp->loaded = true;
	descp->min_rank = min_rank;
	descp->convert_func = convert_func;
	descp->trigger_func = trigger_func;
	descp->delete_func = delete_func;
	descp->physics_func = physics_func;
	LOG("Registered %s as %d\n", descp->name, type);

	return type;
}

void deregister_blocktype(enum blocktype_t type)
{
	if (type == BLOCK_INVALID) return;

	struct blocktype_desc_t *descp = &s_blocks.items[type];
	descp->loaded = false;
	descp->convert_func = NULL;
	descp->trigger_func = NULL;
	descp->delete_func = NULL;
	descp->physics_func = NULL;
	LOG("Deregistered %s / %d\n", descp->name, type);
}

enum blocktype_t convert(struct level_t *level, unsigned index, const struct block_t *block)
{
	const struct blocktype_desc_t *btd = &s_blocks.items[block->type];
	if (btd->convert_func != NULL)
	{
		return btd->convert_func(level, index, block);
	}

	/* Non-standard block that no longer exists? Show as red... */
	if (block->type >= BLOCK_END) return RED;

	return block->type;
}

int trigger(struct level_t *l, unsigned index, const struct block_t *block)
{
	const struct blocktype_desc_t *btd = &s_blocks.items[block->type];
	if (btd->trigger_func != NULL)
	{
		return btd->trigger_func(l, index, block) ? 2 : 1;
	}
	return 0;
}

void delete(struct level_t *l, unsigned index, const struct block_t *block)
{
	const struct blocktype_desc_t *btd = &s_blocks.items[block->type];
	if (btd->delete_func != NULL)
	{
		btd->delete_func(l, index, block);
	}
}

void physics(struct level_t *level, unsigned index, const struct block_t *block)
{
	const struct blocktype_desc_t *btd = &s_blocks.items[block->type];

	if (btd->physics_func != NULL)
	{
		btd->physics_func(level, index, block);
	}
	level->blocks[index].physics = false;
}

bool light_check(struct level_t *level, unsigned index)
{
	int16_t x, y, z;
	if (!level_get_xyz(level, index, &x, &y, &z)) return true;

	y++;
	for (; y < level->y; y++)
	{
		enum blocktype_t type = level->blocks[level_get_index(level, x, y, z)].type;
		const struct blocktype_desc_t *btd = &s_blocks.items[type];
		if (!btd->clear) return false;
	}

	return true;
}

void physics_air_sub(struct level_t *l, unsigned index2, int16_t x, int16_t y, int16_t z, bool gravity, int data)
{
	// Test x,y,z are valid!
	if (x < 0 || y < 0 || z < 0 || x >= l->x || y >= l->y || z >= l->z) return;

	unsigned index = level_get_index(l, x, y, z);
	if (l->blocks[index].fixed) return;

	switch (l->blocks[index].type)
	{
		default: return;
		case WATER:
		case LAVA:
			/* Reactivate water */
			if (data == 0) level_addupdate(l, index, -1, 0);
			return;

		case SAND:
		case GRAVEL:
			if (gravity)
			{
				level_addupdate(l, index, -1, 0);
			}
			return;
	}
}

bool sponge_test(struct level_t *l, int16_t x, int16_t y, int16_t z)
{
	/* Check neighbouring blocks for sponge */
	int16_t dx, dy, dz, ax, ay, az;

	for (dx = -2; dx <= 2; dx++)
	{
		for (dy = -2; dy <= 2; dy++)
		{
			for (dz = -2; dz <= 2; dz++)
			{
				if (dx == 0 && dy == 0 && dz == 0) continue;

				ax = x + dx;
				ay = y + dy;
				az = z + dz;

				if (ax < 0 || ay < 0 || az < 0 || ax >= l->x || ay >= l->y || az >= l->z) continue;

				unsigned index = level_get_index(l, ax, ay, az);
				if (l->blocks[index].type == SPONGE) return true;
			}
		}
	}

	return false;
}

void physics_air(struct level_t *l, unsigned index, const struct block_t *block)
{
	int16_t x, y, z;
	level_get_xyz(l, index, &x, &y, &z);

	if (block->fixed) return;

	int flood = block->data;
	if (flood == 0)
	{
		/* Check for sponge near us */
		if (sponge_test(l, x, y, z))
		{
//			if (flood == 0)
//			{
				level_addupdate(l, index, -1, 1);
				flood = 1;
//			}
		}
//		else
//		{
//			if (flood == 1)
//			{
//				level_addupdate(l, index, -1, 0);
//				flood = 0;
//			}
//		}
	}

	physics_air_sub(l, index, x, y + 1, z, true, flood);
	physics_air_sub(l, index, x - 1, y, z, false, flood);
	physics_air_sub(l, index, x + 1, y, z, false, flood);
	physics_air_sub(l, index, x, y, z - 1, false, flood);
	physics_air_sub(l, index, x, y, z + 1, false, flood);
}

void physics_dirt(struct level_t *l, unsigned index, const struct block_t *block)
{
	if (block->data > 90)
	{
		if (light_check(l, index))
		{
			level_addupdate(l, index, GRASS, 0);
		}
	}
	else
	{
		level_addupdate(l, index, BLOCK_INVALID, block->data + 1);
	}
}

void physics_active_water_sub(struct level_t *l, int16_t x, int16_t y, int16_t z, enum blocktype_t type)
{
	// Test x,y,z are valid!
	if (x < 0 || y < 0 || z < 0 || x >= l->x || y >= l->y || z >= l->z) return;

	unsigned index = level_get_index(l, x, y, z);
	if (l->blocks[index].type == AIR && !l->blocks[index].fixed && l->blocks[index].data == 0)
	{
		if (!sponge_test(l, x, y, z)) level_addupdate(l, index, type, 0);
	}
}

void physics_active_water(struct level_t *l, unsigned index, const struct block_t *block)
{
	int16_t x, y, z;
	level_get_xyz(l, index, &x, &y, &z);

	physics_active_water_sub(l, x, y - 1, z, block->type);
	physics_active_water_sub(l, x - 1, y, z, block->type);
	physics_active_water_sub(l, x + 1, y, z, block->type);
	physics_active_water_sub(l, x, y, z - 1, block->type);
	physics_active_water_sub(l, x, y, z + 1, block->type);
}

void physics_gravity(struct level_t *l, unsigned index, const struct block_t *block)
{
	if (l->blocks[index].fixed) return;

	int16_t x, y, z;
	level_get_xyz(l, index, &x, &y, &z);

	if (y == 0) return;

	unsigned index2 = level_get_index(l, x, y - 1, z);
	if (l->blocks[index2].fixed) return;

	switch (l->blocks[index2].type)
	{
		case AIR:
		case WATER:
		case LAVA:
		case SHRUB:
		case YELLOWFLOWER:
		case REDFLOWER:
		case MUSHROOM:
		case REDMUSHROOM:
			level_addupdate(l, index2, block->type, 0);
			level_addupdate(l, index, AIR, 0);
			break;
	}
}

void delete_sponge(struct level_t *l, unsigned index, const struct block_t *block)
{
	int16_t x, y, z, dx, dy, dz, ax, ay, az;
	level_get_xyz(l, index, &x, &y, &z);

	for (dx = -2; dx <= 2; dx++)
	{
		for (dy = -2; dy <= 2; dy++)
		{
			for (dz = -2; dz <= 2; dz++)
			{
				if (dx == 0 && dy == 0 && dz == 0) continue;

				ax = x + dx;
				ay = y + dy;
				az = z + dz;

				if (ax < 0 || ay < 0 || az < 0 || ax >= l->x || ay >= l->y || az >= l->z) continue;

				unsigned index2 = level_get_index(l, ax, ay, az);
				if (l->blocks[index2].type == AIR)
				{
					level_addupdate(l, index2, AIR, 0);
				}
			}
		}
	}
}

void physics_sponge(struct level_t *l, unsigned index, const struct block_t *block)
{
	int16_t x, y, z, dx, dy, dz, ax, ay, az;
	level_get_xyz(l, index, &x, &y, &z);

	for (dx = -2; dx <= 2; dx++)
	{
		for (dy = -2; dy <= 2; dy++)
		{
			for (dz = -2; dz <= 2; dz++)
			{
				if (dx == 0 && dy == 0 && dz == 0) continue;

				ax = x + dx;
				ay = y + dy;
				az = z + dz;

				if (ax < 0 || ay < 0 || az < 0 || ax >= l->x || ay >= l->y || az >= l->z) continue;

				unsigned index2 = level_get_index(l, ax, ay, az);
				switch (l->blocks[index2].type)
				{
					case WATER:
					case WATERSTILL:
						level_addupdate(l, index2, AIR, 1);
						break;
				}
			}
		}
	}
}

enum blocktype_t convert_active_sponge(struct level_t *level, unsigned index, const struct block_t *block)
{
	return AIR;
}

void physics_active_sponge_sub(struct level_t *l, int16_t x, int16_t y, int16_t z, enum blocktype_t type)
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

void physics_active_sponge(struct level_t *l, unsigned index, const struct block_t *block)
{
	int16_t x, y, z;
	level_get_xyz(l, index, &x, &y, &z);

	if (block->data == 0)
	{
		physics_active_sponge_sub(l, x - 1, y, z, block->type);
		physics_active_sponge_sub(l, x + 1, y, z, block->type);
		physics_active_sponge_sub(l, x, y - 1, z, block->type);
		physics_active_sponge_sub(l, x, y, z - 1, block->type);
		physics_active_sponge_sub(l, x, y, z + 1, block->type);

		/* Leave a 'trail' of 10 blocks of active sponge to prevent
		 * active water/lava coming back so soon. */
		level_addupdate(l, index, BLOCK_INVALID, 10);
	}
	else
	{
		if (block->data - 1 == 0)
		{
			level_addupdate(l, index, AIR, 0);
		}
		else
		{
			level_addupdate(l, index, BLOCK_INVALID, block->data - 1);
		}
	}
}

enum blocktype_t convert_single_stair(struct level_t *level, unsigned index, const struct block_t *block)
{
	return STAIRCASESTEP;
}

bool trigger_stair(struct level_t *l, unsigned index, const struct block_t *block)
{
	level_addupdate(l, index, blocktype_get_by_name("single_stair"), 0);

	return false;
}

void physics_stair(struct level_t *l, unsigned index, const struct block_t *block)
{
	int16_t x, y, z;
	level_get_xyz(l, index, &x, &y, &z);

	if (y > 0)
	{
		unsigned index2 = level_get_index(l, x, y - 1, z);

		const struct block_t *below = &l->blocks[index2];
		if (below->type == STAIRCASESTEP || below->type == blocktype_get_by_name("single_stair"))
		{
			level_addupdate(l, index, AIR, 0);
			level_addupdate(l, index2, STAIRCASEFULL, 0);
		}
	}

	level_addupdate(l, index, blocktype_get_by_name("single_stair"), 0);
}

enum blocktype_t convert_door(struct level_t *level, unsigned index, const struct block_t *block)
{
	return block->data ? AIR : TRUNK;
}

enum blocktype_t convert_door_obsidian(struct level_t *level, unsigned index, const struct block_t *block)
{
	return block->data ? AIR : OBSIDIAN;
}

enum blocktype_t convert_door_glass(struct level_t *level, unsigned index, const struct block_t *block)
{
	return block->data ? AIR : GLASS;
}

enum blocktype_t convert_door_stair(struct level_t *level, unsigned index, const struct block_t *block)
{
	return block->data ? AIR : STAIRCASESTEP;
}

void trigger_door_sub(struct level_t *l, int16_t x, int16_t y, int16_t z, enum blocktype_t type)
{
	// Test x,y,z are valid!
	if (x < 0 || y < 0 || z < 0 || x >= l->x || y >= l->y || z >= l->z) return;

	unsigned index = level_get_index(l, x, y, z);
	if (l->blocks[index].type == type && l->blocks[index].data == 0)
	{
		level_addupdate(l, index, -1, 20);
	}
}

bool trigger_door(struct level_t *l, unsigned index, const struct block_t *block)
{
	if (block->data == 0)
	{
		level_addupdate(l, index, -1, 20);
	}

	return false;
}

void physics_door(struct level_t *l, unsigned index, const struct block_t *block)
{
	if (block->data > 0)
	{
		if (block->data == 20)
		{
			int16_t x, y, z;
			level_get_xyz(l, index, &x, &y, &z);

			trigger_door_sub(l, x - 1, y, z, block->type);
			trigger_door_sub(l, x + 1, y, z, block->type);
			trigger_door_sub(l, x, y - 1, z, block->type);
			trigger_door_sub(l, x, y + 1, z, block->type);
			trigger_door_sub(l, x, y, z - 1, block->type);
			trigger_door_sub(l, x, y, z + 1, block->type);
		}

		level_addupdate(l, index, -1, block->data - 1);

		//LOG("Door physics: %d\n", block->data);
	}
}

enum blocktype_t convert_parquet(struct level_t *level, unsigned index, const struct block_t *block)
{
	int16_t x, y, z;
	level_get_xyz(level, index, &x, &y, &z);
	return (x + y + z) % 2 ? TRUNK : WOOD;
}

void blocktype_init()
{
	register_blocktype(AIR, "air", RANK_GUEST, NULL, NULL, NULL, &physics_air, true);
	register_blocktype(ROCK, "stone", RANK_GUEST, NULL, NULL, NULL, NULL, false);
	register_blocktype(GRASS, "grass", RANK_GUEST, NULL, NULL, NULL, NULL, false);
	register_blocktype(DIRT, "dirt", RANK_GUEST, NULL, NULL, NULL, NULL, false);
	register_blocktype(STONE, "cobblestone", RANK_GUEST, NULL, NULL, NULL, NULL, false);
	register_blocktype(WOOD, "wood", RANK_GUEST, NULL, NULL, NULL, NULL, false);
	register_blocktype(SHRUB, "plant", RANK_GUEST, NULL, NULL, NULL, NULL, true);
	register_blocktype(ADMINIUM, "adminium", RANK_OP, NULL, NULL, NULL, NULL, false);
	register_blocktype(WATER, "active_water", RANK_ADV_BUILDER, NULL, NULL, NULL, &physics_active_water, false);
	register_blocktype(WATERSTILL, "water", RANK_BUILDER, NULL, NULL, NULL, NULL, false);
	register_blocktype(LAVA, "active_lava", RANK_ADV_BUILDER, NULL, NULL, NULL, NULL, false);
	register_blocktype(LAVASTILL, "lava", RANK_BUILDER, NULL, NULL, NULL, NULL, false);
	register_blocktype(SAND, "sand", RANK_GUEST, NULL, NULL, NULL, &physics_gravity, false);
	register_blocktype(GRAVEL, "gravel", RANK_GUEST, NULL, NULL, NULL, &physics_gravity, false);
	register_blocktype(GOLDROCK, "gold_ore", RANK_GUEST, NULL, NULL, NULL, NULL, false);
	register_blocktype(IRONROCK, "iron_ore", RANK_GUEST, NULL, NULL, NULL, NULL, false);
	register_blocktype(COAL, "coal", RANK_GUEST, NULL, NULL, NULL, NULL, false);
	register_blocktype(TRUNK, "tree", RANK_GUEST, NULL, NULL, NULL, NULL, false);
	register_blocktype(LEAF, "leaves", RANK_GUEST, NULL, NULL, NULL, NULL, true);
	register_blocktype(SPONGE, "sponge", RANK_GUEST, NULL, NULL, &delete_sponge, &physics_sponge, false);
	register_blocktype(GLASS, "glass", RANK_GUEST, NULL, NULL, NULL, NULL, true);
	register_blocktype(RED, "red", RANK_GUEST, NULL, NULL, NULL, NULL, false);
	register_blocktype(ORANGE, "orange", RANK_GUEST, NULL, NULL, NULL, NULL, false);
	register_blocktype(YELLOW, "yellow", RANK_GUEST, NULL, NULL, NULL, NULL, false);
	register_blocktype(LIGHTGREEN, "greenyellow", RANK_GUEST, NULL, NULL, NULL, NULL, false);
	register_blocktype(GREEN, "green", RANK_GUEST, NULL, NULL, NULL, NULL, false);
	register_blocktype(AQUAGREEN, "springgreen", RANK_GUEST, NULL, NULL, NULL, NULL, false);
	register_blocktype(CYAN, "cyan", RANK_GUEST, NULL, NULL, NULL, NULL, false);
	register_blocktype(LIGHTBLUE, "blue", RANK_GUEST, NULL, NULL, NULL, NULL, false);
	register_blocktype(BLUE, "blueviolet", RANK_GUEST, NULL, NULL, NULL, NULL, false);
	register_blocktype(PURPLE, "indigo", RANK_GUEST, NULL, NULL, NULL, NULL, false);
	register_blocktype(LIGHTPURPLE, "purple", RANK_GUEST, NULL, NULL, NULL, NULL, false);
	register_blocktype(PINK, "magenta", RANK_GUEST, NULL, NULL, NULL, NULL, false);
	register_blocktype(DARKPINK, "pink", RANK_GUEST, NULL, NULL, NULL, NULL, false);
	register_blocktype(DARKGREY, "black", RANK_GUEST, NULL, NULL, NULL, NULL, false);
	register_blocktype(LIGHTGREY, "grey", RANK_GUEST, NULL, NULL, NULL, NULL, false);
	register_blocktype(WHITE, "white", RANK_GUEST, NULL, NULL, NULL, NULL, false);
	register_blocktype(YELLOWFLOWER, "yellow_flower", RANK_GUEST, NULL, NULL, NULL, NULL, true);
	register_blocktype(REDFLOWER, "red_flower", RANK_GUEST, NULL, NULL, NULL, NULL, true);
	register_blocktype(MUSHROOM, "brown_shroom", RANK_GUEST, NULL, NULL, NULL, NULL, true);
	register_blocktype(REDMUSHROOM, "red_shroom", RANK_GUEST, NULL, NULL, NULL, NULL, true);
	register_blocktype(GOLDSOLID, "gold", RANK_GUEST, NULL, NULL, NULL, NULL, false);
	register_blocktype(IRON, "iron", RANK_GUEST, NULL, NULL, NULL, NULL, false);
	register_blocktype(STAIRCASEFULL, "double_stair", RANK_GUEST, NULL, &trigger_stair, NULL, NULL, false);
	register_blocktype(STAIRCASESTEP, "stair", RANK_GUEST, NULL, NULL, NULL, &physics_stair, false);
	register_blocktype(BRICK, "brick", RANK_GUEST, NULL, NULL, NULL, NULL, false);
	register_blocktype(TNT, "tnt", RANK_GUEST, NULL, NULL, NULL, NULL, false);
	register_blocktype(BOOKCASE, "bookcase", RANK_GUEST, NULL, NULL, NULL, NULL, false);
	register_blocktype(STONEVINE, "mossy_cobblestone", RANK_GUEST, NULL, NULL, NULL, NULL, false);
	register_blocktype(OBSIDIAN, "obsidian", RANK_GUEST, NULL, NULL, NULL, NULL, false);

	register_blocktype(BLOCK_INVALID, "single_stair", RANK_GUEST, &convert_single_stair, NULL, NULL, NULL, false);
	register_blocktype(BLOCK_INVALID, "door", RANK_BUILDER, &convert_door, &trigger_door, NULL, &physics_door, false);
	register_blocktype(BLOCK_INVALID, "door_obsidian", RANK_BUILDER, &convert_door_obsidian, &trigger_door, NULL, &physics_door, false);
	register_blocktype(BLOCK_INVALID, "door_glass", RANK_BUILDER, &convert_door_glass, &trigger_door, NULL, &physics_door, true);
	register_blocktype(BLOCK_INVALID, "door_step", RANK_BUILDER, &convert_door_stair, &trigger_door, NULL, &physics_door, false);
	register_blocktype(BLOCK_INVALID, "parquet", RANK_GUEST, &convert_parquet, NULL, NULL, NULL, false);

	module_load("wireworld.so");

	register_blocktype(BLOCK_INVALID, "active_sponge", RANK_ADV_BUILDER, &convert_active_sponge, NULL, NULL, &physics_active_sponge, false);

	module_load("tnt.so");
	module_load("spleef.so");

}

void blocktype_deinit()
{
	unsigned i;
	for (i = 0; i < s_blocks.used; i++)
	{
		free(s_blocks.items[i].name);
	}

	blocktype_desc_list_free(&s_blocks);
}

const char *blocktype_get_name(enum blocktype_t type)
{
	return s_blocks.items[type].name;
}

enum blocktype_t blocktype_get_by_name(const char *name)
{
	unsigned i;
	for (i = 0; i < s_blocks.used; i++)
	{
		if (s_blocks.items[i].name == NULL) continue;
		if (strcasecmp(s_blocks.items[i].name, name) == 0) return i;
	}

	return BLOCK_INVALID;
}

struct block_t block_convert_from_mcs(uint8_t type)
{
	static struct block_t b = { false, false, 0, false, 0, 0 };

	b.fixed = false;
	switch (type)
	{
		case 100: /* OP_GLASS */ b.type = GLASS; b.fixed = true; break;
		case 101: /* OPSIDIAN */ b.type = OBSIDIAN; b.fixed = true; break;
		case 102: /* OP_BRICK */ b.type = BRICK; b.fixed = true; break;
		case 103: /* OP_STONE */ b.type = ROCK; b.fixed = true; break;
		case 104: /* OP_COBBLESTONE */ b.type = STONE; b.fixed = true; break;
		case 105: /* OP_AIR */ b.type = AIR; b.fixed = true; break;
		case 106: /* OP_WATER */ b.type = WATER; b.fixed = true; break;

		case 110: /* WOOD_FLOAT */ b.type = WOOD; break;
		case 111: /* DOOR */ b.type = blocktype_get_by_name("door"); break;
		case 112: /* LAVA_FAST */ b.type = LAVA; break;
		case 113: /* DOOR2 */ b.type = blocktype_get_by_name("door_obsidian"); break;
		case 114: /* DOOR3 */ b.type = blocktype_get_by_name("door_glass"); break;

		default: b.type = (type < BLOCK_END) ? type : AIR; break;
	}
	return b;
}

bool blocktype_has_physics(enum blocktype_t type)
{
	return s_blocks.items[type].physics_func != NULL;
}

enum rank_t blocktype_min_rank(enum blocktype_t type)
{
	return s_blocks.items[type].min_rank;
}

void blocktype_list(struct client_t *c)
{
	char buf[64];
	char *bufp = buf;
	unsigned i;

	memset(buf, 0, sizeof buf);

	for (i = 0; i < s_blocks.used; i++)
	{
		const struct blocktype_desc_t *b = &s_blocks.items[i];
		if (b->name == NULL) continue;

		char buf2[64];
		snprintf(buf2, sizeof buf2, "%s%s%s%s", !b->loaded ? TAG_YELLOW : "", b->name, !b->loaded ? TAG_WHITE : "", (i < s_blocks.used - 1) ? ", " : "");

		size_t len = strlen(buf2);
		if (len >= sizeof buf - (bufp - buf))
		{
			client_notify(c, buf);
			memset(buf, 0, sizeof buf);
			bufp = buf;
		}

		strcpy(bufp, buf2);
		bufp += len;
	}

	client_notify(c, buf);
}
