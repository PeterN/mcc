#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include "colour.h"
#include "commands.h"
#include "config.h"
#include "hook.h"
#include "mcc.h"
#include "network.h"
#include "network_worker.h"
#include "socket.h"
#include "timer.h"

struct irc_packet_t
{
	char message[500];
	struct irc_packet_t *next;
};

struct irc_t
{
	struct {
		char *hostname;
		int port;
		char *name;
		char *channel;
		char *pass;
	} settings;

	int fd;
	struct timer_t *timer;

	int irc_stage;
	struct irc_packet_t *queue;
	struct irc_packet_t **queue_end;

	char read_buf[2048];
	char *read_pos;
};

static void irc_queue(struct irc_t *s, const char *message)
{
	*s->queue_end = calloc(1, sizeof **s->queue_end);
	if (*s->queue_end == NULL) {
		LOG("[irc] couldn't allocate %zu bytes\n", sizeof **s->queue_end);
		return;
	}

	strncpy((*s->queue_end)->message, message, sizeof (*s->queue_end)->message);

	/* First message, flag for writing */
	if (s->queue_end == &s->queue) socket_flag_write(s->fd);

	s->queue_end = &(*s->queue_end)->next;
}

void irc_command(char *src, char *command)
{
/*	char *bufp = command;
	char *param[10];
	int params = 0;

	memset(param, 0, sizeof param);

	for (;;)
	{
		size_t l = strcspn(bufp, " ,");
		bool end = false;

		if (bufp[l] == '\0') end = true;
		bufp[l] = '\0';
		param[params++] = bufp;
		bufp += l + 1;

		if (end) break;
	}

	if (!command_process(NULL, params, (const char **)param))
	{
		//client_notify(c, "Unknown command");
		return;
	}*/
}

static void irc_process(struct irc_t *s, char *message)
{
	char buf[512];
	char *src = NULL;
	char *cmd = NULL;
	char *arg = NULL;

	if (message[0] == ':')
	{
		src = message + 1;
		cmd = strchr(message, ' ');
		if (cmd == NULL) return;
		*cmd++ = '\0';

		/* Crop source to just the nick */
		char *tmp = strchr(src, '!');
		if (tmp != NULL) *tmp = '\0';
	}
	else
	{
		cmd = message;
	}

	arg = strchr(cmd, ' ');
	if (arg != NULL) *arg++ = '\0';

	if (strcmp(cmd, "PING") == 0)
	{
		snprintf(buf, sizeof buf, "PONG %s\r\n", arg);
		irc_queue(s, buf);
	}
	else if (strncmp(cmd, "255", 3) == 0)
	{
		if (s->irc_stage == 1)
		{
			s->irc_stage = 2;
		}
	}
	else if (strcmp(cmd, "PRIVMSG") == 0)
	{
		char *notify = strchr(arg, ':');
		if (notify == NULL) return;

		notify++;

		if (arg[0] != '#')
		{
			/* Not a channel message, must be a private message */
			irc_command(src, notify);
		}
		else
		{
			if (strncmp(notify, "\001ACTION", 7) == 0)
			{
				/* Trim \001 character from end */
				notify[strlen(notify) - 1] = '\0';
				snprintf(buf, sizeof buf, "! " TAG_NAVY "* %s%s", src, notify + 7);
			}
			else
			{
				snprintf(buf, sizeof buf, "! " TAG_NAVY "%s:" TAG_WHITE " %s", src, notify);
			}

			/* Strip non-ascii characters */
			int i;
			for (i = 0; i < strlen(buf); i++)
			{
				if (buf[i] < 32 || buf[i] > 127) buf[i] = '?';
			}

			net_notify_all(buf);
		}
	}
}

static int irc_convert_colour(int colour)
{
	switch (colour)
	{
		case '0': return 1;
		case '1': return 2;
		case '2': return 3;
		case '3': return 10;
		case '4': return 5;
		case '5': return 6;
		case '6': return 7;
		case '7': return 15;
		case '8': return 14;
		case '9': return 12;
		case 'a': return 9;
		case 'b': return 11;
		case 'c': return 4;
		case 'd': return 13;
		case 'e': return 8;
		case 'f': return 0;
		default: return colour;
	}
}

static void irc_message(int hook, void *data, void *arg)
{
	struct irc_t *s = arg;

	char *d = data;
	if (d[0] != '!') return;

	char buf[512];
	snprintf(buf, sizeof buf, "PRIVMSG %s :%s\r\n", s->settings.channel, (char *)data);

	unsigned i;
	size_t len = strlen(buf);
	for (i = 0; i < len - 1; i++)
	{
		if (buf[i] == '&' && buf[i + 1] != ' ')
		{
			int col = irc_convert_colour(buf[i + 1]);
			if (col == 0)
			{
				buf[i] = 3;
				buf[i + 1] = ' ';
			}
			else if (col >= 10)
			{
				memmove(&buf[i + 2], &buf[i + 1], len - i);
				buf[i] = 3;
				buf[i + 1] = (col / 10) + '0';
				buf[i + 2] = (col % 10) + '0';
				i++;
				len++;
			}
			else
			{
				buf[i] = 3;
				buf[i + 1] = col + '0';
			}
			i++;
		}
	}

	irc_queue(s, buf);
}

static void irc_end(struct irc_t *s)
{
	LOG("[irc] Closed connection\n");

	close(s->fd);

	deregister_hook(&irc_message, s);
	deregister_socket(s->fd);

	/* Free remaining packets */
	while (s->queue != NULL)
	{
		struct irc_packet_t *ircp = s->queue;
		s->queue = s->queue->next;
		free(ircp);
	}

	s->fd = -1;
}

static void irc_run(int fd, bool can_write, bool can_read, void *arg)
{
	struct irc_t *s = arg;
	char buf[513];

	switch (s->irc_stage)
	{
		case 0:
			if (s->settings.pass != NULL)
			{
				snprintf(buf, sizeof buf, "PASS %s\r\n", s->settings.pass);
				irc_queue(s, buf);
			}
			snprintf(buf, sizeof buf, "NICK %s\r\n", s->settings.name);
			irc_queue(s, buf);
			snprintf(buf, sizeof buf, "USER %s 8 * :%s\r\n", s->settings.name, s->settings.name);
			irc_queue(s, buf);

			s->irc_stage = 1;
			break;

		case 1:
			break;

		case 2:
			snprintf(buf, sizeof buf, "JOIN %s\r\n", s->settings.channel);
			irc_queue(s, buf);

			s->irc_stage = 3;
			break;
	}

	if (can_read)
	{
		int res = recv(fd, s->read_pos, sizeof s->read_buf - (s->read_pos - s->read_buf) - 1, 0);
		if (res == -1)
		{
			if (errno != EWOULDBLOCK && errno != EAGAIN)
			{
				LOG("[irc] recv: %s\n", strerror(errno));
				irc_end(s);
			}
			return;
		}
		else if (res == 0)
		{
			irc_end(s);
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
			int res = send(fd, s->queue->message, strlen(s->queue->message), MSG_NOSIGNAL);
			if (res == -1)
			{
				//if (errno != EWOULDBLOCK)
				//{
					LOG("[irc] send: %s\n", strerror(errno));
				//}
				return;
			}

			s->queue = s->queue->next;

			free(ircp);
		}

		socket_clear_write(s->fd);

		s->queue_end = &s->queue;
	}
}

static void irc_connected(int fd, void *arg)
{
	struct irc_t *s = arg;

	if (fd != -1)
	{
		register_socket(fd, &irc_run, s);

		s->fd = fd;
		s->irc_stage = 0;
		s->queue = NULL;
		s->queue_end = &s->queue;
		s->read_pos = s->read_buf;

		socket_flag_write(s->fd);

		register_hook(HOOK_CHAT, &irc_message, s);
	}
}

static void irc_start(void *arg)
{
	struct irc_t *s = arg;

	/* Already connected? */
	if (s->fd != -1) return;

	network_connect(s->settings.hostname, s->settings.port, &irc_connected, s);
}

void module_init(void **arg)
{
	struct irc_t *s = malloc(sizeof *s);
	if (s == NULL)
	{
		LOG("[irc] couldn't allocate %zu bytes\n", sizeof *s);
		return;
	}

	memset(s, 0, sizeof *s);
	*arg = s;

	if (!config_get_string("irc.hostname", &s->settings.hostname) ||
		!config_get_int("irc.port", &s->settings.port) ||
		!config_get_string("irc.name", &s->settings.name) ||
		!config_get_string("irc.channel", &s->settings.channel))
	{
		free(s);
		*arg = NULL;
		return;
	}

	config_get_string("irc.pass", &s->settings.pass);

	s->fd = -1;
	s->timer = register_timer("irc", 60000, &irc_start, s, false);
}

void module_deinit(void *arg)
{
	struct irc_t *s = arg;

	if (s == NULL) return;

	irc_end(s);
	deregister_timer(s->timer);

	free(s);
}
