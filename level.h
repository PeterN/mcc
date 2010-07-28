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
struct undodb_t;

static inline bool user_compare(unsigned *a, unsigned *b)
{
	return *a == *b;
}
LIST(user, unsigned, user_compare)

/* Teleporter types */
enum {
	TP_TELEPORT,
	TP_TRIGGER,
};

struct teleporter_t
{
	char name[32];
	char dest[32];
	char dest_level[32];
	struct position_t pos;
	int type;
};

static inline bool teleporter_t_compare(struct teleporter_t *a, struct teleporter_t *b)
{
	return strcasecmp(a->name, b->name) == 0;
}
LIST(teleporter, struct teleporter_t, teleporter_t_compare)

struct event_data_t
{
	size_t len;
	void *data;
};

enum event_t
{
	EVENT_TICK,
	EVENT_CHAT,
	EVENT_MOVE,
	EVENT_SPAWN,
	EVENT_DESPAWN,
};

typedef void(*event_hook_t)(struct level_t *l, enum event_t event, struct player_t *p, void *arg);

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
	struct teleporter_list_t teleporters;

	unsigned physics_iter, physics_done;
	unsigned updates_iter;
	unsigned physics_runtime, updates_runtime;

	unsigned physics_runtime_last, updates_runtime_last;
	unsigned physics_count_last, updates_count_last;

	event_hook_t event_hook;
	struct event_data_t event_data;

	/* Max players on a level */
	struct client_t *clients[MAX_CLIENTS_PER_LEVEL];

	struct undodb_t *undo;

	int type;
	int height_range;
	int sea_height;

	bool changed;
	bool instant;
	bool physics_pause;
	pthread_t thread;
	bool thread_valid;
	bool convert;
	bool delete;
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

void level_set_teleporter(struct level_t *level, const char *name, struct position_t *pos, const char *dest, const char *dest_level, int type);

void level_addupdate(struct level_t *level, unsigned index, enum blocktype_t newtype, uint16_t newdata);
void level_addupdate_override(struct level_t *level, unsigned index, enum blocktype_t newtype, uint16_t newdata);
void physics_remove(struct level_t *level, unsigned index);

void level_process_physics(bool can_init);
void level_process_updates(bool can_init);

#endif /* LEVEL_H */
