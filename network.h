#ifndef NET_H
#define NET_H

#include <stdbool.h>

struct client_t;

void net_init(int port);
void net_deinit(void);

void net_run(void);
void net_close(struct client_t *c, const char *reason);

void net_notify_all(const char *message);
void net_notify_ops(const char *message);

struct sockaddr_in;
bool resolve(const char *hostname, int port, struct sockaddr_in *addr);

#endif /* NET_H */
