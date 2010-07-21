#ifndef TIMER_H
#define TIMER_H

typedef void(*timer_func_t)(void *arg);

struct timer_t;

struct timer_t *register_timer(int interval, timer_func_t timer_func, void *arg);
void deregister_timer(struct timer_t *handle);
void process_timers(int tick);

#endif /* TIMER_H */
