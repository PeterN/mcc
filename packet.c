#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "packet.h"
#include "network.h"
#include "client.h"
#include "player.h"

void packet_init(struct packet_t *p)
{
	memset(p->buffer, 0, sizeof p->buffer);
	p->loc = p->buffer;
	p->size = 0;
	p->pos = 0;
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
	int len;

	for (len = sizeof (string_t); len > 0; len--)
	{
		if (p->loc[len - 1] != ' ') break;
	}

	char *v = malloc(len + 1);
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
	*p->loc++ = ((uint16_t)data & 0xFF) >> 8;
	*p->loc++ = ((uint16_t)data & 0xFF);
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
	uint8_t version = packet_recv_byte(p);
	char *username = packet_recv_string(p);
	char *key = packet_recv_string(p);
	uint8_t unused = packet_recv_byte(p);

    struct player_t *player = player_get_by_name(username);
    if (player == NULL)
    {
        player = player_add(username);
        //message_queue("%s connected", username);
    }
    else
    {
        net_close(client_get_by_player(player), false);
        //message_queue("%s reconnected", username);
    }

    c->player = player;

	free(username);
	free(key);
}

void packet_recv_set_block(struct client_t *c, struct packet_t *p)
{
	int16_t x = packet_recv_short(p);
	int16_t y = packet_recv_short(p);
	int16_t z = packet_recv_short(p);
	uint8_t m = packet_recv_byte(p);
	uint8_t t = packet_recv_byte(p);

	if (m > 1)
	{
		net_close(c, true);
		return;
	}
}

void packet_recv_position(struct client_t *c, struct packet_t *p)
{
	uint8_t player_id = packet_recv_byte(p);
	int16_t x = packet_recv_short(p);
	int16_t y = packet_recv_short(p);
	int16_t z = packet_recv_short(p);
	uint8_t heading = packet_recv_byte(p);
	uint8_t pitch = packet_recv_byte(p);

	if (player_id != 0xFF)
	{
		net_close(c, true);
		return;
	}

//	player_move(c->player, x, y, z, heading, pitch);
}

void packet_recv_message(struct client_t *c, struct packet_t *p)
{
	uint8_t unused = packet_recv_byte(p);
	char *message = packet_recv_string(p);

	printf("Player said: %s\n", message);
}

void packet_recv(struct client_t *c, struct packet_t *p)
{
	uint8_t type = packet_recv_byte(p);
	printf("Woo! 0x%02X - %lu\n", type, p->pos);

	switch (type)
	{
		case 0x00: packet_recv_player_id(c, p); break;
		case 0x05: packet_recv_set_block(c, p); break;
		case 0x08: packet_recv_position(c, p); break;
		case 0x0D: packet_recv_message(c, p); break;
		default: break;
	}

	free(p);
}

/* Sending packets */

void packet_send_player_id(uint8_t protocol, const char *server_name, const char *server_motd, uint8_t user_type)
{
	struct packet_t p;

	packet_init(&p);
	packet_send_byte(&p, 0x00);
	packet_send_byte(&p, protocol);
	packet_send_string(&p, server_name);
	packet_send_string(&p, server_motd);
	packet_send_byte(&p, user_type);
}

void packet_send_ping()
{
	struct packet_t p;

	packet_init(&p);
	packet_send_byte(&p, 0x01);
}

void packet_send_level_initialize()
{
	struct packet_t p;

	packet_init(&p);
	packet_send_byte(&p, 0x02);
}

void packet_send_level_data_chunk(int16_t chunk_length, uint8_t *data, uint8_t percent)
{
	struct packet_t p;

	packet_init(&p);
	packet_send_byte(&p, 0x03);
	packet_send_short(&p, chunk_length);
	packet_send_byte_array(&p, data, chunk_length);
	packet_send_byte(&p, percent);
}

void packet_send_level_finalize(int16_t x, int16_t y, int16_t z)
{
	struct packet_t p;

	packet_init(&p);
	packet_send_byte(&p, 0x04);
	packet_send_short(&p, x);
	packet_send_short(&p, y);
	packet_send_short(&p, z);
}

void packet_send_set_block(int16_t x, int16_t y, int16_t z, uint8_t type)
{
	struct packet_t p;

	packet_init(&p);
	packet_send_byte(&p, 0x06);
	packet_send_short(&p, x);
	packet_send_short(&p, y);
	packet_send_short(&p, z);
	packet_send_byte(&p, type);
}

void packet_send_spawn_player(uint8_t player_id, const char *player_name, int16_t x, int16_t y, int16_t z, uint8_t heading, uint8_t pitch)
{
	struct packet_t p;

	packet_init(&p);
	packet_send_byte(&p, 0x07);
	packet_send_byte(&p, player_id);
	packet_send_string(&p, player_name);
	packet_send_short(&p, x);
	packet_send_short(&p, y);
	packet_send_short(&p, z);
	packet_send_byte(&p, heading);
	packet_send_byte(&p, pitch);
}

void packet_send_teleport_player(uint8_t player_id, int16_t x, int16_t y, int16_t z, uint8_t heading, uint8_t pitch)
{
	struct packet_t p;

	packet_init(&p);
	packet_send_byte(&p, 0x08);
	packet_send_byte(&p, player_id);
	packet_send_short(&p, x);
	packet_send_short(&p, y);
	packet_send_short(&p, z);
	packet_send_byte(&p, heading);
	packet_send_byte(&p, pitch);
}

void packet_send_despawn_player(uint8_t player_id)
{
	struct packet_t p;

	packet_init(&p);
	packet_send_byte(&p, 0x0C);
	packet_send_byte(&p, player_id);
}

void packet_send_message(uint8_t player_id, const char *message)
{
	struct packet_t p;

	packet_init(&p);
	packet_send_byte(&p, 0x0D);
	packet_send_byte(&p, player_id);
	packet_send_string(&p, message);
}

void packet_send_disconnect_player(const char *reason)
{
	struct packet_t p;

	packet_init(&p);
	packet_send_byte(&p, 0x0E);
	packet_send_string(&p, reason);
}

void packet_send_update_user_type(uint8_t player_id, uint8_t user_type)
{
	struct packet_t p;

	packet_init(&p);
	packet_send_byte(&p, 0x0F);
	packet_send_byte(&p, player_id);
	packet_send_byte(&p, user_type);
}

