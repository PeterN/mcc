#ifndef BLOCK_H
#define BLOCK_H

#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include "bitstuff.h"
#include "list.h"
#include "rank.h"

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
	BLOCK_END,

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
	
	BLOCK_INVALID = UINT_MAX
};

enum
{
	TRIG_NONE,  /* Not triggered */
	TRIG_EMPTY, /* Triggered, leave the block empty */
	TRIG_FILL,  /* Triggered, put original block back */
};

struct level_t;

struct block_t
{
	uint16_t fixed:1;
	uint16_t physics:1;
	uint16_t type:12;
	uint16_t teleporter:1;
	uint16_t data:16;
	uint32_t owner:31;
	uint32_t touched:1;
};

typedef enum blocktype_t(*convert_func_t)(struct level_t *level, unsigned index, const struct block_t *block);
typedef bool(*trigger_func_t)(struct level_t *l, unsigned index, const struct block_t *block);
typedef void(*delete_func_t)(struct level_t *l, unsigned index, const struct block_t *block);
typedef void(*physics_func_t)(struct level_t *l, unsigned index, const struct block_t *block);

struct blocktype_desc_t
{
	char *name;
	bool loaded;
	bool clear;
	enum rank_t min_rank;
	convert_func_t convert_func;
	trigger_func_t trigger_func;
	delete_func_t delete_func;
	physics_func_t physics_func;
};

static inline bool blocktype_desc_t_compare(struct blocktype_desc_t *a, struct blocktype_desc_t *b)
{
	/* Compare pointers */
	return a->name == a->name;
}
LIST(blocktype_desc, struct blocktype_desc_t, blocktype_desc_t_compare)

/*bool blocktype_is_phys(enum blocktype_t type);*/

void blocktype_init();
const char *blocktype_get_name(enum blocktype_t type);
enum blocktype_t blocktype_get_by_name(const char *name);
struct block_t block_convert_from_mcs(uint8_t type);
bool blocktype_has_physics(enum blocktype_t type);
enum rank_t blocktype_min_rank(enum blocktype_t type);

int register_blocktype(enum blocktype_t type, const char *name, enum rank_t min_rank, convert_func_t convert_func, trigger_func_t trigger_func, delete_func_t delete_func, physics_func_t physics_func, bool clear);
void deregister_blocktype(enum blocktype_t type);
enum blocktype_t convert(struct level_t *level, unsigned index, const struct block_t *block);
int trigger(struct level_t *level, unsigned index, const struct block_t *block);
void delete(struct level_t *level, unsigned index, const struct block_t *block);
void physics(struct level_t *level, unsigned index, const struct block_t *block);

struct client_t;

void blocktype_list(struct client_t *c);

bool trigger_door(struct level_t *l, unsigned index, const struct block_t *block);
void physics_door(struct level_t *l, unsigned index, const struct block_t *block);

#endif /* BLOCK_H */
