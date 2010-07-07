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
#include "packet.h"
#include "player.h"
#include "mcc.h"

static void resolve(const char *hostname, int port, struct sockaddr_in *addr)
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
        return;
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
}

static struct sockaddr_in serv_addr;
static struct sockaddr_in beat_addr;
static int s_listenfd;
static int s_heartbeatfd = -1;
static int s_heartbeat_stage;

static void net_set_nonblock(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void heartbeat_start()
{
    if (s_heartbeatfd != -1) return;

    s_heartbeatfd = socket(AF_INET, SOCK_STREAM, 0);
    if (s_heartbeatfd < 0)
    {
        perror("socket");
        return;
    }

    net_set_nonblock(s_heartbeatfd);

    if (connect(s_heartbeatfd, (struct sockaddr *)&beat_addr, sizeof beat_addr) < 0)
    {
        if (errno != EINPROGRESS) {
            perror("connect");
            s_heartbeatfd = -1;
        }
    }

    s_heartbeat_stage = 0;
}

void heartbeat_run(bool can_write, bool can_read)
{
    switch (s_heartbeat_stage)
    {
        case 0:
        {
            if (!can_write) return;

            static const char url[] = "/heartbeat.jsp";
            static const char host[] = "www.minecraft.net";
            char postdata[1024];
            snprintf(postdata, sizeof postdata, "port=%u&users=%u&max=%u&name=%s&public=%s&version=7&salt=a7ebefb9bf1d4063\r\n",
                      25565, g_server.players, g_server.max_players, g_server.name, g_server.public ? "true" : "false");

            char request[2048];
            snprintf(request, sizeof request,
                        "POST %s HTTP/1.0\r\n"
                        "Host: %s\r\n"
                        //"Accept: */*\r\n"
                        //"Connection: close"
                        //"User-Agent: mcc/0.1\r\n"
                        "Content-Length: %lu\r\n"
                        "Content-Type: application/x-www-form-urlencoded\r\n\r\n"
                        "%s",
                        url, host, strlen(postdata), postdata);

            int res = write(s_heartbeatfd, request, strlen(request));
            if (res < 0)
            {
                perror("write");
                break;
            }

            s_heartbeat_stage = 1;
            break;
        }

        case 1:
        {
            if (!can_read) return;

            char buf[2048];

            int res = read(s_heartbeatfd, buf, sizeof buf);
            if (res < 0)
            {
                perror("read");
                break;
            }

            char *b = strstr(buf, "\r\n\r\n") + 4;
            char *c = strstr(b, "\r\n");
            *c = '\0';

            printf("%s\n", b);
            /* Fall through */
        }

        default:
            close(s_heartbeatfd);
            s_heartbeatfd = -1;
            break;
    }
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

    resolve("www.minecraft.net", 80, &beat_addr);
    heartbeat_start();
}

void net_close(struct client_t *c, bool remove_player)
{
	printf("Closing connection\n");
	close(c->sock);

	/* Mark client for deletion */
	c->close = true;

	if (remove_player) player_del(c->player);
}

static void net_packetsend(struct client_t *c)
{
    size_t res;
    struct packet_t *p = c->packet_send;

    if (p == NULL) return;

    while (true)
    {
        p = c->packet_send;

        res = send(c->sock, p->buffer, p->loc - p->buffer, 0);
        if (res == -1)
        {
            //if (errno != EWOULDBLOCK)
            //{
                perror("send");
            //}
            return;
        }

        c->packet_send = p->next;
        free(p);

        if (c->packet_send == NULL) return;
    }
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
			net_close(c, true);
			return;
		}

		if (p->size == 1)
		{
			p->size = packet_recv_size(p->buffer[0]);
			if (p->size == -1)
			{
				printf("Unrecognised packet type 0x%02X!\n", p->buffer[0]);
				net_close(c, true);
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
		if (s_clients.items[i].close)
		{
			client_list_del(&s_clients, s_clients.items[i]);
			/* Restart :/ */
			i = -1;
			clients = s_clients.used;
		}
	}

	FD_ZERO(&read_fd);
	FD_ZERO(&write_fd);

	/* Add listen socket */
	FD_SET(s_listenfd, &read_fd);

	if (s_heartbeatfd >= 0)
	{
	    FD_SET(s_heartbeatfd, &read_fd);
	    FD_SET(s_heartbeatfd, &write_fd);
	}

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
				c.packet_send = NULL;
				client_list_add(&s_clients, c);
			}
		}
	}

    if (s_heartbeatfd >= 0)
	{
	    bool can_write = FD_ISSET(s_heartbeatfd, &write_fd);
	    bool can_read  = FD_ISSET(s_heartbeatfd, &read_fd);

	    if (can_write || can_read)
	    {
	        heartbeat_run(can_write, can_read);
	    }
	}

	for (i = 0; i < clients; i++)
	{
		struct client_t *c = &s_clients.items[i];
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
