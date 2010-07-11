#ifndef LEVEL_H
#define LEVEL_H

#include <pthread.h>
#include "block.h"
#include "physics.h"
#include "position.h"

struct client_t;

struct level_t
{
    char name[64];
	unsigned x;
	unsigned y;
	unsigned z;

	struct position_t spawn;

	struct block_t *blocks;
	struct physics_list_t physics;

    int type;
    bool changed;
	pthread_t thread;
	pthread_mutex_t mutex;
};

bool level_t_compare(struct level_t **a, struct level_t **b);
LIST(level, struct level_t *, level_t_compare)

extern struct level_list_t s_levels;

static inline unsigned level_get_index(struct level_t *level, unsigned x, unsigned y, unsigned z)
{
	//return x + (z * level->y + y) * level->x;
	return x + (z + y * level->z) * level->x;
}

void level_init(struct level_t *level, unsigned x, unsigned y, unsigned z, const char *name);
void level_set_block(struct level_t *level, struct block_t *block, unsigned index);
void level_clear_block(struct level_t *level, unsigned index);
bool level_send(struct client_t *client);
void level_gen(struct level_t *level, int type);
bool level_get_by_name(const char *name, struct level_t **level);
bool level_load(const char *name, struct level_t **level);
void level_save_all();
void level_unload_empty();

void level_change_block(struct level_t *level, struct client_t *c, int16_t x, int16_t y, int16_t z, uint8_t m, uint8_t t);
void level_change_block_force(struct level_t *level, struct block_t *block, unsigned index);

#endif /* LEVEL_H */
