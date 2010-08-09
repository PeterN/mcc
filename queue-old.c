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
    struct queue_object_t **head;
    struct queue_object_t **tail;
    struct queue_object_t *list;
};

struct queue_t *queue_new()
{
    struct queue_t *queue = malloc(sizeof *queue);
    queue->list = malloc(sizeof *queue->list);
    queue->list->next = NULL;
    queue->list->data = NULL;
    queue->head = &queue->list;
    queue->tail = &queue->list->next;

    LOG("[queue_new] %p %p %p\n", queue->list, queue->head, *queue->tail);

    return queue;
}

void queue_delete(struct queue_t *queue)
{
    int n = 0;
    while (queue->list != NULL)
    {
        struct queue_object_t *qo = queue->list;
        queue->list = qo->next;

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
    *queue->tail = qo;
    queue->tail = &qo->next;
    /* */

    LOG("[queue_produce] Added to queue\n");

    int n = 0;
    while (queue->list != *queue->head)
    {
        struct queue_object_t *qo = queue->list;
        queue->list = qo->next;

        free(qo->data);
        free(qo);

        n++;
    }

    if (n > 0) LOG("[queue_produce] Removed %d items from queue\n", n);

    return true;
}

int queue_consume(struct queue_t *queue, void **data)
{
    struct queue_object_t *next = *queue->head;
    next = next->next;

    LOG("[queue_consume] %p %p %p\n", queue->list, queue->head, *queue->tail);
    if (next != *queue->tail)
    {
        queue->head = &next->next;
        *data = next->next->data;
        return true;
    }

    return false;
}
