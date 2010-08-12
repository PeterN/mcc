#include <stdint.h>
#include "block.h"
#include "landscape.h"
#include "perlin.h"

void landscape_generate(struct landscape_t *l, struct block_t *blocks, int16_t x, int16_t y, int16_t z, int32_t offset_x, int32_t offset_y, int32_t offset_z)
{
    struct perlin_t *pp = perlin_init(x, z, l->seed, l->p, l->o);
    perlin_set_offset(pp, offset_x, offset_z);
    perlin_noise(pp);
    const float *map = perlin_map(pp);

    struct perlin_t *pp2 = perlin_init(x, z, l->seed2, l->p2, l->o2);
    perlin_set_offset(pp2, offset_x, offset_z);
    perlin_noise(pp2);
    const float *map2 = perlin_map(pp2);

    struct perlin_t *pp3 = perlin_init(x, z, l->seed + l->seed2, l->p, l->o2);
    perlin_set_offset(pp3, offset_x, offset_z);
    perlin_noise(pp3);
    const float *map3 = perlin_map(pp3);

    struct perlin_t *pp4 = perlin_init(x, z, l->seed, 0.100, 7);
    perlin_set_offset(pp4, offset_x, offset_z);
    perlin_noise(pp4);
    const float *map4 = perlin_map(pp4);

    int dx, dy, dz;
    int sh = -offset_y + y / 2;
    for (dz = 0; dz < z; dz++)
    {
        for (dx = 0; dx < x; dx++)
        {
            float p4 = map4[dx + dz * x] * 10;
            int h = map[dx + dz * x] * l->height_range * p4 - offset_y + y / 2;
            int h2 = abs(map2[dx + dz * x] * 5) % 128;
            int rh = h - 5;

            for (dy = 0; dy < y; dy++)
            {
//                struct block_t *b = &blocks[(dx << 8) | (dy << 4) | dz];
                struct block_t *b = &blocks[dx + (dy + dz * y) * x];

                if (dy == h)
                {
                    if (h2 & 1) b->type = DIRT;
                    else if (h2 & 2) b->type = SAND;
                    else if (h2 & 4) b->type = GRAVEL;
                    else if (h2 & 8) b->type = ROCK;
                    else b->type = dy < sh ? GRAVEL : GRASS;
                }
                else if (dy < h)
                {
                    b->type = (dy < rh) ? ROCK : DIRT;
                }
                else
                {
                    b->type = (dy < sh) ? WATER : AIR;
                }
            }

            h2 = map2[dx + dz * x] * 32;
            int h3 = map3[dx + dz * x] * l->height_range / 4 - offset_y + y / 2;
            for (dy = 0; dy < y; dy++)
            {
                if (dy > h3 && dy < h3 + h2)
                {
                    struct block_t *b = &blocks[dx + (dy + dz * y) * x];
                    b->type = AIR;
                }
            }
        }
    }

    perlin_deinit(pp);
    perlin_deinit(pp2);
    perlin_deinit(pp3);
    perlin_deinit(pp4);
}
