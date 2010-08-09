#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "chunk.h"
#include "chunked_level.h"

#define HASH_BITS 4
#define HASH_SIZE (1 << HASH_BITS)
#define HASH_MASK (HASH_SIZE - 1)

void *chunked_level_get_chunk_thread(void *arg);

bool chunked_level_init(struct chunked_level_t *cl, const char *name)
{
    memset(cl, 0, sizeof *cl);

    snprintf(cl->name, sizeof cl->name, "%s", name);

    cl->min_x = cl->min_y = cl->min_z = INT_MAX;
    cl->max_x = cl->max_y = cl->max_z = INT_MIN;

    cl->chunk_hash = calloc(sizeof *cl->chunk_hash, HASH_SIZE);

    cl->landscape.seed = rand();
    cl->landscape.height_range = 64;
    cl->landscape.p = 0.9;
    cl->landscape.o = 16;

    cl->landscape.seed2 = rand();
    cl->landscape.p2 = 0.75;
    cl->landscape.o2 = 6;

    cl->load_queue = queue_new();

    if (pthread_create(&cl->load_thread, NULL, &chunked_level_get_chunk_thread, cl) != 0)
    {
        LOG("[chunked_level] Could not start chunk thread, expect delays\n");
        queue_delete(cl->load_queue);
        cl->load_queue = NULL;
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

void *chunked_level_get_chunk_thread(void *arg)
{
    struct chunked_level_t *cl = arg;

    while (true)
    {
        struct chunk_t *chunk;
        if (queue_consume(cl->load_queue, &chunk))
        {
            if (!chunk_load(cl->name, chunk))
            {
                chunk_generate(chunk, &cl->landscape);
            }

            LOG("Consumed from queue\n");
        }

        usleep(1000);
    }

    return NULL;
}

struct chunk_t *chunked_level_get_chunk(struct chunked_level_t *cl, int32_t x, int32_t y, int32_t z)
{
    uint32_t bucket = mix(x, y, z) & HASH_MASK;
    struct chunk_t *chunk = cl->chunk_hash[bucket];

    int n = 0;
    for (; chunk != NULL; chunk = chunk->hash_next)
    {
        if (chunk->x == x && chunk->y == y && chunk->z == z) return chunk;
        n++;
    }

    chunk = malloc(sizeof *chunk);
    chunk->x = x;
    chunk->y = y;
    chunk->z = z;

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
        }
    }

    return chunk;
}

void chunked_level_update_size(struct chunked_level_t *cl)
{
    cl->size_x = (cl->max_x - cl->min_x) * CHUNK_SIZE;
    cl->size_y = (cl->max_y - cl->min_y) * CHUNK_SIZE;
    cl->size_z = (cl->max_z - cl->min_z) * CHUNK_SIZE;
}

void chunked_level_load_area(struct chunked_level_t *cl, int32_t x, int32_t y, int32_t z, int r)
{
    int dx, dy, dz;
    for (dx = -r; dx <= r; dx++)
    {
        for (dy = -r; dy <= r; dy++)
        {
            for (dz = -r; dz <= r; dz++)
            {
                chunked_level_get_chunk(cl, x + dx, y + dy, z + dz);
            }
        }
    }

    chunked_level_update_size(cl);
}

void chunked_level_set_area(struct chunked_level_t *cl, int32_t x, int32_t y, int32_t z, int r)
{
    cl->min_x = x - r;
    cl->min_y = y - r;
    cl->min_z = z - r;
    cl->max_x = x + r;
    cl->max_y = y + r;
    cl->max_z = z + r;
    chunked_level_update_size(cl);
}

void chunked_level_hash_analysis(struct chunked_level_t *cl)
{
    printf("\n");
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

        printf("%d ", n);

        avg += n;
        if (n < min) min = n;
        if (n > max) max = n;
    }

    printf("\n");

    avg /= HASH_SIZE;

    LOG("chunked_level: hash analysis: min %d max %d avg %d\n", min, max, avg);
}
