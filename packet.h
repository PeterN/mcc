#ifndef PACKET_H
#define PACKET_H

#include <stdint.h>

typedef char string_t[64];
typedef uint8_t data_t[1024];

struct client_t;

struct packet_t
{
	uint8_t buffer[1500];
	uint8_t *loc;
	size_t size;
	size_t pos;

	struct packet_t *next;
};

void packet_init(struct packet_t *p);

size_t packet_recv_size(uint8_t type);
void packet_recv(struct client_t *c, struct packet_t *p);

struct packet_t *packet_send_player_id(uint8_t protocol, const char *server_name, const char *server_motd, uint8_t user_type);
struct packet_t *packet_send_level_initialize();
struct packet_t *packet_send_level_data_chunk(int16_t chunk_length, uint8_t *data, uint8_t percent);
struct packet_t *packet_send_level_finalize(int16_t x, int16_t y, int16_t z);

#endif /* PACKET_H */
