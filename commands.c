#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <dirent.h>
#include "client.h"
#include "level.h"
#include "packet.h"
#include "player.h"
#include "playerdb.h"
#include "mcc.h"
#include "network.h"

static void notify_file(struct client_t *c, const char *filename)
{
    char buf[64];
    FILE *f = fopen(filename, "r");
    if (f == NULL)
    {
        snprintf(buf, sizeof buf, "No %s found", filename);
        client_notify(c, buf);
        return;
    }

    while (!feof(f))
    {
        memset(buf, 0, sizeof buf);
        fgets(buf, sizeof buf, f);
        if (buf[0] != '\0') client_notify(c, buf);
    }

    fclose(f);
}

typedef void(*command_func)(struct client_t *c, int params, const char **param);

struct command_t
{
    const char *command;
    enum rank_t rank;
    command_func func;
};

struct command_t s_commands[];

#define CMD(x) static void cmd_ ## x (struct client_t *c, int params, const char **param)

CMD(ban)
{
    char buf[64];
    struct player_t *p;
    int oldrank;

    if (params != 2)
    {
        client_notify(c, "ban <user>");
        return;
    }

    oldrank = playerdb_get_rank(param[1]);
    if (c->player->rank != RANK_ADMIN && oldrank >= RANK_OP)
    {
        client_notify(c, "Cannot ban op or admin");
        return;
    }

    playerdb_set_rank(param[1], RANK_BANNED);
    p = player_get_by_name(param[1]);
    if (p != NULL)
    {
        p->rank = RANK_BANNED;
    }

    snprintf(buf, sizeof buf, "%s banned", param[1]);
    net_notify_all(buf);
}

CMD(commands)
{
    char buf[64];
    char *bufp = buf;

    struct command_t *comp = s_commands;
    for (; comp->command != NULL; comp++)
    {
        if (c->player->rank < comp->rank) continue;

        size_t len = strlen(comp->command) + 1;
        if (len >= sizeof buf - (bufp - buf))
        {
            client_notify(c, buf);
            bufp = buf;
        }

        strcpy(bufp, comp->command);
        bufp[len - 1] = ' ';
        bufp += len;
    }

    client_notify(c, buf);
}

CMD(cuboid)
{
	if (params > 2)
	{
		client_notify(c, "cuboid [<type>]");
		return;
	}

	c->player->mode = MODE_CUBOID;
	c->player->cuboid_start = UINT_MAX;

	if (params == 2)
	{
		c->player->cuboid_type = blocktype_get_by_name(param[1]);
	}
	else
	{
		c->player->cuboid_type = -1;
	}

    char buf[64];
    snprintf(buf, sizeof buf, "Place corners of cuboid");
    client_notify(c, buf);
}

CMD(exit)
{
    g_server.exit = true;
}

CMD(fixed)
{
    ToggleBit(c->player->flags, FLAG_PLACE_FIXED);

    char buf[64];
    snprintf(buf, sizeof buf, "Fixed %s", HasBit(c->player->flags, FLAG_PLACE_FIXED) ? "on" : "off");
    client_notify(c, buf);
}

CMD(goto)
{
    if (params != 2)
    {
        client_notify(c, "goto <name>");
        return;
    }

    struct level_t *l;
    if (level_get_by_name(param[1], &l))
    {
        if (player_change_level(c->player, l)) level_send(c);
    }
    else
    {
        char buf[64];
        snprintf(buf, sizeof buf, "Cannot go to level '%s'", param[1]);
        client_notify(c, buf);
    }
}

CMD(hide)
{
    c->hidden = !c->hidden;

    if (c->hidden)
    {
        client_send_despawn(c, true);
    }
    else
    {
        client_send_spawn(c, true);
    }

    char buf[64];
    snprintf(buf, sizeof buf, "Hidden %s", c->hidden ? "on" : "off");
    client_notify(c, buf);
}

CMD(home)
{
    char name[64];
    struct level_t *l;

    snprintf(name, sizeof name, "%s_home", c->player->username);

    if (!level_get_by_name(name, &l))
    {
        l = malloc(sizeof *l);
        level_init(l, 32, 32, 32, name);
        level_gen(l, 0);
        level_list_add(&s_levels, l);
    }

    /* Don't resend the level if play is already on it */
    if (player_change_level(c->player, l)) level_send(c);
}

CMD(identify)
{
    if (params != 2)
    {
        client_notify(c, "identify <password>");
        return;
    }
}

CMD(info)
{
    player_toggle_mode(c->player, MODE_INFO);

    char buf[64];
    snprintf(buf, sizeof buf, "Block info %s", (c->player->mode == MODE_INFO) ? "on" : "off");
    client_notify(c, buf);
}

CMD(kick)
{
    if (params != 2)
    {
        client_notify(c, "kick <user>");
        return;
    }

    char buf[64];
    snprintf(buf, sizeof buf, "kicked by %s", c->player->username);

    int i;
    for (i = 0; i < s_clients.used; i++)
    {
        struct client_t *c = s_clients.items[i];
        if (c->player == NULL) continue;
        if (strcasecmp(c->player->username, param[1]) == 0)
        {
            net_close(c, buf);
        }
    }
}

CMD(lava)
{
    player_toggle_mode(c->player, MODE_PLACE_LAVA);

    char buf[64];
    snprintf(buf, sizeof buf, "Lava %s", (c->player->mode == MODE_PLACE_LAVA) ? "on" : "off");
    client_notify(c, buf);
}

static int level_filename_filter(const struct dirent *d)
{
    return strstr(d->d_name, ".mcl") != NULL;
}

CMD(levels)
{
    char buf[64];
    char *bufp;
    struct dirent **namelist;
    int n, i;

    strcpy(buf, "Levels: ");
    bufp = buf + strlen(buf);

    n = scandir("levels", &namelist, &level_filename_filter, alphasort);
    if (n < 0)
    {
        client_notify(c, "Unable to get list of levels");
        return;
    }

    for (i = 0; i < n; i++)
    {
        size_t len = strlen(namelist[i]->d_name) + (i < n - 1 ? 2 : 0);
        if (len >= sizeof buf - (bufp - buf))
        {
            client_notify(c, buf);
            bufp = buf;
        }

        strcpy(bufp, namelist[i]->d_name);
        bufp += len;

        free(namelist[i]);

        if (i < n - 1) strcpy(bufp - 2, ", ");
    }

    client_notify(c, buf);
}

CMD(mapinfo)
{
    char buf[64];
    snprintf(buf, sizeof buf, "Level '%s': %d x %d x %d", c->player->level->name, c->player->level->x, c->player->level->y, c->player->level->z);
    client_notify(c, buf);
}

CMD(motd)
{
    notify_file(c, "motd.txt");
}

CMD(newlvl)
{
    if (params != 6)
    {
        client_notify(c, "newlvl <name> <x> <y> <z> <type>");
        client_notify(c, " type: 0=flat 1=flat/adminium 2=smooth 6=rough");
        return;
    }

    const char *name = param[1];
    int x = strtol(param[2], NULL, 10);
    int y = strtol(param[3], NULL, 10);
    int z = strtol(param[4], NULL, 10);
    int t = strtol(param[5], NULL, 10);

    if (!level_get_by_name(name, NULL))
    {
        struct level_t *l = malloc(sizeof *l);
        level_init(l, x, y, z, name);
        level_gen(l, t);
        level_list_add(&s_levels, l);

        client_notify(c, "Starting level creation");
    }
    else
    {
        char buf[64];
        snprintf(buf, sizeof buf, "Level '%s' already exists", name);
        client_notify(c, buf);
    }
}

CMD(rules)
{
    notify_file(c, "rules.txt");
}

CMD(setrank)
{
    int oldrank;
    int newrank;
    struct player_t *p;

    if (params != 3)
    {
        client_notify(c, "setrank <name> <rank>");
        return;
    }

    newrank = rank_get_by_name(param[2]);
    if (newrank == -1 && c->player->rank == RANK_ADMIN)
    {
        client_notify(c, "Invalid rank: banned guest builder advbuilder op admin");
        return;
    }
    if ((newrank == -1 || newrank >= RANK_OP) && c->player->rank == RANK_OP)
    {
        client_notify(c, "Invalid rank: banned guest builder advbuilder");
        return;
    }

    oldrank = playerdb_get_rank(param[1]);
    if (oldrank == newrank)
    {
        client_notify(c, "User already at rank");
        return;
    }
    if (c->player->rank != RANK_ADMIN && oldrank == RANK_ADMIN)
    {
        client_notify(c, "Cannot demote admin");
        return;
    }

    playerdb_set_rank(param[1], newrank);
    p = player_get_by_name(param[1]);
    if (p != NULL)
    {
        p->rank = newrank;
    }

    char buf[64];
    snprintf(buf, sizeof buf, "%s rank set to %s", param[1], param[2]);
    net_notify_all(buf);
}

CMD(setspawn)
{
    c->player->level->spawn = c->player->pos;
    c->player->level->changed = true;
}

CMD(spawn)
{
	c->player->pos = c->player->level->spawn;
	client_add_packet(c, packet_send_teleport_player(0xFF, &c->player->pos));
}

CMD(solid)
{
    player_toggle_mode(c->player, MODE_PLACE_SOLID);

    char buf[64];
    snprintf(buf, sizeof buf, "Solid %s", (c->player->mode == MODE_PLACE_SOLID) ? "on" : "off");
    client_notify(c, buf);
}

static char s_pattern[256];
static int undo_filename_filter(const struct dirent *d)
{
    return strncmp(d->d_name, s_pattern, strlen(s_pattern)) == 0;
}

CMD(teleporter)
{
    if (params < 2 || params > 5)
    {
        client_notify(c, "teleporter <name> [<dest> [<level>]]");
        return;
    }

    level_set_teleporter(c->player->level, param[1], &c->player->pos, param[2], param[3]);
}

CMD(tp)
{
    char buf[64];

    if (params != 2)
    {
        client_notify(c, "tp <user>");
        return;
    }

    const struct player_t *p = player_get_by_name(param[1]);
    if (p == NULL)
    {
        snprintf(buf, sizeof buf, "%s is offline", param[1]);
        client_notify(c, buf);
        return;
    }
    if (p->level != c->player->level)
    {
        snprintf(buf, sizeof buf, "%s is on '%s'", param[1], c->player->level->name);
        client_notify(c, buf);
        return;
    }

    client_add_packet(c, packet_send_teleport_player(0xFF, &p->pos));
}

CMD(undo)
{
    char buf[64];

    if (params < 3 || params > 4)
    {
        client_notify(c, "undo <user> <level> [<time>]");
        return;
    }

    if (params == 3)
    {
        char *bufp;
        struct dirent **namelist;
        int n, i;

        strcpy(buf, "Undo log: ");
        bufp = buf + strlen(buf);

        snprintf(s_pattern, sizeof s_pattern, "%s_%s_", param[2], param[1]);

        n = scandir("undo", &namelist, &undo_filename_filter, alphasort);
        if (n < 0)
        {
            client_notify(c, "Unable to get list of undo logs");
            return;
        }

        for (i = 0; i < n; i++)
        {
            size_t len = strlen(namelist[i]->d_name + strlen(s_pattern)) + (i < n - 1 ? 2 : 0);
            if (len >= sizeof buf - (bufp - buf))
            {
                client_notify(c, buf);
                bufp = buf;
            }

            strcpy(bufp, namelist[i]->d_name + strlen(s_pattern));
            bufp += len;

            free(namelist[i]);

            if (i < n - 1) strcpy(bufp - 2, ", ");
        }

        client_notify(c, buf);
        return;
    }

    player_undo(c, param[1], param[2], param[3]);
}

CMD(water)
{
    player_toggle_mode(c->player, MODE_PLACE_WATER);

    char buf[64];
    snprintf(buf, sizeof buf, "Water %s", (c->player->mode == MODE_PLACE_WATER) ? "on" : "off");
    client_notify(c, buf);
}

struct command_t s_commands[] = {
    { "ban", RANK_OP, &cmd_ban },
    { "commands", RANK_BANNED, &cmd_commands },
    { "cuboid", RANK_ADV_BUILDER, &cmd_cuboid },
    { "z", RANK_ADV_BUILDER, &cmd_cuboid },
    { "exit", RANK_ADMIN, &cmd_exit },
    { "fixed", RANK_OP, &cmd_fixed },
    { "goto", RANK_GUEST, &cmd_goto },
    { "hide", RANK_OP, &cmd_hide },
    { "home", RANK_GUEST, &cmd_home },
    { "identify", RANK_GUEST, &cmd_identify },
    { "info", RANK_BUILDER, &cmd_info },
    { "kick", RANK_OP, &cmd_kick },
    { "lava", RANK_BUILDER, &cmd_lava },
    { "levels", RANK_GUEST, &cmd_levels },
    { "mapinfo", RANK_GUEST, &cmd_mapinfo },
    { "motd", RANK_BANNED, &cmd_motd },
    { "newlvl", RANK_ADMIN, &cmd_newlvl },
    { "rules", RANK_BANNED, &cmd_rules },
    { "setrank", RANK_OP, &cmd_setrank },
    { "setspawn", RANK_OP, &cmd_setspawn },
    { "spawn", RANK_GUEST, &cmd_spawn },
    { "solid", RANK_OP, &cmd_solid },
    { "teleporter", RANK_ADV_BUILDER, &cmd_teleporter },
    { "tp", RANK_BUILDER, &cmd_tp },
    { "undo", RANK_OP, &cmd_undo },
    { "water", RANK_BUILDER, &cmd_water },
    { NULL, -1, NULL },
};

bool command_process(struct client_t *client, int params, const char **param)
{
    struct command_t *comp = s_commands;
    for (; comp->command != NULL; comp++)
    {
        if (client->player->rank >= comp->rank && strcasecmp(param[0], comp->command) == 0)
        {
            comp->func(client, params, param);
            return true;
        }
    }

    return false;
}
