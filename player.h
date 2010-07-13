#ifndef PLAYER_H
#define PLAYER_H

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include "block.h"
#include "list.h"
#include "position.h"

struct client_t;
struct level_t;

enum rank_t
{
    RANK_BANNED,
    RANK_GUEST,
    RANK_BUILDER,
    RANK_ADV_BUILDER,
    RANK_OP,
    RANK_ADMIN,
};

enum
{
    FLAG_PLACE_FIXED,
};

enum mode_t
{
    MODE_NORMAL,
    MODE_INFO,
    MODE_CUBOID,
    MODE_PLACE_SOLID,
    MODE_PLACE_WATER,
    MODE_PLACE_LAVA,
};

struct player_t
{
    int globalid;
    int levelid;
    char *username;
    enum rank_t rank;
    enum mode_t mode;
    uint8_t flags;

    struct position_t pos;
    struct position_t oldpos;

    struct level_t *level;
    struct client_t *client;

    unsigned cuboid_start;
    enum blocktype_t cuboid_type;

    FILE *undo_log;
    char undo_log_name[256];
};

bool player_t_compare(struct player_t **a, struct player_t **b);
LIST(player, struct player_t *, player_t_compare);

struct player_t *player_add(const char *username);
void player_del(struct player_t *player);
struct player_t *player_get_by_name(const char *username);

bool player_change_level(struct player_t *player, struct level_t *level);
void player_move(struct player_t *player, struct position_t *pos);

void player_info();

void player_undo_log(struct player_t *player, unsigned index);
void player_undo(const char *username, const char *levelname, const char *timestamp);

enum rank_t rank_get_by_name(const char *rank);

static inline void player_toggle_mode(struct player_t *player, enum mode_t mode)
{
    player->mode = (player->mode == mode) ? MODE_NORMAL : mode;
}

void player_send_positions();

#endif /* PLAYER_H */
