#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include "mcc.h"
#include "config.h"
#include "network.h"
#include "network_worker.h"
#include "socket.h"
#include "timer.h"

struct heartbeat_t
{
	int fd;
	struct timer_t *timer;
	struct timer_t *timeout_timer;

	int heartbeat_stage;

	struct {
		char *hostname;
		char *path;
		int interval;
	} settings;
};

static void heartbeat_run(int fd, bool can_write, bool can_read, void *arg)
{
	bool success = false;
	struct heartbeat_t *h = arg;

	switch (h->heartbeat_stage)
	{
		case 0:
		{
			if (!can_write) return;

			char postdata[1024];
			snprintf(postdata, sizeof postdata,
						"port=%d&users=%d&max=%d&name=%s&admin-slot=false&public=%s&version=7&salt=%s\r\n",
						g_server.port, g_server.players, g_server.max_players,
						g_server.name, g_server.public ? "true" : "false", g_server.salt);

			/* Strip out spaces */
			char *p;
			for (p = postdata; *p != '\0'; p++)
			{
				if (*p == ' ') *p = '+';
			}

			char request[2048];
			snprintf(request, sizeof request,
						"POST /%s HTTP/1.0\r\n"
						"Host: %s\r\n"
						//"Accept: */*\r\n"
						//"Connection: close"
						//"User-Agent: mcc/0.1\r\n"
						"Content-Length: %llu\r\n"
						"Content-Type: application/x-www-form-urlencoded\r\n\r\n"
						"%s",
						h->settings.path, h->settings.hostname, (long long unsigned)strlen(postdata), postdata);

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

			char buf[2049];

			int res = recv(fd, buf, sizeof buf - 1, 0);
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
				success = true;
				break;
			}

			/* Terminate data */
			buf[res] = '\0';

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
			return;
		}

		default:
			break;
	}

	if (h->timeout_timer != NULL)
	{
		deregister_timer(h->timeout_timer);
		h->timeout_timer = NULL;
	}

	close(fd);

	deregister_socket(fd);

	h->fd = -1;

	timer_set_interval(h->timer, success ? h->settings.interval * 1000 : 15000);
}

static void heartbeat_timeout(void *arg)
{
	struct heartbeat_t *h = arg;

	if (h->fd != -1)
	{
		close(h->fd);
		deregister_socket(h->fd);
		h->fd = -1;

		LOG("[heartbeat] Timeout out during %s\n", h->heartbeat_stage == 0 ? "connect" : "transfer");

		timer_set_interval(h->timer, 15000);
	}

	if (h->timeout_timer != NULL)
	{
		deregister_timer(h->timeout_timer);
		h->timeout_timer = NULL;
	}
}

static void heartbeat_connected(int fd, void *arg)
{
	struct heartbeat_t *h = arg;

	if (fd == -1)
	{
		timer_set_interval(h->timer, 15000);
	}
	else
	{
		timer_set_interval(h->timer, h->settings.interval * 1000);

		h->fd = fd;

		register_socket(h->fd, &heartbeat_run, h);

		socket_flag_write(h->fd);

		h->heartbeat_stage = 0;
		h->timeout_timer = register_timer("heartbeat_timeout", 10000, &heartbeat_timeout, h, true);
	}
}

static void heartbeat_start(void *arg)
{
	struct heartbeat_t *h = arg;

	if (h->fd != -1) return;

	network_connect(h->settings.hostname, 80, &heartbeat_connected, h);
}

void module_init(void **arg)
{
	struct heartbeat_t *h = malloc(sizeof *h);

	memset(h, 0, sizeof *h);
	*arg = h;

	char *url;
	if (!config_get_string("heartbeat.url", &url))
	{
		url = "http://minecraft.net/heartbeat.jsp";
	}

	if (strstr(url, "http://") != url)
	{
		LOG("[heartbeat] unsupported URL scheme\n");
		goto badinit;
	}

	h->settings.hostname = strdup(url + 7); // Skip http:// part of string
	h->settings.path = strchr(h->settings.hostname, '/');
	if (h->settings.path == NULL)
	{
		LOG("[heartbeat] invalid URL\n");
		free(h->settings.hostname);
		goto badinit;
	}
	*h->settings.path++ = '\0';

	if (!config_get_int("heartbeat.interval", &h->settings.interval))
	{
		h->settings.interval = 45;
	}

	h->fd = -1;
	h->timer = register_timer("heartbeat", h->settings.interval * 1000, &heartbeat_start, h, false);

	return;

badinit:
	free(h);
	*arg = NULL;
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
	if (h->timeout_timer != NULL)
	{
		deregister_timer(h->timeout_timer);
	}

	free(h->settings.hostname);
	/* h->settings.url is part of h->settings.hostname and should not
	 * be freed. */
	free(h);
}
