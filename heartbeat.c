#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include "mcc.h"
#include "network.h"
#include "timer.h"

struct heartbeat_t
{
	int fd;
	struct timer_t *timer;

	struct sockaddr_in heartbeat_addr;
	int heartbeat_stage;
	bool heartbeat_resolved;
};

static void heartbeat_run(int fd, bool can_write, bool can_read, void *arg)
{
	struct heartbeat_t *h = arg;

	switch (h->heartbeat_stage)
	{
		case 0:
		{
			if (!can_write) return;

			static const char url[] = "/heartbeat.jsp";
			static const char host[] = "www.minecraft.net";
			char postdata[1024];
			snprintf(postdata, sizeof postdata, "port=%u&users=%u&max=%u&name=%s&public=%s&version=7&salt=64986852\r\n",
					  g_server.port, g_server.players, g_server.max_players, g_server.name, g_server.public ? "true" : "false");

			char request[2048];
			snprintf(request, sizeof request,
						"POST %s HTTP/1.0\r\n"
						"Host: %s\r\n"
						//"Accept: */*\r\n"
						//"Connection: close"
						//"User-Agent: mcc/0.1\r\n"
						"Content-Length: %llu\r\n"
						"Content-Type: application/x-www-form-urlencoded\r\n\r\n"
						"%s",
						url, host, (long long unsigned)strlen(postdata), postdata);

			int res = send(fd, request, strlen(request), MSG_NOSIGNAL);
			if (res == -1)
			{
				LOG("[heartbeat] send: %s\n", strerror(errno));
				break;
			}

			h->heartbeat_stage = 1;
			return;
		}

		case 1:
		{
			if (!can_read) return;

			char buf[2048];

			int res = recv(fd, buf, sizeof buf, 0);
			if (res == -1)
			{
				if (errno != EWOULDBLOCK && errno != EAGAIN)
				{
					LOG("[heartbeat] recv: %s\n", strerror(errno));
					/* Error, break to close */
					break;
				}
				/* Not an error, return to continue (!) */
				return;
			}
			else if (res == 0)
			{
				/* Read nothing, break to close */
				break;
			}

			char *b = strstr(buf, "\r\n\r\n");
			if (b != NULL)
			{
				b += 4;
				char *c = strstr(b, "\r\n");
				if (c != NULL)
				{
					*c = '\0';
				}
				LOG("[heartbeat] %s\n", b);
			}
			else
			{
				LOG("[heartbeat] %s\n", buf);
			}
			break;
		}

		default:
			break;
	}

	close(fd);

	deregister_socket(fd);

	h->fd = -1;
}

static void heartbeat_start(void *arg)
{
	struct heartbeat_t *h = arg;

	if (h->fd != -1) return;

	if (!h->heartbeat_resolved)
	{
		if (!resolve("www.minecraft.net", 80, &h->heartbeat_addr))
		{
			LOG("Unable to resolve heartbeat server\n");
			return;
		}
		h->heartbeat_resolved = true;
	}

	h->fd = socket(AF_INET, SOCK_STREAM, 0);
	if (h->fd < 0)
	{
		LOG("[heartbeat] socket: %s\n", strerror(errno));
		return;
	}

	net_set_nonblock(h->fd);

	if (connect(h->fd, (struct sockaddr *)&h->heartbeat_addr, sizeof h->heartbeat_addr) < 0)
	{
		if (errno != EINPROGRESS) {
			LOG("[heartbeat] connect: %s\n", strerror(errno));
			h->fd = -1;
			return;
		}
	}

	register_socket(h->fd, &heartbeat_run, h);

	h->heartbeat_stage = 0;
}

void module_init(void **arg)
{
	struct heartbeat_t *h = malloc(sizeof *h);
	h->fd = -1;
	h->heartbeat_resolved = false;
	h->timer = register_timer("heartbeat", 60000, &heartbeat_start, h);

	*arg = h;
}

void module_deinit(void *arg)
{
	struct heartbeat_t *h = arg;

	if (h->fd != -1)
	{
		close(h->fd);
		deregister_socket(h->fd);
	}

	deregister_timer(h->timer);

	free(h);
}
