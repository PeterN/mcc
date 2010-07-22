#include <stdbool.h>
#include <time.h>
#include "list.h"
#include "timer.h"
#include "mcc.h"

static unsigned gettime()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);

	return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

struct timer_t
{
	char *name;
	unsigned interval;
	timer_func_t timer_func;
	void *arg;

	unsigned next_trigger;
};

static inline bool timer_t_compare(struct timer_t **a, struct timer_t **b)
{
	return *a == *b;
}

LIST(timer, struct timer_t *, timer_t_compare)
static struct timer_list_t s_timers;

struct timer_t *register_timer(const char *name, unsigned interval, timer_func_t timer_func, void *arg)
{
	struct timer_t *t = malloc(sizeof *t);
	t->name       = strdup(name);
	t->interval   = interval;
	t->timer_func = timer_func;
	t->arg        = arg;

	/* Make timer run on next tick */
	t->next_trigger = gettime();

	timer_list_add(&s_timers, t);

	LOG("Registered %s timer with %u ms interval\n", name, interval);

	return t;
}

void deregister_timer(struct timer_t *t)
{
	timer_list_del_item(&s_timers, t);

	LOG("Deregistered %s timer\n", t->name);

	free(t->name);
	free(t);
}

void process_timers(unsigned tick)
{
	unsigned i;

	for (i = 0; i < s_timers.used; i++)
	{
		struct timer_t *t = s_timers.items[i];
		if (tick >= t->next_trigger)
		{
			t->next_trigger = tick + t->interval;
			t->timer_func(t->arg);
		}
	}
}