#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "mcc.h"
#include "network.h"

struct ipc_t
{
	int fd;
};

static void ipc_run(int fd, bool can_write, bool can_read, void *arg)
{
	struct ipc_t *ipc = arg;
}

static void ipc_init(struct ipc_t *ipc)
{
	struct sockaddr_un saun;

	ipc->fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (ipc->fd < 0)
	{
		perror("socket");
		return;
	}

	saun.sun_family = AF_UNIX;
	strncpy(saun.sun_path, "/tmp/mccsocket", sizeof saun.sun_path);

	if (bind(ipc->fd, (const struct sockaddr *)&saun, sizeof saun) < 0)
	{
		perror("bind");
		return;
	}

	if (listen(ipc->fd, 0) < 0)
	{
		perror("listen");
		return;
	}

	register_socket(ipc->fd, &ipc_run, ipc);
}

void module_init(void **arg)
{
	struct ipc_t *ipc = malloc(sizeof *ipc);
	ipc->fd = -1;

	ipc_init(ipc);

	*arg = ipc;
}

void module_deinit(void *arg)
{
	struct ipc_t *ipc = arg;

	if (ipc->fd != -1)
	{
		close(ipc->fd);
		deregister_socket(ipc->fd);
	}

	free(ipc);
}
