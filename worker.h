#ifndef WORKER_H
#define WORKER_H

#include <pthread.h>

typedef void worker_callback(void *arg);

struct worker
{
	char name[16];
	int thread_valid;
	unsigned timeout;

	struct queue_t *queue;
	pthread_t thread;
	worker_callback *callback;
};

void worker_init(struct worker *worker, const char *name, unsigned timeout, worker_callback *callback);
void worker_deinit(struct worker *worker);
void worker_queue(struct worker* worker, void *data);

#endif /* WORKER_H */
