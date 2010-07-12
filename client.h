#ifndef CLIENT_H
#define CLIENT_H

#include <stdbool.h>
#include <netinet/in.h>
#include "list.h"
#include "packet.h"

struct packet_t;
struct player_t;

struct client_t
{
	int sock;
	bool writable;
	bool close;
	bool waiting_for_level;

	struct sockaddr_storage sin;

	struct packet_t *packet_recv;
	struct packet_t *packet_send;
	struct player_t *player;
};

static inline bool client_t_compare(struct client_t *a, struct client_t *b)
{
	return a->sock == b->sock;
}
LIST(client, struct client_t, client_t_compare)

extern struct client_list_t s_clients;

struct client_t *client_get_by_player(struct player_t *p);

void client_add_packet(struct client_t *c, struct packet_t *p);
void client_process(struct client_t *c, char *message);

static inline void client_notify(struct client_t *c, const char *message)
{
    client_add_packet(c, packet_send_message(0xFF, message));
}

#endif /* CLIENT_H */
