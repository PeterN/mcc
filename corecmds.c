#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/time.h>
#include "client.h"
#include "commands.h"
#include "cuboid.h"
#include "level.h"
#include "packet.h"
#include "player.h"
#include "playerdb.h"
#include "mcc.h"
#include "network.h"
#include "undodb.h"
#include "util.h"
#include "level_worker.h"
#include "gettime.h"
#include "timer.h"

static const char s_on[] = TAG_RED "on";
static const char s_off[] = TAG_GREEN "off";

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

static void reconstruct(char *buf, size_t len, int params, const char **param)
{
	int i;
	for (i = 0; i < params; i++)
	{
		strcat(buf, param[i]);
		if (i < params - 1) strcat(buf, " ");
	}
}

static char *prettytime(char *buf, const char *endp, int time, int acc)
{
	int seconds = time % 60;
	int minutes = time / 60 % 60;
	int hours   = time / 3600 % 24;
	int days    = time / 86400;

	char *bufp = buf;
	if (days > 0)
	{
		bufp += snprintf(bufp, endp - bufp, "%d day%s ", days, days == 1 ? "" : "s");
	}
	if (hours > 0 || (days > 0 && acc))
	{
		bufp += snprintf(bufp, endp - bufp, "%d hour%s ", hours, hours == 1 ? "" : "s");
		if (days > 0 && !acc) return bufp;
	}
	if (minutes > 0 || ((hours > 0 || days > 0) && acc))
	{
		bufp += snprintf(bufp, endp - bufp, "%d minute%s ", minutes, minutes == 1 ? "" : "s");
		if (hours > 0 && !acc) return bufp;
	}
	bufp += snprintf(bufp, endp - bufp, "%d second%s ", seconds, seconds == 1 ? "" : "s");
	return bufp;
}

#define CMD(x) static bool cmd_ ## x (struct client_t *c, int params, const char **param)

static const char help_activelava[] =
"/activelava\n"
"Toggle active lava mode. Any block placed will be converted to lava.";

CMD(activelava)
{
	if (HasBit(c->player->flags, FLAG_GAMES))
	{
		client_notify(c, TAG_RED "You can't /activelava when playing games");
		return false;
	}

	player_toggle_mode(c->player, MODE_PLACE_ACTIVE_LAVA);

	char buf[64];
	snprintf(buf, sizeof buf, "Active lava %s", (c->player->mode == MODE_PLACE_ACTIVE_LAVA) ? s_on : s_off);
	client_notify(c, buf);

	return false;
}

static const char help_activewater[] =
"/activewater\n"
"Toggle active water mode. Any block placed will be converted to water.";

CMD(activewater)
{
	if (HasBit(c->player->flags, FLAG_GAMES))
	{
		client_notify(c, TAG_RED "You can't /activewater when playing games");
		return false;
	}

	player_toggle_mode(c->player, MODE_PLACE_ACTIVE_WATER);

	char buf[64];
	snprintf(buf, sizeof buf, "Active water %s", (c->player->mode == MODE_PLACE_ACTIVE_WATER) ? s_on : s_off);
	client_notify(c, buf);

	return false;
}



static const char help_adminrules[] =
"/adminrules\n"
"Display the server's admin rules.";

CMD(adminrules)
{
	client_notify_file(c, "adminrules.txt");
	return false;
}

static const char help_afk[] =
"/afk [<message>]\n"
"Mark yourself AFK";

CMD(afk)
{
	if (params == 1)
	{
		c->player->afk[0] = '\0';
		client_notify(c, "Away message cleared");
	}
	else
	{
		reconstruct(c->player->afk, sizeof c->player->afk, params - 1, param + 1);
		client_notify(c, "Away message set");
	}
	return false;
}

static const char help_aka[] =
"/aka\n"
"Toggle real names";

CMD(aka)
{
	if (params != 1) return true;

	c->player->namemode++;
	if (c->player->namemode > ((c->player->rank >= RANK_MOD) ? 2 : 1)) c->player->namemode = 0;

	char buf[64];

	switch (c->player->namemode)
	{
		default:
		case 0: snprintf(buf, sizeof buf, TAG_YELLOW "Aliases selected"); break;
		case 1: snprintf(buf, sizeof buf, TAG_YELLOW "Usernames selected"); break;
		case 2: snprintf(buf, sizeof buf, TAG_YELLOW "Rank+usernames selected"); break;
	}
	client_notify(c, buf);

	client_despawn_players(c);
	client_spawn_players(c);

	return false;
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

	p = player_get_by_name(param[1], true);
	const char *name = (p != NULL) ? p->username : param[1];

	oldrank = playerdb_get_rank(name);
	if (c->player->rank != RANK_ADMIN && oldrank >= RANK_OP)
	{
		client_notify(c, "Can't ban op or admin");
		return false;
	}

	playerdb_set_rank(name, RANK_BANNED, c->player->username);
	if (p != NULL)
	{
		p->rank = RANK_BANNED;
		sprintf(p->colourusername, "&%x%s", rank_get_colour(p->rank), p->username);
	}

	if (params == 2)
	{
		snprintf(buf, sizeof buf, "%s banned by %s", name, c->player->username);
	}
	else
	{
		snprintf(buf, sizeof buf, "%s banned (", name);
		reconstruct(buf, sizeof buf, params - 2, param + 2);
		strcat(buf, ")");
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

	char buf[64];
	snprintf(buf, sizeof buf, TAG_AQUA "IP %s banned by %s", param[1], c->player->username);
	net_notify_ops(buf);

	unsigned i;
	for (i = 0; i < s_clients.used; i++)
	{
		struct client_t *client = s_clients.items[i];
		if (strcmp(client->ip, param[1]) == 0)
		{
			net_close(client, "IP banned");
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
				if ((int)c->player->bindings[i] < 0)
				{
					snprintf(buf, sizeof buf, "%s bound to /info", blocktype_get_name(i));
					client_notify(c, buf);
				}
				else if (c->player->bindings[i] != i)
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

			if (*param[2] == '/' && c->player->rank >= RANK_ADV_BUILDER)
			{
				if (i == ADMINIUM && c->player->rank < RANK_OP)
				{
					snprintf(buf, sizeof buf, "You do not have permission to place adminium.");
				}
				else if (i == BLOCK_INVALID)
				{
					snprintf(buf, sizeof buf, "Unknown blocktype %s", param[1]);
				}
				else if (strcasecmp(param[2], "/info") == 0)
				{
					c->player->bindings[i] = -1;
					snprintf(buf, sizeof buf, "Bound %s to /info", blocktype_get_name(i));
				}
				else
				{
					snprintf(buf, sizeof buf, "Unknown command bind %s", param[2]);
				}
			}
			else
			{
				j = blocktype_get_by_name(param[2]);
				if ((i == ADMINIUM || j == ADMINIUM) && c->player->rank < RANK_OP)
				{
					snprintf(buf, sizeof buf, "You do not have permission to place adminium.");
				}
				else if (i == BLOCK_INVALID)
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

static const char help_clones[] =
"/clones\n"
"List all clients connected from same IP address.";

CMD(clones)
{
	return false;
}

static const char help_copylvl[] =
"/copylvl <src> <dst>\n"
"Copy map from src level to dst level.";

CMD(copylvl)
{
	if (params != 3) return true;

	char buf[64];

	struct level_t *src;
	if (!level_get_by_name(param[1], &src))
	{
		snprintf(buf, sizeof buf, TAG_YELLOW "Can't load level %s for copying", param[1]);
		client_notify(c, buf);
		return false;
	}

	struct level_t *dst;
	if (!level_get_by_name(param[2], &dst))
	{
		snprintf(buf, sizeof buf, TAG_YELLOW "Can't load level %s for copying", param[2]);
		client_notify(c, buf);
		return false;
	}

	cuboid_remove_for_level(dst);

	level_copy(src, dst);

	undodb_close(dst->undo);
	dst->undo = NULL;

	char filename[256];
	snprintf(filename, sizeof filename, "undo/%s.db", dst->name);
	lcase(filename);
	unlink(filename);

	level_notify_all(dst, TAG_YELLOW "Started level copy");

	return false;
}


static const char help_cuboid[] =
"/cuboid [<type>]\n"
"Place a cuboid, using two corners specified after using the command. "
"Cuboid will be of the <type> given, or the type held when placing the final corner.";

CMD(cuboid)
{
	char buf[64];

	if (HasBit(c->player->flags, FLAG_GAMES))
	{
		client_notify(c, TAG_RED "You can't /cuboid when playing games");
		return false;
	}

	if (params > 2) return true;

	c->player->mode = MODE_NORMAL;

	if (params == 2)
	{
		c->player->cuboid_type = blocktype_get_by_name(param[1]);
		if (c->player->cuboid_type == BLOCK_INVALID)
		{
			snprintf(buf, sizeof buf, "Unknown block type %s", param[1]);
			client_notify(c, buf);
			return false;
		}
		else if (c->player->rank < blocktype_min_rank(c->player->cuboid_type))
		{
			snprintf(buf, sizeof buf, "You do not have permission to place %s", param[1]);
			client_notify(c, buf);
			return false;
		}
	}
	else
	{
		c->player->cuboid_type = BLOCK_INVALID;
	}

	c->player->mode = MODE_CUBOID;
	c->player->cuboid_start = UINT_MAX;
	c->player->replace_type = BLOCK_INVALID;

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
				level_send_queue(c);
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

static int dir_filename_filter(const struct dirent *d)
{
	return d->d_type == DT_DIR && d->d_name[0] != '.' && strstr(d->d_name, "backups") == NULL;
}

static const char help_dirs[] =
"/dirs\n"
"List all level directories known by the server.";

CMD(dirs)
{
	char buf[64];
	char *bufp;
	struct dirent **namelist;
	int n, i;

	strcpy(buf, "Directories: ");
	bufp = buf + strlen(buf);

	n = scandir("levels", &namelist, &dir_filename_filter, alphasort);
	if (n <= 0)
	{
		client_notify(c, "Unable to get list of directories");
		return false;
	}

	for (i = 0; i < n; i++)
	{
		char buf2[64];
		snprintf(buf2, sizeof buf2, "%s%s", namelist[i]->d_name, (i < n - 1) ? ", " : "");

		size_t len = strlen(buf2);
		if (len >= sizeof buf - (bufp - buf))
		{
			client_notify(c, buf);
			memset(buf, 0, sizeof buf);
			bufp = buf;
		}

		strcpy(bufp, buf2);
		bufp += len;

		free(namelist[i]);
	}

	free(namelist);

	client_notify(c, buf);

	return false;
}

static const char help_disown[] =
"/disown\n"
"Toggle disown mode. When enabled, any blocks placed will have their owner reset to none. "
"Useful for clearing up some blocks.";

CMD(disown)
{
	if (HasBit(c->player->flags, FLAG_GAMES))
	{
		client_notify(c, TAG_RED "You can't /disown when playing games");
		return false;
	}

	ToggleBit(c->player->flags, FLAG_DISOWN);

	char buf[64];
	snprintf(buf, sizeof buf, "Disown %s", HasBit(c->player->flags, FLAG_DISOWN) ? s_on : s_off);
	client_notify(c, buf);

	return false;
}

static const char help_filter[] =
"/filter [<user>]\n"
"Filter the current level to only show blocks placed by the <user> given. "
"If no <user> is specified, filtering is reset so all blocks are shown.";

CMD(filter)
{
	if (HasBit(c->player->flags, FLAG_GAMES))
	{
		client_notify(c, TAG_RED "You can't /filter when playing games");
		return false;
	}

	if (params > 2) return true;

	if (params == 2)
	{
		const struct player_t *p = player_get_by_name(param[1], true);
		int globalid;

		if (p != NULL)
		{
			globalid = p->globalid;
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

		c->player->filter = globalid;
	}
	else
	{
		c->player->filter = 0;
		client_notify(c, "Filtering disabled");
	}

	c->waiting_for_level = true;
	level_send_queue(c);

	return false;
}

static const char help_fixed[] =
"/fixed\n"
"Toggle fixed mode. When enabled, fixed blocks will not be subject "
"to physics and cannot be removed by other players.";

CMD(fixed)
{
	if (HasBit(c->player->flags, FLAG_GAMES))
	{
		client_notify(c, TAG_RED "You can't /fixed when playing games");
		return false;
	}

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

	if (HasBit(c->player->flags, FLAG_GAMES))
	{
		client_notify(c, TAG_RED "You can't follow when playing games");
		return false;
	}

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

		client_add_packet(c, packet_send_spawn_player(c->player->following->levelid, c->player->following->colourusername, &c->player->following->pos));

		c->player->following = NULL;
		return false;
	}

	struct player_t *p = player_get_by_name(param[1], true);
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
	if (p->client->hidden && c->player->rank < RANK_ADMIN)
	{
		snprintf(buf, sizeof buf, "%s is %s", param[1], c->player->rank == RANK_OP ? "hidden" : "offline");
		client_notify(c, buf);
		return false;
	}

	if (!c->hidden)
	{
		c->hidden = true;
		client_send_despawn(c, true);

		snprintf(buf, sizeof buf, TAG_RED "- %s" TAG_YELLOW " disconnected", c->player->colourusername);
		call_hook(HOOK_CHAT, buf);
		net_notify_all(buf);

		snprintf(buf, sizeof buf, TAG_AQUA "*** %s " TAG_AQUA "is hidden", c->player->colourusername);
		net_notify_ops(buf);
	}

	/* Despawn followed player to prevent following player jitter */
	client_add_packet(c, packet_send_despawn_player(p->levelid));

	snprintf(buf, sizeof buf, "Hidden %s", c->hidden ? s_on : s_off);
	client_notify(c, buf);

	snprintf(buf, sizeof buf, "Now following %s", p->colourusername);
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
		snprintf(buf, sizeof buf, "Can't go to level '%s'", param[1]);
		client_notify(c, buf);
	}

	return false;
}

static const char help_global[] =
"/global\n"
"Toggle global chat mode. When enabled, all chat will go to the global channel instead of to the level.";

CMD(global)
{
	ToggleBit(c->player->flags, FLAG_GLOBAL);

	char buf[64];
	snprintf(buf, sizeof buf, "Global chat %s", HasBit(c->player->flags, FLAG_GLOBAL) ? s_on : s_off);
	client_notify(c, buf);

	return false;
}

static const char help_hide[] =
"/hide\n"
"Toggle hidden mode.";

CMD(hide)
{
	if (HasBit(c->player->flags, FLAG_GAMES))
	{
		client_notify(c, TAG_RED "You can't /hide when playing games");
		return false;
	}

	c->hidden = !c->hidden;

	char buf[64];
	if (c->hidden)
	{
		snprintf(buf, sizeof buf, TAG_RED "- %s" TAG_YELLOW " disconnected", c->player->colourusername);
		call_hook(HOOK_CHAT, buf);
		net_notify_all(buf);

		client_send_despawn(c, true);
	}
	else
	{
		snprintf(buf, sizeof buf, TAG_GREEN "+ %s" TAG_YELLOW " connected", c->player->colourusername);
		call_hook(HOOK_CHAT, buf);
		net_notify_all(buf);

		client_send_spawn(c, true);
	}

	snprintf(buf, sizeof buf, "Hidden %s", c->hidden ? s_on : s_off);
	client_notify(c, buf);

	snprintf(buf, sizeof buf, TAG_AQUA "*** %s " TAG_AQUA "is %s", c->player->colourusername, c->hidden ? "hidden" : "visible");
	net_notify_ops(buf);

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
		l->rankbuild = RANK_OP;
		l->rankown   = RANK_OP;
		user_list_add(&l->uservisit, l->owner);
		user_list_add(&l->userbuild, l->owner);
		user_list_add(&l->userown,   l->owner);

		level_gen(l, "flat", l->y / 2, l->y / 2);
		level_list_add(&s_levels, l);
	}

	/* Don't resend the level if player is already on it */
	player_change_level(c->player, l);

	return false;
}

static const char help_hooks[] =
"/hooks";

CMD(hooks)
{
	if (params != 1) return true;

	int i;
	for (i = 0; i < MAX_HOOKS_PER_LEVEL; i++)
	{
		if (*c->player->level->level_hook[i].name != '\0')
		{
			char buf[64];
			snprintf(buf, sizeof buf, "%d) %s - %s", i + 1, c->player->level->level_hook[i].name, c->player->level->level_hook[i].func == NULL ? "inactive" : "active");
			client_notify(c, buf);
		}
	}

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

static const char help_hookdelete[] =
"/hookdelete <hook>";

CMD(hookdelete)
{
	if (params != 2) return true;

	if (level_hook_delete(c->player->level, param[1]))
	{
		client_notify(c, "Hook deleted");
	}
	else
	{
		client_notify(c, "Hook not deleted");
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
	if (HasBit(c->player->flags, FLAG_GAMES))
	{
		client_notify(c, TAG_RED "You can't /info when playing games");
		return false;
	}

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
			level_send_queue(l->clients[i]);
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

static bool undo_real(int16_t x, int16_t y, int16_t z, int oldtype, int olddata, int newtype, void *arg);

static const char help_kbu[] =
"/kbu <user> [<message>]\n"
"Kicks, bans, and undoes the specified <user> from the server.";

CMD(kbu)
{
	if (params < 2) return true;

	char buf[128];
	struct player_t *p;

	p = player_get_by_name(param[1], true);
	const char *name = (p != NULL) ? p->username : param[1];

	enum rank_t oldrank = playerdb_get_rank(name);
	if (c->player->rank != RANK_ADMIN && oldrank >= RANK_OP)
	{
		client_notify(c, "Can't ban op or admin");
		return false;
	}

	struct level_t *l;
	int globalid;

	playerdb_set_rank(name, RANK_BANNED, c->player->username);
	if (p != NULL)
	{
		l = c->player->level;
		globalid = p->globalid;

		if (params == 2)
		{
			snprintf(buf, sizeof buf, "kickbanned by %s", c->player->username);
		}
		else
		{
			snprintf(buf, sizeof buf, "kickbanned (");
			reconstruct(buf, sizeof buf, params - 2, param + 2);
			strcat(buf, ")");
		}
		net_close(p->client, buf);
	}
	else
	{
		l = c->player->level;
		globalid = playerdb_get_globalid(name, false, NULL);
		if (globalid == -1)
		{
			client_notify(c, "Unknown username.");
			return false;
		}
	}

	if (l == NULL) return false;

	snprintf(buf, sizeof buf, TAG_AQUA "%s kbu'd by %s", name, c->player->username);
	net_notify_ops(buf);

	if (l->undo == NULL) l->undo = undodb_init(l->name);
	if (l->undo != NULL)
	{
		int count = undodb_undo_player(l->undo, globalid, 10000, &undo_real, c);
		if (count > 0)
		{
			l->changed = true;
			snprintf(buf, sizeof buf, TAG_YELLOW "%d blocks changed by undo", count);
			client_notify(c, buf);
		}
	}

	level_user_undo(l, globalid, c);

	return false;
}

static const char help_kick[] =
"/kick <user> [<message>]\n"
"Kicks the specified <user> from the server.";

CMD(kick)
{
	if (params < 2) return true;

	char buf[128];

	struct player_t *p = player_get_by_name(param[1], true);
	if (p == NULL)
	{
		snprintf(buf, sizeof buf, "%s is offline", param[1]);
		client_notify(c, buf);
		return false;
	}

	snprintf(buf, sizeof buf, TAG_AQUA "%s kicked by %s", p->username, c->player->username);
	net_notify_ops(buf);

	if (params == 2)
	{
		snprintf(buf, sizeof buf, "kicked by %s", c->player->username);
	}
	else
	{
		snprintf(buf, sizeof buf, "kicked (");
		reconstruct(buf, sizeof buf, params - 2, param + 2);
		strcat(buf, ")");
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

	p = player_get_by_name(param[1], true);
	const char *name = (p != NULL) ? p->username : param[1];

	enum rank_t oldrank = playerdb_get_rank(name);
	if (c->player->rank != RANK_ADMIN && oldrank >= RANK_OP)
	{
		client_notify(c, "Can't ban op or admin");
		return false;
	}

	playerdb_set_rank(name, RANK_BANNED, c->player->username);
	if (p == NULL)
	{
		snprintf(buf, sizeof buf, "%s is offline", param[1]);
		client_notify(c, buf);
		return false;
	}

	snprintf(buf, sizeof buf, TAG_AQUA "%s kickbanned by %s", p->username, c->player->username);
	net_notify_ops(buf);

	if (params == 2)
	{
		snprintf(buf, sizeof buf, "kickbanned by %s", c->player->username);
	}
	else
	{
		snprintf(buf, sizeof buf, "kickbanned (");
		reconstruct(buf, sizeof buf, params - 2, param + 2);
		strcat(buf, ")");
	}
	net_close(p->client, buf);

	return false;
}

static const char help_lava[] =
"/lava\n"
"Toggle lava mode. Any block placed will be converted to static lava.";

CMD(lava)
{
	if (HasBit(c->player->flags, FLAG_GAMES))
	{
		client_notify(c, TAG_RED "You can't /lava when playing games");
		return false;
	}

	player_toggle_mode(c->player, MODE_PLACE_LAVA);

	char buf[64];
	snprintf(buf, sizeof buf, "Lava %s", (c->player->mode == MODE_PLACE_LAVA) ? s_on : s_off);
	client_notify(c, buf);

	return false;
}

static int level_filename_filter(const struct dirent *d)
{
	if (strstr(d->d_name, ".mcl") != NULL || strstr(d->d_name, ".lvl") != NULL)
	{
		return strstr(d->d_name, "_home") == NULL;
	}
	return 0;
}

static const char help_levels[] =
"/levels [<dir>]\n"
"List all levels known by the server.";

CMD(levels)
{
	char buf[64];
	char *bufp;
	struct dirent **namelist;
	int n, i;

	if (params > 2) return true;

	strcpy(buf, "Levels: ");
	bufp = buf + strlen(buf);

	char path[128];
	if (params == 2)
	{
		if (strchr(param[1], '/') != NULL)
		{
			return true;
		}

		snprintf(path, sizeof path, "levels/%s", param[1]);
		lcase(path);
	}
	else
	{
		strcpy(path, "levels");
	}

	n = scandir(path, &namelist, &level_filename_filter, alphasort);
	if (n <= 0)
	{
		client_notify(c, "Unable to get list of levels");
		return false;
	}

	for (i = 0; i < n; i++)
	{
		/* Chop name off at extension */
		char *ext = strrchr(namelist[i]->d_name, '.');
		if (ext != NULL) *ext = '\0';

		if (i == 0 || strcmp(namelist[i - 1]->d_name, namelist[i]->d_name) != 0)
		{
			bool loaded;

			if (params == 2)
			{
				char fullname[192];
				snprintf(fullname, sizeof fullname, "%s/%s", param[1], namelist[i]->d_name);
				loaded = level_is_loaded(fullname);
			}
			else
			{
				loaded = level_is_loaded(namelist[i]->d_name);
			}

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
	free(namelist);

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
	unsigned i;
	const struct level_t *l = c->player->level;
	char buf[4096];
	char *bufp, *endp = buf + sizeof buf;

	snprintf(buf, sizeof buf, "Level: %s  Owner: %s", l->name, playerdb_get_username(l->owner));
	client_notify(c, buf);
	snprintf(buf, sizeof buf, "Extents: %d x %d x %d", l->x, l->y, l->z);
	client_notify(c, buf);

	bufp = buf;
	bufp += snprintf(bufp, endp - bufp, "Visit permission: %s", rank_get_name(l->rankvisit));
	for (i = 0; i < c->player->level->uservisit.used; i++)
	{
		bufp += snprintf(bufp, endp - bufp, " %s", playerdb_get_username(c->player->level->uservisit.items[i]));
	}
	client_notify(c, buf);

	bufp = buf;
	bufp += snprintf(bufp, endp - bufp, "Build permission: %s", rank_get_name(l->rankbuild));
	for (i = 0; i < c->player->level->userbuild.used; i++)
	{
		bufp += snprintf(bufp, endp - bufp, " %s", playerdb_get_username(c->player->level->userbuild.items[i]));
	}
	client_notify(c, buf);

	bufp = buf;
	bufp += snprintf(bufp, endp - bufp, "Own permission: %s", rank_get_name(l->rankown));
	for (i = 0; i < c->player->level->userown.used; i++)
	{
		bufp += snprintf(bufp, endp - bufp, " %s", playerdb_get_username(c->player->level->userown.items[i]));
	}
	client_notify(c, buf);

	return false;
}

static const char help_me[] =
"/me <message>\n";

CMD(me)
{
	if (params < 2) return true;

	char buf[128];
	snprintf(buf, sizeof buf, "%s%c%c* %s ", HasBit(c->player->flags, FLAG_GLOBAL) ? "! " : "", c->player->colourusername[0], c->player->colourusername[1], c->player->username);
	reconstruct(buf, sizeof buf, params - 1, param + 1);

	call_hook(HOOK_CHAT, buf);
	if (HasBit(c->player->flags, FLAG_GLOBAL))
	{
		net_notify_all(buf);
	}
	else
	{
		level_notify_all(c->player->level, buf);
	}

	return false;
}

static const char help_motd[] =
"/motd\n"
"Display the server's Message of the Day.";

CMD(motd)
{
	client_notify_file(c, "motd.txt");
	return false;
}

static const char help_newlvl[] =
//"/newlvl <name> <x> <y> <z> <type> <height_range> <sea_height>\n"
//"Create a new level. <y> is height. "
//"Type: 0=flat 1=flat/adminium 2=smooth 6=rough";
"/newlvl <name> <x> <y> <z> <type>\n"
"Create a new level. <y> is height. "
"Type: flat, adminium, pixel, island, mountains, ocean, forest";

CMD(newlvl)
{
	if (params != 6) return true;

	const char *name = param[1];
	int x = strtol(param[2], NULL, 10);
	int y = strtol(param[3], NULL, 10);
	int z = strtol(param[4], NULL, 10);
	const char *t = param[5];
//	int hr = strtol(param[6], NULL, 10);
//	int sh = strtol(param[7], NULL, 10);

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

/*
	if (t < 0 || t > 6)
	{
		client_notify(c, "Type must be from 0 to 6 only");
		return false;
	}
*/
	if (strcmp(t, "flat") && strcmp(t, "adminium") && strcmp(t, "pixel") &&
		strcmp(t, "island") && strcmp(t, "mountains") && strcmp(t, "ocean") &&
		strcmp(t, "forest"))
	{
		client_notify(c, "Invalid type, see /help newlvl");
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

		level_gen(l, t, 0, 0);
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

static const char help_paint[] =
"/paint\n"
"Toggle paint mode. When enabled, any removed block will instead "
"be replaced by the currently held block.";

CMD(paint)
{
	if (HasBit(c->player->flags, FLAG_GAMES))
	{
		client_notify(c, TAG_RED "You can't /paint when playing games");
		return false;
	}

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

		if (user_list_contains(&c->player->level->userbuild, globalid))
		{
			if (param[1][0] == '+')
			{
				client_notify(c, "User already in list");
				return false;
			}
			user_list_del_item(&c->player->level->userbuild, globalid);
		}
		else
		{
			if (param[1][0] == '-')
			{
				client_notify(c, "user not in list");
				return false;
			}
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

		if (user_list_contains(&c->player->level->userown, globalid))
		{
			if (param[1][0] == '+')
			{
				client_notify(c, "User already in list");
				return false;
			}
			user_list_del_item(&c->player->level->userown, globalid);
		}
		else
		{
			if (param[1][0] == '-')
			{
				client_notify(c, "user not in list");
				return false;
			}
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

		if (user_list_contains(&c->player->level->uservisit, globalid))
		{
			if (param[1][0] == '+')
			{
				client_notify(c, "User already in list");
				return false;
			}
			user_list_del_item(&c->player->level->uservisit, globalid);
		}
		else
		{
			if (param[1][0] == '-')
			{
				client_notify(c, "user not in list");
				return false;
			}
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
			level_reset_physics(l);
			level_notify_all(l, TAG_YELLOW "Physics reset");
		}
		else if (strcasecmp(param[1], "reinit") == 0)
		{
			level_reinit_physics(l);
			level_notify_all(l, TAG_YELLOW "Physics reinited");
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

	if (HasBit(c->player->flags, FLAG_GAMES))
	{
		client_notify(c, TAG_RED "You can't /place when playing games");
		return false;
	}

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

static const char help_ranks[] =
"/ranks\n"
"Display information about ranks.";

CMD(ranks)
{
	client_notify_file(c, "ranks.txt");
	return false;
}

static const char help_replace[] =
"/replace <oldtype> [<newtype>]\n"
"";

CMD(replace)
{
	char buf[64];

	if (HasBit(c->player->flags, FLAG_GAMES))
	{
		client_notify(c, TAG_RED "You can't /replace when playing games");
		return false;
	}

	if (params != 2 && params != 3) return true;

	c->player->mode = MODE_NORMAL;

	c->player->replace_type = blocktype_get_by_name(param[1]);
	if (c->player->replace_type == BLOCK_INVALID)
	{
		snprintf(buf, sizeof buf, "Unknown block type %s", param[1]);
		client_notify(c, buf);
		return false;
	}
	else if (c->player->rank < blocktype_min_rank(c->player->replace_type))
	{
		snprintf(buf, sizeof buf, "You do not have permission to replace %s", param[1]);
		client_notify(c, buf);
		return false;
	}

	if (params == 3)
	{
		c->player->cuboid_type = blocktype_get_by_name(param[2]);
		if (c->player->cuboid_type == BLOCK_INVALID)
		{
			snprintf(buf, sizeof buf, "Unknown block type %s", param[2]);
			client_notify(c, buf);
			return false;
		}
		else if (c->player->rank < blocktype_min_rank(c->player->cuboid_type))
		{
			snprintf(buf, sizeof buf, "You do not have permission to place %s", param[2]);
			client_notify(c, buf);
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

	if (HasBit(c->player->flags, FLAG_GAMES))
	{
		client_notify(c, TAG_RED "You can't /replaceall when playing games");
		return false;
	}

	if (params != 3) return true;

	enum blocktype_t oldtype = blocktype_get_by_name(param[1]);
	if (oldtype == BLOCK_INVALID)
	{
		snprintf(buf, sizeof buf, "Unknown block type %s", param[1]);
		client_notify(c, buf);
		return false;
	}
	else if (oldtype == ADMINIUM && c->player->rank < RANK_OP)
	{
		snprintf(buf, sizeof buf, "You do not have permission to replace adminium");
		client_notify(c, buf);
		return false;
	}

	enum blocktype_t newtype = blocktype_get_by_name(param[2]);
	if (newtype == BLOCK_INVALID)
	{
		snprintf(buf, sizeof buf, "Unknown block type %s", param[2]);
		client_notify(c, buf);
		return false;
	}
	else if (newtype == ADMINIUM && c->player->rank < RANK_OP)
	{
		snprintf(buf, sizeof buf, "You do not have permission to place adminium");
		client_notify(c, buf);
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
"/resetlvl <type>\n"
"Resets the current level. "
"Type: flat, adminium, pixel, island, mountains, ocean, forest";

CMD(resetlvl)
{
	if (params != 2) return true;

	struct level_t *l = c->player->level;

	if (c->player->rank < RANK_OP && c->player->globalid != l->owner)
	{
		client_notify(c, "You do not have permission to reset this level.");
		return false;
	}

	const char *t = param[1];

	if (strcmp(t, "flat") && strcmp(t, "adminium") && strcmp(t, "pixel") &&
		strcmp(t, "island") && strcmp(t, "mountains") && strcmp(t, "ocean") &&
		strcmp(t, "forest"))
	{
		client_notify(c, "Invalid type, see /help newlvl");
		return false;
	}

	cuboid_remove_for_level(l);

//	physics_list_free(&l->physics);
//	physics_list_free(&l->physics2);
//	block_update_list_free(&l->updates);

	/* Reset physics for level */
	l->physics.used = 0;
	l->physics2.used = 0;
	l->updates.used = 0;
	l->physics_iter = 0;
	l->updates_iter = 0;
	l->physics_done = 0;


	undodb_close(l->undo);
	l->undo = NULL;

	char filename[256];
	snprintf(filename, sizeof filename, "undo/%s.db", l->name);
	lcase(filename);
	unlink(filename);

	level_gen(l, t, 0, 0);

	unsigned i;
	for (i = 0; i < MAX_CLIENTS_PER_LEVEL; i++)
	{
		struct client_t *c = l->clients[i];
		if (c == NULL) continue;
		c->waiting_for_level = true;
	}

	return false;
}

static const char help_rp[] =
"/rp\n"
"Toggle Remove Pillar mode.";

CMD(rp)
{
	if (HasBit(c->player->flags, FLAG_GAMES))
	{
		client_notify(c, TAG_RED "You can't /rp when playing games");
		return false;
	}

	player_toggle_mode(c->player, MODE_REMOVE_PILLAR);

	char buf[64];
	snprintf(buf, sizeof buf, "Remove pillars %s", (c->player->mode == MODE_REMOVE_PILLAR) ? s_on : s_off);
	client_notify(c, buf);

	return false;
}

static const char help_rules[] =
"/rules\n"
"Display the server rules.";

CMD(rules)
{
	client_notify_file(c, "rules.txt");
	return false;
}

static const char help_setalias[] =
"/setalias <user> [<alias>]\n";

CMD(setalias)
{
	if (params != 2 && params != 3) return true;

	struct player_t *p = player_get_by_name(param[1], true);
	if (p == NULL)
	{
		client_notify(c, "Player is offline.");
		return false;
	}

	player_set_alias(p, params == 2 ? NULL : param[2], true);
	client_notify(c, params == 2 ? "Alias cleared" : "Alias set");
	return false;
}

static const char help_setcuboidmax[] =
"/setcuboidmax <max>\n"
"Set maximum block changes to perform during cuboiding.";

CMD(setcuboidmax)
{
	if (params != 2) return true;

	int max = atoi(param[1]);

	g_server.cuboid_max = max;

	char buf[64];
	snprintf(buf, sizeof buf, TAG_YELLOW "Cuboid max set to %d per 40ms", g_server.cuboid_max);
	client_notify(c, buf);
	LOG("%s\n", buf);

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

static const char help_setplayermax[] =
"/setplayermax <max>\n"
"Set maximum number of players connected before guests cannot join.";

CMD(setplayermax)
{
	if (params != 2) return true;

	int max = atoi(param[1]);

	g_server.max_players = max;

	char buf[64];
	snprintf(buf, sizeof buf, TAG_YELLOW "Player max set to %d", g_server.max_players);
	client_notify(c, buf);
	LOG("%s\n", buf);

	return false;
}



static const char help_setposinterval[] =
"/setposinterval <interval>\n"
"Set player position update interval in ms. Minimum is 10ms.";

CMD(setposinterval)
{
	if (params != 2) return true;

	int interval = atoi(param[1]);
	if (interval < 10) interval = 10;

	g_server.pos_interval = interval;
	timer_set_interval_by_name("positions", interval);

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

	p = player_get_by_name(param[1], true);
	const char *name = (p != NULL) ? p->username : param[1];

	oldrank = playerdb_get_rank(name);
	if (oldrank == newrank)
	{
		client_notify(c, "User already at rank");
		return false;
	}
	if (c->player->rank != RANK_ADMIN && oldrank >= RANK_OP)
	{
		client_notify(c, "Can't demote op or admin");
		return false;
	}

	playerdb_set_rank(name, newrank, c->player->username);
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

		client_send_despawn(p->client, false);
		client_send_spawn(p->client, false);
	}

	char buf[64];

	snprintf(buf, sizeof buf, TAG_AQUA "Rank set to %s for %s", rank_get_name(newrank), name);
	net_notify_ops(buf);

	if (p != NULL)
	{
		snprintf(buf, sizeof buf, TAG_YELLOW "%s %s to %s", name, newrank > oldrank ? "promoted" : "demoted", rank_get_name(newrank));
		net_notify_all(buf);
	}

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
	if (HasBit(c->player->flags, FLAG_GAMES))
	{
		client_notify(c, TAG_RED "You can't /spawn when playing games");
		return false;
	}

	player_teleport(c->player, &c->player->level->spawn, true);

	return false;
}

static const char help_solid[] =
"/solid\n"
"Toggle solid mode. Any block placed will be replaced with adminium.";

CMD(solid)
{
	if (HasBit(c->player->flags, FLAG_GAMES))
	{
		client_notify(c, TAG_RED "You can't /solid when playing games");
		return false;
	}

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

	if (HasBit(c->player->flags, FLAG_GAMES))
	{
		client_notify(c, TAG_RED "You can't /summon when playing games");
		return false;
	}

	if (params != 2) return true;

	struct player_t *p = player_get_by_name(param[1], true);
	if (p == NULL)
	{
		snprintf(buf, sizeof buf, "%s is offline", param[1]);
		client_notify(c, buf);
		return false;
	}
	if (p->level != c->player->level)
	{
		snprintf(buf, sizeof buf, "%s is on '%s'", p->username, p->level->name);
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

	if (HasBit(c->player->flags, FLAG_GAMES))
	{
		client_notify(c, TAG_RED "You can't /tp when playing games");
		return false;
	}

	if (params != 2) return true;

	const struct player_t *p = player_get_by_name(param[1], true);
	if (p == NULL)
	{
		snprintf(buf, sizeof buf, TAG_YELLOW "%s is offline", param[1]);
		client_notify(c, buf);
	}
	else if (p->level != c->player->level)
	{
		snprintf(buf, sizeof buf, TAG_YELLOW "%s is on '%s'", p->username, p->level->name);
		client_notify(c, buf);
	}
	else if (p->client->hidden && c->player->rank < RANK_ADMIN)
	{
		snprintf(buf, sizeof buf, TAG_YELLOW "%s is %s", p->username, c->player->rank == RANK_OP ? "hidden" : "offline");
		client_notify(c, buf);
	}
	else if (c->player->rank < RANK_OP && !level_user_can_own(c->player->level, c->player))
	{
		client_notify(c, TAG_YELLOW "You can't /tp on this level");
	}
	else
	{
		player_teleport(c->player, &p->pos, true);
	}

	return false;
}

static const char help_u[] =
"/u <user>\n"
"Removes blocks placed by <user> from the level.";

CMD(u)
{
	if (params < 2) return true;

	struct player_t *p;

	p = player_get_by_name(param[1], true);
	struct level_t *l = c->player->level;
	int globalid;

	if (p != NULL)
	{
		if (c->player->rank != RANK_ADMIN && p->rank >= RANK_OP)
		{
			client_notify(c, "Can't undo op or admin");
			return false;
		}

		globalid = p->globalid;
	}
	else
	{
		enum rank_t rank = playerdb_get_rank(param[1]);
		if (c->player->rank != RANK_ADMIN && rank >= RANK_OP)
		{
			client_notify(c, "Can't undo op or admin");
			return false;
		}

		globalid = playerdb_get_globalid(param[1], false, NULL);
		if (globalid == -1)
		{
			client_notify(c, "Unknown username.");
			return false;
		}
	}

	if (l == NULL) return false;

	if (l->undo == NULL) l->undo = undodb_init(l->name);
	if (l->undo != NULL)
	{
		int count = undodb_undo_player(l->undo, globalid, 10000, &undo_real, c);
		if (count > 0)
		{
			l->changed = true;
			char buf[64];
			snprintf(buf, sizeof buf, TAG_YELLOW "%d blocks changed by undo", count);
			client_notify(c, buf);
		}
	}

	level_user_undo(l, globalid, c);

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

static bool undo_show(int16_t x, int16_t y, int16_t z, int oldtype, int olddata, int newtype, void *arg)
{
	struct client_t *client = arg;
	struct level_t *l = client->player->level;

	if (x >= l->x || y >= l->y || z >= l->z) return false;
	unsigned index = level_get_index(l, x, y, z);
	struct block_t *b = &l->blocks[index];

	if (b->type != newtype) return false;
	struct block_t backup = *b;
	b->type = oldtype;
	b->data = olddata;
	client_add_packet(client, packet_send_set_block(x, y, z, convert(l, index, b)));
	*b = backup;

	return true;
}

static bool undo_real(int16_t x, int16_t y, int16_t z, int oldtype, int olddata, int newtype, void *arg)
{
	struct client_t *client = arg;
	struct level_t *l = client->player->level;

	if (x >= l->x || y >= l->y || z >= l->z) return false;
	unsigned index = level_get_index(l, x, y, z);
	const struct block_t *b = &l->blocks[index];

	if (b->type != newtype) return false;
	level_addupdate_with_owner(l, index, oldtype, olddata, 0);

	return true;
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
		client_notify(c, "Can't load level.");
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
		client_notify(c, "Can't preview undo on a different level.");
		return false;
	}

	//client_add_packet(client, packet_send_set_block(x, y, z, pt));
	int limit = strtol(param[3], NULL, 10);

	int count = undodb_undo_player(l->undo, globalid, limit, params == 4 ? &undo_show : &undo_real, c);
	if (count > 0)
	{
		l->changed = true;
		char buf[64];
		snprintf(buf, sizeof buf, TAG_YELLOW "%d blocks changed by undo", count);
		client_notify(c, buf);
	}

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
	char buf[128];

	char *bufp = buf, *endp = buf + sizeof buf;
	bufp += snprintf(bufp, endp - bufp, "Server uptime is ");
	bufp = prettytime(bufp, endp, uptime, true);
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
	if (HasBit(c->player->flags, FLAG_GAMES))
	{
		client_notify(c, TAG_RED "You can't /water when playing games");
		return false;
	}

	player_toggle_mode(c->player, MODE_PLACE_WATER);

	char buf[64];
	snprintf(buf, sizeof buf, "Water %s", (c->player->mode == MODE_PLACE_WATER) ? s_on : s_off);
	client_notify(c, buf);

	return false;
}

static const char help_who[] =
"/w [<level>]\n"
"Display list of users on the current level, or the level specified.";

CMD(who)
{
	if (params > 2) return true;

	char buf[64];
	char *bufp = buf;
	char *endp = buf + sizeof buf;

	struct level_t *l;
	if (params == 1)
	{
		l = c->player->level;
	}
	else
	{
		if (!level_get_by_name(param[1], &l))
		{
			client_notify(c, TAG_YELLOW "Level does not exist");
			return false;
		}
	}

	int names = -1;
	int i;
	for (i = 0; i < MAX_CLIENTS_PER_LEVEL; i++)
	{
		struct client_t *c2 = l->clients[i];
		if (c2 == NULL || c2->hidden) continue;
		names++;
	}

	bufp += snprintf(bufp, endp - bufp, TAG_YELLOW "Players on %s: ", l->name);

	int j;
	for (i = 0, j = 0; i < MAX_CLIENTS_PER_LEVEL; i++, j++)
	{
		struct client_t *c2 = l->clients[i];
		if (c2 == NULL || c2->hidden) continue;

		bool last = j == names;
		j++;

		const char *name = playername(c2->player, 2);
		if (strlen(name) + (last ? 0 : 3) >= endp - bufp - 1)
		{
			client_notify(c, buf);
			bufp = buf;
		}

		bufp += snprintf(bufp, endp - bufp, "%s%s", name, last ? "" : TAG_WHITE ", ");
	}
	client_notify(c, buf);

	return false;
}

static const char help_whois[] =
"/whois <user>\n"
"Display information about the specified <user>.";

CMD(whois)
{
	char buf[128];
	char *bufp;
	char *endp = buf + sizeof buf;

	if (params != 2) return true;

	struct player_t *p = player_get_by_name(param[1], true);
	const char *name = (p != NULL) ? p->username : param[1];

	unsigned globalid = playerdb_get_globalid(name, false, NULL);
	if (globalid == -1)
	{
		snprintf(buf, sizeof buf, "%s is not known here.", name);
		client_notify(c, buf);
		return false;
	}

	if (p == NULL || p->client->hidden)
	{
		snprintf(buf, sizeof buf, "%s is offline", param[1]);
		client_notify(c, buf);

		bufp = buf;
		bufp += snprintf(bufp, endp - bufp, "Rank: %s", rank_get_name(playerdb_get_rank(param[1])));

		if (c->player->rank >= RANK_OP)
		{
			bufp += snprintf(bufp, endp - bufp, "  Last IP: %s", playerdb_get_last_ip(globalid, 0));
		}
		client_notify(c, buf);
	}
	else
	{
		unsigned idle = gettime() / 1000 - p->last_active;

		snprintf(buf, sizeof buf, "%s" TAG_WHITE " is online, on level %s", p->colourusername, p->level->name);
		client_notify(c, buf);

		bufp = buf;
		bufp += snprintf(bufp, endp - bufp, "Rank: %s  Idle: ", rank_get_name(p->rank));
		bufp = prettytime(bufp, endp, idle, false);

		if (c->player->rank >= RANK_OP)
		{
			bufp += snprintf(bufp, endp - bufp, " IP: %s", p->client->ip);
		}
		client_notify(c, buf);

		if (p->afk[0] != '\0')
		{
			snprintf(buf, sizeof buf, "Away: %s\n", p->afk);
			client_notify(c, buf);
		}
	}

	return false;
}

static const struct command s_core_commands[] = {
	{ "activelava", RANK_ADV_BUILDER, &cmd_activelava, help_activelava },
	{ "activewater", RANK_ADV_BUILDER, &cmd_activewater, help_activewater },
	{ "adminrules", RANK_GUEST, &cmd_adminrules, help_adminrules },
	{ "afk", RANK_GUEST, &cmd_afk, help_afk },
	{ "aka", RANK_GUEST, &cmd_aka, help_aka },
	{ "al", RANK_ADV_BUILDER, &cmd_activelava, help_activelava },
	{ "aw", RANK_ADV_BUILDER, &cmd_activewater, help_activewater },
	{ "ban", RANK_OP, &cmd_ban, help_ban },
	{ "banip", RANK_OP, &cmd_banip, help_banip },
	{ "bind", RANK_BUILDER, &cmd_bind, help_bind },
	{ "blocks", RANK_BUILDER, &cmd_blocks, help_blocks },
	{ "clones", RANK_OP, &cmd_clones, help_clones },
	{ "copylvl", RANK_OP, &cmd_copylvl, help_copylvl },
	{ "cuboid", RANK_ADV_BUILDER, &cmd_cuboid, help_cuboid },
	{ "disown", RANK_OP, &cmd_disown, help_disown },
	{ "dellvl", RANK_OP, &cmd_dellvl, help_dellvl },
	{ "dirs", RANK_MOD, &cmd_dirs, help_dirs },
	{ "fixed", RANK_MOD, &cmd_fixed, help_fixed },
	{ "filter", RANK_MOD, &cmd_filter, help_filter },
	{ "follow", RANK_OP, &cmd_follow, help_follow },
	{ "goto", RANK_GUEST, &cmd_goto, help_goto },
	{ "global", RANK_BUILDER, &cmd_global, help_global },
	{ "hide", RANK_OP, &cmd_hide, help_hide },
	{ "home", RANK_GUEST, &cmd_home, help_home },
	{ "hooks", RANK_OP, &cmd_hooks, help_hooks },
	{ "hookattach", RANK_OP, &cmd_hookattach, help_hookattach },
	{ "hookdelete", RANK_OP, &cmd_hookdelete, help_hookdelete },
	{ "hookdetach", RANK_OP, &cmd_hookdetach, help_hookdetach },
	{ "identify", RANK_GUEST, &cmd_identify, help_identify },
	{ "info", RANK_BUILDER, &cmd_info, help_info },
	{ "instant", RANK_OP, &cmd_instant, help_instant },
	{ "kbu", RANK_OP, &cmd_kbu, help_kbu },
	{ "kick", RANK_OP, &cmd_kick, help_kick },
	{ "kickban", RANK_OP, &cmd_kickban, help_kickban },
	{ "lava", RANK_GUEST, &cmd_lava, help_lava },
	{ "levels", RANK_GUEST, &cmd_levels, help_levels },
	{ "lvlowner", RANK_OP, &cmd_lvlowner, help_lvlowner },
	{ "mapinfo", RANK_GUEST, &cmd_mapinfo, help_mapinfo },
	{ "me", RANK_GUEST, &cmd_me, help_me },
	{ "motd", RANK_BANNED, &cmd_motd, help_motd },
	{ "newlvl", RANK_OP, &cmd_newlvl, help_newlvl },
	{ "paint", RANK_BUILDER, &cmd_paint, help_paint },
	{ "perbuild", RANK_GUEST, &cmd_perbuild, help_perbuild },
	{ "perown", RANK_GUEST, &cmd_perown, help_perown },
	{ "pervisit", RANK_GUEST, &cmd_pervisit, help_pervisit },
	{ "physics", RANK_OP, &cmd_physics, help_physics },
	{ "place", RANK_ADV_BUILDER, &cmd_place, help_place },
	{ "players", RANK_GUEST, &cmd_players, help_players },
	{ "r", RANK_ADV_BUILDER, &cmd_replace, help_replace },
	{ "ra", RANK_OP, &cmd_replaceall, help_replaceall },
	{ "ranks", RANK_GUEST, &cmd_ranks, help_ranks },
	{ "replace", RANK_ADV_BUILDER, &cmd_replace, help_replace },
	{ "replaceall", RANK_OP, &cmd_replaceall, help_replaceall },
	{ "resetlvl", RANK_GUEST, &cmd_resetlvl, help_resetlvl },
	{ "rp", RANK_MOD, &cmd_rp, help_rp },
	{ "rules", RANK_BANNED, &cmd_rules, help_rules },
	{ "setalias", RANK_ADMIN, &cmd_setalias, help_setalias },
	{ "setcuboidmax", RANK_OP, &cmd_setcuboidmax, help_setcuboidmax },
	{ "setpassword", RANK_BUILDER, &cmd_setpassword, help_setpassword },
	{ "setposinterval", RANK_OP, &cmd_setposinterval, help_setposinterval },
	{ "setplayermax", RANK_OP, &cmd_setplayermax, help_setplayermax },
	{ "setrank", RANK_OP, &cmd_setrank, help_setrank },
	{ "setspawn", RANK_GUEST, &cmd_setspawn, help_setspawn },
	{ "spawn", RANK_GUEST, &cmd_spawn, help_spawn },
	{ "solid", RANK_OP, &cmd_solid, help_solid },
	{ "summon", RANK_OP, &cmd_summon, help_summon },
	{ "time", RANK_GUEST, &cmd_time, help_time },
	{ "tp", RANK_BUILDER, &cmd_tp, help_tp },
	{ "u", RANK_MOD, &cmd_u, help_u },
	{ "unbanip", RANK_OP, &cmd_unbanip, help_unbanip },
	{ "undo", RANK_OP, &cmd_undo, help_undo },
	{ "uptime", RANK_GUEST, &cmd_uptime, help_uptime },
	{ "w", RANK_GUEST, &cmd_who, help_who },
	{ "water", RANK_GUEST, &cmd_water, help_water },
	{ "who", RANK_GUEST, &cmd_who, help_who },
	{ "whois", RANK_GUEST, &cmd_whois, help_whois },
	{ "z", RANK_ADV_BUILDER, &cmd_cuboid, help_cuboid },
	{ NULL, -1, NULL, NULL },
};


void module_init(void **data)
{
	const struct command *comp = s_core_commands;
	for (; comp->command != NULL; comp++)
	{
		register_command(comp->command, comp->rank, comp->func, comp->help);
	}
}

void module_deinit(void *data)
{
	const struct command *comp = s_core_commands;
	for (; comp->command != NULL; comp++)
	{
		deregister_command(comp->command);
	}
}
