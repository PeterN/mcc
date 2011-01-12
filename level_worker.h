#ifndef LEVEL_WORKER_H
#define LEVEL_WORKER_H

struct client_t;
struct level_t;

void level_worker_init(void);
void level_worker_deinit(void);

void level_save_queue(struct level_t *level);
void level_load_queue(struct level_t *level);
void level_make_queue(struct level_t *level);
void level_send_queue(struct client_t *client);

#endif /* LEVEL_WORKER_H */
