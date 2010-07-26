#ifndef FILTER_H
#define FILTER_H

struct filter_t;

struct filter_t *filter_init(int x, int y);
void filter_deinit(struct filter_t *f);
void filter_process(struct filter_t *f, const float *map);
const float *filter_map(struct filter_t *f);

#endif /* FILTER_H */
