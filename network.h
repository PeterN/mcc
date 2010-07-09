#ifndef NET_H
#define NET_H

#include <stdbool.h>

struct client_t;

void net_init();
void net_run();
void net_close(struct client_t *c, bool remove_player);

void heartbeat_start();

#endif /* NET_H */
