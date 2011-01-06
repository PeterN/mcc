#include <stdbool.h>
#include <pthread.h>
#include "client.h"
#include "player.h"
#include "level.h"
#include "level_send.h"
#include "mcc.h"
#include "queue.h"
#include "util.h"

static struct queue_t *s_send_queue;
static pthread_t s_send_thread;
static bool s_send_threaded;

void *level_send_thread(void *arg)
{
	LOG("[level_send] Thread started\n");

	while (true)
	{
		struct client_t *client;
		if (queue_consume(s_send_queue, (void *)&client))
		{
			/* Null item added to the queue indicate we should exit. */
			if (client == NULL)
			{
				break;
			}

/*
			if (client->player->new_level != NULL)
			{
				LOG("[level_send] Sending level %s to %s\n", client->player->new_level->name, client->player->username);
			}
			else
			{
				LOG("[level_send] Sending initial level to %s\n", client->player->username);
			}
*/

			level_send(client);

			/* If level wasn't sent, push it back to the queue */
			if (client->waiting_for_level)
			{
				queue_produce(s_send_queue, client);
			}
		}

		/* Should wait on a signal really! */
		usleep(10000);
	}

	LOG("[level_send] Thread exiting\n");

	return NULL;
}

void level_send_init()
{
	s_send_queue = queue_new();

	s_send_threaded = (pthread_create(&s_send_thread, NULL, &level_send_thread, NULL) == 0);

	if (!s_send_threaded)
	{
		LOG("[level_send] Could not start level send thread, expect delays\n");
	}
}

void level_send_deinit()
{
	if (s_send_threaded)
	{
		queue_produce(s_send_queue, NULL);
		pthread_join(s_send_thread, NULL);
	}

	queue_delete(s_send_queue);
	s_send_queue = NULL;
}

void level_send_run()
{
	/* Let thread handle us if we're running threaded. */
	if (s_send_threaded || s_send_queue == NULL) return;

	struct client_t *client;
	if (queue_consume(s_send_queue, (void *)&client))
	{
		level_send(client);
	}
}

void level_send_queue(struct client_t *client)
{
	if (!queue_produce(s_send_queue, client))
	{
		LOG("[level_send] Unable to queue level send\n");
	}
}

