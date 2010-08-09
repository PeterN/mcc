#ifndef CLIENT_H
#define CLIENT_H

#include <stdbool.h>
#include <netinet/in.h>
#include "hook.h"
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
	bool hidden;

	struct sockaddr_storage sin;
	char ip[16];
	char *close_reason;

	struct packet_t *packet_recv;
	struct packet_t *packet_send;
	struct packet_t **packet_send_end;
	struct player_t *player;

	int packet_send_count;
};

static inline bool client_t_compare(struct client_t **a, struct client_t **b)
{
	return (*a) == (*b);
}
LIST(client, struct client_t *, client_t_compare)

extern struct client_list_t s_clients;

struct client_t *client_get_by_player(struct player_t *p);

void client_add_packet(struct client_t *c, struct packet_t *p);
void client_process(struct client_t *c, char *message);
void client_send_spawn(struct client_t *c, bool hiding);
void client_send_despawn(struct client_t *c, bool hiding);

void client_notify(struct client_t *c, const char *message);

void client_info();

#endif /* CLIENT_H */
