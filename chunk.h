#ifndef CHUNK_H
#define CHUNK_H

#include "block.h"

#define CHUNK_SIZE 16

struct landscape_t;

struct chunk_t
{
    int32_t x;
    int32_t y;
    int32_t z;

    struct block_t blocks[CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE];

    bool dirty;

    struct chunk_t *hash_next;
};

void chunk_generate(struct chunk_t *chunk, struct landscape_t *landscape);
bool chunk_load(const char *name, struct chunk_t *chunk);
bool chunk_save(const char *name, struct chunk_t *chunk);

#endif
