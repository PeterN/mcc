#ifndef LEVEL_H
#define LEVEL_H

#include <pthread.h>
#include <string.h>
#include "block.h"
#include "physics.h"
#include "position.h"
#include "list.h"

#define MAX_CLIENTS_PER_LEVEL 64

struct client_t;

static inline bool user_compare(unsigned *a, unsigned *b)
{
	return *a == *b;
}
LIST(user, unsigned, user_compare)

struct teleporter_t
{
    char name[32];
    char dest[32];
    char dest_level[32];
    struct position_t pos;
};

static inline bool teleporter_t_compare(struct teleporter_t *a, struct teleporter_t *b)
{
    return strcasecmp(a->name, b->name) == 0;
}
LIST(teleporter, struct teleporter_t, teleporter_t_compare)

struct level_t
{
    char name[64];
	unsigned x;
	unsigned y;
	unsigned z;

	struct position_t spawn;

    unsigned owner;
	uint8_t rankvisit;
	uint8_t rankbuild;
	struct user_list_t uservisit;
	struct user_list_t userbuild;

	struct block_t *blocks;
	struct physics_list_t physics, physics_remove;
	struct block_update_list_t updates;
    struct teleporter_list_t teleporters;

    int physics_iter;
    int updates_iter;

	/* Max players on a level */
	struct client_t *clients[MAX_CLIENTS_PER_LEVEL];

    int type;
    bool changed;
	pthread_t thread;
	bool thread_valid;
	bool convert;
	pthread_mutex_t mutex;
};

bool level_t_compare(struct level_t **a, struct level_t **b);
LIST(level, struct level_t *, level_t_compare)

extern struct level_list_t s_levels;

bool level_get_xyz(const struct level_t *level, unsigned index, int16_t *x, int16_t *y, int16_t *z);
static inline unsigned level_get_index(struct level_t *level, unsigned x, unsigned y, unsigned z)
{
	//return x + (z * level->y + y) * level->x;
	return x + (z + y * level->z) * level->x;
}

bool level_init(struct level_t *level, unsigned x, unsigned y, unsigned z, const char *name, bool zero);
void level_set_block(struct level_t *level, struct block_t *block, unsigned index);
bool level_send(struct client_t *client);
void level_gen(struct level_t *level, int type);
bool level_get_by_name(const char *name, struct level_t **level);
bool level_load(const char *name, struct level_t **level);
void level_save_all();
void level_unload_empty();

void level_change_block(struct level_t *level, struct client_t *c, int16_t x, int16_t y, int16_t z, uint8_t m, uint8_t t);
void level_change_block_force(struct level_t *level, struct block_t *block, unsigned index);

void level_set_teleporter(struct level_t *level, const char *name, struct position_t *pos, const char *dest, const char *dest_level);

void level_addupdate(struct level_t *level, unsigned index, enum blocktype_t newtype, uint16_t newdata);

#endif /* LEVEL_H */
