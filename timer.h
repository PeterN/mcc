#ifndef TIMER_H
#define TIMER_H

typedef void(*timer_func_t)(void *arg);

struct timer_t;

struct timer_t *register_timer(const char *name, unsigned interval, timer_func_t timer_func, void *arg);
void deregister_timer(struct timer_t *handle);
void process_timers(unsigned tick);

#endif /* TIMER_H */
