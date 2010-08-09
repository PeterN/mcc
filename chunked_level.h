#ifndef CHUNKED_LEVEL_H
#define CHUNKED_LEVEL_H

#include <stdbool.h>
#include <pthread.h>
#include "landscape.h"
#include "queue.h"

struct chunk_t;

struct chunked_level_t
{
    char name[32];
    struct landscape_t landscape;

    struct chunk_t **chunk_hash;

    int32_t min_x, max_x, size_x;
    int32_t min_y, max_y, size_y;
    int32_t min_z, max_z, size_z;

    struct queue_t *load_queue;
    pthread_t load_thread;
};

bool chunked_level_init(struct chunked_level_t *cl, const char *name);
struct chunk_t *chunked_level_get_chunk(struct chunked_level_t *cl, int32_t x, int32_t y, int32_t z);

#endif /* CHUNKED_LEVEL_H */
