#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>
#include "queue.h"
#include "worker.h"
#include "mcc.h"
#include "gettime.h"

void *worker_thread(void *arg)
{
	struct worker *worker = arg;

	bool timeout = false;
	int jobs = 0;
	pid_t tid = (pid_t)syscall(SYS_gettid);

	nice(worker->nice);

	LOG("Queue worker %s thread (%u) started with nice %d\n", worker->name, tid, worker->nice);

	while (true)
	{
		struct timespec ts;
		clock_gettime(CLOCK_REALTIME, &ts);
		ts.tv_sec += worker->timeout;

		int s = sem_timedwait(&worker->sem, &ts);
		if (s == -1)
		{
			if (errno == ETIMEDOUT) {
				timeout = true;
				break;
			}
			LOG("Queue worker %s thread (%u): %s\n", worker->name, tid, strerror(errno));
			break;
		}

		void *data;
		if (queue_consume(worker->queue, &data))
		{
			/* Null item added to the queue indicate we should exit. */
			if (data == NULL) break;

			worker->callback(data);
			jobs++;
		}
	}

	if (timeout)
	{
		LOG("Queue worker %s thread (%u) exiting after %d jobs due to timeout\n", worker->name, tid, jobs);
	}
	else
	{
		LOG("Queue worker %s thread (%u) exiting after %d jobs\n", worker->name, tid, jobs);
	}

	return NULL;
}

void worker_init(struct worker *worker, const char *name, unsigned timeout, int nice, worker_callback callback)
{
	memset(worker, 0, sizeof *worker);

	strncpy(worker->name, name, sizeof worker->name);
	worker->thread_valid = false;
	worker->thread_timeout = false;
	worker->timeout = timeout / 1000;
	worker->nice = nice;

	worker->queue = queue_new();
	worker->callback = callback;
	sem_init(&worker->sem, 0, 0);

	LOG("Queue worker %s initialised\n", worker->name);
}

void worker_deinit(struct worker *worker)
{
	if (worker->thread_valid)
	{
		if (!worker->thread_timeout)
		{
			if (queue_produce(worker->queue, NULL))
			{
				sem_post(&worker->sem);
			}
		}

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

	if (queue_produce(worker->queue, data))
	{
		sem_post(&worker->sem);
	}
	else
	{
		LOG("Queue worker %s unable to queue\n", worker->name);
	}
}


