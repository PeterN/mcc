#ifndef PLAYER_H
#define PLAYER_H

#include <stdbool.h>
#include <stdint.h>
#include "block.h"
#include "list.h"
#include "position.h"

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
    PLAYER_BANNED = 2,
    PLAYER_PLACE_SOLID = 3,
    PLAYER_PLACE_FIXED = 4,
};

struct player_t
{
    char *username;
    enum rank_t rank;

    struct position_t pos;
    uint8_t flags;

    struct level_t *level;

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

#endif /* PLAYER_H */
