#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include "client.h"
#include "level.h"
#include "packet.h"
#include "player.h"
#include "playerdb.h"
#include "mcc.h"
#include "network.h"
#include "module.h"
#include "undodb.h"

static const char s_on[] = TAG_RED "on";
static const char s_off[] = TAG_GREEN "off";

void notify_file(struct client_t *c, const char *filename)
{
	char buf[1024];
	FILE *f = fopen(filename, "r");
	if (f == NULL)
	{
		LOG("No %s found\n", filename);
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

struct notify_t
{
	struct client_t *c;
	char buf[64];
	char *bufp;
};

void notify_multipart(const char *text, void *arg)
{
	struct notify_t *t = arg;

	size_t len = strlen(text);
	if (len >= sizeof t->buf - (t->bufp - t->buf))
	{
		client_notify(t->c, t->buf);
		memset(t->buf, 0, sizeof t->buf);
		t->bufp = t->buf;
	}

	strcpy(t->bufp, text);
	t->bufp += len;
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
"/ban <user> [<message>]\n"
"Ban user";

CMD(ban)
{
	char buf[64];
	struct player_t *p;
	int oldrank;

	if (params < 2) return true;

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
		sprintf(p->colourusername, "&%x%s", rank_get_colour(p->rank), p->username);
	}

	if (params == 2)
	{
		snprintf(buf, sizeof buf, "%s banned by %s", param[1], c->player->username);
	}
	else
	{
		unsigned i;
		snprintf(buf, sizeof buf, "%s banned (", param[1]);
		for (i = 2; i < params; i++)
		{
			strcat(buf, param[i]);
			strcat(buf, i < params - 1 ? " " : ")");
		}
	}
	net_notify_all(buf);
	return false;
}

static const char help_banip[] =
"/banip <ip>\n"
"Ban an IP address.";

CMD(banip)
{
	if (params != 2) return true;

	playerdb_ban_ip(param[1]);

	unsigned i;
	for (i = 0; i < s_clients.used; i++)
	{
		struct client_t *client = s_clients.items[i];
		if (strcmp(client->ip, param[1]) == 0)
		{
			net_close(client, "Banned");
		}
	}

	LOG("%s banned IP %s\n", c->player->username, param[1]);
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
			if (i == BLOCK_INVALID)
			{
				snprintf(buf, sizeof buf, "Unknown blocktype %s", param[1]);
			}
			else
			{
				c->player->bindings[i] = i;
				snprintf(buf, sizeof buf, "Unbound %s", blocktype_get_name(i));
			}

			client_notify(c, buf);
			break;

		case 3:
			i = blocktype_get_by_name(param[1]);
			j = blocktype_get_by_name(param[2]);
			if ((i == ADMINIUM || j == ADMINIUM) && c->player->rank < RANK_OP)
			{
				client_notify(c, "You do not have permission to place adminium.");
				return false;
			}

			if (i == BLOCK_INVALID)
			{
				snprintf(buf, sizeof buf, "Unknown blocktype %s", param[1]);
			}
			else if (j == BLOCK_INVALID)
			{
				snprintf(buf, sizeof buf, "Unknown blocktype %s", param[2]);
			}
			else
			{
				c->player->bindings[i] = j;
				snprintf(buf, sizeof buf, "Bound %s to %s", blocktype_get_name(i), blocktype_get_name(j));
			}

			client_notify(c, buf);
			break;
	}
	return false;
}

static const char help_blocks[] =
"/blocks\n"
"List all block types available.";

CMD(blocks)
{
	blocktype_list(c);

	return false;
}

static const char help_commands[] =
"/commands\n"
"List all commands available to you.";

CMD(commands)
{
	char buf[64];
	char *bufp;

	strcpy(buf, "Commands: ");
	bufp = buf + strlen(buf);

	struct command_t *comp = s_commands;
	for (; comp->command != NULL; comp++)
	{
		if (c->player->rank < comp->rank) continue;

		char buf2[64];
		snprintf(buf2, sizeof buf2, "%s%s", comp->command, (comp + 1)->command == NULL ? "" : ", ");
		size_t len = strlen(buf2);
		if (len >= sizeof buf - (bufp - buf))
		{
			client_notify(c, buf);
			memset(buf, 0, sizeof buf);
			bufp = buf;
		}

		strcpy(bufp, buf2);
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
			snprintf(buf, sizeof buf, "Unknown block type %s", param[1]);
			return false;
		}
		else if (c->player->cuboid_type == ADMINIUM && c->player->rank < RANK_OP)
		{
			snprintf(buf, sizeof buf, "You do not have permission to place adminium");
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

static const char help_dellvl[] =
"/dellvl <level>\n"
"Delete a level.";

CMD(dellvl)
{
	if (params != 2) return true;

	if (level_is_loaded(param[1]))
	{
		struct level_t *l;
		level_get_by_name(param[1], &l);

		struct level_t *l2;
		level_get_by_name("main", &l2);

		unsigned i;
		for (i = 0; i < s_clients.used; i++)
		{
			struct client_t *c = s_clients.items[i];
			if (c->player == NULL) continue;
			if (c->player->level == l)
			{
				c->player->new_level = l2;
				client_notify(c, "Moving to main, level deleted.");
			}
			if (c->player->new_level == l)
			{
				c->player->new_level = c->player->level;
				c->waiting_for_level = false;
				client_notify(c, "Level change aborted, level deleted.");
				continue;
			}
		}

		cuboid_remove_for_level(l);

		l->delete = true;
	}

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
	snprintf(buf, sizeof buf, "Disown %s", HasBit(c->player->flags, FLAG_DISOWN) ? s_on : s_off);
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

	c->waiting_for_level = true;

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
	snprintf(buf, sizeof buf, "Fixed %s", HasBit(c->player->flags, FLAG_PLACE_FIXED) ? s_on : s_off);
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

	snprintf(buf, sizeof buf, "Hidden %s", c->hidden ? s_on : s_off);
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
		player_change_level(c->player, l);
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
	snprintf(buf, sizeof buf, "Hidden %s", c->hidden ? s_on : s_off);
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
		l->rankown   = RANK_ADMIN;
		user_list_add(&l->uservisit, l->owner);
		user_list_add(&l->userbuild, l->owner);
		user_list_add(&l->userown,   l->owner);

		level_gen(l, 0, l->y / 2, l->y / 2);
		level_list_add(&s_levels, l);
	}

	/* Don't resend the level if player is already on it */
	player_change_level(c->player, l);

	return false;
}

static const char help_hookattach[] =
"/hookattach <hook>";

CMD(hookattach)
{
	if (params != 2) return true;

	if (level_hook_attach(c->player->level, param[1]))
	{
		client_notify(c, "Hook attached");
	}
	else
	{
		client_notify(c, "Hook not found");
	}

	return false;
}

static const char help_hookdetach[] =
"/hookdetach <hook>";

CMD(hookdetach)
{
	if (params != 2) return true;

	if (level_hook_detach(c->player->level, param[1]))
	{
		client_notify(c, "Hook detached");
	}
	else
	{
		client_notify(c, "Hook not attached");
	}

	return false;
}

static const char help_identify[] =
"/identify <password>\n"
"Identify your account so that you may use privileged commands.";

CMD(identify)
{
	if (params != 2) return true;

	if (playerdb_password_check(c->player->username, param[1]) != 1)
	{
		client_notify(c, "Invalid password.");
		LOG("Identify %s: invalid password\n", c->player->username);
		return false;
	}

	int oldrank = c->player->rank;

	c->player->rank = playerdb_get_rank(c->player->username);
	client_notify(c, "Identified successfully.");
	LOG("Identify %s: success\n", c->player->username);

	playerdb_log_identify(c->player->globalid, 1);

	if (oldrank >= RANK_OP)
	{
		/* Remove op status */
		client_add_packet(c, packet_send_update_user_type(0x00));
	}
	if (c->player->rank >= RANK_OP)
	{
		/* Give op status */
		client_add_packet(c, packet_send_update_user_type(0x64));
	}

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
	snprintf(buf, sizeof buf, "Block info %s", (c->player->mode == MODE_INFO) ? s_on : s_off);
	client_notify(c, buf);

	return false;
}

static const char help_instant[] =
"/instant\n"
"Toggle instant mode for the current level. Cuboid and physics will not send updates to the client "
"until instant is turned off again.";

CMD(instant)
{
	struct level_t *l = c->player->level;

	if (l == NULL) return false;

	if (l->instant)
	{
		l->instant = false;

		unsigned i;
		for (i = 0; i < MAX_CLIENTS_PER_LEVEL; i++)
		{
			if (l->clients[i] == NULL) continue;

			l->clients[i]->waiting_for_level = true;
			client_notify(l->clients[i], "Instant mode turned off");
		}
	}
	else
	{
		l->instant = true;

		unsigned i;
		for (i = 0; i < MAX_CLIENTS_PER_LEVEL; i++)
		{
			if (l->clients[i] == NULL) continue;
			client_notify(l->clients[i], "Instant mode turned on");
		}
	}

	return false;
}

static const char help_kick[] =
"/kick <user> [<message>]\n"
"Kicks the specified <user> from the server.";

CMD(kick)
{
	if (params < 2) return true;

	char buf[128];

	struct player_t *p = player_get_by_name(param[1]);
	if (p == NULL)
	{
		snprintf(buf, sizeof buf, "%s is offline", param[1]);
		client_notify(c, buf);
		return false;
	}

	if (params == 2)
	{
		snprintf(buf, sizeof buf, "kicked by %s", c->player->username);
	}
	else
	{
		unsigned i;
		snprintf(buf, sizeof buf, "kicked (");
		for (i = 2; i < params; i++)
		{
			strcat(buf, param[i]);
			strcat(buf, i < params - 1 ? " " : ")");
		}
	}
	net_close(p->client, buf);

	return false;
}

static const char help_kickban[] =
"/kickban <user> [<message>]\n"
"Kicks the specified <user> from the server.";

CMD(kickban)
{
	if (params < 2) return true;

	char buf[128];
	struct player_t *p;

	enum rank_t oldrank = playerdb_get_rank(param[1]);
	if (c->player->rank != RANK_ADMIN && oldrank >= RANK_OP)
	{
		client_notify(c, "Cannot ban op or admin");
		return false;
	}

	playerdb_set_rank(param[1], RANK_BANNED);
	p = player_get_by_name(param[1]);
	if (p == NULL)
	{
		snprintf(buf, sizeof buf, "%s is offline", param[1]);
		client_notify(c, buf);
		return false;
	}

	if (params == 2)
	{
		snprintf(buf, sizeof buf, "kickbanned by %s", c->player->username);
	}
	else
	{
		unsigned i;
		snprintf(buf, sizeof buf, "kickbanned (");
		for (i = 2; i < params; i++)
		{
			strcat(buf, param[i]);
			strcat(buf, i < params - 1 ? " " : ")");
		}
	}
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
	snprintf(buf, sizeof buf, "Lava %s", (c->player->mode == MODE_PLACE_LAVA) ? s_on : s_off);
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

		if (i > 0 && strcmp(namelist[i - 1]->d_name, namelist[i]->d_name) != 0 && strstr(namelist[i]->d_name, "_home") == NULL)
		{
			bool loaded = level_is_loaded(namelist[i]->d_name);

			char buf2[64];
			snprintf(buf2, sizeof buf2, "%s%s%s%s", loaded ? TAG_GREEN : "", namelist[i]->d_name, (loaded && i < n - 1) ? TAG_WHITE : "", (i < n - 1) ? ", " : "");

			size_t len = strlen(buf2);
			if (len >= sizeof buf - (bufp - buf))
			{
				client_notify(c, buf);
				memset(buf, 0, sizeof buf);
				bufp = buf;
			}

			strcpy(bufp, buf2);
			bufp += len;
		}
		if (i > 0)
		{
			free(namelist[i - 1]);
		}
	}

	free(namelist[i - 1]);

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
		snprintf(buf, sizeof buf, "Own permission: %s", rank_get_name(l->rankown));
		client_notify(c, buf);
	
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

		for (i = 0; i < c->player->level->userown.used; i++)
		{
			snprintf(buf, sizeof buf, "Own permission: %s", playerdb_get_username(c->player->level->userown.items[i]));
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

	struct module_t *m = module_get_by_name(param[1]);
	if (m == 0)
	{
		client_notify(c, "Module not loaded");
		return false;
	}

	module_unload(m);
	return false;
}

static const char help_modules[] =
"/modules\n"
"List loaded modules.";

CMD(modules)
{
	char buf[64];
	char *bufp;
	unsigned i;

	strcpy(buf, "Modules: ");
	bufp = buf + strlen(buf);

	for (i = 0; i < s_modules.used; i++)
	{
		const char *name = s_modules.items[i]->name;

		char buf2[64];
		snprintf(buf2, sizeof buf2, "%s%s", name, (i < s_modules.used - 1) ? ", " : "");

		size_t len = strlen(buf2);
		if (len >= sizeof buf - (bufp - buf))
		{
			client_notify(c, buf);
			memset(buf, 0, sizeof buf);
			bufp = buf;
		}

		strcpy(bufp, buf2);
		bufp += len;
	}

	client_notify(c, buf);

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
		client_notify(c, "Dimension must be power of two (16, 32, 64, 128, 256, 512)");
		return false;
	}

	if (x * y * z > 512*512*512)
	{
		client_notify(c, "Volume too large");
		return false;
	}

	if (t < 0 || t > 6)
	{
		client_notify(c, "Type must be from 0 to 6 only");
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
		l->rankown   = c->player->rank;

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
	snprintf(buf, sizeof buf, "Paint %s", HasBit(c->player->flags, FLAG_PAINT) ? s_on : s_off);
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

	return false;
}

static const char help_perown[] =
"/perown [<rank>|+/-<user>]\n";

CMD(perown)
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
			user_list_del_item(&c->player->level->userown, globalid);
		}
		else
		{
			user_list_add(&c->player->level->userown, globalid);
		}
		c->player->level->changed = true;
		client_notify(c, "Own permission set");
	}
	else
	{
		int rank = rank_get_by_name(param[1]);
		if (rank == -1)
		{
			client_notify(c, "Invalid rank");
			return false;
		}

		c->player->level->rankown = rank;
		c->player->level->changed = true;
		client_notify(c, "Own permission set");
	}

	return false;
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

	return false;
}

static const char help_physics[] =
"/physics\n"
"Show physics statistics for this level.";

CMD(physics)
{
	char buf[64];
	struct level_t *l = c->player->level;

	if (params == 2)
	{
		if (strcasecmp(param[1], "pause") == 0)
		{
			l->physics_pause = true;
			level_notify_all(l, TAG_YELLOW "Physics paused");
		}
		else if (strcasecmp(param[1], "resume") == 0)
		{
			l->physics_pause = false;
			level_notify_all(l, TAG_YELLOW "Physics resumed");
		}
		else if (strcasecmp(param[1], "reset") == 0)
		{
			l->physics.used = 0;
			l->physics2.used = 0;
			l->updates.used = 0;
			l->physics_iter = 0;
			l->updates_iter = 0;
			l->physics_done = 0;
			level_notify_all(l, TAG_YELLOW "Physics reset");
		}
		return false;
	}

	snprintf(buf, sizeof buf, "Physics runtime: %ums  count: %u", l->physics_runtime_last, l->physics_count_last);
	client_notify(c, buf);
	snprintf(buf, sizeof buf, "Updates runtime: %ums  count: %u", l->updates_runtime_last, l->updates_count_last);
	client_notify(c, buf);

	return false;
}

static const char help_place[] =
"/place <type> [<x> <y> <z>]\n"
"Place a block at the specified coordinates.";

CMD(place)
{
	char buf[64];

	if (params != 2 || params > 5) return true;

	if (c->player->level == NULL || c->waiting_for_level)
	{
		client_notify(c, "You must be on a level to use this command");
		return false;
	}

	enum blocktype_t type = blocktype_get_by_name(param[1]);
	if (type == BLOCK_INVALID)
	{
		snprintf(buf, sizeof buf, "Unknown block type %s", param[1]);
		client_notify(c, buf);
		return false;
	}

	int16_t x, y, z;
	if (params == 2)
	{
		x = c->player->pos.x / 32;
		y = c->player->pos.y / 32 - 1;
		z = c->player->pos.z / 32;
	}
	else
	{
		x = strtol(param[2], NULL, 10);
		y = strtol(param[3], NULL, 10);
		z = strtol(param[4], NULL, 10);
	}

	if (x < 0 || y < 0 || z < 0 || x >= c->player->level->x || y >= c->player->level->y || z >= c->player->level->z)
	{
		client_notify(c, "Coordinates out of range");
		return false;
	}

	level_change_block(c->player->level, c, x, y, z, 1, type, false);
	return false;
}

static const char help_players[] =
"/players [<level>]\n"
"List all players connected. If <level> is specified, only players on that level are listed.";

CMD(players)
{
	if (params > 2) return true;

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

	if (params != 2 && params != 3) return true;

	c->player->replace_type = blocktype_get_by_name(param[1]);
	if (c->player->replace_type == BLOCK_INVALID)
	{
		snprintf(buf, sizeof buf, "Unknown block type %s", param[1]);
		return false;
	}
	else if (c->player->replace_type == ADMINIUM && c->player->rank < RANK_OP)
	{
		snprintf(buf, sizeof buf, "You do not have permission to replace adminium");
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
		else if (c->player->cuboid_type == ADMINIUM && c->player->rank < RANK_OP)
		{
			snprintf(buf, sizeof buf, "You do not have permission to place adminium");
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

static const char help_replaceall[] =
"/replaceall <oldtype> <newtype>\n"
"";

CMD(replaceall)
{
	char buf[64];

	if (params != 3) return true;

	enum blocktype_t oldtype = blocktype_get_by_name(param[1]);
	if (oldtype == BLOCK_INVALID)
	{
		snprintf(buf, sizeof buf, "Unknown block type %s", param[1]);
		return false;
	}
	else if (oldtype == ADMINIUM && c->player->rank < RANK_OP)
	{
		snprintf(buf, sizeof buf, "You do not have permission to replace adminium");
		return false;
	}

	enum blocktype_t newtype = blocktype_get_by_name(param[2]);
	if (newtype == BLOCK_INVALID)
	{
		snprintf(buf, sizeof buf, "Unknown block type %s", param[2]);
		return false;
	}
	else if (newtype == ADMINIUM && c->player->rank < RANK_OP)
	{
		snprintf(buf, sizeof buf, "You do not have permission to place adminium");
		return false;
	}

	snprintf(buf, sizeof buf, "Replacing all %s with %s on %s", param[1], param[2], c->player->level->name);
	client_notify(c, buf);

	struct level_t *l = c->player->level;
	unsigned end = level_get_index(l, l->x - 1, l->y - 1, l->z - 1);

	level_cuboid(c->player->level, 0, end, oldtype, newtype, c->player);

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

	if (t < 0 || t > 6)
	{
		client_notify(c, "Type must be from 0 to 6 only");
		return false;
	}

	unsigned i;
	for (i = 0; i < MAX_CLIENTS_PER_LEVEL; i++)
	{
		struct client_t *c = l->clients[i];
		if (c == NULL) continue;
		c->waiting_for_level = true;
	}

	cuboid_remove_for_level(l);

	level_gen(l, t, hr, sh);
	return false;
}

static const char help_rules[] =
"/rules\n"
"Display the server rules.";

CMD(rules)
{
	notify_file(c, "rules.txt");
	return false;
}

static const char help_setpassword[] =
"/setpassword <password> <password> [<oldpassword>]\n";

CMD(setpassword)
{
	if (params != 3 && params != 4) return true;

	if (strcmp(param[1], param[2]) != 0)
	{
		client_notify(c, "Password must match.");
		return false;
	}

	if (playerdb_password_check(c->player->username, params == 4 ? param[3] : "") != 1)
	{
		client_notify(c, "Invalid password.");
		LOG("Setpassword %s: invalid password\n", c->player->username);
		return false;
	}

	playerdb_set_password(c->player->username, param[1]);
	client_notify(c, "Password set.");
	LOG("Setpassword %s: success\n", c->player->username);
	return false;
}

static const char help_setposinterval[] =
"/setposinterval <interval>\n"
"Set player position update interval in ms. Minimum is 40ms.";

CMD(setposinterval)
{
	if (params != 2) return true;

	int interval = atoi(param[1]);
	interval /= 40;
	interval *= 40;
	if (interval < 40) interval = 40;

	g_server.pos_interval = interval;

	char buf[64];
	snprintf(buf, sizeof buf, "Player position update interval set to %d ms", g_server.pos_interval);
	client_notify(c, buf);
	LOG("%s\n", buf);

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
	if (c->player->rank != RANK_ADMIN && oldrank >= RANK_OP)
	{
		client_notify(c, "Cannot demote op or admin");
		return false;
	}

	playerdb_set_rank(param[1], newrank);
	p = player_get_by_name(param[1]);
	if (p != NULL)
	{
		p->rank = newrank;
		sprintf(p->colourusername, "&%x%s", rank_get_colour(p->rank), p->username);

		if (oldrank >= RANK_OP)
		{
			/* Remove op status */
			client_add_packet(p->client, packet_send_update_user_type(0x00));
		}
		if (newrank >= RANK_OP)
		{
			/* Give op status */
			client_add_packet(p->client, packet_send_update_user_type(0x64));
		}
	}

	char buf[64];
	snprintf(buf, sizeof buf, "Rank set to %s for %s", rank_get_name(newrank), param[1]);
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
	player_teleport(c->player, &c->player->level->spawn, true);

	return false;
}

static const char help_solid[] =
"/solid\n"
"Toggle solid mode. Any block placed will be replaced with adminium.";

CMD(solid)
{
	player_toggle_mode(c->player, MODE_PLACE_SOLID);

	char buf[64];
	snprintf(buf, sizeof buf, "Solid %s", (c->player->mode == MODE_PLACE_SOLID) ? s_on : s_off);
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

	player_teleport(p, &c->player->pos, true);

	snprintf(buf, sizeof buf, "You were summoned by %s", c->player->username);
	client_notify(p->client, buf);
	snprintf(buf, sizeof buf, "%s summoned", p->username);
	client_notify(c, buf);

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

	player_teleport(c->player, &p->pos, true);

	return false;
}

static const char help_unbanip[] =
"/unbanip <ip>\n"
"Unban an IP address.";

CMD(unbanip)
{
	if (params != 2) return true;

	playerdb_unban_ip(param[1]);

	LOG("%s unbanned IP %s\n", c->player->username, param[1]);
	return false;
}

static void undo_show(int16_t x, int16_t y, int16_t z, int oldtype, int olddata, void *arg)
{
	struct client_t *client = arg;
	struct level_t *l = client->player->level;
	unsigned index = level_get_index(l, x, y, z);
	struct block_t *b = &l->blocks[index];
	struct block_t backup = *b;
	b->type = oldtype;
	b->data = olddata;
	client_add_packet(client, packet_send_set_block(x, y, z, convert(l, index, b)));
	*b = backup;
}

static void undo_real(int16_t x, int16_t y, int16_t z, int oldtype, int olddata, void *arg)
{
	struct client_t *client = arg;
	struct level_t *l = client->player->level;
	unsigned index = level_get_index(l, x, y, z);
	struct block_t *b = &l->blocks[index];

	b->type = oldtype;
	b->data = olddata;

	unsigned i;
	for (i = 0; i < s_clients.used; i++)
	{
		struct client_t *c = s_clients.items[i];
		if (c->player == NULL) continue;
		if (c->player->level == l)
		{
			client_add_packet(c, packet_send_set_block(x, y, z, convert(l, index, b)));
		}
	}

	l->changed = true;
}

static const char help_undo[] =
"/undo <level> [<user> [<count> [commit]]]\n"
"Undo user actions for the specified <user> and <level>.";

CMD(undo)
{
	struct notify_t n;

	if (params < 2 || params > 5) return true;

	struct level_t *l;
	if (!level_get_by_name(param[1], &l))
	{
		client_notify(c, "Cannot load level.");
		return false;
	}

	if (l->undo == NULL) l->undo = undodb_init(l->name);
	if (l->undo == NULL)
	{
		client_notify(c, "Undo not available for level.");
		return false;
	}

	n.c = c;
	memset(n.buf, 0, sizeof n.buf);

	if (params == 2)
	{
		strcpy(n.buf, "Undo log: ");
		n.bufp = n.buf + strlen(n.buf);
		undodb_query(l->undo, &notify_multipart, &n);
		client_notify(c, n.buf);
		return false;
	}
	
	int globalid = playerdb_get_globalid(param[2], false, NULL);
	if (globalid == -1)
	{
		client_notify(c, "Unknown username.");
		return false;
	}

	if (params == 3)
	{
		strcpy(n.buf, "Undo log: ");
		n.bufp = n.buf + strlen(n.buf);
		undodb_query_player(l->undo, globalid, &notify_multipart, &n);
		client_notify(c, n.buf);
		return false;
	}

	if (c->player->level != l)
	{
		client_notify(c, "Cannot preview undo on a different level.");
		return false;
	}

	//,...,...,...client_add_packet(client, packet_send_set_block(x, y, z, pt));
	int limit = strtol(param[3], NULL, 10);

	undodb_undo_player(l->undo, globalid, limit, params == 4 ? &undo_show : &undo_real, c);
	return false;

	//player_undo(c, param[1], param[2], param[3]);
	return false;
}

static const char help_uptime[] =
"/uptime\n"
"Display server uptime and load.";

CMD(uptime)
{
	time_t uptime = time(NULL) - g_server.start_time;
	char buf[128], *bufp = buf;

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

	FILE *f = fopen("/proc/self/status", "r");
	char buf2[1024];

	while (fgets(buf2, sizeof buf2, f) != NULL)
	{
		if (strncmp(buf2, "VmRSS:", 6) == 0)
		{
			snprintf(buf, sizeof buf, "Server memory usage: %s", buf2 + 7);
			client_notify(c, buf);
			break;
		}
	}
	fclose(f);

	return false;
}

static const char help_water[] =
"/water\n"
"Toggle water mode. Any block placed will be converted to static water.";

CMD(water)
{
	player_toggle_mode(c->player, MODE_PLACE_WATER);

	char buf[64];
	snprintf(buf, sizeof buf, "Water %s", (c->player->mode == MODE_PLACE_WATER) ? s_on : s_off);
	client_notify(c, buf);

	return false;
}

static const char help_whois[] =
"/whois <user>\n"
"Display information about the specified <user>.";

CMD(whois)
{
	char buf[128];

	if (params != 2) return true;

	unsigned globalid = playerdb_get_globalid(param[1], false, NULL);
	if (globalid == -1)
	{
		snprintf(buf, sizeof buf, "%s is not known here.", param[1]);
		client_notify(c, buf);
		return false;
	}

	struct player_t *p = player_get_by_name(param[1]);
	if (p == NULL)
	{
		snprintf(buf, sizeof buf, "%s is offline", param[1]);
		client_notify(c, buf);

		if (c->player->rank >= RANK_OP)
		{
			snprintf(buf, sizeof buf, "%s last connected from %s", param[1], playerdb_get_last_ip(globalid));
			client_notify(c, buf);
		}
	}
	else
	{
		snprintf(buf, sizeof buf, "%s" TAG_WHITE " is online, on level %s", p->colourusername, p->level->name);
		client_notify(c, buf);

		if (c->player->rank >= RANK_OP)
		{
			snprintf(buf, sizeof buf, "%s is connected from %s", p->username, p->client->ip);
			client_notify(c, buf);
		}
	}

	return false;
}

struct command_t s_commands[] = {
	{ "adminrules", RANK_GUEST, &cmd_adminrules, help_adminrules },
	{ "afk", RANK_GUEST, &cmd_afk, help_afk },
	{ "ban", RANK_OP, &cmd_ban, help_ban },
	{ "banip", RANK_OP, &cmd_banip, help_banip },
	{ "bind", RANK_BUILDER, &cmd_bind, help_bind },
	{ "blocks", RANK_BUILDER, &cmd_blocks, help_blocks },
	{ "commands", RANK_BANNED, &cmd_commands, help_commands },
	{ "cuboid", RANK_ADV_BUILDER, &cmd_cuboid, help_cuboid },
	{ "disown", RANK_OP, &cmd_disown, help_disown },
	{ "dellvl", RANK_OP, &cmd_dellvl, help_dellvl },
	{ "z", RANK_ADV_BUILDER, &cmd_cuboid, help_cuboid },
	{ "exit", RANK_ADMIN, &cmd_exit, help_exit },
	{ "fixed", RANK_OP, &cmd_fixed, help_fixed },
	{ "filter", RANK_OP, &cmd_filter, help_filter },
	{ "follow", RANK_OP, &cmd_follow, help_follow },
	{ "goto", RANK_GUEST, &cmd_goto, help_goto },
	{ "help", RANK_GUEST, &cmd_help, help_help },
	{ "hide", RANK_OP, &cmd_hide, help_hide },
	{ "home", RANK_GUEST, &cmd_home, help_home },
	{ "hookattach", RANK_OP, &cmd_hookattach, help_hookattach },
	{ "hookdetach", RANK_OP, &cmd_hookdetach, help_hookdetach },
	{ "identify", RANK_GUEST, &cmd_identify, help_identify },
	{ "info", RANK_BUILDER, &cmd_info, help_info },
	{ "instant", RANK_OP, &cmd_instant, help_instant },
	{ "kick", RANK_OP, &cmd_kick, help_kick },
	{ "kickban", RANK_OP, &cmd_kickban, help_kickban },
	{ "lava", RANK_GUEST, &cmd_lava, help_lava },
	{ "levels", RANK_GUEST, &cmd_levels, help_levels },
	{ "lvlowner", RANK_OP, &cmd_lvlowner, help_lvlowner },
	{ "mapinfo", RANK_GUEST, &cmd_mapinfo, help_mapinfo },
	{ "module_load", RANK_ADMIN, &cmd_module_load, help_module_load },
	{ "module_unload", RANK_ADMIN, &cmd_module_unload, help_module_unload },
	{ "modules", RANK_ADMIN, &cmd_modules, help_modules },
	{ "motd", RANK_BANNED, &cmd_motd, help_motd },
	{ "newlvl", RANK_OP, &cmd_newlvl, help_newlvl },
	{ "opglass", RANK_OP, &cmd_opglass, help_opglass },
	{ "paint", RANK_BUILDER, &cmd_paint, help_paint },
	{ "perbuild", RANK_GUEST, &cmd_perbuild, help_perbuild },
	{ "perown", RANK_GUEST, &cmd_perown, help_perown },
	{ "pervisit", RANK_GUEST, &cmd_pervisit, help_pervisit },
	{ "physics", RANK_OP, &cmd_physics, help_physics },
	{ "place", RANK_ADV_BUILDER, &cmd_place, help_place },
	{ "players", RANK_GUEST, &cmd_players, help_players },
	{ "replace", RANK_ADV_BUILDER, &cmd_replace, help_replace },
	{ "replaceall", RANK_OP, &cmd_replaceall, help_replaceall },
	{ "resetlvl", RANK_GUEST, &cmd_resetlvl, help_resetlvl },
	{ "rules", RANK_BANNED, &cmd_rules, help_rules },
	{ "setpassword", RANK_BUILDER, &cmd_setpassword, help_setpassword },
	{ "setposinterval", RANK_OP, &cmd_setposinterval, help_setposinterval },
	{ "setrank", RANK_OP, &cmd_setrank, help_setrank },
	{ "setspawn", RANK_GUEST, &cmd_setspawn, help_setspawn },
	{ "spawn", RANK_GUEST, &cmd_spawn, help_spawn },
	{ "solid", RANK_OP, &cmd_solid, help_solid },
	{ "summon", RANK_OP, &cmd_summon, help_summon },
	{ "time", RANK_GUEST, &cmd_time, help_time },
	{ "tp", RANK_BUILDER, &cmd_tp, help_tp },
	{ "unbanip", RANK_OP, &cmd_unbanip, help_unbanip },
	{ "undo", RANK_OP, &cmd_undo, help_undo },
	{ "uptime", RANK_GUEST, &cmd_uptime, help_uptime },
	{ "water", RANK_GUEST, &cmd_water, help_water },
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
