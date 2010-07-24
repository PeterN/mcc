#ifndef PERLIN_H
#define PERLIN_H

struct perlin_t;

struct perlin_t *perlin_init(int x, int y, float persistence, int octaves);
void perlin_deinit(struct perlin_t *pp);
void perlin_noise(struct perlin_t *pp);
const float *perlin_map(struct perlin_t *pp);

#endif /* PERLIN_H */
