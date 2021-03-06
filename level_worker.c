#include "worker.h"
#include "client.h"
#include "level.h"
#include "level_worker.h"

struct {
	struct worker save;
	struct worker load;
	struct worker make;
	struct worker send;
} s_level_workers;

struct level_make_job
{
	struct level_t *level;
	char *type;
};

void save_worker(void *data)
{
	level_save_thread(data);
}

void load_worker(void *data)
{
	level_load_thread(data);
}

void make_worker(void *data)
{
	struct level_make_job *job = data;
	level_gen_thread(job->level, job->type);
	free(job->type);
	free(job);
}

void send_worker(void *data)
{
	struct client_t *client = data;

	if (client_inuse(client, true))
	{
		level_send(client);
		if (client->waiting_for_level) level_send_queue(client);
		client_inuse(client, false);
	}
}

void level_worker_init(void)
{
	worker_init(&s_level_workers.save, "save", 30000, 10, &save_worker);
	worker_init(&s_level_workers.load, "load", 30000, 1, &load_worker);
	worker_init(&s_level_workers.make, "make", 30000, 10, &make_worker);
	worker_init(&s_level_workers.send, "send", 30000, 1, &send_worker);
}

void level_worker_deinit(void)
{
	worker_deinit(&s_level_workers.save);
	worker_deinit(&s_level_workers.load);
	worker_deinit(&s_level_workers.make);
	worker_deinit(&s_level_workers.send);
}

void level_save_queue(struct level_t *level)
{
	worker_queue(&s_level_workers.save, level);
}

void level_load_queue(struct level_t *level)
{
	worker_queue(&s_level_workers.load, level);
}

void level_make_queue(struct level_t *level, const char *type)
{
	struct level_make_job *job = malloc(sizeof *job);
	job->level = level;
	job->type  = strdup(type);

	worker_queue(&s_level_workers.make, job);
}

void level_send_queue(struct client_t *client)
{
	worker_queue(&s_level_workers.send, client);
}
