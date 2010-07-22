#ifndef NET_H
#define NET_H

#include <stdbool.h>

struct client_t;

typedef void(*socket_func_t)(int fd, bool can_read, bool can_write, void *arg);

struct socket_t
{
	int fd;
	socket_func_t socket_func;
	void *arg;
};

void register_socket(int fd, socket_func_t socket_func, void *arg);
void deregister_socket(int fd);

void net_init(int port);
void net_run();
void net_close(struct client_t *c, const char *reason);

void net_notify_all(const char *message);

void net_set_nonblock(int fd);

struct sockaddr_in;
bool resolve(const char *hostname, int port, struct sockaddr_in *addr);

#endif /* NET_H */
