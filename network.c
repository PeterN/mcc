#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "client.h"
#include "level.h"
#include "packet.h"
#include "player.h"
#include "network.h"
#include "mcc.h"
#include "playerdb.h"

static inline bool socket_t_compare(const struct socket_t *a, const struct socket_t *b)
{
	return a->fd == b->fd;
}

LIST(socket, struct socket_t, socket_t_compare)
static struct socket_list_t s_sockets;

void register_socket(int fd, socket_func_t socket_func, void *arg)
{
	struct socket_t s;
	s.fd = fd;
	s.socket_func = socket_func;
	s.arg = arg;

	socket_list_add(&s_sockets, s);
}

void deregister_socket(int fd)
{
	struct socket_t s;
	s.fd = fd;

	socket_list_del_item(&s_sockets, s);
}

bool resolve(const char *hostname, int port, struct sockaddr_in *addr)
{
	struct addrinfo *ai;
	struct addrinfo hints;
	memset(&hints, 0, sizeof hints);
	hints.ai_family   = AF_INET;
	hints.ai_flags    = AI_ADDRCONFIG;
	hints.ai_socktype = SOCK_STREAM;

	char port_name[6];
	snprintf(port_name, sizeof port_name, "%u", port);

	int e = getaddrinfo(hostname, port_name, &hints, &ai);

	if (e != 0)
	{
		perror("getaddrinfo");
		return false;
	}

	struct addrinfo *runp;
	for (runp = ai; runp != NULL; runp = runp->ai_next)
	{
		struct sockaddr_in *ai_addr = (struct sockaddr_in *)runp->ai_addr;

		/* Take the first address */
		*addr = *ai_addr;
		break;
	}

	freeaddrinfo(ai);

	return true;
}

bool getip(const struct sockaddr *addr, size_t addr_len, char *ip, size_t ip_len)
{
	int res = getnameinfo(addr, addr_len, ip, ip_len, NULL, 0, NI_NUMERICHOST | NI_NUMERICSERV);
	if (res != 0)
	{
		perror("getnameinfo");
	}
	return res == 0;
}

static struct sockaddr_in serv_addr;

static int s_listenfd;

void net_set_nonblock(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void net_init(int port)
{
	s_listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if (s_listenfd < 0)
	{
		perror("socket");
		return;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(port);

	if (bind(s_listenfd, (struct sockaddr *)&serv_addr, sizeof serv_addr) < 0)
	{
		perror("bind");
		return;
	}

	if (listen(s_listenfd, 1) != 0)
	{
		close(s_listenfd);
		perror("listen");
		return;
	}

	net_set_nonblock(s_listenfd);
}

void net_close(struct client_t *c, const char *reason)
{
	char buf[128];
	struct packet_t *p;
	unsigned packets = 0;

	/* Remove all queued packets */
	while (c->packet_send != NULL)
	{
		p = c->packet_send;
		c->packet_send = p->next;
		free(p);

		packets++;
	}

	if (packets > 0)
	{
		LOG("Remove %u packets from queue\n", packets);
	}

	/* Send client disconnect message straight away */
	if (reason != NULL)
	{
		p = packet_send_disconnect_player(reason);
		send(c->sock, p->buffer, p->loc - p->buffer, MSG_NOSIGNAL);
		free(p);
	}

	close(c->sock);

	/* Mark client for deletion */
	c->close = true;

	if (c->player == NULL)
	{
		LOG("Closing connection from %s: %s\n", c->ip, reason == NULL ? "closed" : reason);
	}
	else
	{
		unsigned i;
		for (i = 0; i < s_clients.used; i++)
		{
			struct client_t *client = s_clients.items[i];
			if (client->player != NULL && client->player->following == c->player)
			{
				snprintf(buf, sizeof buf, "Stopped following %s", c->player->username);
				client_notify(client, buf);

				client->player->following = NULL;
			}
		}

		if (c->player->level != NULL)
		{
			client_send_despawn(c, false);
			c->player->level->clients[c->player->levelid] = NULL;
		}

		if (reason == NULL)
		{
			snprintf(buf, sizeof buf, TAG_RED "- %s" TAG_YELLOW " disconnected", c->player->colourusername);
		}
		else
		{
			snprintf(buf, sizeof buf, TAG_RED "- %s" TAG_YELLOW " disconnected (%s)", c->player->colourusername, reason);
		}
		call_hook(HOOK_CHAT, buf);
		net_notify_all(buf);

		LOG("Closing connection from %s - %s (%d): %s\n", c->ip, c->player->username, c->player->globalid, reason);
		player_del(c->player);
	}
}

static void net_packetsend(struct client_t *c)
{
	int res;
	struct packet_t *p = c->packet_send;

	if (p == NULL) return;

	while (true)
	{
		p = c->packet_send;

		res = send(c->sock, p->buffer, p->loc - p->buffer, MSG_NOSIGNAL);
		if (res == -1)
		{
			//if (errno != EWOULDBLOCK)
			//{
				perror("send");
			//}
			break;
		}

		c->packet_send = p->next;
		free(p);

		if (c->packet_send == NULL) break;
	}
}

static void net_packetrecv(struct client_t *c)
{
	int res;
	struct packet_t *p;

	if (c->packet_recv == NULL)
	{
		c->packet_recv = packet_init(1500);
	}

	p = c->packet_recv;

	/* We need to read the packet type to determine packet size! */
	if (p->size == 0) p->size = 1;

	while (p->pos < p->size)
	{
		res = recv(c->sock, p->buffer + p->pos, p->size - p->pos, 0);
		if (res == -1)
		{
			if (errno != EWOULDBLOCK)
			{
				perror("recv");
			}
			return;
		}
		else if (res == 0)
		{
			/* Normal disconnect? */
			net_close(c, NULL);
			return;
		}

		if (p->size == 1)
		{
			int s = packet_recv_size(p->buffer[0]);
			if (s == -1)
			{
				char buf[64];
				snprintf(buf, sizeof buf, "unrecognised packet type 0x%02X!\n", p->buffer[0]);
				net_close(c, buf);
				return;
			}
			p->size = s;
		}
		p->pos += res;
	}

	c->packet_recv = NULL;

	packet_recv(c, p);
}

void net_run()
{
	size_t clients = s_clients.used;
	unsigned i;
	int n;
	fd_set read_fd, write_fd;
	struct timeval tv;

	for (i = 0; i < clients; i++)
	{
		if (s_clients.items[i]->close)
		{
			free(s_clients.items[i]);
			client_list_del_index(&s_clients, i);
			/* Restart :/ */
			i = -1;
			clients = s_clients.used;
		}
	}

	FD_ZERO(&read_fd);
	FD_ZERO(&write_fd);

	/* Add listen socket */
	FD_SET(s_listenfd, &read_fd);

	/* Add service sockets */
	for (i = 0; i < s_sockets.used; i++)
	{
		int fd = s_sockets.items[i].fd;
		FD_SET(fd, &read_fd);
		FD_SET(fd, &write_fd);
	}

	/* Add clients */
	for (i = 0; i < clients; i++)
	{
		FD_SET(s_clients.items[i]->sock, &read_fd);
		FD_SET(s_clients.items[i]->sock, &write_fd);
	}

	tv.tv_sec = 0;
	tv.tv_usec = 1000;

	n = select(FD_SETSIZE, &read_fd, &write_fd, NULL, &tv);
	if (n == -1)
	{
		perror("select");
		return;
	}

	if (FD_ISSET(s_listenfd, &read_fd))
	{
		while (1)
		{
			struct client_t *c;
			struct sockaddr_storage sin;
			socklen_t sin_len = sizeof sin;

			/* This better be nonblocking! */
			int fd = accept(s_listenfd, (struct sockaddr *)&sin, &sin_len);
			if (fd == -1) break;

			c = calloc(1, sizeof *c);
			c->sock = fd;
			c->sin = sin;
//			c->sin_len = sin_len;

			getip((struct sockaddr *)&sin, sin_len, c->ip, sizeof c->ip);
			LOG("network: accepted connection from %s\n", c->ip);

			if (playerdb_check_ban(c->ip))
			{
				net_close(c, "Banned");
				free(c);
			}
			else
			{
				client_list_add(&s_clients, c);
			}
		}
	}

	for (i = 0; i < s_sockets.used; i++)
	{
		struct socket_t *s = &s_sockets.items[i];
		int fd = s->fd;
		bool can_write = FD_ISSET(fd, &write_fd);
		bool can_read  = FD_ISSET(fd, &read_fd);

		if (can_write || can_read)
		{
			s->socket_func(fd, can_write, can_read, s->arg);
		}
	}

	for (i = 0; i < clients; i++)
	{
		struct client_t *c = s_clients.items[i];
		if (FD_ISSET(c->sock, &write_fd))
		{
			net_packetsend(c);
		}
		if (FD_ISSET(c->sock, &read_fd))
		{
			net_packetrecv(c);
		}
	}
}

void net_notify_all(const char *message)
{
	unsigned i;
	for (i = 0; i < s_clients.used; i++)
	{
		struct client_t *c = s_clients.items[i];
		client_notify(c, message);
	}

	LOG("%s\n", message);
}
