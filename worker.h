#ifndef WORKER_H
#define WORKER_H

#include <pthread.h>
#include <semaphore.h>

typedef void(*worker_callback)(void *arg);

struct worker
{
	char name[16];
	int thread_valid;
	int thread_timeout;
	unsigned timeout;
	int nice;

	struct queue_t *queue;
	pthread_t thread;
	worker_callback callback;
	sem_t sem;
};

void worker_init(struct worker *worker, const char *name, unsigned timeout, int nice, worker_callback callback);
void worker_deinit(struct worker *worker);
void worker_queue(struct worker* worker, void *data);

#endif /* WORKER_H */
