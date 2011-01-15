#ifndef ASTAR_WORKER_H
#define ASTAR_WORKER_H

struct level_t;
struct point;

typedef void (*astar_callback)(struct level_t *level, struct point *path, void *data);

void astar_worker_init(void);
void astar_worker_deinit(void);

void astar_queue(struct level_t *level, const struct point *a, const struct point *b, astar_callback callback, void *data);

#endif /* ASTAR_WORKER_H */
