#ifndef QUEUE_H
#define QUEUE_H

struct queue_t;

struct queue_t *queue_new();
void queue_delete(struct queue_t *queue);
int queue_produce(struct queue_t *queue, void *data);
int queue_consume(struct queue_t *queue, void **data);

#endif /* QUEUE_H */
