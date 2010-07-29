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

	struct packet_t **ip = &c->packet_send;
	while (*ip != NULL)
	{
		ip = &(*ip)->next;
	}

	*ip = p;
}

void client_notify(struct client_t *c, const char *message)
{
	char buf[64];
	const char *bufp = message;
	const char *bufe = message + strlen(message);

	while (bufp < bufe)
	{
		const char *last_space = NULL;
		const char *bufpp = bufp;
		while (bufpp - bufp < sizeof buf)
		{
			if (*bufpp == '\0' || *bufpp == '\n')
			{
				last_space = bufpp;
				break;
			}
			else if (*bufpp == ' ') last_space = bufpp;
			bufpp++;
		}

		memset(buf, 0, sizeof buf);
		memcpy(buf, bufp, last_space - bufp);

		client_add_packet(c, packet_send_message(0, buf));

		bufp = last_space + 1;
	}
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
	else if (strcasestr(message, "place 2 shrooms") != NULL) bot = true;
	else if (strncasecmp(message, ".paste ", 7) == 0) botuser = true;
	else if (strncasecmp(message, ".say ", 5) == 0) botuser = true;
	else if (strncasecmp(message, ".copy ", 6) == 0) botuser = true;
	else if (strncasecmp(message, ".drawline ", 10) == 0) botuser = true;
	else if (strncasecmp(message, "!paste ", 7) == 0) botuser = true;
	else if (strncasecmp(message, "!say ", 5) == 0) botuser = true;
	else if (strncasecmp(message, "!copy ", 6) == 0) botuser = true;
	else if (strncasecmp(message, "!drawline ", 10) == 0) botuser = true;

	if (bot)
	{
		LOG("%s triggered bot detection: %s\n", c->player->username, message);
		playerdb_set_rank(c->player->username, RANK_BANNED);
		net_close(c, "Bot detected");
		return true;
	}
	if (botuser)
	{
		LOG("%s triggered bot user detection: %s\n", c->player->username, message);
		playerdb_set_rank(c->player->username, RANK_BANNED);
		net_close(c, "Bot user detected");
		return true;
	}

	return false;
}

void client_process(struct client_t *c, char *message)
{
	/* Max length of username + message is 64 + 64 */
	char buf[128];

	if (message[0] == '!' || message[0] == '/')
	{
		char *bufp = message + 1;
		char *param[10];
		int params = 0;

		memset(param, 0, sizeof param);

		for (;;)
		{
			size_t l = strcspn(bufp, " ,");
			bool end = false;

			if (bufp[l] == '\0') end = true;
			bufp[l] = '\0';
			param[params++] = bufp;
			bufp += l + 1;

			if (end) break;
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
				size_t l = strcspn(message, " ,");
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

//			case ';':
//				snprintf(buf, sizeof buf, "* %s %s", c->player->colourusername, message + 1);
//				break;

			case '\'':
				snprintf(buf, sizeof buf, "%s:" TAG_WHITE " %s", c->player->colourusername, message + 1);
				break;

			default:
				if (client_botcheck(c, message)) return;
				snprintf(buf, sizeof buf, "%s:" TAG_WHITE " %s", c->player->colourusername, message);
				break;
		}

		call_hook(HOOK_CHAT, buf);
		call_level_hook(EVENT_CHAT, c->player->level, c, message);
		net_notify_all(buf);
	}
}

void client_send_spawn(struct client_t *c, bool hiding)
{
	if (c->player == NULL || c->player->level == NULL) return;
	struct level_t *level = c->player->level;

	unsigned i;
	for (i = 0; i < MAX_CLIENTS_PER_LEVEL; i++)
	{
		if (level->clients[i] != NULL && level->clients[i] != c)
		{
			client_add_packet(level->clients[i], packet_send_spawn_player(c->player->levelid, c->player->colourusername, &c->player->pos));
			//printf("Told %s (%d) about %s joining %s\n", level->clients[i]->player->username, i, c->player->username, level->name);
		}
	}

	call_level_hook(EVENT_SPAWN, level, c, c->player->hook_data);
	c->player->hook_data = NULL;
}

void client_send_despawn(struct client_t *c, bool hiding)
{
	if (c->player == NULL || c->player->level == NULL) return;
	struct level_t *level = c->player->level;

	unsigned i;
	for (i = 0; i < MAX_CLIENTS_PER_LEVEL; i++)
	{
		if (level->clients[i] != NULL && level->clients[i] != c)
		{
			client_add_packet(level->clients[i], packet_send_despawn_player(c->player->levelid));
			//printf("Told %s (%d) about %s leaving %s\n", level->clients[i]->player->username, i, c->player->username, level->name);
		}
	}

	call_level_hook(EVENT_DESPAWN, level, c, NULL);
}

void client_info()
{
	unsigned i;
	for (i = 0; i < s_clients.used; i++)
	{
		const struct client_t *c = s_clients.items[i];
		if (c->player == NULL)
		{
			printf("%d: Connecting...\n", i);
		}
		else if (c->player->level == NULL)
		{
			printf("%d: %s (%d)\n", i, c->player->username, c->player->globalid);
		}
		else
		{
			printf("%d: %s (%d) on %s (%d)\n", i, c->player->username, c->player->globalid, c->player->level->name, c->player->levelid);
		}
	}
}
