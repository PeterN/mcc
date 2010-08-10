#ifndef LANDSCAPE_H
#define LANDSCAPE_H

struct landscape_t
{
	int seed;
	float p;
	int o;
	int height_range;

	int seed2;
	float p2;
	int o2;
};

void landscape_generate(struct landscape_t *l, struct block_t *blocks, int16_t x, int16_t y, int16_t z, int32_t offset_x, int32_t offset_y, int32_t offset_z);

#endif /* LANDSCAPE_H */
