#ifndef CLIENT_H
#define CLIENT_H

#include <stdbool.h>
#include "packet.h"

struct client_t
{
	int sock;
	bool writable;
	bool close;
	struct packet_t *packet_recv;
};

#endif /* CLIENT_H */
