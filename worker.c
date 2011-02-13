#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "queue.h"
#include "worker.h"
#include "mcc.h"
#include "gettime.h"

void *worker_thread(void *arg)
{
	struct worker *worker = arg;

	unsigned last = gettime();
	bool timeout = false;
	int jobs = 0;

	nice(worker->nice);

	LOG("Queue worker %s thread started with nice %d\n", worker->name, worker->nice);

	while (true)
	{
		void *data;
		if (queue_consume(worker->queue, &data))
		{
			/* Null item added to the queue indicate we should exit. */
			if (data == NULL) break;

			worker->callback(data);

			last = gettime();
			jobs++;
		}
		else if (worker->timeout != 0)
		{
			if (gettime() - last > worker->timeout)
			{
				timeout = true;
				worker->thread_timeout = true;
				break;
			}
		}

		/* Should wait on a signal really! */
		usleep(10000);
	}

	if (timeout)
	{
		LOG("Queue worker %s thread exiting after %d jobs due to timeout\n", worker->name, jobs);
	}
	else
	{
		LOG("Queue worker %s thread exiting after %d jobs\n", worker->name, jobs);
	}

	return NULL;
}

void worker_init(struct worker *worker, const char *name, unsigned timeout, int nice, worker_callback callback)
{
	memset(worker, 0, sizeof *worker);

	strncpy(worker->name, name, sizeof worker->name);
	worker->thread_valid = false;
	worker->thread_timeout = false;
	worker->timeout = timeout;
	worker->nice = nice;

	worker->queue = queue_new();
	worker->callback = callback;

	LOG("Queue worker %s initialised\n", worker->name);
}

void worker_deinit(struct worker *worker)
{
	if (worker->thread_valid)
	{
		if (!worker->thread_timeout) queue_produce(worker->queue, NULL);
		pthread_join(worker->thread, NULL);
	}

	queue_delete(worker->queue);

	LOG("Queue worker %s deinitialised\n", worker->name);
}

void worker_queue(struct worker *worker, void *data)
{
	if (worker->thread_timeout)
	{
		pthread_join(worker->thread, NULL);
		worker->thread_timeout = false;
		worker->thread_valid = false;
	}

	if (!worker->thread_valid)
	{
		worker->thread_valid = (pthread_create(&worker->thread, NULL, &worker_thread, worker) == 0);
	}

	if (!queue_produce(worker->queue, data))
	{
		LOG("Queue worker %s unable to queue\n", worker->name);
	}
}


