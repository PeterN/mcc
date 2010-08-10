#ifndef CHUNK_H
#define CHUNK_H

#include "block.h"

#define CHUNK_BITS_X 4
#define CHUNK_BITS_Y 7
#define CHUNK_BITS_Z 4
#define CHUNK_SIZE_X (1 << CHUNK_BITS_X)
#define CHUNK_SIZE_Y (1 << CHUNK_BITS_Y)
#define CHUNK_SIZE_Z (1 << CHUNK_BITS_Z)
#define CHUNK_MASK_X (CHUNK_SIZE_X - 1)
#define CHUNK_MASK_Y (CHUNK_SIZE_Y - 1)
#define CHUNK_MASK_Z (CHUNK_SIZE_Z - 1)

struct landscape_t;

struct chunk_t
{
    int32_t x;
    int32_t y;
    int32_t z;

    struct block_t blocks[CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z];

    bool dirty;
    bool ready;
    bool inuse;
    bool purge;

    struct chunk_t *hash_next;
};

void chunk_generate(struct chunk_t *chunk, struct landscape_t *landscape);
bool chunk_load(const char *name, struct chunk_t *chunk);
bool chunk_save(const char *name, struct chunk_t *chunk);

#endif
