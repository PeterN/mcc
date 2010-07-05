#ifndef NET_H
#define NET_H

struct client_t;

void net_init();
void net_run();
void net_close(struct client_t *c);

#endif /* NET_H */
