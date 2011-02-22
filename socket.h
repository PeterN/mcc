#ifndef SOCKET_H
#define SOCKET_H

#include <stdbool.h>

typedef void(*socket_func)(int fd, bool can_write, bool can_read, void *arg);

struct socket
{
	int fd;
	socket_func socket_func;
	void *arg;
};

void register_socket(int fd, socket_func socket_func, void *arg);
void deregister_socket(int fd);

void net_init(int port);
void socket_run(void);

void socket_set_nonblock(int fd);

void socket_flag_write(int fd);
void socket_clear_write(int fd);

#endif /* SOCKET_H */
