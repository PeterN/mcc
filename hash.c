#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "hash.h"

void hash_init(struct hash *h, hash_proc hash, unsigned num_buckets)
{
	assert(h != NULL);
	h->hash = hash;
	h->size = 0;
	h->num_buckets = num_buckets;
	h->buckets = malloc(num_buckets * (sizeof *h->buckets + sizeof *h->buckets_in_use));
	h->buckets_in_use = (bool *)(h->buckets + num_buckets);
	h->freeh = false;
	memset(h->buckets, 0, num_buckets * (sizeof *h->buckets + sizeof *h->buckets_in_use));
}

struct hash *hash_new(hash_proc hash, unsigned num_buckets)
{
	struct hash *h = malloc(sizeof *h);

	hash_init(h, hash, num_buckets);
	h->freeh = true;

	return h;
}

void hash_delete(struct hash *h, bool free_values)
{
	int i;
	for (i = 0; i < h->num_buckets; i++)
	{
		if (!h->buckets_in_use[i]) continue;

		if (free_values) free(h->buckets[i].value);

		struct hashnode *node = h->buckets[i].next;
		while (node != NULL)
		{
			struct hashnode *prev = node;
			node = node->next;
			if (free_values) free(prev->value);
			free(prev);
		}
	}
	free(h->buckets);
	if (h->freeh) free(h);
}

void hash_clear(struct hash *h, bool free_values)
{
	int i;
	for (i = 0; i < h->num_buckets; i++)
	{
		if (!h->buckets_in_use[i]) continue;

		if (free_values) free(h->buckets[i].value);

		struct hashnode *node = h->buckets[i].next;
		while (node != NULL)
		{
			struct hashnode *prev = node;
			node = node->next;
			if (free_values) free(prev->value);
			free(prev);
		}
	}
	h->size = 0;
}

static struct hashnode *hash_findnode(const struct hash *h, int key1, int key2, struct hashnode **prev_out)
{
	unsigned hash = h->hash(key1, key2);
	struct hashnode *result = NULL;

	if (!h->buckets_in_use[hash])
	{
		if (prev_out != NULL) *prev_out = NULL;
		result = NULL;
	}
	else if (h->buckets[hash].key1 == key1 && h->buckets[hash].key2 == key2)
	{
		result = h->buckets + hash;
		if (prev_out != NULL) *prev_out = NULL;
	}
	else
	{
		struct hashnode *prev = h->buckets + hash;
		struct hashnode *node;

		for (node = prev->next; node != NULL; node = node->next)
		{
			if (node->key1 == key1 && node->key2 == key2)
			{
				result = node;
				break;
			}
			prev = node;
		}
		if (prev_out != NULL) *prev_out = prev;
	}
	return result;
}

void *hashnode_delete(struct hash *h, int key1, int key2)
{
	void *result;
	struct hashnode *prev;
	struct hashnode *node = hash_findnode(h, key1, key2, &prev);

	if (node == NULL)
	{
		result = NULL;
	}
	else if (prev == NULL)
	{
		result = node->value;
		if (node->next != NULL)
		{
			struct hashnode *next = node->next;
			*node = *next;
			free(next);
		}
		else
		{
			unsigned hash = h->hash(key1, key2);
			h->buckets_in_use[hash] = false;
		}
	}
	else
	{
		result = node->value;
		prev->next = node->next;
		free(node);
	}
	if (result != NULL) h->size--;
	return result;
}

void *hashnode_set(struct hash *h, int key1, int key2, void *value)
{
	struct hashnode *prev;
	struct hashnode *node = hash_findnode(h, key1, key2, &prev);

	if (node != NULL)
	{
		void *result = node->value;
		node->value = value;
		return result;
	}
	if (prev == NULL)
	{
		uint hash = h->hash(key1, key2);
		h->buckets_in_use[hash] = true;
		node = h->buckets + hash;
	}
	else
	{
		node = malloc(sizeof *node);
		prev->next = node;
	}

	node->next = NULL;
	node->key1 = key1;
	node->key2 = key2;
	node->value = value;
	h->size++;

	return NULL;
}

void *hashnode_get(const struct hash *h, int key1, int key2)
{
	struct hashnode *node = hash_findnode(h, key1, key2, NULL);
	return node != NULL ? node->value : NULL;
}

unsigned hash_size(const struct hash *h)
{
	return h->size;
}
