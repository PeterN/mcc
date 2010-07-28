#ifndef HOOK_H
#define HOOK_H

enum
{
	HOOK_CHAT = 1,
};

typedef void(*hook_func_t)(int hook, void *data, void *arg);

void register_hook(int hooks, hook_func_t func, void *arg);
void deregister_hook(hook_func_t func, void *arg);

void call_hook(int hook, void *data);

#endif /* HOOK_H */
