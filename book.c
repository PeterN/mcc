#include <stdio.h>
#include <stdlib.h>
#include "block.h"
#include "client.h"
#include "colour.h"
#include "level.h"
#include "player.h"

#define FLOOR1 IRON
#define FLOOR2 GOLDSOLID

static struct
{
	enum blocktype_t book;
} s;

static enum blocktype_t convert_book(struct level_t *level, unsigned index, const struct block_t *block)
{
	return BOOKCASE;
}

static int trigger_book(struct level_t *level, unsigned index, const struct block_t *block, struct client_t *c)
{
	char filename[20];
	sprintf(filename, "book%04d.txt", block->data);
	client_notify_file(c, filename);
	return TRIG_FILL;
}

void delete_book(struct level_t *l, unsigned index, const struct block_t *block)
{
	char filename[20];
	sprintf(filename, "book%04d.txt", block->data);
	remove(filename);
}

struct book_t
{
	int book[MAX_CLIENTS_PER_LEVEL];
};

int book_allocate()
{
	int i;
	for (i = 1; i < 10000; i++)
	{
		char filename[20];
		sprintf(filename, "book%04d.txt", i);
		FILE *f = fopen(filename, "r");
		if (f == NULL)
		{
			f = fopen(filename, "w");
			if (f != NULL)
			{
				fclose(f);
				return i;
			}
		}
		fclose(f);
	}
	return 0;
}

static bool book_handle_chat(struct level_t *l, struct client_t *c, char *data, struct book_t *arg)
{
	char buf[128];

	if (arg->book[c->player->levelid] != 0)
	{
		if (strcmp(data, "cancel book") == 0)
		{
			char filename[20];
			sprintf(filename, "book%04d.txt", arg->book[c->player->levelid]);
			remove(filename);

			arg->book[c->player->levelid] = 0;
			client_notify(c, TAG_YELLOW "Canceled writing book");
		}
		else
		{
			char filename[20];
			sprintf(filename, "book%04d.txt", arg->book[c->player->levelid]);
			FILE *f = fopen(filename, "a");
			fprintf(f, "%s\n", data);
			fclose(f);

			client_notify(c, data);
		}
		return true;
	}

	if (!level_user_can_build(l, c->player)) return false;

	if (strcasecmp("write book", data) == 0)
	{
		arg->book[c->player->levelid] = book_allocate();
		snprintf(buf, sizeof buf, TAG_YELLOW "Writing book %04d, type 'cancel book' or click block to finish", arg->book[c->player->levelid]);
		client_notify(c, buf);
		return true;
	}

	return false;
}

static void book_handle_block(struct level_t *l, struct client_t *c, struct block_event *be, struct book_t *arg)
{
	if (arg->book[c->player->levelid] != 0)
	{
		be->nt = s.book;
		be->data = arg->book[c->player->levelid];
		arg->book[c->player->levelid] = 0;
		client_notify(c, TAG_YELLOW "Finished writing book");
	}
}

static bool book_level_hook(int event, struct level_t *l, struct client_t *c, void *data, struct level_hook_data_t *arg)
{
	switch (event)
	{
		case EVENT_CHAT:
			return book_handle_chat(l, c, data, arg->data);

		case EVENT_BLOCK:
			book_handle_block(l, c, data, arg->data);
			break;

		case EVENT_INIT:
			if (arg->size != 0) free(arg->data);
			arg->size = sizeof (struct book_t);
			arg->data = calloc(1, arg->size);
			break;

		case EVENT_DEINIT:
			if (l == NULL) break;

			free(arg->data);
			arg->size = 0;
			arg->data = NULL;
			break;
	}

	return false;
}

void module_init(void **data)
{
	s.book = register_blocktype(BLOCK_INVALID, "book", RANK_ADMIN, &convert_book, &trigger_book, &delete_book, NULL, false, false, false);
	register_level_hook_func("book", &book_level_hook);
}

void module_deinit(void *data)
{
	deregister_blocktype(s.book);
	deregister_level_hook_func("book");
}
