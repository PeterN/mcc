#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <errno.h>
#include "network.h"
#include "network_worker.h"
#include "worker.h"
#include "mcc.h"

static struct worker s_network_worker;

struct network_job
{
	char *host;
	int port;
	network_callback callback;
	void *data;
};

static void network_worker(void *arg)
{
	struct network_job *job = arg;

	if (job->callback != NULL)
	{
		struct sockaddr_in addr;
		if (!resolve(job->host, job->port, &addr))
		{
			LOG("Unable to resolve %s:%d\n", job->host, job->port);
			if (job->callback != NULL)
			{
				job->callback(-1, job->data);
			}
		}
		else if (job->callback != NULL)
		{
			int fd = socket(AF_INET, SOCK_STREAM, 0);
			if (fd < 0)
			{
				LOG("socket: %s\n", strerror(errno));
			}
			else
			{
				net_set_nonblock(fd);
				if (connect(fd, (struct sockaddr *)&addr, sizeof addr) < 0)
				{
					if (errno != EINPROGRESS)
					{
						LOG("connect: %s\n", strerror(errno));
						fd = -1;
					}
				}
			}
			job->callback(fd, job->data);
		}
	}

	free(job->host);
	free(job);
}

void network_worker_init(void)
{
	worker_init(&s_network_worker, "network", 60000, 1, network_worker);
}

void network_worker_deinit(void)
{
	worker_deinit(&s_network_worker);
}

void *network_connect(const char *host, int port, network_callback callback, void *data)
{
	struct network_job *job = malloc(sizeof *job);
	job->host = strdup(host);
	job->port = port;
	job->callback = callback;
	job->data = data;

	worker_queue(&s_network_worker, job);
	return job;
}

