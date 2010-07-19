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
#include "irc.h"
#include "heartbeat.h"
#include "network.h"
#include "mcc.h"

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

static struct sockaddr_in serv_addr;

static int s_listenfd;

void net_set_nonblock(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void net_init()
{
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

    heartbeat_start();
}

void net_close(struct client_t *c, const char *reason)
{
    char buf[64];

    close(c->sock);

    /* Mark client for deletion */
	c->close = true;

	if (c->player == NULL)
	{
		LOG("Closing connection: %s\n", reason == NULL ? "closed" : reason);
	}
	else
	{
		int i;
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

	    client_send_despawn(c, false);
	    c->player->level->clients[c->player->levelid] = NULL;

        if (reason == NULL)
        {
            snprintf(buf, sizeof buf, TAG_RED "- %s" TAG_YELLOW " disconnected", c->player->colourusername);
        }
	    else
	    {
	        snprintf(buf, sizeof buf, TAG_RED "- %s" TAG_YELLOW " disconnected (%s)", c->player->colourusername, reason);
	    }
	    net_notify_all(buf);

		LOG("Closing connection %s (%d): %s\n", c->player->username, c->player->globalid, reason);
		player_del(c->player);
	}
}

static void net_packetsend(struct client_t *c)
{
    size_t res;
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
	size_t res;
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
			p->size = packet_recv_size(p->buffer[0]);
			if (p->size == -1)
			{
			    char buf[64];
			    snprintf(buf, sizeof buf, "unrecognised packet type 0x%02X!\n", p->buffer[0]);
				net_close(c, buf);
				return;
			}
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

	if (s_heartbeat_fd >= 0)
	{
	    FD_SET(s_heartbeat_fd, &read_fd);
	    FD_SET(s_heartbeat_fd, &write_fd);
	}

	if (s_irc_fd >= 0)
	{
	    FD_SET(s_irc_fd, &read_fd);
	    FD_SET(s_irc_fd, &write_fd);
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
			c->sin  = sin;
			client_list_add(&s_clients, c);
		}
	}

    if (s_heartbeat_fd >= 0)
	{
	    bool can_write = FD_ISSET(s_heartbeat_fd, &write_fd);
	    bool can_read  = FD_ISSET(s_heartbeat_fd, &read_fd);

	    if (can_write || can_read)
	    {
	        heartbeat_run(can_write, can_read);
	    }
	}

	if (s_irc_fd >= 0)
	{
	    bool can_write = FD_ISSET(s_irc_fd, &write_fd);
	    bool can_read  = FD_ISSET(s_irc_fd, &read_fd);

	    if (can_write || can_read)
	    {
	        irc_run(can_write, can_read);
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
    int i;
    for (i = 0; i < s_clients.used; i++)
    {
        struct client_t *c = s_clients.items[i];
        client_add_packet(c, packet_send_message(0, message));
    }

    LOG("%s\n", message);
}
