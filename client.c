#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include "client.h"
#include "commands.h"
#include "player.h"
#include "playerdb.h"
#include "packet.h"
#include "level.h"
#include "network.h"
#include "mcc.h"

struct client_list_t s_clients;

/*struct client_t *client_get_by_player(struct player_t *p)
{
	int i;
	for (i = 0; i < s_clients.used; i++)
	{
		if (s_clients.items[i]->player == p) return s_clients.items[i];
	}

	return NULL;
}*/

void client_add_packet(struct client_t *c, struct packet_t *p)
{
	/* Don't add packets for closed sockets */
	if (c->close) {
		free(p);
		return;
	}

	if (p == NULL)
	{
		LOG("[client] client_add_packet(): Tried to queue NULL packet\n");
		return;
	}

	pthread_mutex_lock(&c->packet_send_mutex);
	*c->packet_send_end = p;
	c->packet_send_end = &p->next;

	c->packet_send_count++;
	pthread_mutex_unlock(&c->packet_send_mutex);
}

void client_notify(struct client_t *c, const char *message)
{
	/* One char extra to hold a NUL, which minecraft doesn't need */
	char buf[65];
	const char *bufp = message;
	const char *bufe = message + strlen(message);
	char last_colour[3] = { 0, 0, 0};

	while (bufp < bufe)
	{
		const char *last_space = NULL;
		const char *bufpp = bufp;
		while (bufpp - bufp < sizeof buf - (last_colour[0] == 0 ? 1 : 3))
		{
			if (*bufpp == '\0' || *bufpp == '\n' || bufpp - bufp >= sizeof buf - (last_colour[0] == 0 ? 2 : 4))
			{
				last_space = bufpp;
				last_colour[1] = last_colour[2];
				break;
			}
			else if (*bufpp == ' ') {
				last_space = bufpp;
				last_colour[1] = last_colour[2];
			}
			else if (*bufpp == '&')
			{
				last_colour[2] = *(bufpp + 1);
			}
			bufpp++;
		}

		/* If string starts with a colour code, don't continue the previous colour. */
		if (bufp[0] == '&') last_colour[0] = 0;

		memset(buf, 0, sizeof buf);
		if (last_colour[0] == 'f') last_colour[0] = 0;
		if (last_colour[0] != 0)
		{
			buf[0] = '&';
			buf[1] = last_colour[0];
		}
		memcpy(buf + (last_colour[0] == 0 ? 0 : 2), bufp, last_space - bufp);

		/* Check if string ends with a colour code, and remove. */
		size_t l = (last_colour[0] == 0 ? 0 : 2) + (last_space - bufp) - 1;

		for (; l > 1; l--)
		{
			if (buf[l] == '&' || buf[l] == ' ')
			{
				buf[l] = '\0';
			}
			else if (buf[l - 1] == '&')
			{
				buf[l - 1] = '\0';
			}
			else
			{
				break;
			}
		}

		last_colour[0] = last_colour[1];

		client_add_packet(c, packet_send_message(0, buf));

		switch (*last_space)
		{
			case '\0':
			case '\n':
			case ' ':
				bufp = last_space + 1;
				break;

			default:
				bufp = last_space;
				break;
		}
	}
}

void client_notify_file(struct client_t *c, const char *filename)
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
		if (fgets(buf, sizeof buf, f) != NULL)
		{
			if (*buf != '\0') client_notify(c, buf);
		}
	}

	fclose(f);
}

bool client_notify_by_username(const char *username, const char *message)
{
	unsigned i;
	for (i = 0; i < s_clients.used; i++)
	{
		struct client_t *c = s_clients.items[i];
		if (c->player == NULL) continue;
		if (strcasecmp(c->player->username, username) == 0)
		{
			client_notify(c, message);
			return true;
		}
	}

	return false;
}

bool client_botcheck(struct client_t *c, char *message)
{
	bool bot = false;
	bool botuser = false;
	if (strcasestr(message, "place one brown mushroom") != NULL) bot = true;
	else if (strcasestr(message, "pasting file") != NULL) bot = true;
	else if (strcasestr(message, "place 2 brown shrooms") != NULL) bot = true;
	else if (strcasestr(message, "place 2 brown mushroom") != NULL) bot = true;
	else if (strcasestr(message, "place 2 shrooms") != NULL) bot = true;
	else if (strcasestr(message, ".reset to reset") != NULL) bot = true;
	else if (strcasestr(message, "!reset to reset") != NULL) bot = true;
	else if (strncasecmp(message, ".paste ", 7) == 0) botuser = true;
	else if (strncasecmp(message, ".say ", 5) == 0) botuser = true;
	else if (strncasecmp(message, ".copy ", 6) == 0) botuser = true;
	else if (strncasecmp(message, ".drawline ", 10) == 0) botuser = true;
	else if (strncasecmp(message, ".cuboid", 7) == 0) botuser = true;
	else if (strncasecmp(message, "!paste ", 7) == 0) botuser = true;
	else if (strncasecmp(message, "!say ", 5) == 0) botuser = true;
	else if (strncasecmp(message, "!copy ", 6) == 0) botuser = true;
	else if (strncasecmp(message, "!drawline ", 10) == 0) botuser = true;
	else if (strncasecmp(message, "!cuboid", 7) == 0) botuser = true;

	if (bot)
	{
		LOG("%s triggered bot detection: %s\n", c->player->username, message);
		playerdb_set_rank(c->player->username, RANK_BANNED, "auto-bot");
		net_close(c, "Bot detected");
		return true;
	}
	if (botuser)
	{
		LOG("%s triggered bot user detection: %s\n", c->player->username, message);
		playerdb_set_rank(c->player->username, RANK_BANNED, "auto-botuser");
		net_close(c, "Bot user detected");
		return true;
	}

	return false;
}

void client_process(struct client_t *c, char *message)
{
	/* Max length of username + message is 64 + 64 */
	char buf[128];

	if (message[0] == '\\' || message[0] == '/')
	{
		char *bufp = message + 1;
		char *param[10];
		int params = 0;

		memset(param, 0, sizeof param);

		for (;;)
		{
			size_t l = strcspn(bufp, " ");
			bool end = false;

			if (bufp[l] == '\0') end = true;
			bufp[l] = '\0';
			param[params++] = bufp;
			bufp += l + 1;

			if (end || params == 10) break;
		}

		if (!command_process(c, params, (const char **)param))
		{
			client_notify(c, "Unknown command");
			return;
		}
	}
	else
	{
		switch (message[0])
		{
			case '@': // Private message
			{
				size_t l = strcspn(message, " ");
				if (l != strlen(message))
				{
					message[l] = '\0';
					snprintf(buf, sizeof buf, "(%s:" TAG_WHITE " %s)", c->player->colourusername, message + l + 1);
					if (!client_notify_by_username(message + 1, buf))
					{
						client_notify(c, "User is offline");
					}
					else
					{
						client_notify(c, buf);
					}
				}
				return;
			}

			case '!':
				snprintf(buf, sizeof buf, "! %s:" TAG_WHITE " %s", c->player->colourusername, message + 1);
				call_hook(HOOK_CHAT, buf);
				net_notify_all(buf);
				return;

//			case ';':
//				snprintf(buf, sizeof buf, "* %s %s", c->player->colourusername, message + 1);
//				break;

			case '\'':
				snprintf(buf, sizeof buf, "%s:" TAG_WHITE " %s", playername(c->player, 1), message + 1);
				break;

			default:
				if (HasBit(c->player->flags, FLAG_GLOBAL))
				{
					snprintf(buf, sizeof buf, "! %s:" TAG_WHITE " %s", c->player->colourusername, message);
					call_hook(HOOK_CHAT, buf);
					net_notify_all(buf);
					return;
				}

				if (client_botcheck(c, message)) return;
				snprintf(buf, sizeof buf, "%s:" TAG_WHITE " %s", playername(c->player, 1), message);
				break;
		}

		call_hook(HOOK_CHAT, buf);
		if (!call_level_hook(EVENT_CHAT, c->player->level, c, message))
		{
			level_notify_all(c->player->level, buf);
		}
	}
}

void client_send_spawn(struct client_t *c, bool hiding)
{
	if (c->player == NULL || c->player->level == NULL) return;
	struct level_t *level = c->player->level;

	unsigned i;
	for (i = 0; i < MAX_CLIENTS_PER_LEVEL; i++)
	{
		struct client_t *c2 = level->clients[i];
		if (c2 != NULL && c2 != c && !c2->sending_level)
		{
			client_add_packet(c2, packet_send_spawn_player(c->player->levelid, playername(c->player, c2->player->namemode), &c->player->pos));
			//printf("Told %s (%d) about %s joining %s\n", level->clients[i]->player->username, i, c->player->username, level->name);
		}
	}
}

void client_send_despawn(struct client_t *c, bool hiding)
{
	if (c->player == NULL || c->player->level == NULL) return;
	struct level_t *level = c->player->level;

	unsigned i;
	for (i = 0; i < MAX_CLIENTS_PER_LEVEL; i++)
	{
		struct client_t *c2 = level->clients[i];
		if (c2 != NULL && c2 != c && !c2->sending_level)
		{
			client_add_packet(c2, packet_send_despawn_player(c->player->levelid));
			//printf("Told %s (%d) about %s leaving %s\n", level->clients[i]->player->username, i, c->player->username, level->name);
		}
	}
}

void client_spawn_players(struct client_t *c)
{
	if (c->player == NULL || c->player->level == NULL) return;
	struct level_t *l = c->player->level;

	unsigned i;
	for (i = 0; i < MAX_CLIENTS_PER_LEVEL; i++)
	{
		struct client_t *c2 = l->clients[i];
		if (c2 != NULL && c2 != c && !c2->hidden && !c2->sending_level)
		{
			client_add_packet(c, packet_send_spawn_player(c2->player->levelid, playername(c2->player, c->player->namemode), &c2->player->pos));
		}
	}
}

void client_despawn_players(struct client_t *c)
{
	if (c->player == NULL || c->player->level == NULL) return;
	struct level_t *l = c->player->level;

	unsigned i;
	for (i = 0; i < MAX_CLIENTS_PER_LEVEL; i++)
	{
		struct client_t *c2 = l->clients[i];
		if (c2 != NULL && c2 != c && !c2->hidden && !c2->sending_level)
		{
			client_add_packet(c, packet_send_despawn_player(c2->player->levelid));
		}
	}
}

pthread_mutex_t s_client_list_mutex;

bool client_inuse(struct client_t *c, bool inuse)
{
	pthread_mutex_lock(&s_client_list_mutex);
	if (!client_is_valid(c) || c->inuse < 0)
	{
		pthread_mutex_unlock(&s_client_list_mutex);
		return false;
	}

	c->inuse += inuse ? 1 : -1;
	pthread_mutex_unlock(&s_client_list_mutex);
	return true;
}
