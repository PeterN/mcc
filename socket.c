#define USE_POLL

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#ifdef USE_POLL
#include <sys/epoll.h>
#endif /* USE_POLL */
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include "socket.h"
#include "list.h"
#include "mcc.h"

struct socket_t
{
	int fd;
	socket_func socket_func;
	void *arg;
};

static inline bool socket_compare(struct socket_t **a, struct socket_t **b)
{
	return (*a)->fd == (*b)->fd;
}

LIST(socket, struct socket_t *, socket_compare)
static struct socket_list_t s_sockets;
static pthread_mutex_t s_sockets_mutex;

#ifdef USE_POLL
static int s_epoll_fd = -1;
#endif /* USE_POLL */

static struct socket_t *socket_get_by_fd(int fd)
{
	size_t i;
	for (i = 0; i < s_sockets.used; i++)
	{
		struct socket_t *s = s_sockets.items[i];
		if (s->fd == fd) return s;
	}

	return NULL;
}

void register_socket(int fd, socket_func socket_func, void *arg)
{
	struct socket_t *s = malloc(sizeof *s);
	s->fd = fd;
	s->socket_func = socket_func;
	s->arg = arg;

	pthread_mutex_lock(&s_sockets_mutex);
	socket_list_add(&s_sockets, s);

#ifdef USE_POLL
	if (s_epoll_fd == -1)
	{
		s_epoll_fd = epoll_create(32);
		if (s_epoll_fd == -1) LOG("register_socket(): epoll_create: %s\n", strerror(errno));
	}

	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.ptr = s;
	int r = epoll_ctl(s_epoll_fd, EPOLL_CTL_ADD, fd, &ev);
	if (r == -1) LOG("register_socket(): epoll_ctl: %s\n", strerror(errno));
#endif /* USE_POLL */

	pthread_mutex_unlock(&s_sockets_mutex);
}

void socket_flag_write(int fd)
{
#ifdef USE_POLL
	struct epoll_event ev;
	ev.events = EPOLLIN | EPOLLOUT;
	ev.data.ptr = socket_get_by_fd(fd);;
	int r = epoll_ctl(s_epoll_fd, EPOLL_CTL_MOD, fd, &ev);
	if (r == -1) LOG("socket_flag_write(): epoll_ctl: %s\n", strerror(errno));
#endif /* USE_POLL */
}

void socket_clear_write(int fd)
{
#ifdef USE_POLL
	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.ptr = socket_get_by_fd(fd);
	int r = epoll_ctl(s_epoll_fd, EPOLL_CTL_MOD, fd, &ev);
	if (r == -1) LOG("socket_clear_write(): epoll_ctl: %s\n", strerror(errno));
#endif /* USE_POLL */
}

void deregister_socket(int fd)
{
	struct socket_t s;
	s.fd = fd;

	pthread_mutex_lock(&s_sockets_mutex);
	socket_list_del_item(&s_sockets, &s);
	pthread_mutex_unlock(&s_sockets_mutex);
}

void socket_deinit(void)
{
	if (s_sockets.used > 0)
	{
		LOG("[network] socket_deinit(): %zu sockets remaining in list\n", s_sockets.used);
	}

	close(s_epoll_fd);

	socket_list_free(&s_sockets);
}

void socket_set_nonblock(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	int res = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	if (res != 0)
	{
		LOG("[network] socket_set_nonblocK(): Could not set nonblocking IO: %s\n", strerror(errno));
	}

	int b = 1;
	res = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &b, sizeof b);
	if (res != 0)
	{
		LOG("[network] socket_set_nonblock(): Could not set TCP_NODELAY: %s\n", strerror(errno));
	}
}

#ifdef USE_POLL

#define EPOLL_EVENTS 256

void socket_run(void)
{
	struct epoll_event events[EPOLL_EVENTS];
	int n = epoll_wait(s_epoll_fd, events, EPOLL_EVENTS, 1);
	if (n == -1) LOG("socket_run(): epoll_wait: %s\n", strerror(errno));

	int i;
	for (i = 0; i < n; i++)
	{
		struct socket_t *s = events[i].data.ptr;
		if (s == NULL) continue;

		s->socket_func(s->fd, !!(events[i].events & EPOLLOUT), !!(events[i].events & EPOLLIN), s->arg);
	}
}

#else /* USE_POLL */

void socket_run(void)
{
	unsigned i;
	int n;
	fd_set read_fd, write_fd;
	struct timeval tv;

	FD_ZERO(&read_fd);
	FD_ZERO(&write_fd);

	/* Add service sockets */
	for (i = 0; i < s_sockets.used; i++)
	{
		int fd = s_sockets.items[i]->fd;
		FD_SET(fd, &read_fd);
		FD_SET(fd, &write_fd);
	}

	tv.tv_sec = 0;
	tv.tv_usec = 1000;

	n = select(FD_SETSIZE, &read_fd, &write_fd, NULL, &tv);
	if (n == -1)
	{
		LOG("select: %s\n", strerror(errno));
		return;
	}
	if (n == 0) return;

	for (i = 0; i < s_sockets.used; i++)
	{
		struct socket_t *s = s_sockets.items[i];
		if (s == NULL) continue;

		int fd = s->fd;
		bool can_write = FD_ISSET(fd, &write_fd);
		bool can_read  = FD_ISSET(fd, &read_fd);

		if (can_write || can_read)
		{
			s->socket_func(fd, can_write, can_read, s->arg);
		}
	}
}

#endif /* USE_POLL */
