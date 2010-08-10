#include <stdbool.h>
#include <zlib.h>
#include "chunk.h"
#include "landscape.h"

static bool chunk_format_name(char *buf, size_t buf_len, const char *name, int32_t x, int32_t y, int32_t z)
{
    char px = x < 0 ? 'X' : 'x';
    char py = y < 0 ? 'Y' : 'y';
    char pz = z < 0 ? 'Z' : 'z';

    size_t len = snprintf(buf, buf_len, "%s/chunk_%c%d%c%d%c%d", name, px, abs(x), py, abs(y), pz, abs(z));
    return len < buf_len;
}

void chunk_generate(struct chunk_t *chunk, struct landscape_t *landscape)
{
    landscape_generate(landscape, chunk->blocks, CHUNK_SIZE_X, CHUNK_SIZE_Y, CHUNK_SIZE_Z, chunk->x * CHUNK_SIZE_X, chunk->y * CHUNK_SIZE_Y, chunk->z * CHUNK_SIZE_Z);
    chunk->dirty = true;
}

bool chunk_load(const char *name, struct chunk_t *chunk)
{
    char filename[64];
    chunk_format_name(filename, sizeof filename, name, chunk->x, chunk->y, chunk->z);

    gzFile gz = gzopen(filename, "rb");
    if (gz == NULL) return false;

    gzread(gz, chunk->blocks, sizeof chunk->blocks);
    gzclose(gz);

    chunk->dirty = false;

    return true;
}

bool chunk_save(const char *name, struct chunk_t *chunk)
{
    char filename[64];
    chunk_format_name(filename, sizeof filename, name, chunk->x, chunk->y, chunk->z);

    gzFile gz = gzopen(filename, "wb");
    if (gz == NULL) return false;

    gzwrite(gz, chunk->blocks, sizeof chunk->blocks);
    gzclose(gz);

    chunk->dirty = false;

    return true;
}
