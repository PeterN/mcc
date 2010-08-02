#ifndef PLAYER_H
#define PLAYER_H

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include "rank.h"
#include "block.h"
#include "colour.h"
#include "list.h"
#include "position.h"

struct client_t;
struct level_t;

enum
{
	FLAG_PLACE_FIXED,
	FLAG_PAINT,
	FLAG_DISOWN,
};

enum mode_t
{
	MODE_NORMAL,
	MODE_INFO,
	MODE_CUBOID,
	MODE_REPLACE,
	MODE_PLACE_SOLID,
	MODE_PLACE_WATER,
	MODE_PLACE_LAVA,
};

struct player_t
{
	unsigned globalid;
	unsigned levelid;
	char colourusername[128];
	char *username;
	enum rank_t rank;
	enum mode_t mode;
	uint8_t flags;

	struct position_t pos;
	struct position_t oldpos;
	int speed;
	int speeds[10];
	int warnings;
	unsigned spam[60];
	int spampos1, spampos2;

	struct level_t *level, *new_level;
	struct client_t *client;
	struct player_t *following;
	unsigned filter;

	unsigned cuboid_start;
	enum blocktype_t cuboid_type;
	enum blocktype_t replace_type;

	enum blocktype_t bindings[BLOCK_END];

	const char *hook_data;
};

bool player_t_compare(struct player_t **a, struct player_t **b);
LIST(player, struct player_t *, player_t_compare);

struct player_t *player_add(const char *username, struct client_t *c, bool *newuser, int *identified);
void player_del(struct player_t *player);
struct player_t *player_get_by_name(const char *username);

bool player_change_level(struct player_t *player, struct level_t *level);
void player_move(struct player_t *player, struct position_t *pos);

void player_info();

void player_undo(struct client_t *c, const char *username, const char *levelname, const char *timestamp);

enum rank_t rank_get_by_name(const char *rank);
const char *rank_get_name(enum rank_t rank);
enum colour_t rank_get_colour(enum rank_t rank);

static inline void player_toggle_mode(struct player_t *player, enum mode_t mode)
{
	player->mode = (player->mode == mode) ? MODE_NORMAL : mode;
}

void player_send_positions();
bool player_check_spam(struct player_t *player);

#endif /* PLAYER_H */
