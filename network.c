#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include "client.h"
#include "level.h"
#include "packet.h"
#include "player.h"
#include "network.h"
#include "socket.h"
#include "mcc.h"
#include "playerdb.h"

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
		LOG("getaddrinfo: %s\n", strerror(errno));
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
		LOG("getnameinfo: %s\n", strerror(errno));
	}
        else
        {
                /* Remove IPv4 embedding prefix */
                if (!strncmp(ip, "::ffff:", 7)) {
                        memmove(ip, ip + 7, strlen(ip) - 6);
                }
        }
	return res == 0;
}

static struct sockaddr_in6 serv_addr;

static void net_close_real(struct client_t *c)
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
		LOG("[network] removed %u packets from queue\n", packets);
	}

	char *reason = c->close_reason;

	/* Send client disconnect message straight away */
	if (reason != NULL)
	{
		p = packet_send_disconnect_player(reason);
		send(c->sock, p->buffer, p->loc - p->buffer, MSG_NOSIGNAL);
		free(p);
	}

	close(c->sock);
	deregister_socket(c->sock);

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

		if (!c->hidden)
		{
			call_hook(HOOK_CHAT, buf);
			net_notify_all(buf);
		}

		LOG("Closing connection from %s - %s (%d): %s\n", c->ip, c->player->username, c->player->globalid, reason);
		player_del(c->player);

		c->player = NULL;

		free(c->close_reason);
	}

	free(c->packet_recv);
	free(c);
}

void net_close(struct client_t *c, const char *reason)
{
	c->close = true;
	c->close_reason = (reason == NULL) ? NULL : strdup(reason);
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
			if (errno == ECONNRESET)
			{
				/* Connection reset by peer... normal disconnect */
				net_close(c, NULL);
			}
			else if (errno != EWOULDBLOCK && errno != EAGAIN)
			{
				/* Abnormal error */
				char buf[128];
				snprintf(buf, sizeof buf, "send: %s", strerror(errno));
				net_close(c, buf);
			}
			break;
		}

		pthread_mutex_lock(&c->packet_send_mutex);

		c->packet_send = p->next;
		free(p);

		c->packet_send_count--;

		if (c->packet_send == NULL)
		{
			/* Send queue is empty, reset packet_send_end */
			if (c->packet_send_count != 0)
			{
				LOG("[network] Reached end of send queue but packet send count is %d for %s\n",
					c->packet_send_count, c->player == NULL ? c->ip : c->player->username);
				c->packet_send_count = 0;
			}

			socket_clear_write(c->sock);

			c->packet_send_end = &c->packet_send;
		}
		pthread_mutex_unlock(&c->packet_send_mutex);

		break;
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
			if (errno == ECONNRESET)
			{
				/* Connection reset by peer... normal disconnect */
				net_close(c, NULL);
			}
			else if (errno != EWOULDBLOCK && errno != EAGAIN)
			{
				/* Abnormal error */
				char buf[128];
				snprintf(buf, sizeof buf, "recv: %s", strerror(errno));
				net_close(c, buf);
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

	//c->packet_recv = NULL;

	packet_recv(c, p);
}

void client_run(int fd, bool can_write, bool can_read, void *arg)
{
	struct client_t *c = arg;
	if (c->close) return;
	if (can_write) net_packetsend(c);
	if (c->close) return;
	if (can_read) net_packetrecv(c);
}

void net_run(void)
{
	unsigned i;

	for (i = 0; i < s_clients.used; i++)
	{
		struct client_t *c = s_clients.items[i];
		if (!c->close && c->packet_send_count > 12000)
		{
			net_close(c, "Excessive send queue");
		}
		if (c->close && !c->sending_level)
		{
			if (pthread_mutex_trylock(&s_client_list_mutex))
			{
				if (c->inuse > 0)
				{
					printf("Can't unload yet\n");
					pthread_mutex_unlock(&s_client_list_mutex);
					continue;
				}
				else
				{
					c->inuse = -1;
					pthread_mutex_unlock(&s_client_list_mutex);

					net_close_real(c);
					client_list_del_index(&s_clients, i);

					/* Item at end of list replaces this one, so process this again. */
					i--;
				}
			}
		}
	}
}

void net_accept(int fd, bool can_write, bool can_read, void *arg)
{
	while (true)
	{
		struct client_t *c;
		struct sockaddr_storage sin;
		socklen_t sin_len = sizeof sin;

		/* This better be nonblocking! */
		int nfd = accept(fd, (struct sockaddr *)&sin, &sin_len);
		if (nfd == -1) break;

		socket_set_nonblock(nfd);

		c = calloc(1, sizeof *c);
		if (c == NULL)
		{
			LOG("[network] net_run(): couldn't allocate %zu bytes\n", sizeof *c);
			close(nfd);
		}
		else
		{
			c->sock = nfd;
			c->sin = sin;

			getip((struct sockaddr *)&sin, sin_len, c->ip, sizeof c->ip);
			LOG("[network] accepted connection from %s\n", c->ip);

			pthread_mutex_init(&c->packet_send_mutex, NULL);
			c->packet_send_end = &c->packet_send;

			if (playerdb_check_ban(c->ip))
			{
				LOG("[network] client connected from banned IP address %s\n", c->ip);
				//net_close(c, "Banned");
				close(nfd);
				free(c);
			}
			else
			{
				client_list_add(&s_clients, c);
				register_socket(nfd, client_run, c);
			}
		}
	}
}

static int s_listenfd;

void net_init(int port)
{
	s_listenfd = socket(AF_INET6, SOCK_STREAM, 0);
	if (s_listenfd < 0)
	{
		LOG("socket: %s\n", strerror(errno));
		return;
	}

	serv_addr.sin6_family = AF_INET6;
	serv_addr.sin6_addr = in6addr_any;
	serv_addr.sin6_port = htons(port);

	int on = 1;
	if (setsockopt(s_listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on) == -1)
	{
		LOG("[network] net_init(): Could not set SO_REUSEADDR: %s\n", strerror(errno));
	}

	if (bind(s_listenfd, (struct sockaddr *)&serv_addr, sizeof serv_addr) < 0)
	{
		LOG("bind: %s\n", strerror(errno));
		return;
	}

	if (listen(s_listenfd, 1) != 0)
	{
		close(s_listenfd);
		LOG("listen: %s\n", strerror(errno));
		return;
	}

	socket_set_nonblock(s_listenfd);

	register_socket(s_listenfd, &net_accept, NULL);
}

void net_deinit(void)
{
	close(s_listenfd);
	deregister_socket(s_listenfd);
}

void net_notify_all(const char *message)
{
	unsigned i;
	for (i = 0; i < s_clients.used; i++)
	{
		struct client_t *c = s_clients.items[i];
		if (c->close) continue;
		client_notify(c, message);
	}

	LOG("[ALL] %s\n", message);
}

void net_notify_ops(const char *message)
{
	unsigned i;
	for (i = 0; i < s_clients.used; i++)
	{
		struct client_t *c = s_clients.items[i];
		if (c->close) continue;
		if (c->player == NULL || c->player->rank < RANK_OP) continue;
		client_notify(c, message);
	}

	LOG("[OPS] %s\n", message);
}
