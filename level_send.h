#ifndef LEVEL_SEND_H
#define LEVEL_SEND_H

void level_send_init();
void level_send_deinit();
void level_send_run();
void level_send_queue(struct client_t *client);

#endif /* LEVEL_SEND_H */
