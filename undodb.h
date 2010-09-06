#ifndef UNDODB_H
#define UNDODB_H

#include <stdint.h>

struct undodb_t;

struct undodb_t *undodb_init(const char *name);
void undodb_close(struct undodb_t *);
void undodb_log(struct undodb_t *u, int playerid, int16_t x, int16_t y, int16_t z, int oldtype, int olddata, int newtype);

typedef void(*query_func_t)(const char *text, void *arg);
void undodb_query(struct undodb_t *u, query_func_t func, void *arg);
void undodb_query_player(struct undodb_t *u, int playerid, query_func_t func, void *arg);

typedef void(*undo_func_t)(int16_t x, int16_t y, int16_t z, int oldtype, int olddata, int newtype, void *arg);
void undodb_undo_player(struct undodb_t *u, int playerid, int limit, undo_func_t func, void *arg);

#endif /* UNDODB_H */
