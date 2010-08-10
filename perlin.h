#ifndef PERLIN_H
#define PERLIN_H

struct perlin_t;

struct perlin_t *perlin_init(int x, int y, int seed, float persistence, int octaves);
void perlin_deinit(struct perlin_t *pp);
void perlin_set_offset(struct perlin_t *pp, int x, int y);
void perlin_noise(struct perlin_t *pp);
const float *perlin_map(struct perlin_t *pp);

#endif /* PERLIN_H */
