#include <stdio.h>
#include <stdbool.h>
#include "hook.h"
#include "list.h"

struct hook_t
{
	int hooks;
	hook_func_t func;
	void *arg;
};

static inline bool hook_t_compare(struct hook_t *a, struct hook_t *b)
{
	return a->func == b->func && a->arg == b->arg;
}

LIST(hook, struct hook_t, hook_t_compare)

static struct hook_list_t s_hooks;

void register_hook(int hooks, hook_func_t func, void *arg)
{
	struct hook_t h;
	h.hooks = hooks;
	h.func  = func;
	h.arg   = arg;

	hook_list_add(&s_hooks, h);
}

void deregister_hook(hook_func_t func, void *arg)
{
	struct hook_t h;
	h.func = func;
	h.arg  = arg;

	hook_list_del_item(&s_hooks, h);
}

void call_hook(int hook, void *data)
{
	unsigned i;
	for (i = 0; i < s_hooks.used; i++)
	{
		struct hook_t *h = &s_hooks.items[i];
		if ((h->hooks & hook) != 0)
		{
			h->func(hook, data, h->arg);
		}
	}
}
