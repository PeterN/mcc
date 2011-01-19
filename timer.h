#ifndef TIMER_H
#define TIMER_H

typedef void(*timer_func_t)(void *arg);

struct timer_t;

struct timer_t *register_timer(const char *name, unsigned interval, timer_func_t timer_func, void *arg, bool wait);
void deregister_timer(struct timer_t *handle);
void process_timers(unsigned tick);

void timer_set_interval(struct timer_t *t, unsigned interval);

#endif /* TIMER_H */
