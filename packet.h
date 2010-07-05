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
};

void packet_init(struct packet_t *p);

size_t packet_recv_size(uint8_t type);
void packet_recv(struct client_t *c, struct packet_t *p);

#endif /* PACKET_H */
