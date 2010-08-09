#include <stdlib.h>
#include "queue.h"
#include "mcc.h"

struct queue_object_t
{
    struct queue_object_t *next;
    void *data;
};

struct queue_t
{
    struct queue_object_t *head;
    struct queue_object_t *tail;
    struct queue_object_t *divider;
};

struct queue_t *queue_new()
{
    struct queue_t *queue = malloc(sizeof *queue);
    struct queue_object_t *qo = malloc(sizeof *qo);
    qo->next = NULL;
    qo->data = NULL;

    queue->head = qo;
    queue->tail = qo;
    queue->divider = qo;

    LOG("[queue_new] %p %p %p\n", queue->head, queue->tail, queue->divider);

    return queue;
}

void queue_delete(struct queue_t *queue)
{
    int n = 0;
    while (queue->head != NULL)
    {
        struct queue_object_t *qo = queue->head;
        queue->head = qo->next;

        free(qo->data);
        free(qo);
        n++;
    }

    LOG("[queue_delete] Removed %d items from queue\n", n);
}

int queue_produce(struct queue_t *queue, void *data)
{
    if (queue == NULL) return false;

    struct queue_object_t *qo = malloc(sizeof *qo);
    qo->next = NULL;
    qo->data = data;

    /* Lock needed? */
    queue->tail->next = qo;
    queue->tail = qo;
    /* */

    LOG("[queue_produce] Added to queue\n");
    LOG("[queue_produce] %p %p %p\n", queue->head, queue->tail, queue->divider);

    int n = 0;
    while (queue->head != queue->divider)
    {
        struct queue_object_t *qo = queue->head;
        queue->head = qo->next;

        free(qo->data);
        free(qo);

        n++;
    }

    if (n > 0) LOG("[queue_produce] Removed %d items from queue\n", n);

    return true;
}

int queue_consume(struct queue_t *queue, void **data)
{
    LOG("[queue_consume] %p %p %p\n", queue->head, queue->tail, queue->divider);
    if (queue->divider != queue->tail)
    {
        *data = queue->divider->next->data;
        queue->divider = queue->divider->next;
        return true;
    }

    return false;
}
