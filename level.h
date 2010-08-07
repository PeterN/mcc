#ifndef LEVEL_H
#define LEVEL_H

#include <pthread.h>
#include <string.h>
#include "block.h"
#include "physics.h"
#include "position.h"
#include "list.h"

#define MAX_CLIENTS_PER_LEVEL 64

struct player_t;
struct client_t;
struct undodb_t;

static inline bool user_compare(unsigned *a, unsigned *b)
{
	return *a == *b;
}
LIST(user, unsigned, user_compare)

struct level_hook_data_t
{
	unsigned size;
	void *data;
};

enum
{
	EVENT_TICK    = 1 << 0,
	EVENT_CHAT    = 1 << 1,
	EVENT_MOVE    = 1 << 2,
	EVENT_SPAWN   = 1 << 3,
	EVENT_DESPAWN = 1 << 4,
	EVENT_LOAD    = 1 << 5,
	EVENT_SAVE    = 1 << 6,
	EVENT_UNLOAD  = 1 << 7,
	EVENT_INIT    = 1 << 8,
};

typedef void(*level_hook_func_t)(int event, struct level_t *l, struct client_t *c, void *data, struct level_hook_data_t *arg);

struct level_t
{
	char name[64];
	int16_t x;
	int16_t y;
	int16_t z;

	struct position_t spawn;

	unsigned owner;
	uint8_t rankvisit;
	uint8_t rankbuild;
	struct user_list_t uservisit;
	struct user_list_t userbuild;

	struct block_t *blocks;
	struct physics_list_t physics, physics2;
	struct block_update_list_t updates;

	unsigned physics_iter, physics_done;
	unsigned updates_iter;
	unsigned physics_runtime, updates_runtime;

	unsigned physics_runtime_last, updates_runtime_last;
	unsigned physics_count_last, updates_count_last;

	char level_hook_name[16];
	level_hook_func_t level_hook_func;
	struct level_hook_data_t level_hook_data;

	/* Max players on a level */
	struct client_t *clients[MAX_CLIENTS_PER_LEVEL];

	struct undodb_t *undo;

	int type;
	int height_range;
	int sea_height;

	uint8_t changed:1;
	uint8_t instant:1;
	uint8_t physics_pause:1;
	uint8_t convert:1;
	uint8_t delete:1;
	uint8_t thread_valid:1;

	pthread_t thread;
	pthread_mutex_t mutex;
};

bool level_t_compare(struct level_t **a, struct level_t **b);
LIST(level, struct level_t *, level_t_compare)

extern struct level_list_t s_levels;

bool level_get_xyz(const struct level_t *level, unsigned index, int16_t *x, int16_t *y, int16_t *z);
static inline unsigned level_get_index(const struct level_t *level, unsigned x, unsigned y, unsigned z)
{
	//return x + (z * level->y + y) * level->x;
	return x + (z + y * level->z) * level->x;
}

bool level_init(struct level_t *level, int16_t x, int16_t y, int16_t z, const char *name, bool zero);
void level_set_block(struct level_t *level, struct block_t *block, unsigned index);
bool level_send(struct client_t *client);
void level_gen(struct level_t *level, int type, int height_range, int sea_height);
bool level_is_loaded(const char *name);
bool level_get_by_name(const char *name, struct level_t **level);
bool level_load(const char *name, struct level_t **level);
void level_save_all(void *arg);
void level_unload_empty(void *arg);

void level_change_block(struct level_t *level, struct client_t *c, int16_t x, int16_t y, int16_t z, uint8_t m, uint8_t t, bool click);
void level_change_block_force(struct level_t *level, struct block_t *block, unsigned index);

void level_addupdate(struct level_t *level, unsigned index, enum blocktype_t newtype, uint16_t newdata);
void level_addupdate_override(struct level_t *level, unsigned index, enum blocktype_t newtype, uint16_t newdata);

void level_process_physics(bool can_init);
void level_process_updates(bool can_init);

void register_level_hook_func(const char *name, level_hook_func_t level_hook_func);
void deregister_level_hook_func(const char *name);

bool level_hook_attach(struct level_t *l, const char *name);
bool level_hook_detach(struct level_t *l, const char *name);

void call_level_hook(int hook, struct level_t *l, struct client_t *c, void *data);

bool level_user_can_visit(const struct level_t *l, const struct player_t *p);
bool level_user_can_build(const struct level_t *l, const struct player_t *p);

#endif /* LEVEL_H */
