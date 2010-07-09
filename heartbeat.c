#include <stdio.h>
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

int s_heartbeat_fd = -1;

static struct sockaddr_in s_heartbeat_addr;
static int s_heartbeat_stage;
static bool s_heartbeat_resolved;

void heartbeat_start()
{
    if (s_heartbeat_fd != -1) return;

    if (!s_heartbeat_resolved)
    {
        if (!resolve("www.minecraft.net", 80, &s_heartbeat_addr))
        {
            fprintf(stderr, "Unable to resolve heartbeat server\n");
            return;
        }
        s_heartbeat_resolved = true;
    }

    s_heartbeat_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (s_heartbeat_fd < 0)
    {
        perror("socket");
        return;
    }

    net_set_nonblock(s_heartbeat_fd);

    if (connect(s_heartbeat_fd, (struct sockaddr *)&s_heartbeat_addr, sizeof s_heartbeat_addr) < 0)
    {
        if (errno != EINPROGRESS) {
            perror("connect");
            s_heartbeat_fd = -1;
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

            int res = write(s_heartbeat_fd, request, strlen(request));
            if (res < 0)
            {
                perror("write");
                break;
            }

            s_heartbeat_stage = 1;
            return;
        }

        case 1:
        {
            if (!can_read) return;

            char buf[2048];

            int res = read(s_heartbeat_fd, buf, sizeof buf);
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

    close(s_heartbeat_fd);
    s_heartbeat_fd = -1;
}
