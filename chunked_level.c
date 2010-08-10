#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "chunk.h"
#include "chunked_level.h"

#define HASH_BITS 12
#define HASH_SIZE (1 << HASH_BITS)
#define HASH_MASK (HASH_SIZE - 1)

void *chunked_level_save_thread(void *arg);
void *chunked_level_get_chunk_thread(void *arg);

bool chunked_level_init(struct chunked_level_t *cl, const char *name)
{
    memset(cl, 0, sizeof *cl);

    snprintf(cl->name, sizeof cl->name, "%s", name);

    cl->min_x = cl->min_y = cl->min_z = INT_MAX;
    cl->max_x = cl->max_y = cl->max_z = INT_MIN;

    cl->chunk_hash = calloc(sizeof *cl->chunk_hash, HASH_SIZE);

    cl->landscape.seed = rand();
    cl->landscape.height_range = 32;
    cl->landscape.p = 0.75;
    cl->landscape.o = 6;

    cl->landscape.seed2 = rand();
    cl->landscape.p2 = 0.75;
    cl->landscape.o2 = 6;

    cl->load_queue = queue_new();
    cl->save_queue = queue_new();
    cl->purge_queue = queue_new();

    int i;
    for (i = 0; i < 4; i++)
    {
        if (pthread_create(&cl->load_thread[i], NULL, &chunked_level_get_chunk_thread, cl) != 0)
        {
            if (i == 0)
            {
                LOG("[chunked_level] Could not start chunk load thread, expect delays\n");
                queue_delete(cl->load_queue);
                cl->load_queue = NULL;
            }
            break;
        }
    }

    LOG("[chunked_level] Started %d consumer threads\n", i);

    if (pthread_create(&cl->save_thread, NULL, &chunked_level_save_thread, cl) != 0)
    {
        LOG("[chunked_level] Could not start chunk save thread, expect delays\n");
        queue_delete(cl->save_queue);
        cl->save_queue = NULL;
    }

    return true;
}

static uint32_t mix(uint32_t a, uint32_t b, uint32_t c)
{
    a -= b; a -= c; a ^= c >> 13;
    b -= c; b -= a; b ^= a << 8;
    c -= a; c -= b; c ^= b >> 13;
    a -= b; a -= c; a ^= c >> 12;
    b -= c; b -= a; b ^= a << 16;
    c -= a; c -= b; c ^= b >> 5;
    a -= b; a -= c; a ^= c >> 3;
    b -= c; b -= a; b ^= a << 10;
    c -= a; c -= b; c ^= b >> 15;
    return c;
}

void *chunked_level_save_thread(void *arg)
{
    struct chunked_level_t *cl = arg;

    while (true)
    {
        struct chunk_t *chunk;
        if (queue_consume(cl->save_queue, &chunk))
        {
            chunk_save(cl->name, chunk);
        }
        else
        {
            /* Sleep 10ms if there was nothing to consume */
            usleep(10000);
        }
    }
}

void chunked_level_purge(struct chunked_level_t *cl, struct chunk_t *chunk)
{
    /* Don't purge if the chunk is in use again */
    if (chunk->inuse) {
        chunk->purge = false;
        return;
    }

    uint32_t bucket = mix(chunk->x, chunk->y, chunk->z) & HASH_MASK;
    struct chunk_t *chunkp = cl->chunk_hash[bucket];
    struct chunk_t **last = &cl->chunk_hash[bucket];

    for (; chunkp != NULL; chunkp = chunkp->hash_next)
    {
        if (chunkp == chunk)
        {
            *last = chunk->hash_next;

            LOG("[chunked_level] Purge chunk at %d x %d x %d\n", chunk->x, chunk->y, chunk->z);
            free(chunk);
            return;
        }

        last = &chunkp->hash_next;
    }
}

void *chunked_level_get_chunk_thread(void *arg)
{
    struct chunked_level_t *cl = arg;

    while (true)
    {
        bool wait = true;
        struct chunk_t *chunk;
        if (queue_consume(cl->load_queue, &chunk))
        {
            if (!chunk_load(cl->name, chunk))
            {
                chunk_generate(chunk, &cl->landscape);
                if (!queue_produce(cl->save_queue, chunk))
                {
                    chunk_save(cl->name, chunk);
                }
            }
            chunk->ready = true;
            wait = false;
        }
        if (queue_consume(cl->purge_queue, &chunk))
        {
            chunked_level_purge(cl, chunk);
            wait = false;
        }

        /* Sleep 1ms if there was nothing to consume */
        if (wait) usleep(1000);
    }

    return NULL;
}

struct chunk_t *chunked_level_get_chunk(struct chunked_level_t *cl, int32_t x, int32_t y, int32_t z, bool fill)
{
    uint32_t bucket = mix(x, y, z) & HASH_MASK;
    struct chunk_t *chunk = cl->chunk_hash[bucket];

    for (; chunk != NULL; chunk = chunk->hash_next)
    {
        if (chunk->x == x && chunk->y == y && chunk->z == z) return chunk;
        if (!chunk->inuse && !chunk->purge)
        {
            LOG("[chunked_level] Chunk at %d x %d x %d is not in use\n", chunk->x, chunk->y, chunk->z);
            chunk->purge = true;
            queue_produce(cl->purge_queue, chunk);
        }
    }

    if (!fill) return NULL;

    chunk = malloc(sizeof *chunk);
    chunk->x = x;
    chunk->y = y;
    chunk->z = z;
    chunk->ready = false;
    chunk->inuse = true;

    memset(chunk->blocks, 0, sizeof chunk->blocks);

    if (x < cl->min_x) cl->min_x = x;
    if (y < cl->min_y) cl->min_y = y;
    if (z < cl->min_z) cl->min_z = z;
    if (x > cl->max_x) cl->max_x = x;
    if (y > cl->max_y) cl->max_y = y;
    if (z > cl->max_z) cl->max_z = z;

    /* Insert chunk into hash */
    chunk->hash_next = cl->chunk_hash[bucket];
    cl->chunk_hash[bucket] = chunk;

    if (!queue_produce(cl->load_queue, chunk))
    {
        if (!chunk_load(cl->name, chunk))
        {
            chunk_generate(chunk, &cl->landscape);
            if (!queue_produce(cl->save_queue, chunk))
            {
                chunk_save(cl->name, chunk);
            }
        }
        chunk->ready = true;
    }

    return chunk;
}

void chunked_level_update_size(struct chunked_level_t *cl)
{
    cl->size_x = (cl->max_x - cl->min_x + 1) * CHUNK_SIZE_X;
    cl->size_y = (cl->max_y - cl->min_y + 1) * CHUNK_SIZE_Y;
    cl->size_z = (cl->max_z - cl->min_z + 1) * CHUNK_SIZE_Z;
}

void chunked_level_load_area(struct chunked_level_t *cl, int32_t x, int32_t y, int32_t z, int rx, int ry, int rz)
{
    int dx, dy, dz;
    for (dx = 0; dx < rx; dx++)
    {
        for (dy = 0; dy < ry; dy++)
        {
            for (dz = 0; dz < rz; dz++)
            {
                chunked_level_get_chunk(cl, x + dx, y + dy, z + dz, true);
            }
        }
    }

    chunked_level_update_size(cl);
}

void chunked_level_set_area(struct chunked_level_t *cl, int32_t x, int32_t y, int32_t z, int rx, int ry, int rz)
{
    cl->min_x = x;
    cl->min_y = y;
    cl->min_z = z;
    cl->max_x = x + rx - 1;
    cl->max_y = y + ry - 1;
    cl->max_z = z + rz - 1;
    chunked_level_update_size(cl);
}

void chunked_level_hash_analysis(struct chunked_level_t *cl)
{
    int i;
    int avg = 0, min = INT_MAX, max = 0;
    for (i = 0; i < HASH_SIZE; i++)
    {
        struct chunk_t *chunk = cl->chunk_hash[i];
        int n = 0;
        for (; chunk != NULL; chunk = chunk->hash_next)
        {
            n++;
        }

        avg += n;
        if (n < min) min = n;
        if (n > max) max = n;
    }

    avg /= HASH_SIZE;

    LOG("chunked_level: hash analysis: min %d max %d avg %d\n", min, max, avg);
}
