#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "mcc.h"
#include "block.h"
#include "level.h"

static struct blocktype_desc_list_t s_blocks;

int register_blocktype(enum blocktype_t type, const char *name, convert_func_t convert_func, trigger_func_t trigger_func, physics_func_t physics_func)
{
    struct blocktype_desc_t desc;

    if (type == -1)
    {
        desc.name = name;
        desc.convert_func = convert_func;
        desc.trigger_func = trigger_func;
        desc.physics_func = physics_func;
        blocktype_desc_list_add(&s_blocks, desc);

        //LOG("Registered %s as %lu\n", desc.name, s_blocks.used - 1);

        return s_blocks.used - 1;
    }
    else
    {
        memset(&desc, 0, sizeof desc);

        /* Add blank entries until we reach the block we want */
        while (s_blocks.used <= type)
        {
            blocktype_desc_list_add(&s_blocks, desc);
        }

        struct blocktype_desc_t *descp = &s_blocks.items[type];
        descp->name = name;
        descp->convert_func = convert_func;
        descp->trigger_func = trigger_func;
        descp->physics_func = physics_func;

        //LOG("Registered %s as %d\n", descp->name, type);

        return type;
    }
}

enum blocktype_t convert(struct level_t *level, unsigned index, const struct block_t *block)
{
    const struct blocktype_desc_t *btd = &s_blocks.items[block->type];
    if (btd->convert_func != NULL)
    {
        return btd->convert_func(level, index, block);
    }
    return block->type;
}

bool trigger(struct level_t *l, unsigned index, const struct block_t *block)
{
    const struct blocktype_desc_t *btd = &s_blocks.items[block->type];
    if (btd->trigger_func != NULL)
    {
        //LOG("Calling trigger\n");
        return btd->trigger_func(l, index, block);
    }
    //LOG("No trigger\n");
    return false;
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

void physics_air_sub(struct level_t *l, unsigned index2, int16_t x, int16_t y, int16_t z, bool gravity)
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
			level_addupdate(l, index, -1, 0);
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

void physics_air(struct level_t *l, unsigned index, const struct block_t *block)
{
    int16_t x, y, z;
    level_get_xyz(l, index, &x, &y, &z);

    if (l->blocks[index].fixed) return;

    physics_air_sub(l, index, x, y + 1, z, true);
    physics_air_sub(l, index, x - 1, y, z, false);
    physics_air_sub(l, index, x + 1, y, z, false);
    physics_air_sub(l, index, x, y, z - 1, false);
    physics_air_sub(l, index, x, y, z + 1, false);
}

void physics_active_water_sub(struct level_t *l, int16_t x, int16_t y, int16_t z, enum blocktype_t type)
{
    // Test x,y,z are valid!
    if (x < 0 || y < 0 || z < 0 || x >= l->x || y >= l->y || z >= l->z) return;

    unsigned index = level_get_index(l, x, y, z);
    if (l->blocks[index].type == AIR && !l->blocks[index].fixed)
    {
        level_addupdate(l, index, type, 0);
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

    //if (block->data == 0)
    //{
        //block->data = 1;

        physics_active_sponge_sub(l, x - 1, y, z, block->type);
        physics_active_sponge_sub(l, x + 1, y, z, block->type);
        physics_active_sponge_sub(l, x, y - 1, z, block->type);
        physics_active_sponge_sub(l, x, y, z - 1, block->type);
        physics_active_sponge_sub(l, x, y, z + 1, block->type);
        level_addupdate(l, index, AIR, 0);
    //}
    //else
    //{
     //   block->data--;
     //   if (block->data == 0)
     //   {

        //}
    //}
}

enum blocktype_t convert_single_stair(struct level_t *level, unsigned index, const struct block_t *block)
{
    return STAIRCASESTEP;
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

/*
        int16_t x, y, z;
        level_get_xyz(l, index, &x, &y, &z);

        trigger_door_sub(l, x - 1, y, z, block->type);
        trigger_door_sub(l, x + 1, y, z, block->type);
        trigger_door_sub(l, x, y - 1, z, block->type);
        trigger_door_sub(l, x, y + 1, z, block->type);
        trigger_door_sub(l, x, y, z - 1, block->type);
        trigger_door_sub(l, x, y, z + 1, block->type);*/
    }

    //LOG("Door trigger: %d\n", block->data);

    return true;
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

enum blocktype_t convert_wire(struct level_t *level, unsigned index, const struct block_t *block)
{
    switch (block->data)
    {
        default: return GOLDSOLID;
        case 1: return RED;
        case 2: return BLUE;
    }
}

bool trigger_wire(struct level_t *l, unsigned index, const struct block_t *block)
{
    if (block->data == 0)
    {
        level_addupdate(l, index, -1, 1);
    }

    return true;
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
        //LOG("Door physics: %d\n", block->data);
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
        //LOG("Door physics: %d\n", block->data);
    }
}

enum blocktype_t convert_active_tnt(struct level_t *level, unsigned index, const struct block_t *block)
{
    return TNT;
}

bool trigger_active_tnt(struct level_t *l, unsigned index, const struct block_t *block)
{
    level_addupdate(l, index, blocktype_get_by_name("explosion"), 0x305);

    return true;
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

    if (l->blocks[index].type == blocktype_get_by_name("active_tnt"))
    {
        magnitude = 0x305;
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
                    if (dx == x && dy == y && dz == z) continue;
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

    switch (rand() % 4)
    {
        case 0: return RED;
        case 1: return ORANGE;
        case 2: return YELLOW;
        default: return LIGHTGREY;
    }
}

bool trigger_fuse(struct level_t *l, unsigned index, const struct block_t *block)
{
    level_addupdate(l, index, -1, 10);

    return true;
}

void physics_fuse_sub(struct level_t *l, int16_t x, int16_t y, int16_t z, enum blocktype_t type, enum blocktype_t tnt)
{
    // Test x,y,z are valid!
    if (x < 0 || y < 0 || z < 0 || x >= l->x || y >= l->y || z >= l->z) return;

    unsigned index = level_get_index(l, x, y, z);
    if (l->blocks[index].type == type)
    {
        level_addupdate(l, index, type, 10);
    }
    else if (l->blocks[index].type == tnt)
    {
        level_addupdate(l, index, blocktype_get_by_name("explosion"), 0x305);
    }
}

void physics_fuse(struct level_t *l, unsigned index, const struct block_t *block)
{
    if (block->data == 0) return;
    if (block->data == 1)
    {
        enum blocktype_t tnt = blocktype_get_by_name("active_tnt");
        int16_t x, y, z, dx, dy, dz;
        level_get_xyz(l, index, &x, &y, &z);

        for (dx = -1; dx <= 1; dx++)
        {
            for (dy = -1; dy <= 1; dy++)
            {
                for (dz = -1; dz <= 1; dz++)
                {
                    if (dx == 0 && dy == 0 && dz == 0) continue;
                    physics_fuse_sub(l, x + dx, y + dy, z + dz, block->type, tnt);
                }
            }
        }

        level_addupdate(l, index, AIR, 0);
    }
    else
    {
        level_addupdate(l, index, -1, block->data - 1);
    }
}

//register_blocktype(-1, "active_tnt", &convert_active_tnt, &trigger_active_tnt, NULL);
//register_blocktype(-1, "explosion", &convert_explosion, NULL, &physics_explosion);

void blocktype_init()
{
    register_blocktype(AIR, "air", NULL, NULL, &physics_air);
    register_blocktype(ROCK, "stone", NULL, NULL, NULL);
    register_blocktype(GRASS, "grass", NULL, NULL, NULL);
    register_blocktype(DIRT, "dirt", NULL, NULL, NULL);
    register_blocktype(STONE, "cobblestone", NULL, NULL, NULL);
    register_blocktype(WOOD, "wood", NULL, NULL, NULL);
    register_blocktype(SHRUB, "plant", NULL, NULL, NULL);
    register_blocktype(ADMINIUM, "adminium", NULL, NULL, NULL);
    register_blocktype(WATER, "active_water", NULL, NULL, &physics_active_water);
    register_blocktype(WATERSTILL, "water", NULL, NULL, NULL);
    register_blocktype(LAVA, "active_lava", NULL, NULL, NULL);
    register_blocktype(LAVASTILL, "lava", NULL, NULL, NULL);
    register_blocktype(SAND, "sand", NULL, NULL, &physics_gravity);
    register_blocktype(GRAVEL, "gravel", NULL, NULL, &physics_gravity);
    register_blocktype(GOLDROCK, "gold_ore", NULL, NULL, NULL);
    register_blocktype(IRONROCK, "iron_ore", NULL, NULL, NULL);
    register_blocktype(COAL, "coal", NULL, NULL, NULL);
    register_blocktype(TRUNK, "tree", NULL, NULL, NULL);
    register_blocktype(LEAF, "leaves", NULL, NULL, NULL);
    register_blocktype(SPONGE, "sponge", NULL, NULL, NULL);
    register_blocktype(GLASS, "glass", NULL, NULL, NULL);
    register_blocktype(RED, "red", NULL, NULL, NULL);
    register_blocktype(ORANGE, "orange", NULL, NULL, NULL);
	register_blocktype(YELLOW, "yellow", NULL, NULL, NULL);
	register_blocktype(LIGHTGREEN, "greenyellow", NULL, NULL, NULL);
	register_blocktype(GREEN, "green", NULL, NULL, NULL);
	register_blocktype(AQUAGREEN, "springgreen", NULL, NULL, NULL);
	register_blocktype(CYAN, "cyan", NULL, NULL, NULL);
	register_blocktype(LIGHTBLUE, "blue", NULL, NULL, NULL);
	register_blocktype(BLUE, "blueviolet", NULL, NULL, NULL);
	register_blocktype(PURPLE, "indigo", NULL, NULL, NULL);
	register_blocktype(LIGHTPURPLE, "purple", NULL, NULL, NULL);
	register_blocktype(PINK, "magenta", NULL, NULL, NULL);
	register_blocktype(DARKPINK, "pink", NULL, NULL, NULL);
	register_blocktype(DARKGREY, "black", NULL, NULL, NULL);
	register_blocktype(LIGHTGREY, "grey", NULL, NULL, NULL);
	register_blocktype(WHITE, "white", NULL, NULL, NULL);
	register_blocktype(YELLOWFLOWER, "yellow_flower", NULL, NULL, NULL);
	register_blocktype(REDFLOWER, "red_flower", NULL, NULL, NULL);
	register_blocktype(MUSHROOM, "brown_shroom", NULL, NULL, NULL);
	register_blocktype(REDMUSHROOM, "red_shroom", NULL, NULL, NULL);
	register_blocktype(GOLDSOLID, "gold", NULL, NULL, NULL);
	register_blocktype(IRON, "iron", NULL, NULL, NULL);
	register_blocktype(STAIRCASEFULL, "double_stair", NULL, NULL, NULL);
	register_blocktype(STAIRCASESTEP, "stair", NULL, NULL, &physics_stair);
	register_blocktype(BRICK, "brick", NULL, NULL, NULL);
	register_blocktype(TNT, "tnt", NULL, NULL, NULL);
	register_blocktype(BOOKCASE, "bookcase", NULL, NULL, NULL);
	register_blocktype(STONEVINE, "mossy_cobblestone", NULL, NULL, NULL);
    register_blocktype(OBSIDIAN, "obsidian", NULL, NULL, NULL);

    register_blocktype(-1, "single_stair", &convert_single_stair, NULL, NULL);
    register_blocktype(-1, "door", &convert_door, &trigger_door, &physics_door);
    register_blocktype(-1, "door_obsidian", &convert_door_obsidian, &trigger_door, &physics_door);
    register_blocktype(-1, "door_glass", &convert_door_glass, &trigger_door, &physics_door);
    register_blocktype(-1, "door_step", &convert_door_stair, &trigger_door, &physics_door);
    register_blocktype(-1, "parquet", &convert_parquet, NULL, NULL);

    register_blocktype(-1, "wire", &convert_wire, &trigger_wire, &physics_wire);
    register_blocktype(-1, "wire3d", &convert_wire, &trigger_wire, &physics_wire3d);

    register_blocktype(-1, "active_sponge", &convert_active_sponge, NULL, &physics_active_sponge);

    register_blocktype(-1, "active_tnt", &convert_active_tnt, &trigger_active_tnt, NULL);
    register_blocktype(-1, "explosion", &convert_explosion, NULL, &physics_explosion);
    register_blocktype(-1, "fuse", &convert_fuse, &trigger_fuse, &physics_fuse);
}

/*static const char *s_op_blocks[] = {
	"op_glass",
	"opsidian",
	"op_brick",
	"op_stone",
	"op_cobblestone",
	"op_air",
	"op_water",
};*/

/*static const char *s_phys_blocks[] = {
	"wood_float",
	"door",
	"lava_fast",
	"door2",
	"door3",
};*/

/* Physics blocks that are currently active */
/*static const char *s_active_blocks[] = {
	"air_flood",
	"door_air",
	"air_flood_layer",
	"air_flood_up",
	"air_flood_down",
	"door2_air",
	"door3_air",
};*/

bool blocktype_is_placable(enum blocktype_t type)
{
	switch (type)
	{
		case AIR:
		case GRASS:
		case ADMINIUM:
		case WATER:
		case WATERSTILL:
		case LAVA:
		case LAVASTILL:
			return false;

		default:
			return type <= OBSIDIAN;
	}
}

/*bool blocktype_passes_light(enum blocktype_t type)
{
	switch (type)
	{
		case AIR:
		case GLASS:
		case OP_AIR:
		case OP_GLASS:
		case LEAF:
		case REDFLOWER:
		case YELLOWFLOWER:
		case MUSHROOM:
		case REDMUSHROOM:
		case SHRUB:
		case DOOR3:
		case DOOR_AIR:
		case DOOR2_AIR:
		case DOOR3_AIR:
			return true;

		default:
			return false;
	}
}

bool blocktype_is_phys(enum blocktype_t type)
{
	switch (type)
	{
		case WATER:
		case LAVA:
		case SAND:
		case GRAVEL:
		case TRUNK:
		case LEAF:
		case SPONGE:
		case WOOD_FLOAT:
		case LAVA_FAST:
		case AIR_FLOOD:
		case DOOR_AIR:
		case AIR_FLOOD_LAYER:
		case AIR_FLOOD_DOWN:
		case AIR_FLOOD_UP:
		case DOOR2_AIR:
		case DOOR3_AIR:
			return true;

		default:
			return false;
	}
}

enum blocktype_t blocktype_convert_to_client(enum blocktype_t type)
{
	switch (type)
	{
		case OP_GLASS: return GLASS;
		case OPSIDIAN: return OBSIDIAN;
		case OP_BRICK: return BRICK;
		case OP_STONE: return ROCK;
		case OP_COBBLESTONE: return STONE;
		case OP_AIR: return AIR;
		case OP_WATER: return WATERSTILL;

		case WOOD_FLOAT: return WOOD;
		case DOOR: return TRUNK;
		case LAVA_FAST: return LAVA;
		case DOOR2: return OBSIDIAN;
		case DOOR3: return GLASS;

		case AIR_FLOOD:
		case DOOR_AIR:
		case AIR_FLOOD_LAYER:
		case AIR_FLOOD_DOWN:
		case AIR_FLOOD_UP:
		case DOOR2_AIR:
		case DOOR3_AIR:
			return AIR;

		default:
			return type;
	}
}

enum blocktype_t blocktype_convert_to_save(enum blocktype_t type)
{
	switch (type)
	{
		case DOOR_AIR: return DOOR;
		case DOOR2_AIR: return DOOR2;
		case DOOR3_AIR: return DOOR3;

		case AIR_FLOOD:
		case AIR_FLOOD_LAYER:
		case AIR_FLOOD_DOWN:
		case AIR_FLOOD_UP:
			return AIR;

		default:
			return type;
	}
}*/

const char *blocktype_get_name(enum blocktype_t type)
{
    return s_blocks.items[type].name;
}

enum blocktype_t blocktype_get_by_name(const char *name)
{
	int i;
	for (i = 0; i < s_blocks.used; i++)
	{
		if (strcasecmp(s_blocks.items[i].name, name) == 0) return i;
	}

	return -1;
}

const struct block_t block_convert_from_mcs(uint8_t type)
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
