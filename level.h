#ifndef LEVEL_H
#define LEVEL_H

#include <pthread.h>
#include <string.h>
#include "block.h"
#include "physics.h"
#include "position.h"
#include "list.h"
#include "npc.h"

#define MAX_CLIENTS_PER_LEVEL 64
#define MAX_NPCS_PER_LEVEL 64
#define MAX_HOOKS_PER_LEVEL 4

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
	EVENT_DEINIT  = 1 << 9,
};

typedef bool(*level_hook_func_t)(int event, struct level_t *l, struct client_t *c, void *data, struct level_hook_data_t *arg);

struct level_hooks_t
{
	char name[16];
	level_hook_func_t func;
	struct level_hook_data_t data;
};

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
	uint8_t rankown;
	struct user_list_t uservisit;
	struct user_list_t userbuild;
	struct user_list_t userown;

	struct block_t *blocks;
	struct physics_list_t physics, physics2;
	struct block_update_list_t updates;

	unsigned physics_iter, physics_done;
	unsigned updates_iter;
	unsigned physics_runtime, updates_runtime;

	unsigned physics_runtime_last, updates_runtime_last;
	unsigned physics_count_last, updates_count_last;

	struct level_hooks_t level_hook[MAX_HOOKS_PER_LEVEL];

	/* Max players on a level */
	struct client_t *clients[MAX_CLIENTS_PER_LEVEL];
	struct npc *npcs[MAX_NPCS_PER_LEVEL];

	struct undodb_t *undo;

	char *type;
	int height_range;
	int sea_height;

	uint8_t changed:1;
	uint8_t instant:1;
	uint8_t physics_pause:1;
	uint8_t convert:1;
	uint8_t delete:1;
	uint8_t no_changes:1;

	pthread_mutex_t mutex;

	int inuse;
	pthread_mutex_t inuse_mutex;
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

static inline bool level_valid_xyz(const struct level_t *level, int x, int y, int z)
{
	return x >= 0 && x < level->x && y >= 0 && y < level->y && z >= 0 && z < level->z;
}

static inline enum blocktype_t level_get_blocktype(const struct level_t *level, int x, int y, int z)
{
	if (y >= level->y) return AIR;
	if (!level_valid_xyz(level, x, y, z)) return ADMINIUM;
	return level->blocks[level_get_index(level, x, y, z)].type;
}

bool level_init(struct level_t *level, int16_t x, int16_t y, int16_t z, const char *name, bool zero);
void level_set_block(struct level_t *level, struct block_t *block, unsigned index);
bool level_send(struct client_t *client);
void level_gen(struct level_t *level, const char *type, int height_range, int sea_height);
bool level_is_loaded(const char *name);
bool level_get_by_name(const char *name, struct level_t **level);
bool level_load(const char *name, struct level_t **level);
void level_save_all(void *arg);
void level_unload_empty(void *arg);

int level_get_new_npc_id(struct level_t *level, struct npc *npc);

void level_notify_all(struct level_t *level, const char *message);

void level_change_block(struct level_t *level, struct client_t *c, int16_t x, int16_t y, int16_t z, uint8_t m, uint8_t t, bool click);
void level_change_block_force(struct level_t *level, struct block_t *block, unsigned index);

void level_addupdate(struct level_t *level, unsigned index, enum blocktype_t newtype, uint16_t newdata);
void level_addupdate_override(struct level_t *level, unsigned index, enum blocktype_t newtype, uint16_t newdata);

void level_process_physics(bool can_init);
void level_process_updates(bool can_init);

void register_level_hook_func(const char *name, level_hook_func_t level_hook_func);
void deregister_level_hook_func(const char *name);

void level_copy(struct level_t *src, struct level_t *dst);

bool level_hook_attach(struct level_t *l, const char *name);
bool level_hook_detach(struct level_t *l, const char *name);
bool level_hook_delete(struct level_t *l, const char *name);

bool call_level_hook(int hook, struct level_t *l, struct client_t *c, void *data);

bool level_user_can_visit(const struct level_t *l, const struct player_t *p);
bool level_user_can_build(const struct level_t *l, const struct player_t *p);
bool level_user_can_own(const struct level_t *l, const struct player_t *p);

void level_user_undo(struct level_t *level, unsigned globalid);

bool level_inuse(struct level_t *level, bool inuse);

#endif /* LEVEL_H */
