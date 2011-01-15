#ifndef HASH_H
#define HASH_H

#include <stdbool.h>

struct hashnode
{
	int key1;
	int key2;
	void *value;
	struct hashnode *next;
};

typedef unsigned (*hash_proc)(int key1, int key2);

struct hash
{
	hash_proc hash;
	unsigned size;
	unsigned num_buckets;
	struct hashnode *buckets;
	bool *buckets_in_use;
	bool freeb;
	bool freeh;
};

void *hashnode_delete(struct hash *h, int key1, int key2);
void *hashnode_set(struct hash *h, int key1, int key2, void *value);
void *hashnode_get(const struct hash *h, int key1, int key2);

struct hash *hash_new(hash_proc hash, unsigned num_buckets);
void hash_init(struct hash *h, hash_proc hash, unsigned num_buckets);

void hash_delete(struct hash *h, bool free_values);
void hash_clear(struct hash *h, bool free_values);
unsigned hash_size(const struct hash *h);

#endif /* HASH_H */
