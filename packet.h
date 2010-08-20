#ifndef PACKET_H
#define PACKET_H

#include <stdint.h>
#include "position.h"

typedef char string_t[64];
typedef uint8_t data_t[1024];

struct client_t;

struct packet_t
{
	uint8_t *loc;
	size_t size;
	size_t pos;

	struct packet_t *next;
	uint8_t buffer[0];
};

struct packet_t *packet_init(size_t len);

size_t packet_recv_size(uint8_t type);
void packet_recv(struct client_t *c, struct packet_t *p);

struct packet_t *packet_send_player_id(uint8_t protocol, const char *server_name, const char *server_motd, uint8_t user_type);
struct packet_t *packet_send_ping();
struct packet_t *packet_send_level_initialize();
struct packet_t *packet_send_level_data_chunk(int16_t chunk_length, uint8_t *data, uint8_t percent);
struct packet_t *packet_send_level_finalize(int16_t x, int16_t y, int16_t z);
struct packet_t *packet_send_set_block(int16_t x, int16_t y, int16_t z, uint8_t type);
struct packet_t *packet_send_spawn_player(uint8_t player_id, const char *player_name, const struct position_t *pos);
struct packet_t *packet_send_teleport_player(uint8_t player_id, const struct position_t *pos);
struct packet_t *packet_send_full_position_update(uint8_t player_id, int8_t dx, int8_t dy, int8_t dz, const struct position_t *pos);
struct packet_t *packet_send_position_update(uint8_t player_id, int8_t dx, int8_t dy, int8_t dz);
struct packet_t *packet_send_orientation_update(uint8_t player_id, const struct position_t *pos);
struct packet_t *packet_send_despawn_player(uint8_t player_id);
struct packet_t *packet_send_message(uint8_t player_id, const char *message);
struct packet_t *packet_send_disconnect_player(const char *reason);
struct packet_t *packet_send_update_user_type(uint8_t user_type);

#endif /* PACKET_H */
