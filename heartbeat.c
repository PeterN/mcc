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

void heartbeat_run(int fd, bool can_write, bool can_read, void *arg)
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

			int res = write(fd, request, strlen(request));
			if (res < 0)
			{
				perror("write");
				break;
			}

			h->heartbeat_stage = 1;
			return;
		}

		case 1:
		{
			if (!can_read) return;

			char buf[2048];

			int res = read(fd, buf, sizeof buf);
			if (res < 0)
			{
				perror("read");
				break;
			}

			char *b = strstr(buf, "\r\n\r\n") + 4;
			char *c = strstr(b, "\r\n");
			*c = '\0';

			printf("%s\n", b);
			break;
		}

		default:
			break;
	}

	close(fd);

	deregister_socket(fd);

	h->fd = -1;
}

void heartbeat_start(void *arg)
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
		perror("socket");
		return;
	}

	net_set_nonblock(h->fd);

	if (connect(h->fd, (struct sockaddr *)&h->heartbeat_addr, sizeof h->heartbeat_addr) < 0)
	{
		if (errno != EINPROGRESS) {
			perror("connect");
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
