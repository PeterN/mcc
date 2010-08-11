#include "block.h"
#include "mcc.h"

#define DOOR1(n, b) \
	static enum blocktype_t convert_door_ ## n(struct level_t *level, unsigned index, const struct block_t *block) \
	{ \
		return block->data ? AIR : b; \
	}

#define DOOR2(n) \
	register_blocktype(BLOCK_INVALID, "door_" #n, RANK_BUILDER, &convert_door_ ## n, &trigger_door, NULL, &physics_door, false)

#define DOOR3(n, r) \
	register_blocktype(BLOCK_INVALID, "door_" #n, r, &convert_door_ ## n, &trigger_door, NULL, &physics_door, false)

DOOR1(stone, ROCK)
DOOR1(grass, GRASS)
DOOR1(dirt, DIRT)
DOOR1(cobblestone, STONE)
DOOR1(wood, WOOD)
DOOR1(plant, SHRUB)
DOOR1(adminium, ADMINIUM)
DOOR1(sand, SAND)
DOOR1(gravel, GRAVEL)
DOOR1(gold_ore, GOLDROCK)
DOOR1(iron_ore, IRONROCK)
DOOR1(coal, COAL)
DOOR1(leaves, LEAF)
DOOR1(sponge, SPONGE)
DOOR1(red, RED)
DOOR1(orange, ORANGE)
DOOR1(yellow, YELLOW)
DOOR1(greenyellow, LIGHTGREEN)
DOOR1(green, GREEN)
DOOR1(springgreen, AQUAGREEN)
DOOR1(cyan, CYAN)
DOOR1(blue, LIGHTBLUE)
DOOR1(blueviolet, BLUE)
DOOR1(indigo, PURPLE)
DOOR1(purple, LIGHTPURPLE)
DOOR1(magenta, PINK)
DOOR1(pink, DARKPINK)
DOOR1(black, DARKGREY)
DOOR1(grey, LIGHTGREY)
DOOR1(white, WHITE)
DOOR1(yellow_flower, YELLOWFLOWER)
DOOR1(red_flower, REDFLOWER)
DOOR1(brown_shroom, MUSHROOM)
DOOR1(red_shroom, REDMUSHROOM)
DOOR1(gold, GOLDSOLID)
DOOR1(iron, IRON)
DOOR1(double_step, STAIRCASEFULL)
DOOR1(brick, BRICK)
DOOR1(tnt, TNT)
DOOR1(bookcase, BOOKCASE)
DOOR1(mossy_cobblestone, STONEVINE)

void module_init(void **data)
{
	DOOR2(stone);
	DOOR2(grass);
	DOOR2(dirt);
	DOOR2(cobblestone);
	DOOR2(wood);
	DOOR2(plant);
	DOOR3(adminium, RANK_OP);
	DOOR2(sand);
	DOOR2(gravel);
	DOOR2(gold_ore);
	DOOR2(iron_ore);
	DOOR2(coal);
	DOOR2(leaves);
	DOOR2(sponge);
	DOOR2(red);
	DOOR2(orange);
	DOOR2(yellow);
	DOOR2(greenyellow);
	DOOR2(green);
	DOOR2(springgreen);
	DOOR2(cyan);
	DOOR2(blue);
	DOOR2(blueviolet);
	DOOR2(indigo);
	DOOR2(purple);
	DOOR2(magenta);
	DOOR2(pink);
	DOOR2(black);
	DOOR2(grey);
	DOOR2(white);
	DOOR2(yellow_flower);
	DOOR2(red_flower);
	DOOR2(brown_shroom);
	DOOR2(red_shroom);
	DOOR2(gold);
	DOOR2(iron);
	DOOR2(double_step);
	DOOR2(brick);
	DOOR2(tnt);
	DOOR2(bookcase);
	DOOR2(mossy_cobblestone);
}

void module_deinit(void *data)
{
}
