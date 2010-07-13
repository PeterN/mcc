#include <strings.h>
#include "block.h"

static const char *s_standard_blocks[] = {
	"air",
	"stone",
	"grass",
	"dirt",
	"cobblestone",
	"wood",
	"plant",
	"adminium",
	"active_water",
	"water",
	"active_lava",
	"lava",
	"sand",
	"gravel",
	"gold_ore",
	"iron_ore",
	"coal",
	"tree",
	"leaves",
	"sponge",
	"glass",
	"red",
	"orange",
	"yellow",
	"greenyellow",
	"green",
	"springgreen",
	"cyan",
	"blue",
	"blueviolet",
	"indigo",
	"purple",
	"magenta",
	"pink",
	"black",
	"grey",
	"white",
	"yellow_flower",
	"red_flower",
	"brown_shroom",
	"red_shroom",
	"gold",
	"iron",
	"double_stair",
	"stair",
	"brick",
	"tnt",
	"bookcase",
	"mossy_cobblestone",
	"obsidian",
};

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
    return s_standard_blocks[type];
}
