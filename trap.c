#include "block.h"
#include "level.h"

static enum blocktype_t s_trap;

static enum blocktype_t convert_trap(struct level_t *level, unsigned index, const struct block_t *block)
{
	return block->data < 10 ? TRUNK : AIR;
}

static void physics_trap(struct level_t *l, unsigned index, const struct block_t *block)
{
	level_addupdate(l, index, BLOCK_INVALID, (block->data + 1) % 40);
}

void module_init(void **data)
{
	s_trap = register_blocktype(BLOCK_INVALID, "trap", RANK_MOD, &convert_trap, NULL, NULL, &physics_trap, false, true, false);
}

void module_deinit(void *data)
{
	deregister_blocktype(s_trap);
}
