#ifndef CLIENT_H
#define CLIENT_H

#include <stdbool.h>
#include <netinet/in.h>
#include <pthread.h>
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
	bool sending_level;
	bool hidden;

	struct sockaddr_storage sin;
	char ip[INET6_ADDRSTRLEN];
	char *close_reason;

	struct packet_t *packet_recv;
	struct packet_t *packet_send;
	struct packet_t **packet_send_end;
	struct player_t *player;

	pthread_mutex_t packet_send_mutex;

	int packet_send_count;
	int inuse;
};

static inline bool client_t_compare(struct client_t **a, struct client_t **b)
{
	return (*a) == (*b);
}
LIST(client, struct client_t *, client_t_compare)

extern struct client_list_t s_clients;
extern pthread_mutex_t s_client_list_mutex;

struct client_t *client_get_by_player(struct player_t *p);

void client_add_packet(struct client_t *c, struct packet_t *p);
void client_process(struct client_t *c, char *message);
void client_send_spawn(struct client_t *c, bool hiding);
void client_send_despawn(struct client_t *c, bool hiding);

void client_spawn_players(struct client_t *c);
void client_despawn_players(struct client_t *c);

void client_notify(struct client_t *c, const char *message);
void client_notify_file(struct client_t *c, const char *filename);

static inline bool client_is_valid(struct client_t *c)
{
	return client_list_contains(&s_clients, c);
}

bool client_inuse(struct client_t *c, bool inuse);

#endif /* CLIENT_H */
