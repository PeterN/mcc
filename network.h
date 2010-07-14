#ifndef NET_H
#define NET_H

#include <stdbool.h>

struct client_t;

void net_init();
void net_run();
void net_close(struct client_t *c, const char *reason);

void net_notify_all(const char *message);

void net_set_nonblock(int fd);

struct sockaddr_in;
bool resolve(const char *hostname, int port, struct sockaddr_in *addr);

#endif /* NET_H */
