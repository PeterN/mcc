#include "level.h"
#include "block.h"

static struct
{
	enum blocktype_t parquet;
	enum blocktype_t chequer;
	enum blocktype_t oldcobble;
} s;

static int oddoreven(const struct level_t *level, unsigned index)
{
	int16_t x, y, z;
	level_get_xyz(level, index, &x, &y, &z);
	return (x + y + z) % 2;
}

static enum blocktype_t convert_parquet(struct level_t *level, unsigned index, const struct block_t *block)
{
	return oddoreven(level, index) ? TRUNK : WOOD;
}

static enum blocktype_t convert_chequer(struct level_t *level, unsigned index, const struct block_t *block)
{
	return oddoreven(level, index) ? DARKGREY : WHITE;
}

static enum blocktype_t convert_oldcobble(struct level_t *level, unsigned index, const struct block_t *block)
{
	return block->data > 10 ? STONE : STONEVINE;
}

void physics_oldcobble(struct level_t *level, unsigned index, const struct block_t *block)
{
	if (block->data == 0) level_addupdate(level, index, block->type, (rand() % 100) + 1);
}

void module_init(void **data)
{
	s.parquet = register_blocktype(BLOCK_INVALID, "parquet", RANK_GUEST, &convert_parquet, NULL, NULL, NULL, false, false, false);
	s.chequer = register_blocktype(BLOCK_INVALID, "chequer", RANK_GUEST, &convert_chequer, NULL, NULL, NULL, false, false, false);
	s.oldcobble = register_blocktype(BLOCK_INVALID, "oldcobble", RANK_GUEST, &convert_oldcobble, NULL, NULL, &physics_oldcobble, false, false, false);
}

void module_deinit(void *data)
{
	deregister_blocktype(s.parquet);
	deregister_blocktype(s.chequer);
	deregister_blocktype(s.oldcobble);
}
