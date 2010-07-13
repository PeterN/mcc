#ifndef BLOCK_H
#define BLOCK_H

#include <stdint.h>
#include <stdbool.h>
#include "bitstuff.h"

/* Client block types */
enum blocktype_t
{
	AIR = 0,
	ROCK,
	GRASS,
	DIRT,
	STONE,
	WOOD,
	SHRUB,
	ADMINIUM,
	WATER,
	WATERSTILL,
	LAVA,
	LAVASTILL,
	SAND,
	GRAVEL,
	GOLDROCK,
	IRONROCK,
	COAL,
	TRUNK,
	LEAF,
	SPONGE,
	GLASS,
	RED,
	ORANGE,
	YELLOW,
	LIGHTGREEN,
	GREEN,
	AQUAGREEN,
	CYAN,
	LIGHTBLUE,
	BLUE,
	PURPLE,
	LIGHTPURPLE,
	PINK,
	DARKPINK,
	DARKGREY,
	LIGHTGREY,
	WHITE,
	YELLOWFLOWER,
	REDFLOWER,
	MUSHROOM,
	REDMUSHROOM,
	GOLDSOLID,
	IRON,
	STAIRCASEFULL,
	STAIRCASESTEP,
	BRICK,
	TNT,
	BOOKCASE,
	STONEVINE,
	OBSIDIAN,
	ROCK_END,

	/*OP_GLASS = 100,
	OPSIDIAN,
	OP_BRICK,
	OP_STONE,
	OP_COBBLESTONE,
	OP_AIR,
	OP_WATER,

	WOOD_FLOAT = 110,
	DOOR,
	LAVA_FAST,
	DOOR2,
	DOOR3,

	AIR_FLOOD = 200,
	DOOR_AIR,
	AIR_FLOOD_LAYER,
	AIR_FLOOD_DOWN,
	AIR_FLOOD_UP,
	DOOR2_AIR,
	DOOR3_AIR,*/
};

/*bool blocktype_is_phys(enum blocktype_t type);*/

struct block_t
{
    uint16_t fixed:1;
    uint16_t physics:1;
	uint16_t type:13;
	uint16_t teleporter:1;
	uint16_t data:16;
	uint32_t owner;
};

static inline bool block_is_fixed(const struct block_t *block)
{
	//return HasBit(block->type, 15);
	return block->fixed;
}

static inline bool block_has_physics(const struct block_t *block)
{
	//return HasBit(block->type, 14);
	return block->physics;
}

static inline enum blocktype_t block_get_blocktype(const struct block_t *block)
{
	//return GetBits(block->type, 0, 14);
	return block->type;
}

const char *blocktype_get_name(enum blocktype_t type);

#endif /* BLOCK_H */
