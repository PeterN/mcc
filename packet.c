#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "packet.h"
#include "network.h"
#include "client.h"
#include "commands.h"
#include "player.h"
#include "level.h"
#include "mcc.h"

struct packet_t *packet_init(size_t len)
{
	struct packet_t *p = malloc(sizeof *p + len);
	if (p == NULL)
	{
		LOG("[packet] packet_init(): couldn't allocate %zu bytes\n", sizeof *p + len);
		return NULL;
	}

	memset(p->buffer, 0, len);
	p->loc = p->buffer;
	p->size = 0;
	p->pos = 0;
	p->next = NULL;

	return p;
}

/* Low-level packet receiving */

static uint8_t packet_recv_byte(struct packet_t *p)
{
	return *p->loc++;
}

static int16_t packet_recv_short(struct packet_t *p)
{
	uint8_t v = *p->loc++;
	return (v << 8) | *p->loc++;
}

static char *packet_recv_string(struct packet_t *p)
{
	/* Strings are spaced padded, so we need to count down to get the length */
	unsigned len;

	for (len = sizeof (string_t); len > 0; len--)
	{
		if (p->loc[len - 1] != ' ') break;
	}

	char *v = malloc(len + 1);
	if (v == NULL)
	{
		LOG("[packet] packet_recv_string(): couldn't allocate %u bytes\n", len + 1);
		return NULL;
	}
	memcpy(v, p->loc, len);
	v[len] = '\0';

	p->loc += sizeof (string_t);

	return v;
}

/* Low-level packet sending */

static void packet_send_byte(struct packet_t *p, uint8_t data)
{
	*p->loc++ = data;
}

static void packet_send_short(struct packet_t *p, int16_t data)
{
	*p->loc++ = ((uint16_t)data >> 8) & 0xFF;;
	*p->loc++ =  (uint16_t)data	   & 0xFF;
}

static void packet_send_string(struct packet_t *p, const char *data)
{
	size_t i;

	size_t len = strlen(data);
	if (len > sizeof (string_t)) len = sizeof (string_t);
	for (i = 0; i < len; i++)
	{
		*p->loc++ = data[i];
	}

	/* Pad with spaces */
	for (; i < sizeof (string_t); i++)
	{
		*p->loc++ = ' ';
	}
}

static void packet_send_byte_array(struct packet_t *p, const uint8_t *data, int16_t length)
{
	size_t i;

	size_t len = length;
	if (len > sizeof (data_t)) len = sizeof (data_t);
	for (i = 0; i < len; i++)
	{
		*p->loc++ = data[i];
	}

	/* Pad with zeroes */
	for (; i < sizeof (data_t); i++)
	{
		*p->loc++ = 0x00;
	}
}

/* Receiving packets */

size_t packet_recv_size(uint8_t type)
{
	switch (type)
	{
		case 0x00: return 131;
		case 0x05: return 9;
		case 0x08: return 10;
		case 0x0D: return 66;
		default: return -1;
	}
}

void packet_recv_player_id(struct client_t *c, struct packet_t *p)
{
	char buf[64];

	if (c->player != NULL)
	{
		net_close(c, "Already logged in");
		return;
	}

	uint8_t version = packet_recv_byte(p);
	if (version != 7)
	{
		net_close(c, "Invalid protocol");
		return;
	}

	char *username = packet_recv_string(p);
	char *key = packet_recv_string(p);
	/* uint8_t unused = */ packet_recv_byte(p);

	LOG("%s - %s\n", username, key);

	const char *type;

	struct player_t *player = player_get_by_name(username, false);
	if (player == NULL)
	{
		type = "connected";
	}
	else
	{
		type = "reconnected";
		net_close(player->client, type);
	}

	bool newuser;
	int identified;
	player = player_add(username, c, &newuser, &identified);

	/* No longer need these */
	free(username);
	free(key);

	if (player == NULL)
	{
		net_close(c, "Cannot get global id");
		return;
	}

	snprintf(buf, sizeof buf, TAG_GREEN "+ %s" TAG_YELLOW " %s", player->colourusername, type);

	if (player->rank == RANK_BANNED)
	{
		player_del(player);
		net_close(c, "Banned");
		return;
	}

	if (player->rank < RANK_REGULAR && g_server.players > g_server.max_players)
	{
		player_del(player);
		net_close(c, "Too many players online, please try later");
		return;
	}

	c->player = player;
	player->client = c;

	client_add_packet(c, packet_send_player_id(7, g_server.name, g_server.motd, (c->player->rank >= RANK_OP) ? 0x64 : 0));

	call_hook(HOOK_CHAT, buf);
	net_notify_all(buf);
	if (identified == 2)
	{
		client_notify(c, TAG_LIME "Automatically identified.");
	}
	else if (identified == 1)
	{
		client_notify(c, TAG_YELLOW "You must identify to use privileged commands.");
	}

	player_change_level(c->player, NULL);

	client_notify_file(c, "motd.txt");

	if (newuser)
	{
		client_notify_file(c, "rules.txt");
	}
}

void packet_recv_set_block(struct client_t *c, struct packet_t *p)
{
	int16_t x = packet_recv_short(p);
	int16_t y = packet_recv_short(p);
	int16_t z = packet_recv_short(p);
	uint8_t m = packet_recv_byte(p);
	uint8_t t = packet_recv_byte(p);

	if (c->player == NULL)
	{
		net_close(c, "Not logged in");
		return;
	}

	if (m > 1)
	{
		net_close(c, "Invalid set block data");
		return;
	}

	if (c->sending_level) return;

	if (c->player->rank < RANK_ADV_BUILDER)
	{
		if (c->player->speed > 500)
		{
			net_close(c, "Anti-grief: average speed too high to place blocks");
			return;
		}
		else if (c->player->speed > 300)
		{
			c->player->warnings++;
			client_notify(c, "Warning! Your average speed is high.");
		}

		if (player_check_spam(c->player))
		{
			return;
		}
	}

	level_change_block(c->player->level, c, x, y, z, m, t, true);
}

void packet_recv_position(struct client_t *c, struct packet_t *p)
{
	uint8_t player_id = packet_recv_byte(p);

	struct position_t pos;
	pos.x = packet_recv_short(p);
	pos.y = packet_recv_short(p);
	pos.z = packet_recv_short(p);
	pos.h = packet_recv_byte(p);
	pos.p = packet_recv_byte(p);

	if (c->player == NULL)
	{
		net_close(c, "Not logged in");
		return;
	}

	if (player_id != 0xFF)
	{
		net_close(c, "Invalid position data");
		return;
	}

	if (c->sending_level) return;

	player_move(c->player, &pos);
}

void packet_recv_message(struct client_t *c, struct packet_t *p)
{
	/*uint8_t unused =*/ packet_recv_byte(p);
	char *message = packet_recv_string(p);

	if (c->player == NULL)
	{
		net_close(c, "Not logged in");
		return;
	}

	client_process(c, message);

	free(message);
}

void packet_recv(struct client_t *c, struct packet_t *p)
{
	uint8_t type = packet_recv_byte(p);

	switch (type)
	{
		case 0x00: packet_recv_player_id(c, p); break;
		case 0x05: packet_recv_set_block(c, p); break;
		case 0x08: packet_recv_position(c, p); break;
		case 0x0D: packet_recv_message(c, p); break;
		default: break;
	}

	/* Reset packet without causing free/malloc */
	p->size = 0;
	p->pos = 0;
	p->loc = p->buffer;
}

/* Sending packets */

struct packet_t *packet_send_player_id(uint8_t protocol, const char *server_name, const char *server_motd, uint8_t user_type)
{
	struct packet_t *p = packet_init(131);
	if (p == NULL) return NULL;

	packet_send_byte(p, 0x00);
	packet_send_byte(p, protocol);
	packet_send_string(p, server_name);
	packet_send_string(p, server_motd);
	packet_send_byte(p, user_type);

	return p;
}

struct packet_t *packet_send_ping(void)
{
	struct packet_t *p = packet_init(1);
	if (p == NULL) return NULL;

	packet_send_byte(p, 0x01);

	return p;
}

struct packet_t *packet_send_level_initialize(void)
{
	struct packet_t *p = packet_init(1);
	if (p == NULL) return NULL;

	packet_send_byte(p, 0x02);

	return p;
}

struct packet_t *packet_send_level_data_chunk(int16_t chunk_length, uint8_t *data, uint8_t percent)
{
	struct packet_t *p = packet_init(1028);
	if (p == NULL) return NULL;

	packet_send_byte(p, 0x03);
	packet_send_short(p, chunk_length);
	packet_send_byte_array(p, data, chunk_length);
	packet_send_byte(p, percent);

	return p;
}

struct packet_t *packet_send_level_finalize(int16_t x, int16_t y, int16_t z)
{
	struct packet_t *p = packet_init(7);
	if (p == NULL) return NULL;

	packet_send_byte(p, 0x04);
	packet_send_short(p, x);
	packet_send_short(p, y);
	packet_send_short(p, z);

	return p;
}

struct packet_t *packet_send_set_block(int16_t x, int16_t y, int16_t z, uint8_t type)
{
	struct packet_t *p = packet_init(8);
	if (p == NULL) return NULL;

	packet_send_byte(p, 0x06);
	packet_send_short(p, x);
	packet_send_short(p, y);
	packet_send_short(p, z);
	packet_send_byte(p, type);

	return p;
}

struct packet_t *packet_send_spawn_player(uint8_t player_id, const char *player_name, const struct position_t *pos)
{
	struct packet_t *p = packet_init(74);
	if (p == NULL) return NULL;

	packet_send_byte(p, 0x07);
	packet_send_byte(p, player_id);
	packet_send_string(p, player_name);
	packet_send_short(p, pos->x);
	packet_send_short(p, pos->y);
	packet_send_short(p, pos->z);
	packet_send_byte(p, pos->h);
	packet_send_byte(p, pos->p);

	return p;
}

struct packet_t *packet_send_teleport_player(uint8_t player_id, const struct position_t *pos)
{
	struct packet_t *p = packet_init(10);
	if (p == NULL) return NULL;

	packet_send_byte(p, 0x08);
	packet_send_byte(p, player_id);
	packet_send_short(p, pos->x);
	packet_send_short(p, pos->y);
	packet_send_short(p, pos->z);
	packet_send_byte(p, pos->h);
	packet_send_byte(p, pos->p);

	return p;
}

struct packet_t *packet_send_full_position_update(uint8_t player_id, int8_t dx, int8_t dy, int8_t dz, const struct position_t *pos)
{
	struct packet_t *p = packet_init(7);
	if (p == NULL) return NULL;

	packet_send_byte(p, 0x09);
	packet_send_byte(p, player_id);
	packet_send_byte(p, dx);
	packet_send_byte(p, dy);
	packet_send_byte(p, dz);
	packet_send_byte(p, pos->h);
	packet_send_byte(p, pos->p);

	return p;
}

struct packet_t *packet_send_position_update(uint8_t player_id, int8_t dx, int8_t dy, int8_t dz)
{
	struct packet_t *p = packet_init(5);
	if (p == NULL) return NULL;

	packet_send_byte(p, 0x0A);
	packet_send_byte(p, player_id);
	packet_send_byte(p, dx);
	packet_send_byte(p, dy);
	packet_send_byte(p, dz);

	return p;
}

struct packet_t *packet_send_orientation_update(uint8_t player_id, const struct position_t *pos)
{
	struct packet_t *p = packet_init(4);
	if (p == NULL) return NULL;

	packet_send_byte(p, 0x0B);
	packet_send_byte(p, player_id);
	packet_send_byte(p, pos->h);
	packet_send_byte(p, pos->p);

	return p;
}

struct packet_t *packet_send_despawn_player(uint8_t player_id)
{
	struct packet_t *p = packet_init(2);
	if (p == NULL) return NULL;

	packet_send_byte(p, 0x0C);
	packet_send_byte(p, player_id);

	return p;
}

struct packet_t *packet_send_message(uint8_t player_id, const char *message)
{
	struct packet_t *p = packet_init(66);
	if (p == NULL) return NULL;

	packet_send_byte(p, 0x0D);
	packet_send_byte(p, player_id);
	packet_send_string(p, message);

	return p;
}

struct packet_t *packet_send_disconnect_player(const char *reason)
{
	struct packet_t *p = packet_init(65);
	if (p == NULL) return NULL;

	packet_send_byte(p, 0x0E);
	packet_send_string(p, reason);

	return p;
}

struct packet_t *packet_send_update_user_type(uint8_t user_type)
{
	struct packet_t *p = packet_init(2);
	if (p == NULL) return NULL;

	packet_send_byte(p, 0x0F);
	packet_send_byte(p, user_type);

	return p;
}

