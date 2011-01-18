#include "block.h"
#include "level.h"

#define FLOOR1 IRON
#define FLOOR2 GOLDSOLID

static struct
{
	enum blocktype_t spleef1;
	enum blocktype_t spleef2;
	enum blocktype_t spleeft;
} s;

static enum blocktype_t convert_spleef1(struct level_t *level, unsigned index, const struct block_t *block)
{
	switch (block->data)
	{
		case 0: return FLOOR1;
		case 1: return AIR;
		case 2: return RED;
		default: return BLUE;
	}
}

static enum blocktype_t convert_spleef2(struct level_t *level, unsigned index, const struct block_t *block)
{
	switch (block->data)
	{
		case 0: return FLOOR2;
		case 1: return AIR;
		case 2: return RED;
		default: return BLUE;
	}
}

static int trigger_spleef(struct level_t *level, unsigned index, const struct block_t *block, struct client_t *c)
{
	level_addupdate(level, index, block->type, 1);
	return TRIG_EMPTY;
}

static void physics_spleef_sub(struct level_t *level, int16_t x, int16_t y, int16_t z)
{
	if (x < 0 || y < 0 || z < 0 || x >= level->x || y >= level->y || z >= level->z) return;

	unsigned index = level_get_index(level, x, y, z);
	enum blocktype_t type = level->blocks[index].type;
	if (type == s.spleef1 || type == s.spleef2)
	{
		if (level->blocks[index].data < 2)
			level_addupdate(level, index, type, 2);
	}
}

static void physics_spleef(struct level_t *level, unsigned index, const struct block_t *block)
{
	int16_t x, y, z;
	level_get_xyz(level, index, &x, &y, &z);

	switch (block->data)
	{
		case 0:
		case 1:
			return;

		case 2:
			physics_spleef_sub(level, x - 1, y, z);
			physics_spleef_sub(level, x + 1, y, z);
			physics_spleef_sub(level, x, y, z - 1);
			physics_spleef_sub(level, x, y, z + 1);
			/* Fall through */

		default:
			level_addupdate(level, index, block->type, block->data + 1);
			break;

		case 10:
			level_addupdate(level, index, block->type, 0);
			break;
	}
}

static enum blocktype_t convert_spleeft(struct level_t *level, unsigned index, const struct block_t *block)
{
	return GREEN;
}

static int trigger_spleeft(struct level_t *level, unsigned index, const struct block_t *block, struct client_t *c)
{
	int16_t x, y, z;
	level_get_xyz(level, index, &x, &y, &z);

	physics_spleef_sub(level, x - 1, y, z);
	physics_spleef_sub(level, x + 1, y, z);
	physics_spleef_sub(level, x, y, z - 1);
	physics_spleef_sub(level, x, y, z + 1);
	return TRIG_FILL;
}

struct spleef_position_t
{
	int16_t x, y;
};

struct spleef_board_t
{
	int16_t x[2], z[2], y;
	int16_t positions;
	struct spleef_position_t position[4];
};

struct spleef_data_t
{
	int16_t boards;
	struct spleef_board_t board[1];
};

static bool spleef_handle_chat(struct level_t *l, struct client_t *c, char *data, struct spleef_data_t *arg)
{
	return false;
}

static bool spleef_level_hook(int event, struct level_t *l, struct client_t *c, void *data, struct level_hook_data_t *arg)
{
	switch (event)
	{
		case EVENT_CHAT: return spleef_handle_chat(l, c, data, arg->data);
	}

	return false;
}

void module_init(void **data)
{
	s.spleef1 = register_blocktype(BLOCK_INVALID, "spleef1", RANK_BUILDER, &convert_spleef1, &trigger_spleef, NULL, &physics_spleef, false, false, false);
	s.spleef2 = register_blocktype(BLOCK_INVALID, "spleef2", RANK_BUILDER, &convert_spleef2, &trigger_spleef, NULL, &physics_spleef, false, false, false);
	s.spleeft = register_blocktype(BLOCK_INVALID, "spleeft", RANK_BUILDER, &convert_spleeft, &trigger_spleeft, NULL, NULL, false, false, false);

	register_level_hook_func("spleef", &spleef_level_hook);
}

void module_deinit(void *data)
{
	deregister_blocktype(s.spleef1);
	deregister_blocktype(s.spleef2);
	deregister_blocktype(s.spleeft);

	deregister_level_hook_func("spleef");
}
