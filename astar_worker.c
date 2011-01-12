#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include "astar.h"
#include "astar_worker.h"
#include "worker.h"

static struct worker s_astar_worker;

struct astar_job
{
	struct level_t *level;
	struct point a;
	struct point b;
	astar_cb *callback;
	void *data;
};

static void astar_worker(void *arg)
{
	struct astar_job *job = arg;

	struct point *path = as_find(job->level, &job->a, &job->b);
	job->callback(job->level, path, job->data);
	level_inuse(job->level, false);

	free(job);
}

void astar_worker_init()
{
	worker_init(&s_astar_worker, "astar", 30000, astar_worker);
}

void astar_worker_deinit()
{
	worker_deinit(&s_astar_worker);
}

void astar_queue(struct level_t *level, const struct point *a, const struct point *b, astar_cb *callback, void *data)
{
	assert(level != NULL);
	assert(a != NULL);
	assert(b != NULL);
	assert(callback != NULL);

	if (!level_inuse(level, true)) return;

	struct astar_job *job = malloc(sizeof *job);
	job->level = level;
	job->a = *a;
	job->b = *b;
	job->callback = callback;
	job->data = data;

	worker_queue(&s_astar_worker, job);
}

