#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include "mcc.h"
#include "network.h"

struct irc_packet_t
{
	char message[500];
	struct irc_packet_t *next;
};

struct irc_t
{
	struct sockaddr_in irc_addr;
	int irc_stage;
	bool irc_resolved;
	struct irc_packet_t *queue;
	struct irc_packet_t **queue_end;

	char read_buf[2048];
	char *read_pos;
};

static void irc_queue(struct irc_t *s, const char *message)
{
	*s->queue_end = calloc(1, sizeof **s->queue_end);
	strncpy((*s->queue_end)->message, message, sizeof (*s->queue_end)->message);

	s->queue_end = &(*s->queue_end)->next;
}

void irc_process(struct irc_t *s, char *message)
{
	char buf[512];

	if (strncmp(message, "PING :", 6) == 0)
	{
		snprintf(buf, sizeof buf, "PONG :%s\r\n", message + 6);
		irc_queue(s, buf);
		if (s->irc_stage == 1)
		{
			s->irc_stage = 2;
		}
	}
}

void irc_run(int fd, bool can_write, bool can_read, void *arg)
{
	struct irc_t *s = arg;
	char buf[513];

	switch (s->irc_stage)
	{
		case 0:
			if (g_server.irc.pass != NULL)
			{
				snprintf(buf, sizeof buf, "PASS %s\r\n", g_server.irc.pass);
				irc_queue(s, buf);
			}
			snprintf(buf, sizeof buf, "NICK %s\r\n", g_server.irc.name);
			irc_queue(s, buf);
			snprintf(buf, sizeof buf, "USER %s 8 * :%s\r\n", g_server.irc.name, g_server.irc.name);
			irc_queue(s, buf);

			s->irc_stage = 1;
			break;

		case 1:
			break;

		case 2:
			snprintf(buf, sizeof buf, "JOIN %s\r\n", g_server.irc.channel);
			irc_queue(s, buf);

			s->irc_stage = 3;
			break;
	}

	if (can_read)
	{
		int res = recv(fd, s->read_pos, sizeof s->read_buf - (s->read_pos - s->read_buf) - 1, 0);
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
			LOG("Closed IRC connection\n");
			close(fd);
			deregister_socket(fd);
			return;
		}
		s->read_pos[res] = '\0';

		/* Strip off new lines/carriage returns */
		char *c, *bufp = s->read_buf;

		while (bufp < s->read_pos + res)
		{
			c = strstr(bufp, "\r\n");
			if (c == NULL) break;
			*c = '\0';

			irc_process(s, bufp);

			bufp = c + 2;
		}

		if (bufp < s->read_pos + res)
		{
			memmove(s->read_buf, bufp, strlen(bufp));
			s->read_pos = s->read_buf + strlen(bufp);
		}
		else
		{
			s->read_pos = s->read_buf;
		}
	}

	if (can_write)
	{
		while (s->queue != NULL)
		{
			struct irc_packet_t *ircp = s->queue;
			int res = write(fd, s->queue->message, strlen(s->queue->message));
			if (res == -1)
			{
				//if (errno != EWOULDBLOCK)
				//{
					perror("send");
				//}
				return;
			}

			s->queue = s->queue->next;

			free(ircp);
		}

		s->queue_end = &s->queue;
	}
}

void irc_start(void *arg)
{
	struct irc_t *s = arg;

	if (!s->irc_resolved)
	{
		if (!resolve(g_server.irc.hostname, g_server.irc.port, &s->irc_addr))
		{
			LOG("Unable to resolve IRC server\n");
			return;
		}
		s->irc_resolved = true;
	}

	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
	{
		perror("socket");
		return;
	}

	net_set_nonblock(fd);

	if (connect(fd, (struct sockaddr *)&s->irc_addr, sizeof s->irc_addr) < 0)
	{
		if (errno != EINPROGRESS) {
			perror("connect");
			return;
		}
	}
	
	register_socket(fd, &irc_run, s);

	s->irc_stage = 0;
	s->queue = NULL;
	s->queue_end = &s->queue;
	s->read_pos = s->read_buf;
}

void module_init(void **arg)
{
	struct irc_t *s = malloc(sizeof *s);
	*arg = s;

	irc_start(s);
}

void module_deinit(void *arg)
{
}
