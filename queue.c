#include <stdlib.h>
#include <pthread.h>
#include "queue.h"
#include "mcc.h"

struct queue_object_t
{
    struct queue_object_t *next;
    void *data;
};

struct queue_t
{
    pthread_mutex_t produce_mutex;
    pthread_mutex_t consume_mutex;
    struct queue_object_t *head;
    struct queue_object_t *tail;
    struct queue_object_t *divider;
};

struct queue_t *queue_new(void)
{
    struct queue_t *queue = malloc(sizeof *queue);
    struct queue_object_t *qo = malloc(sizeof *qo);
    qo->next = NULL;
    qo->data = NULL;

    queue->head = qo;
    queue->tail = qo;
    queue->divider = qo;

    pthread_mutex_init(&queue->produce_mutex, NULL);
    pthread_mutex_init(&queue->consume_mutex, NULL);

    return queue;
}

void queue_delete(struct queue_t *queue)
{
    int n = 0;
    while (queue->head != NULL)
    {
        struct queue_object_t *qo = queue->head;
        queue->head = qo->next;

        //free(qo->data);
        free(qo);
        n++;
    }

    pthread_mutex_destroy(&queue->produce_mutex);
    pthread_mutex_destroy(&queue->consume_mutex);
}

int queue_produce(struct queue_t *queue, void *data)
{
    if (queue == NULL) return false;

    struct queue_object_t *qo = malloc(sizeof *qo);
    qo->next = NULL;
    qo->data = data;

    /* Lock needed? */
    pthread_mutex_lock(&queue->produce_mutex);
    queue->tail->next = qo;
    queue->tail = qo;
    /* */

    int n = 0;
    while (queue->head != queue->divider)
    {
        struct queue_object_t *qo = queue->head;
        queue->head = qo->next;

        //free(qo->data);
        free(qo);
        n++;
    }
    pthread_mutex_unlock(&queue->produce_mutex);

    return true;
}

int queue_consume(struct queue_t *queue, void **data)
{
    pthread_mutex_lock(&queue->consume_mutex);
    if (queue->divider != queue->tail)
    {
        *data = queue->divider->next->data;
        queue->divider = queue->divider->next;
        pthread_mutex_unlock(&queue->consume_mutex);
        return true;
    }

    pthread_mutex_unlock(&queue->consume_mutex);
    return false;
}
