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

int s_irc_fd = -1;

struct irc_packet_t
{
    char message[500];
    struct irc_packet_t *next;
};

static struct sockaddr_in s_irc_addr;
static int s_irc_stage;
static bool s_irc_resolved;
static struct irc_packet_t *s_queue;
static struct irc_packet_t **s_queue_end;

static char s_read_buf[2048];
static char *s_read_pos;

static void irc_queue(const char *message)
{
    *s_queue_end = calloc(1, sizeof **s_queue_end);
    strncpy((*s_queue_end)->message, message, sizeof (*s_queue_end)->message);

    s_queue_end = &(*s_queue_end)->next;
}

void irc_start()
{
    if (s_irc_fd != -1) return;

    if (!s_irc_resolved)
    {
        if (!resolve(g_server.irc.hostname, g_server.irc.port, &s_irc_addr))
        {
            LOG("Unable to resolve IRC server\n");
            return;
        }
        s_irc_resolved = true;
    }

    s_irc_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (s_irc_fd < 0)
    {
        perror("socket");
        return;
    }

    net_set_nonblock(s_irc_fd);

    if (connect(s_irc_fd, (struct sockaddr *)&s_irc_addr, sizeof s_irc_addr) < 0)
    {
        if (errno != EINPROGRESS) {
            perror("connect");
            s_irc_fd = -1;
        }
    }

    s_irc_stage = 0;
    s_queue = NULL;
    s_queue_end = &s_queue;
    s_read_pos = s_read_buf;
}

void irc_process(char *message)
{
    char buf[512];

    if (strncmp(message, "PING :", 6) == 0)
    {
        snprintf(buf, sizeof buf, "PONG :%s\r\n", message + 6);
        irc_queue(buf);
        if (s_irc_stage == 1)
        {
            s_irc_stage = 2;
        }
    }
}

void irc_run(bool can_write, bool can_read)
{
    char buf[513];

    if (s_irc_fd == -1) return;

    switch (s_irc_stage)
    {
        case 0:
            if (g_server.irc.pass != NULL)
            {
                snprintf(buf, sizeof buf, "PASS %s\r\n", g_server.irc.pass);
                irc_queue(buf);
            }
            snprintf(buf, sizeof buf, "NICK %s\r\n", g_server.irc.name);
            irc_queue(buf);
            snprintf(buf, sizeof buf, "USER %s 8 * :%s\r\n", g_server.irc.name, g_server.irc.name);
            irc_queue(buf);

            s_irc_stage = 1;
            break;

        case 1:
            break;

        case 2:
            snprintf(buf, sizeof buf, "JOIN %s\r\n", g_server.irc.channel);
            irc_queue(buf);

            s_irc_stage = 3;
            break;
    }

    if (can_read)
    {
        int res = recv(s_irc_fd, s_read_pos, sizeof s_read_buf - (s_read_pos - s_read_buf) - 1, 0);
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
            close(s_irc_fd);
            s_irc_fd = -1;
            return;
        }
        s_read_pos[res] = '\0';

        /* Strip off new lines/carriage returns */
        char *c, *bufp = s_read_buf;

        while (bufp < s_read_pos + res)
        {
            c = strstr(bufp, "\r\n");
            if (c == NULL) break;
            *c = '\0';

            irc_process(bufp);

            bufp = c + 2;
        }

        if (bufp < s_read_pos + res)
        {
            memmove(s_read_buf, bufp, strlen(bufp));
            s_read_pos = s_read_buf + strlen(bufp);
        }
        else
        {
            s_read_pos = s_read_buf;
        }
    }

    if (can_write)
    {
        while (s_queue != NULL)
        {
            struct irc_packet_t *ircp = s_queue;
            int res = write(s_irc_fd, s_queue->message, strlen(s_queue->message));
            if (res == -1)
            {
                //if (errno != EWOULDBLOCK)
                //{
                    perror("send");
                //}
                return;
            }

            s_queue = s_queue->next;

            free(ircp);
        }

        s_queue_end = &s_queue;
    }
}
