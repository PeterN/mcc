#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include "list.h"
#include "timer.h"
#include "mcc.h"
#include "gettime.h"

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
static pthread_mutex_t s_timers_mutex;

struct timer_t *register_timer(const char *name, unsigned interval, timer_func_t timer_func, void *arg, bool wait)
{
	struct timer_t *t = malloc(sizeof *t);
	t->name       = strdup(name);
	t->interval   = interval;
	t->timer_func = timer_func;
	t->arg        = arg;

	/* Make timer run on next tick */
	t->next_trigger = gettime() + (wait ? t->interval : 0);

	pthread_mutex_lock(&s_timers_mutex);
	timer_list_add(&s_timers, t);
	pthread_mutex_unlock(&s_timers_mutex);

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

void timers_deinit(void)
{
	while (s_timers.used > 0)
	{
		deregister_timer(s_timers.items[0]);
	}

	timer_list_free(&s_timers);
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

void timer_set_interval(struct timer_t *t, unsigned interval)
{
	/* Adjust next triggering */
	t->next_trigger += (interval - t->interval);
	t->interval = interval;
}

void timer_set_interval_by_name(const char *name, unsigned interval)
{
	unsigned i;
	for (i = 0; i < s_timers.used; i++)
	{
		struct timer_t *t = s_timers.items[i];
		if (strcmp(t->name, name) == 0)
		{
			timer_set_interval(t, interval);
			return;
		}
	}
}
