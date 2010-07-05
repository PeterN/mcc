#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "list.h"
#include "client.h"
#include "packet.h"

static bool client_t_compare(struct client_t *a, struct client_t *b)
{
	return a->sock == b->sock;
}

LIST(client_t, client_t_compare)
static struct client_t_list_t s_clients;

static int s_listenfd;

static void net_set_nonblock(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void net_init()
{
	static struct sockaddr_in serv_addr;

	s_listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if (s_listenfd < 0)
	{
		perror("socket");
		return;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(25565);

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

void net_close(struct client_t *c)
{
	printf("Closing connection\n");
	close(c->sock);

	/* Mark client for deletion */
	c->close = true;
}

static void net_packetrecv(struct client_t *c)
{
	size_t res;
	struct packet_t *p;

	if (c->packet_recv == NULL)
	{
		c->packet_recv = malloc(sizeof *c->packet_recv);
		packet_init(c->packet_recv);
	}

	p = c->packet_recv;

	/* We need to read the packet type to determine packet size! */
	if (p->size == 0) p->size = 1;

	while (p->pos < p->size)
	{
		res = recv(c->sock, p->buffer + p->pos, p->size - p->pos, 0);
		printf("got %d bytes\n", res);
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
			net_close(c);
			return;
		}

		if (p->size == 1)
		{
			p->size = packet_recv_size(p->buffer[0]);
			if (p->size == -1)
			{
				printf("Unrecognised packet type 0x%02X!\n", p->buffer[0]);
				net_close(c);
				return;
			}
			printf("Packet type 0x%02X, expecting %d bytes\n", p->buffer[0], p->size);
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
		if (s_clients.items[i].close)
		{
			client_t_list_del(&s_clients, s_clients.items[i]);
			/* Restart :/ */
			i = -1;
			clients = s_clients.used;
		}
	}

	FD_ZERO(&read_fd);
	FD_ZERO(&write_fd);

	/* Add listen socket */
	FD_SET(s_listenfd, &read_fd);

	/* Add clients */
	for (i = 0; i < clients; i++)
	{
		FD_SET(s_clients.items[i].sock, &read_fd);
		FD_SET(s_clients.items[i].sock, &write_fd);
	}

	tv.tv_sec = 0;
	tv.tv_usec = 0;

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
			/* This better be nonblocking! */
			int fd = accept(s_listenfd, NULL, NULL);
			if (fd == -1) break;

			{
				struct client_t c;
				c.sock = fd;
				c.writable = false;
				c.close = false;
				c.packet_recv = NULL;
				client_t_list_add(&s_clients, c);
			}
		}
	}

	for (i = 0; i < clients; i++)
	{
		struct client_t *c = &s_clients.items[i];
		c->writable = !!FD_ISSET(c->sock, &write_fd);
		if (FD_ISSET(c->sock, &read_fd))
		{
			net_packetrecv(c);
		}
	}
}
