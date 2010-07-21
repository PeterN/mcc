#include <stdbool.h>
#include <time.h>
#include "list.h"
#include "timer.h"


static int gettime()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);

	return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

struct timer_t
{
	int interval;
	timer_func_t timer_func;
	void *arg;

	int last_trigger;
};

static inline bool timer_t_compare(struct timer_t **a, struct timer_t **b)
{
	return *a == *b;
}

LIST(timer, struct timer_t *, timer_t_compare)
static struct timer_list_t s_timers;

struct timer_t *register_timer(int interval, timer_func_t timer_func, void *arg)
{
	struct timer_t *t = malloc(sizeof *t);
	t->interval   = interval;
	t->timer_func = timer_func;
	t->arg        = arg;

	t->last_trigger = gettime();

	timer_list_add(&s_timers, t);

	return t;
}

void deregister_timer(struct timer_t *handle)
{
	timer_list_del_item(&s_timers, handle);

	free(handle);
}

void process_timers(int tick)
{
	unsigned i;

	for (i = 0; i < s_timers.used; i++)
	{
		struct timer_t *t = s_timers.items[i];
		if (tick < t->last_trigger + t->interval)
		{
			t->last_trigger = tick;
			t->timer_func(t->arg);
		}
	}
}
