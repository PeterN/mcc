#include <stdbool.h>
#include <pthread.h>
#include <assert.h>
#include "client.h"
#include "player.h"
#include "level.h"
#include "mcc.h"
#include "astar.h"
#include "astar_thread.h"
#include "queue.h"
#include "util.h"

static struct queue_t *s_astar_queue;
static pthread_t s_astar_thread;
static bool s_astar_threaded;

struct astar_job
{
	struct level_t *level;
	struct point a;
	struct point b;
	astar_cb *callback;
	void *data;
};

void *astar_thread(void *arg)
{
	LOG("[astar] Thread started\n");

	while (true)
	{
		struct astar_job *job;
		if (queue_consume(s_astar_queue, (void *)&job))
		{
			/* Null item added to the queue indicate we should exit. */
			if (job == NULL)
			{
				break;
			}

			struct point *path = as_find(job->level, &job->a, &job->b);
			job->callback(job->level, path, job->data);
			free(job);
		}

		/* Should wait on a signal really! */
		usleep(10000);
	}

	LOG("[astar] Thread exiting\n");

	return NULL;
}

void astar_thread_init()
{
	s_astar_queue = queue_new();

	s_astar_threaded = (pthread_create(&s_astar_thread, NULL, &astar_thread, NULL) == 0);

	if (!s_astar_threaded)
	{
		LOG("[astar] Could not start astar thread, expect delays\n");
	}
}

void astar_thread_deinit()
{
	if (s_astar_threaded)
	{
		queue_produce(s_astar_queue, NULL);
		pthread_join(s_astar_thread, NULL);
	}

	queue_delete(s_astar_queue);
	s_astar_queue = NULL;
}

void astar_thread_run()
{
	/* Let thread handle us if we're running threaded. */
	if (s_astar_threaded || s_astar_queue == NULL) return;

	struct astar_job *job;
	if (queue_consume(s_astar_queue, (void *)&job))
	{
		struct point *path = as_find(job->level, &job->a, &job->b);
		job->callback(job->level, path, job->data);
		free(job);
	}
}

void astar_queue(struct level_t *level, struct point a, struct point b, astar_cb *callback, void *data)
{
	assert(level != NULL);
	assert(callback != NULL);

	struct astar_job *job = malloc(sizeof *job);
	job->level = level;
	job->a = a;
	job->b = b;
	job->callback = callback;
	job->data = data;

	if (!queue_produce(s_astar_queue, job))
	{
		LOG("[astar] Unable to queue A* job");
	}
}

