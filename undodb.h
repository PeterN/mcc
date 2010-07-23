#ifndef UNDODB_H
#define UNDODB_H

#include <stdint.h>

struct undodb_t;

struct undodb_t *undodb_init(const char *name);
void undodb_close(struct undodb_t *);
void undodb_log(struct undodb_t *u, int playerid, int16_t x, int16_t y, int16_t z, int oldtype, int olddata, int newtype);

#endif /* UNDODB_H */
