#ifndef ASTAR_THREAD_H
#define ASTAR_THREAD_H

#include "astar.h"

typedef void astar_cb(struct level_t *level, struct point *path, void *data);

void astar_thread_init();
void astar_thread_deinit();
void astar_thread_run();
void astar_queue(struct level_t *level, struct point a, struct point b, astar_cb *callback, void *data);

#endif /* ASTAR_THREAD_H */
