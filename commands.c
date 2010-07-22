#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <time.h>
#include "client.h"
#include "level.h"
#include "packet.h"
#include "player.h"
#include "playerdb.h"
#include "mcc.h"
#include "network.h"

void notify_file(struct client_t *c, const char *filename)
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

typedef bool(*command_func_t)(struct client_t *c, int params, const char **param);

struct command_t
{
	const char *command;
	enum rank_t rank;
	command_func_t func;
	const char *help;
};

struct command_t s_commands[];

#define CMD(x) static bool cmd_ ## x (struct client_t *c, int params, const char **param)

static const char help_adminrules[] =
"/adminrules\n"
"Display the server's admin rules.";

CMD(adminrules)
{
	notify_file(c, "adminrules.txt");
	return false;
}

static const char help_afk[] =
"/afk [<message>]\n"
"Mark yourself AFK";

CMD(afk)
{
	return true;
}

static const char help_ban[] =
"/ban <user>\n"
"Ban user";

CMD(ban)
{
	char buf[64];
	struct player_t *p;
	int oldrank;

	if (params != 2) return true;

	oldrank = playerdb_get_rank(param[1]);
	if (c->player->rank != RANK_ADMIN && oldrank >= RANK_OP)
	{
		client_notify(c, "Cannot ban op or admin");
		return false;
	}

	playerdb_set_rank(param[1], RANK_BANNED);
	p = player_get_by_name(param[1]);
	if (p != NULL)
	{
		p->rank = RANK_BANNED;
	}

	snprintf(buf, sizeof buf, "%s banned", param[1]);
	net_notify_all(buf);
	return false;
}

static const char help_bind[] =
"/bind [<fromtype> [<totype>]]\n"
"Bind a block, so that placing <fromtype> actually places <totype>. "
"Use /bind on its own to list your binds, or without a <totype> to "
"reset a bind.";

CMD(bind)
{
	char buf[64];
	enum blocktype_t i, j;

	if (params > 3) return true;

	switch (params)
	{
		case 1:
			for (i = 0; i < BLOCK_END; i++)
			{
				if (c->player->bindings[i] != i)
				{
					snprintf(buf, sizeof buf, "%s bound to %s", blocktype_get_name(i), blocktype_get_name(c->player->bindings[i]));
					client_notify(c, buf);
				}
			}
			break;

		case 2:
			i = blocktype_get_by_name(param[1]);
			if (i != BLOCK_INVALID)
			{
				c->player->bindings[i] = i;

			}
			break;

		case 3:
			i = blocktype_get_by_name(param[1]);
			j = blocktype_get_by_name(param[2]);
			if (i != BLOCK_INVALID && j != BLOCK_INVALID)
			{
				c->player->bindings[i] = j;
			}
			break;
	}
	return false;
}

static const char help_commands[] =
"/commands\n"
"List all commands available to you.";

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

	return false;
}

static const char help_cuboid[] =
"/cuboid [<type>]\n"
"Place a cuboid, using two corners specified after using the command. "
"Cuboid will be of the <type> given, or the type held when placing the final corner.";

CMD(cuboid)
{
	char buf[64];

	if (params > 2) return true;

	c->player->mode = MODE_CUBOID;
	c->player->cuboid_start = UINT_MAX;
	c->player->replace_type = BLOCK_INVALID;

	if (params == 2)
	{
		c->player->cuboid_type = blocktype_get_by_name(param[1]);
		if (c->player->cuboid_type == BLOCK_INVALID)
		{
			snprintf(buf, sizeof buf, "Unknown block type %s", param[2]);
			return false;
		}
	}
	else
	{
		c->player->cuboid_type = BLOCK_INVALID;
	}


	snprintf(buf, sizeof buf, "Place corners of cuboid");
	client_notify(c, buf);

	return false;
}

static const char help_disown[] =
"/disown\n"
"Toggle disown mode. When enabled, any blocks placed will have their owner reset to none. "
"Useful for clearing up some blocks.";

CMD(disown)
{
	ToggleBit(c->player->flags, FLAG_DISOWN);

	char buf[64];
	snprintf(buf, sizeof buf, "Disown %s", HasBit(c->player->flags, FLAG_DISOWN) ? "on" : "off");
	client_notify(c, buf);

	return false;
}

static const char help_exit[] =
"/exit\n"
"Exit and shut down the server.";

CMD(exit)
{
	g_server.exit = true;

	return false;
}

static const char help_filter[] =
"/filter [<user>]\n"
"Filter the current level to only show blocks placed by the <user> given. "
"If no <user> is specified, filtering is reset so all blocks are shown.";

CMD(filter)
{
	if (params > 2) return true;

	if (params == 2)
	{
		int filter = playerdb_get_globalid(param[1], false, NULL);
		if (filter == -1)
		{
			client_notify(c, "User does not exist");
			return false;
		}

		c->player->filter = filter;
	}
	else
	{
		c->player->filter = 0;
		client_notify(c, "Filtering disabled");
	}

	level_send(c);

	return false;
}

static const char help_fixed[] =
"/fixed\n"
"Toggle fixed mode. When enabled, fixed blocks will not be subject "
"to physics and cannot be removed by other players.";

CMD(fixed)
{
	ToggleBit(c->player->flags, FLAG_PLACE_FIXED);

	char buf[64];
	snprintf(buf, sizeof buf, "Fixed %s", HasBit(c->player->flags, FLAG_PLACE_FIXED) ? "on" : "off");
	client_notify(c, buf);

	return false;
}

static const char help_follow[] =
"/follow [<user>]\n"
"Enables hidden mode and follows the specified <user>. "
"If no <user> specified, following is disabled.";

CMD(follow)
{
	char buf[64];

	if (params > 2) return true;

	if (params == 1)
	{
		if (c->player->following == NULL)
		{
			client_notify(c, "Not following anyone");
			return false;
		}

		snprintf(buf, sizeof buf, "Stopped following %s", c->player->following->username);
		client_notify(c, buf);

		client_add_packet(c, packet_send_spawn_player(c->player->following->levelid, c->player->following->username, &c->player->following->pos));

		c->player->following = NULL;
		return false;
	}

	struct player_t *p = player_get_by_name(param[1]);
	if (p == NULL)
	{
		snprintf(buf, sizeof buf, "%s is offline", param[1]);
		client_notify(c, buf);
		return false;
	}
	if (p->level != c->player->level)
	{
		snprintf(buf, sizeof buf, "%s is on '%s'", param[1], c->player->level->name);
		client_notify(c, buf);
		return false;
	}

	if (!c->hidden)
	{
		c->hidden = true;
		client_send_despawn(c, true);
	}

	/* Despawn followed player to prevent following player jitter */
	client_add_packet(c, packet_send_despawn_player(p->levelid));

	snprintf(buf, sizeof buf, "Hidden %s", c->hidden ? "on" : "off");
	client_notify(c, buf);

	snprintf(buf, sizeof buf, "Now following %s", p->username);
	client_notify(c, buf);

	c->player->following = p;

	return false;
}

static const char help_goto[] =
"/goto <level>\n"
"Switch to the named level.";

CMD(goto)
{
	if (params != 2) return true;

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

	return false;
}

static const char help_help[] =
"/help <command>\n"
"Display help about a command. See /commands.";

CMD(help)
{
	if (params != 2) return true;

	struct command_t *comp = s_commands;
	for (; comp->command != NULL; comp++)
	{
		if (strcasecmp(comp->command, param[1]) == 0)
		{
			client_notify(c, comp->help);
			return false;
		}
	}

	client_notify(c, "Command not found");
	return false;
}

static const char help_hide[] =
"/hide\n"
"Toggle hidden mode.";

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

	return false;
}

static const char help_home[] =
"/home\n"
"Go to your home level. If you are a new player, a blank level will be created.";

CMD(home)
{
	char name[64];
	struct level_t *l;

	snprintf(name, sizeof name, "%s_home", c->player->username);

	if (!level_get_by_name(name, &l))
	{
		client_notify(c, "Creating your home level...");

		l = malloc(sizeof *l);
		if (!level_init(l, 64, 32, 64, name, true))
		{
			LOG("Unable to create level\n");
			free(l);
			return false;
		}

		l->owner = c->player->globalid;
		l->rankvisit = RANK_GUEST;
		l->rankbuild = RANK_ADMIN;
		user_list_add(&l->uservisit, l->owner);
		user_list_add(&l->userbuild, l->owner);

		level_gen(l, 0, l->y / 2, l->y / 2);
		level_list_add(&s_levels, l);
	}

	/* Don't resend the level if player is already on it */
	if (player_change_level(c->player, l)) level_send(c);

	return false;
}

static const char help_identify[] =
"/identify <password>\n"
"Identify your account so that you may use privileged commands.";

CMD(identify)
{
	if (params != 2) return true;

	return false;
}

static const char help_info[] =
"/info\n"
"Enable block info mode. When enabled, destroying or building a block will "
"give block information instead.";

CMD(info)
{
	player_toggle_mode(c->player, MODE_INFO);

	char buf[64];
	snprintf(buf, sizeof buf, "Block info %s", (c->player->mode == MODE_INFO) ? "on" : "off");
	client_notify(c, buf);

	return false;
}

static const char help_kick[] =
"/kick <user>\n"
"Kicks the specified <user> from the server.";

CMD(kick)
{
	if (params != 2) return true;

	char buf[64];

	struct player_t *p = player_get_by_name(param[1]);
	if (p == NULL)
	{
		snprintf(buf, sizeof buf, "%s is offline", param[1]);
		client_notify(c, buf);
		return false;
	}

	snprintf(buf, sizeof buf, "kicked by %s", c->player->username);
	net_close(p->client, buf);

	return false;
}

static const char help_lava[] =
"/lava\n"
"Toggle lava mode. Any block placed will be converted to static lava.";

CMD(lava)
{
	player_toggle_mode(c->player, MODE_PLACE_LAVA);

	char buf[64];
	snprintf(buf, sizeof buf, "Lava %s", (c->player->mode == MODE_PLACE_LAVA) ? "on" : "off");
	client_notify(c, buf);

	return false;
}

static int level_filename_filter(const struct dirent *d)
{
	return strstr(d->d_name, ".mcl") != NULL || strstr(d->d_name, ".lvl") != NULL;
}

static const char help_levels[] =
"/levels\n"
"List all levels known by the server.";

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
		return false;
	}

	for (i = 0; i < n; i++)
	{
		/* Chop name off at extension */
		char *ext = strrchr(namelist[i]->d_name, '.');
		if (ext != NULL) *ext = '\0';

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

	return false;
}

static const char help_lvlowner[] =
"/lvlowner <user> [<level>]\n"
"Change ownership of a level.";

CMD(lvlowner)
{
	if (params != 2 && params != 3) return true;

	struct level_t *l;
	if (params == 3)
	{
		if (!level_get_by_name(param[2], &l))
		{
			client_notify(c, "Level does not exist");
			return false;
		}
	}
	else
	{
		l = c->player->level;
	}

	int globalid;
	if (strcasecmp(param[1], "none") == 0)
	{
		globalid = 0;
	}
	else
	{
		globalid = playerdb_get_globalid(param[1], false, NULL);
		if (globalid == -1)
		{
			client_notify(c, "User does not exist");
			return false;
		}
	}

	l->owner = globalid;
	l->changed = true;

	return false;
}

static const char help_mapinfo[] =
"/mapinfo\n"
"List information about the current level.";

CMD(mapinfo)
{
	const struct level_t *l = c->player->level;
	char buf[64];
	snprintf(buf, sizeof buf, "Level: %s  Owner: %s", l->name, playerdb_get_username(l->owner));
	client_notify(c, buf);
	snprintf(buf, sizeof buf, "Extents: %d x %d x %d", l->x, l->y, l->z);
	client_notify(c, buf);
	snprintf(buf, sizeof buf, "Visit permission: %s  Build permission: %s", rank_get_name(l->rankvisit), rank_get_name(l->rankbuild));
	client_notify(c, buf);
	
	if (c->player->rank == RANK_ADMIN || c->player->globalid == c->player->level->owner)
	{
		unsigned i;
		for (i = 0; i < c->player->level->uservisit.used; i++)
		{
			snprintf(buf, sizeof buf, "Visit permission: %s", playerdb_get_username(c->player->level->uservisit.items[i]));
			client_notify(c, buf);
		}
		
		for (i = 0; i < c->player->level->userbuild.used; i++)
		{
			snprintf(buf, sizeof buf, "Build permission: %s", playerdb_get_username(c->player->level->userbuild.items[i]));
			client_notify(c, buf);
		}
	}

	return false;
}

static const char help_module_load[] =
"/module_load <name>\n"
"Load a module.";

CMD(module_load)
{
	if (params != 2) return true;

	module_load(param[1]);
	return false;
}

static const char help_module_unload[] =
"/module_unload <name>\n"
"Unload a module.";

CMD(module_unload)
{
	if (params != 2) return true;

	module_load(param[1]);
	return false;
}

static const char help_motd[] =
"/motd\n"
"Display the server's Message of the Day.";

CMD(motd)
{
	notify_file(c, "motd.txt");
	return false;
}

static const char help_newlvl[] =
"/newlvl <name> <x> <y> <z> <type> <height_range> <sea_height>\n"
"Create a new level. <y> is height. "
"Type: 0=flat 1=flat/adminium 2=smooth 6=rough";

CMD(newlvl)
{
	if (params != 8) return true;

	const char *name = param[1];
	int x = strtol(param[2], NULL, 10);
	int y = strtol(param[3], NULL, 10);
	int z = strtol(param[4], NULL, 10);
	int t = strtol(param[5], NULL, 10);
	int hr = strtol(param[6], NULL, 10);
	int sh = strtol(param[7], NULL, 10);

	if (x < 16 || y < 16 || z < 16)
	{
		client_notify(c, "Minimum dimension is 16 blocks");
		return false;
	}

	if ((x & (x - 1)) != 0 || (y & (y - 1)) != 0 || (z & (z - 1)) != 0)
	{
		client_notify(c, "Dimension must be power of two");
		return false;
	}

	if (x * y * z > 512*512*512)
	{
		client_notify(c, "Volume too large");
		return false;
	}

	if (!level_get_by_name(name, NULL))
	{
		struct level_t *l = malloc(sizeof *l);
		if (!level_init(l, x, y, z, name, true))
		{
			client_notify(c, "Unable to create level. Too big?");
			free(l);
			return false;
		}

		client_notify(c, "Starting level creation");

		/* Owner is creator */
		l->owner = c->player->globalid;
		l->rankvisit = c->player->rank;
		l->rankbuild = c->player->rank;

		level_gen(l, t, hr, sh);
		level_list_add(&s_levels, l);
	}
	else
	{
		char buf[64];
		snprintf(buf, sizeof buf, "Level '%s' already exists", name);
		client_notify(c, buf);
	}

	return false;
}

static const char help_opglass[] =
"/opglass\n"
"OpGlass not supported. Use /fixed with glass instead.";

CMD(opglass)
{
	client_notify(c, "OpGlass not supported. Use /fixed with glass instead.");
	return false;
}

static const char help_paint[] =
"/paint\n"
"Toggle paint mode. When enabled, any removed block will instead "
"be replaced by the currently held block.";

CMD(paint)
{
	ToggleBit(c->player->flags, FLAG_PAINT);

	char buf[64];
	snprintf(buf, sizeof buf, "Paint %s", HasBit(c->player->flags, FLAG_PAINT) ? "on" : "off");
	client_notify(c, buf);

	return false;
}

static const char help_perbuild[] =
"/perbuild [<rank>|+/-<user>]\n";

CMD(perbuild)
{
	if (params != 2) return true;

	if (c->player->rank < RANK_OP && c->player->globalid != c->player->level->owner)
	{
		client_notify(c, "You do not have permission to change permissions here");
		return false;
	}
	
	if (param[1][0] == '-' || param[1][0] == '+')
	{
		int globalid = playerdb_get_globalid(param[1] + 1, false, NULL);
		if (globalid == -1)
		{
			client_notify(c, "User does not exist");
			return false;
		}
		
		if (param[1][0] == '-')
		{
			user_list_del_item(&c->player->level->userbuild, globalid);
		}
		else
		{
			user_list_add(&c->player->level->userbuild, globalid);
		}
		c->player->level->changed = true;
		client_notify(c, "Build permission set");
	}
	else
	{
		int rank = rank_get_by_name(param[1]);
		if (rank == -1)
		{
			client_notify(c, "Invalid rank");
			return false;
		}

		c->player->level->rankbuild = rank;
		c->player->level->changed = true;
		client_notify(c, "Build permission set");
	}
}

static const char help_pervisit[] =
"/pervisit [<rank>|+/-<user>]\n";

CMD(pervisit)
{
	if (params != 2) return true;

	if (c->player->rank < RANK_OP && c->player->globalid != c->player->level->owner)
	{
		client_notify(c, "You do not have permission to change permissions here");
		return false;
	}
	
	if (param[1][0] == '-' || param[1][0] == '+')
	{
		int globalid = playerdb_get_globalid(param[1] + 1, false, NULL);
		if (globalid == -1)
		{
			client_notify(c, "User does not exist");
			return false;
		}
		
		if (param[1][0] == '-')
		{
			user_list_del_item(&c->player->level->uservisit, globalid);
		}
		else
		{
			user_list_add(&c->player->level->uservisit, globalid);
		}
		c->player->level->changed = true;
		client_notify(c, "Visit permission set");
	}
	else
	{
		int rank = rank_get_by_name(param[1]);
		if (rank == -1)
		{
			client_notify(c, "Invalid rank");
			return false;
		}

		c->player->level->rankvisit = rank;
		c->player->level->changed = true;
		client_notify(c, "Visit permission set");
	}
}


static const char help_players[] =
"/players [<level>]\n"
"List all players connected. If <level> is specified, only players on that level are listed.";

CMD(players)
{
	if (params > 2) return false;

	struct level_t *l = NULL;
	if (params == 2)
	{
		if (!level_get_by_name(param[1], &l))
		{
			client_notify(c, "Unknown level");
			return false;
		}
	}

	player_list(c, l);
	return false;
}

static const char help_replace[] =
"/replace <oldtype> [<newtype>]\n"
"";

CMD(replace)
{
	char buf[64];

	if (params > 3) return true;

	c->player->replace_type = blocktype_get_by_name(param[1]);
	if (c->player->replace_type == BLOCK_INVALID)
	{
		snprintf(buf, sizeof buf, "Unknown block type %s", param[1]);
		return false;
	}

	if (params == 3)
	{
		c->player->cuboid_type = blocktype_get_by_name(param[2]);
		if (c->player->cuboid_type == BLOCK_INVALID)
		{
			snprintf(buf, sizeof buf, "Unknown block type %s", param[2]);
			return false;
		}
	}
	else
	{
		c->player->cuboid_type = BLOCK_INVALID;
	}

	c->player->mode = MODE_REPLACE;
	c->player->cuboid_start = UINT_MAX;

	snprintf(buf, sizeof buf, "Place corners of cuboid");
	client_notify(c, buf);

	return false;
}

static const char help_resetlvl[] =
"/resetlvl <type> <height_range> <sea_height>\n"
"";

CMD(resetlvl)
{
	if (params != 4) return true;

	struct level_t *l = c->player->level;

	if (c->player->rank < RANK_OP && c->player->globalid != l->owner)
	{
		client_notify(c, "You do not have permission to reset this level.");
		return false;
	}

	int t = strtol(param[1], NULL, 10);
	int hr = strtol(param[2], NULL, 10);
	int sh = strtol(param[3], NULL, 10);

	unsigned i;
	for (i = 0; i < MAX_CLIENTS_PER_LEVEL; i++)
	{
		struct client_t *c = l->clients[i];
		if (c == NULL) continue;
		c->waiting_for_level = true;
	}

	level_gen(l, t, hr, sh);
}

static const char help_rules[] =
"/rules\n"
"Display the server rules.";

CMD(rules)
{
	notify_file(c, "rules.txt");
	return false;
}

static const char help_setrank[] =
"/setrank <name> <rank>\n"
"Sets a user's rank.";

CMD(setrank)
{
	int oldrank;
	int newrank;
	struct player_t *p;

	if (params != 3) return true;

	newrank = rank_get_by_name(param[2]);
	if (newrank == -1 && c->player->rank == RANK_ADMIN)
	{
		client_notify(c, "Invalid rank: banned guest builder advbuilder op admin");
		return false;
	}
	if ((newrank == -1 || newrank >= RANK_OP) && c->player->rank == RANK_OP)
	{
		client_notify(c, "Invalid rank: banned guest builder advbuilder");
		return false;
	}

	oldrank = playerdb_get_rank(param[1]);
	if (oldrank == newrank)
	{
		client_notify(c, "User already at rank");
		return false;
	}
	if (c->player->rank != RANK_ADMIN && oldrank == RANK_ADMIN)
	{
		client_notify(c, "Cannot demote admin");
		return false;
	}

	playerdb_set_rank(param[1], newrank);
	p = player_get_by_name(param[1]);
	if (p != NULL)
	{
		p->rank = newrank;
	}

	char buf[64];
	snprintf(buf, sizeof buf, "Rank set to %s for %s", rank_get_name(newrank), p->username);
	net_notify_all(buf);

	return false;
}

static const char help_setspawn[] =
"/setspawn\n"
"Sets the spawn location for the level to your current position.";

CMD(setspawn)
{
	if (c->player->rank < RANK_OP && c->player->globalid != c->player->level->owner)
	{
		client_notify(c, "You do not have permission to change spawn here");
		return false;
	}

	c->player->level->spawn = c->player->pos;
	c->player->level->changed = true;

	client_notify(c, "Spawn set to current position");

	return false;
}

static const char help_spawn[] =
"/spawn\n"
"Move back the level spawn location.";

CMD(spawn)
{
	c->player->pos = c->player->level->spawn;
	client_add_packet(c, packet_send_teleport_player(0xFF, &c->player->pos));

	return false;
}

static const char help_solid[] =
"/solid\n"
"Toggle solid mode. Any block placed will be replaced with adminium.";

CMD(solid)
{
	player_toggle_mode(c->player, MODE_PLACE_SOLID);

	char buf[64];
	snprintf(buf, sizeof buf, "Solid %s", (c->player->mode == MODE_PLACE_SOLID) ? "on" : "off");
	client_notify(c, buf);

	return false;
}

static const char help_summon[] =
"/summon <user>\n"
"Summons the specified <user> to your current location.";

CMD(summon)
{
	char buf[64];

	if (params != 2) return true;

	struct player_t *p = player_get_by_name(param[1]);
	if (p == NULL)
	{
		snprintf(buf, sizeof buf, "%s is offline", param[1]);
		client_notify(c, buf);
		return false;
	}
	if (p->level != c->player->level)
	{
		snprintf(buf, sizeof buf, "%s is on '%s'", param[1], p->level->name);
		client_notify(c, buf);
		return false;
	}

	p->pos = c->player->pos;
	client_add_packet(p->client, packet_send_teleport_player(0xFF, &p->pos));

	snprintf(buf, sizeof buf, "You were summoned by %s", c->player->username);
	client_notify(p->client, buf);
	snprintf(buf, sizeof buf, "%s summoned", p->username);
	client_notify(c, buf);

	return false;
}

static char s_pattern[256];
static int undo_filename_filter(const struct dirent *d)
{
	return strncmp(d->d_name, s_pattern, strlen(s_pattern)) == 0;
}

static const char help_teleporter[] =
"/teleporter <name> [<dest> [<level>]]\n";

CMD(teleporter)
{
	if (params < 2 || params > 5) return true;

	level_set_teleporter(c->player->level, param[1], &c->player->pos, param[2], param[3]);

	return false;
}

static const char help_time[] =
"/time\n"
"Displays time at the server.";

CMD(time)
{
	struct timeval tv;
	time_t curtime;

	gettimeofday(&tv, NULL);
	curtime = tv.tv_sec;

	char buf[64];
	strftime(buf, sizeof buf, "Server time is %H:%M:%S", localtime(&curtime));
	client_notify(c, buf);

	return false;
}

static const char help_tp[] =
"/tp <user>\n"
"Teleport to the <user> specified.";

CMD(tp)
{
	char buf[64];

	if (params != 2) return true;

	const struct player_t *p = player_get_by_name(param[1]);
	if (p == NULL)
	{
		snprintf(buf, sizeof buf, "%s is offline", param[1]);
		client_notify(c, buf);
		return false;
	}
	if (p->level != c->player->level)
	{
		snprintf(buf, sizeof buf, "%s is on '%s'", param[1], p->level->name);
		client_notify(c, buf);
		return false;
	}

	client_add_packet(c, packet_send_teleport_player(0xFF, &p->pos));

	return false;
}

static const char help_undo[] =
"/undo <user> <level> [<time>]\n"
"Undo user actions for the specified <user> and <level>.";

CMD(undo)
{
	char buf[64];

	if (params < 3 || params > 4) return true;

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
			return false;
		}

		for (i = 0; i < n; i++)
		{
			struct stat statbuf;
			//if (stat(namelist[i]->d_name, &statbuf) == 0)
			{
				int undo_actions = statbuf.st_size / (sizeof (unsigned) + sizeof (struct block_t));
				char buf2[64];
				snprintf(buf2, sizeof buf2, "%s (%d)", namelist[i]->d_name + strlen(s_pattern), undo_actions);

				size_t len = strlen(buf2) + (i < n - 1 ? 2 : 0);
				if (len >= sizeof buf - (bufp - buf))
				{
					client_notify(c, buf);
					bufp = buf;
				}

				strcpy(bufp, buf2);
				bufp += len;

				if (i < n - 1) strcpy(bufp - 2, ", ");
			}

			free(namelist[i]);
		}

		client_notify(c, buf);
		return false;
	}

	player_undo(c, param[1], param[2], param[3]);
	return false;
}

static const char help_uptime[] =
"/uptime\n"
"Display server uptime and load.";

CMD(uptime)
{
	time_t uptime = time(NULL) - g_server.start_time;
	char buf[64], *bufp = buf;

	int seconds = uptime % 60;
	int minutes = uptime / 60 % 60;
	int hours   = uptime / 3600 % 24;
	int days	= uptime / 86400;

	bufp += snprintf(bufp, (sizeof buf) - (bufp - buf), "Server uptime is ");
	if (days > 0)
	{
		bufp += snprintf(bufp, (sizeof buf) - (bufp - buf), "%d days ", days);
	}
	if (hours > 0)
	{
		bufp += snprintf(bufp, (sizeof buf) - (bufp - buf), "%d hours ", hours);
	}
	if (minutes > 0)
	{
		bufp += snprintf(bufp, (sizeof buf) - (bufp - buf), "%d minutes ", minutes);
	}
	if (seconds > 0)
	{
		bufp += snprintf(bufp, (sizeof buf) - (bufp - buf), "%d seconds ", seconds);
	}

	client_notify(c, buf);

	snprintf(buf, sizeof buf, "Server CPU usage is %f%%", g_server.cpu_time);
	client_notify(c, buf);

	struct rusage usage;
	if (getrusage(RUSAGE_SELF, &usage) == 0)
	{
		snprintf(buf, sizeof buf, "Server memory usage: %lu", usage.ru_ixrss);
		client_notify(c, buf);
	}

	return false;
}

static const char help_water[] =
"/water\n"
"Toggle water mode. Any block placed will be converted to static water.";

CMD(water)
{
	player_toggle_mode(c->player, MODE_PLACE_WATER);

	char buf[64];
	snprintf(buf, sizeof buf, "Water %s", (c->player->mode == MODE_PLACE_WATER) ? "on" : "off");
	client_notify(c, buf);

	return false;
}

static const char help_whois[] =
"/whois <user>\n"
"Display information about the specified <user>.";

CMD(whois)
{
	char buf[64];

	if (params != 2) return true;

	struct player_t *p = player_get_by_name(param[1]);
	if (p == NULL)
	{
		snprintf(buf, sizeof buf, "%s is offline", param[1]);
		client_notify(c, buf);
		return false;
	}

	//if (c->rank >= RANK_OP)
	//{
		//snprintf(buf, sizeof buf, "%s is on '%s'"
	//}
	//client_notify(c, buf);

	return false;
}

struct command_t s_commands[] = {
	{ "adminrules", RANK_GUEST, &cmd_adminrules, help_adminrules },
	{ "afk", RANK_GUEST, &cmd_afk, help_afk },
	{ "ban", RANK_OP, &cmd_ban, help_ban },
	{ "bind", RANK_ADV_BUILDER, &cmd_bind, help_bind },
	{ "commands", RANK_BANNED, &cmd_commands, help_commands },
	{ "cuboid", RANK_ADV_BUILDER, &cmd_cuboid, help_cuboid },
	{ "disown", RANK_OP, &cmd_disown, help_disown },
	{ "z", RANK_ADV_BUILDER, &cmd_cuboid, help_cuboid },
	{ "exit", RANK_ADMIN, &cmd_exit, help_exit },
	{ "fixed", RANK_OP, &cmd_fixed, help_fixed },
	{ "filter", RANK_OP, &cmd_filter, help_filter },
	{ "follow", RANK_OP, &cmd_follow, help_follow },
	{ "goto", RANK_GUEST, &cmd_goto, help_goto },
	{ "help", RANK_GUEST, &cmd_help, help_help },
	{ "hide", RANK_OP, &cmd_hide, help_hide },
	{ "home", RANK_GUEST, &cmd_home, help_home },
	{ "identify", RANK_GUEST, &cmd_identify, help_identify },
	{ "info", RANK_BUILDER, &cmd_info, help_info },
	{ "kick", RANK_OP, &cmd_kick, help_kick },
	{ "lava", RANK_BUILDER, &cmd_lava, help_lava },
	{ "levels", RANK_GUEST, &cmd_levels, help_levels },
	{ "lvlowner", RANK_OP, &cmd_lvlowner, help_lvlowner },
	{ "mapinfo", RANK_GUEST, &cmd_mapinfo, help_mapinfo },
	{ "module_load", RANK_ADMIN, &cmd_module_load, help_module_load },
	{ "module_unload", RANK_ADMIN, &cmd_module_unload, help_module_unload },
	{ "motd", RANK_BANNED, &cmd_motd, help_motd },
	{ "newlvl", RANK_ADV_BUILDER, &cmd_newlvl, help_newlvl },
	{ "opglass", RANK_OP, &cmd_opglass, help_opglass },
	{ "paint", RANK_BUILDER, &cmd_paint, help_paint },
	{ "perbuild", RANK_BUILDER, &cmd_perbuild, help_perbuild },
	{ "pervisit", RANK_BUILDER, &cmd_pervisit, help_pervisit },
	{ "players", RANK_GUEST, &cmd_players, help_players },
	{ "replace", RANK_ADV_BUILDER, &cmd_replace, help_replace },
	{ "resetlvl", RANK_GUEST, &cmd_resetlvl, help_resetlvl },
	{ "rules", RANK_BANNED, &cmd_rules, help_rules },
	{ "setrank", RANK_OP, &cmd_setrank, help_setrank },
	{ "setspawn", RANK_BUILDER, &cmd_setspawn, help_setspawn },
	{ "spawn", RANK_GUEST, &cmd_spawn, help_spawn },
	{ "solid", RANK_OP, &cmd_solid, help_solid },
	{ "summon", RANK_OP, &cmd_summon, help_summon },
	{ "teleporter", RANK_ADV_BUILDER, &cmd_teleporter, help_teleporter },
	{ "time", RANK_GUEST, &cmd_time, help_time },
	{ "tp", RANK_BUILDER, &cmd_tp, help_tp },
	{ "undo", RANK_OP, &cmd_undo, help_undo },
	{ "uptime", RANK_GUEST, &cmd_uptime, help_uptime },
	{ "water", RANK_BUILDER, &cmd_water, help_water },
	{ "whois", RANK_GUEST, &cmd_whois, help_whois },
	{ NULL, -1, NULL, NULL },
};

bool command_process(struct client_t *client, int params, const char **param)
{
	struct command_t *comp = s_commands;
	for (; comp->command != NULL; comp++)
	{
		if (client->player->rank >= comp->rank && strcasecmp(param[0], comp->command) == 0)
		{
			if (comp->func(client, params, param))
			{
				client_notify(client, comp->help);
			}
			return true;
		}
	}

	return false;
}
