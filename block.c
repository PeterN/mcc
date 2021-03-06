#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "mcc.h"
#include "block.h"
#include "level.h"
#include "client.h"
#include "colour.h"
#include "player.h"

static struct blocktype_desc_list_t s_blocks;

void preregister_blocktype(const char *name)
{
	struct blocktype_desc_t desc;
	memset(&desc, 0, sizeof desc);

	desc.name = strdup(name);

	blocktype_desc_list_add(&s_blocks, desc);
}

int register_blocktype(enum blocktype_t type, const char *name, enum rank_t min_rank, convert_func_t convert_func, trigger_func_t trigger_func, delete_func_t delete_func, physics_func_t physics_func, bool clear, bool passable, bool swim)
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
			LOG("Allocating new block type %d\n", type);
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
	descp->clear = clear;
	descp->passable = passable;
	descp->swim = swim;
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
	if (type == BLOCK_INVALID || type >= s_blocks.used) return;

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
	if (block->type >= s_blocks.used) return RED;

	const struct blocktype_desc_t *btd = &s_blocks.items[block->type];
	if (btd->convert_func != NULL)
	{
		return btd->convert_func(level, index, block);
	}

	/* Non-standard block that no longer exists? Show as red... */
	if (block->type >= BLOCK_END) return RED;

	return block->type;
}

int trigger(struct level_t *l, unsigned index, const struct block_t *block, struct client_t *c, enum blocktype_t heldblock)
{
	if (block->type >= s_blocks.used) return TRIG_NONE;

	const struct blocktype_desc_t *btd = &s_blocks.items[block->type];
	if (btd->trigger_func != NULL)
	{
		return btd->trigger_func(l, index, block, c, heldblock);
	}
	return TRIG_NONE;
}

void delete(struct level_t *l, unsigned index, const struct block_t *block)
{
	if (block->type >= s_blocks.used) return;

	const struct blocktype_desc_t *btd = &s_blocks.items[block->type];
	if (btd->delete_func != NULL)
	{
		btd->delete_func(l, index, block);
	}
}

void physics(struct level_t *level, unsigned index, const struct block_t *block)
{
	if (block->type >= s_blocks.used) return;

	const struct blocktype_desc_t *btd = &s_blocks.items[block->type];

	level->blocks[index].physics = false;
	if (btd->physics_func != NULL)
	{
		btd->physics_func(level, index, block);
	}
}

bool light_check(struct level_t *level, unsigned index)
{
	int16_t x, y, z;
	if (!level_get_xyz(level, index, &x, &y, &z)) return true;

	y++;
	for (; y < level->y; y++)
	{
		enum blocktype_t type = level->blocks[level_get_index(level, x, y, z)].type;
		if (type >= s_blocks.used) return false;
		const struct blocktype_desc_t *btd = &s_blocks.items[type];
		if (!btd->clear) return false;
	}

	return true;
}

void physics_air_sub(struct level_t *l, unsigned index2, int16_t x, int16_t y, int16_t z, bool gravity, int data)
{
	if (!level_valid_xyz(l, x, y, z)) return;

	unsigned index = level_get_index(l, x, y, z);
	if (l->blocks[index].fixed) return;

	enum blocktype_t type = l->blocks[index].type;
	switch (type)
	{
		default: return;
		case WATER:
		case LAVA:
			/* Reactivate water */
			if (data == 0) level_addupdate(l, index, type, 0);
			return;

		case SAND:
		case GRAVEL:
			if (gravity)
			{
				level_addupdate(l, index, type, 0);
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

void delete_air(struct level_t *l, unsigned index, const struct block_t *block)
{

	int16_t x, y, z;
	level_get_xyz(l, index, &x, &y, &z);

	if (y > 0)
	{
		/* "Fall" down to the surface */
		unsigned index2;
//		do
		{
			y--;
			index2 = level_get_index(l, x, y, z);
		}
//		while (y > 0 && s_blocks.items[l->blocks[index2].type].clear);
		if (l->blocks[index2].fixed) return;

		switch (l->blocks[index2].type)
		{
			case GRASS:
//				if (l->blocks[index2].data == 1)
					level_addupdate(l, index2, GRASS, 0);
				break;

//			case DIRT:
				// We're not deleted yet, so just add update
//				if (l->blocks[index2].data == 91)
//					level_addupdate(l, index2, BLOCK_INVALID, 0);
//				break;
		}
	}
	
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
				level_addupdate(l, index, AIR, 1);
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

void physics_grass(struct level_t *l, unsigned index, const struct block_t *block)
{
	if (block->data > 0)
	{
		if (!light_check(l, index))
		{
			level_addupdate(l, index, DIRT, 1);
		}
	}
	else
	{
		level_addupdate(l, index, GRASS, block->data + 1);
	}
}

void physics_dirt(struct level_t *l, unsigned index, const struct block_t *block)
{
	if (block->data > 0)
	{
		if (light_check(l, index))
		{
			level_addupdate(l, index, GRASS, 1);
		}
	}
	else
	{
		level_addupdate(l, index, DIRT, block->data + 1);
	}
}

static enum blocktype_t convert_water(struct level_t *level, unsigned index, const struct block_t *block)
{
	return WATERSTILL;
}

void physics_active_water_sub(struct level_t *l, int16_t x, int16_t y, int16_t z, enum blocktype_t type, enum blocktype_t clash, enum blocktype_t convert)
{
	if (!level_valid_xyz(l, x, y, z)) return;

	unsigned index = level_get_index(l, x, y, z);
	if (!l->blocks[index].fixed)
	{
		if (l->blocks[index].type == AIR)
		{
			if (l->blocks[index].data == 0 && !sponge_test(l, x, y, z)) level_addupdate(l, index, type, 0);
		}
		else if (l->blocks[index].type == clash)
		{
			level_addupdate(l, index, convert, 0);
		}
	}
}

void physics_active_water(struct level_t *l, unsigned index, const struct block_t *block)
{
	int16_t x, y, z;
	level_get_xyz(l, index, &x, &y, &z);

	physics_active_water_sub(l, x, y - 1, z, block->type, LAVA, OBSIDIAN);
	physics_active_water_sub(l, x - 1, y, z, block->type, LAVA, OBSIDIAN);
	physics_active_water_sub(l, x + 1, y, z, block->type, LAVA, OBSIDIAN);
	physics_active_water_sub(l, x, y, z - 1, block->type, LAVA, OBSIDIAN);
	physics_active_water_sub(l, x, y, z + 1, block->type, LAVA, OBSIDIAN);
}

static enum blocktype_t convert_lava(struct level_t *level, unsigned index, const struct block_t *block)
{
	return LAVASTILL;
}

void physics_active_lava(struct level_t *l, unsigned index, const struct block_t *block)
{
	if (block->data < 20)
	{
		level_addupdate(l, index, LAVA, block->data + 1);
	}
	else
	{
		int16_t x, y, z;
		level_get_xyz(l, index, &x, &y, &z);

		physics_active_water_sub(l, x, y - 1, z, block->type, WATER, STONE);
		physics_active_water_sub(l, x - 1, y, z, block->type, WATER, STONE);
		physics_active_water_sub(l, x + 1, y, z, block->type, WATER, STONE);
		physics_active_water_sub(l, x, y, z - 1, block->type, WATER, STONE);
		physics_active_water_sub(l, x, y, z + 1, block->type, WATER, STONE);
	}
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

#if 0
void physics_active_sponge_sub(struct level_t *l, int16_t x, int16_t y, int16_t z, enum blocktype_t type)
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
		level_addupdate(l, index, block->type, 10);
	}
	else
	{
		if (block->data - 1 == 0)
		{
			level_addupdate(l, index, AIR, 0);
		}
		else
		{
			level_addupdate(l, index, block->type, block->data - 1);
		}
	}
}
#endif

enum blocktype_t convert_single_stair(struct level_t *level, unsigned index, const struct block_t *block)
{
	return STAIRCASESTEP;
}

int trigger_double_stair(struct level_t *l, unsigned index, const struct block_t *block, struct client_t *c, enum blocktype_t heldblock)
{
	/* Only the block owner can remove a double step */
	if (c->player->globalid != block->owner) return TRIG_NONE;

	level_addupdate(l, index, STAIRCASESTEP, 0);

	return TRIG_EMPTY;
}

void physics_stair(struct level_t *l, unsigned index, const struct block_t *block)
{
	int16_t x, y, z;
	level_get_xyz(l, index, &x, &y, &z);

	if (y > 0)
	{
		unsigned index2 = level_get_index(l, x, y - 1, z);

		const struct block_t *below = &l->blocks[index2];
		if (below->type == STAIRCASESTEP)
		{
			level_addupdate(l, index, AIR, 0);
			level_addupdate(l, index2, STAIRCASEFULL, 0);
		}
	}
}

void blocktype_init(void)
{
	FILE *f = fopen("blocks.txt", "r");
	if (f != NULL)
	{
		while (!feof(f))
		{
			char buf[1024];
			if (fgets(buf, sizeof buf, f) > 0)
			{
				char *eol = strchr(buf, '\n');
				if (eol != NULL) *eol = '\0';

				preregister_blocktype(buf);
			}
		}

		fclose(f);
	}

	register_blocktype(AIR, "air", RANK_GUEST, NULL, NULL, &delete_air, &physics_air, true, true, false);
	register_blocktype(ROCK, "stone", RANK_GUEST, NULL, NULL, NULL, NULL, false, false, false);
	register_blocktype(GRASS, "grass", RANK_BUILDER, NULL, NULL, NULL, &physics_grass, false, false, false);
	register_blocktype(DIRT, "dirt", RANK_GUEST, NULL, NULL, NULL, &physics_dirt, false, false, false);
	register_blocktype(STONE, "cobblestone", RANK_GUEST, NULL, NULL, NULL, NULL, false, false, false);
	register_blocktype(WOOD, "wood", RANK_GUEST, NULL, NULL, NULL, NULL, false, false, false);
	register_blocktype(SHRUB, "plant", RANK_GUEST, NULL, NULL, NULL, NULL, true, true, false);
	register_blocktype(ADMINIUM, "adminium", RANK_OP, NULL, NULL, NULL, NULL, false, false, false);
	register_blocktype(WATER, "active_water", RANK_ADV_BUILDER, &convert_water, NULL, NULL, &physics_active_water, false, true, true);
	register_blocktype(WATERSTILL, "water", RANK_BUILDER, NULL, NULL, NULL, NULL, false, true, true);
	register_blocktype(LAVA, "active_lava", RANK_ADV_BUILDER, &convert_lava, NULL, NULL, &physics_active_lava, false, true, true);
	register_blocktype(LAVASTILL, "lava", RANK_BUILDER, NULL, NULL, NULL, NULL, false, true, true);
	register_blocktype(SAND, "sand", RANK_GUEST, NULL, NULL, NULL, &physics_gravity, false, false, false);
	register_blocktype(GRAVEL, "gravel", RANK_GUEST, NULL, NULL, NULL, &physics_gravity, false, false, false);
	register_blocktype(GOLDROCK, "gold_ore", RANK_GUEST, NULL, NULL, NULL, NULL, false, false, false);
	register_blocktype(IRONROCK, "iron_ore", RANK_GUEST, NULL, NULL, NULL, NULL, false, false, false);
	register_blocktype(COAL, "coal", RANK_GUEST, NULL, NULL, NULL, NULL, false, false, false);
	register_blocktype(TRUNK, "tree", RANK_GUEST, NULL, NULL, NULL, NULL, false, false, false);
	register_blocktype(LEAF, "leaves", RANK_GUEST, NULL, NULL, NULL, NULL, true, false, false);
	register_blocktype(SPONGE, "sponge", RANK_BUILDER, NULL, NULL, &delete_sponge, &physics_sponge, false, false, false);
	register_blocktype(GLASS, "glass", RANK_GUEST, NULL, NULL, NULL, NULL, true, false, false);
	register_blocktype(RED, "red", RANK_GUEST, NULL, NULL, NULL, NULL, false, false, false);
	register_blocktype(ORANGE, "orange", RANK_GUEST, NULL, NULL, NULL, NULL, false, false, false);
	register_blocktype(YELLOW, "yellow", RANK_GUEST, NULL, NULL, NULL, NULL, false, false, false);
	register_blocktype(LIGHTGREEN, "greenyellow", RANK_GUEST, NULL, NULL, NULL, NULL, false, false, false);
	register_blocktype(GREEN, "green", RANK_GUEST, NULL, NULL, NULL, NULL, false, false, false);
	register_blocktype(AQUAGREEN, "springgreen", RANK_GUEST, NULL, NULL, NULL, NULL, false, false, false);
	register_blocktype(CYAN, "cyan", RANK_GUEST, NULL, NULL, NULL, NULL, false, false, false);
	register_blocktype(LIGHTBLUE, "blue", RANK_GUEST, NULL, NULL, NULL, NULL, false, false, false);
	register_blocktype(BLUE, "blueviolet", RANK_GUEST, NULL, NULL, NULL, NULL, false, false, false);
	register_blocktype(PURPLE, "indigo", RANK_GUEST, NULL, NULL, NULL, NULL, false, false, false);
	register_blocktype(LIGHTPURPLE, "purple", RANK_GUEST, NULL, NULL, NULL, NULL, false, false, false);
	register_blocktype(PINK, "magenta", RANK_GUEST, NULL, NULL, NULL, NULL, false, false, false);
	register_blocktype(DARKPINK, "pink", RANK_GUEST, NULL, NULL, NULL, NULL, false, false, false);
	register_blocktype(DARKGREY, "black", RANK_GUEST, NULL, NULL, NULL, NULL, false, false, false);
	register_blocktype(LIGHTGREY, "grey", RANK_GUEST, NULL, NULL, NULL, NULL, false, false, false);
	register_blocktype(WHITE, "white", RANK_GUEST, NULL, NULL, NULL, NULL, false, false, false);
	register_blocktype(YELLOWFLOWER, "yellow_flower", RANK_GUEST, NULL, NULL, NULL, NULL, true, true, false);
	register_blocktype(REDFLOWER, "red_flower", RANK_GUEST, NULL, NULL, NULL, NULL, true, true, false);
	register_blocktype(MUSHROOM, "brown_shroom", RANK_GUEST, NULL, NULL, NULL, NULL, true, true, false);
	register_blocktype(REDMUSHROOM, "red_shroom", RANK_GUEST, NULL, NULL, NULL, NULL, true, true, false);
	register_blocktype(GOLDSOLID, "gold", RANK_GUEST, NULL, NULL, NULL, NULL, false, false, false);
	register_blocktype(IRON, "iron", RANK_GUEST, NULL, NULL, NULL, NULL, false, false, false);
	register_blocktype(STAIRCASEFULL, "double_stair", RANK_GUEST, NULL, &trigger_double_stair, NULL, NULL, false, false, false);
	register_blocktype(STAIRCASESTEP, "stair", RANK_GUEST, NULL, NULL, NULL, &physics_stair, false, false, false);
	register_blocktype(BRICK, "brick", RANK_GUEST, NULL, NULL, NULL, NULL, false, false, false);
	register_blocktype(TNT, "tnt", RANK_GUEST, NULL, NULL, NULL, NULL, false, false, false);
	register_blocktype(BOOKCASE, "bookcase", RANK_GUEST, NULL, NULL, NULL, NULL, false, false, false);
	register_blocktype(STONEVINE, "mossy_cobblestone", RANK_GUEST, NULL, NULL, NULL, NULL, false, false, false);
	register_blocktype(OBSIDIAN, "obsidian", RANK_GUEST, NULL, NULL, NULL, NULL, false, false, false);

	register_blocktype(BLOCK_INVALID, "single_stair", RANK_GUEST, &convert_single_stair, NULL, NULL, NULL, false, false, false);

	register_blocktype(BLOCK_INVALID, "active_sponge", RANK_ADMIN, &convert_active_sponge, NULL, NULL, NULL, false, false, false);
}

void blocktype_deinit(void)
{
	unsigned i;
	FILE *f = fopen("blocks.txt", "w");
	if (f == NULL)
	{
		LOG("Unable to write blocks list\n");
	}

	for (i = 0; i < s_blocks.used; i++)
	{
		if (f != NULL) fprintf(f, "%s\n", s_blocks.items[i].name);
		free(s_blocks.items[i].name);
	}

	if (f != NULL) fclose(f);

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

bool blocktype_passable(enum blocktype_t type)
{
	return s_blocks.items[type].passable;
}

bool blocktype_swim(enum blocktype_t type)
{
	return s_blocks.items[type].swim;
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
