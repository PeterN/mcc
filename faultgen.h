#ifndef FAULTGEN_H
#define FAULTGEN_H

struct faultgen_t;

struct faultgen_t *faultgen_init(int x, int y);
void faultgen_deinit(struct faultgen_t *f);
void faultgen_create(struct faultgen_t *f);
const float *faultgen_map(struct faultgen_t *f);

#endif /* FAULTGEN_H */
