#ifndef PLAYER_H
#define PLAYER_H

#include <stdbool.h>
#include <stdint.h>
#include "list.h"

struct level_t;

struct player_t
{
    char *username;
    int16_t x;
    int16_t y;
    int16_t z;
    uint8_t heading;
    uint8_t pitch;

    struct level_t *level;
};

bool player_t_compare(struct player_t **a, struct player_t **b);
LIST(player, struct player_t *, player_t_compare);

struct player_t *player_add(const char *username);
void player_del(struct player_t *player);
struct player_t *player_get_by_name(const char *username);

void player_info();

#endif /* PLAYER_H */
