#include "client.h"
#include "colour.h"
#include "commands.h"
#include "config.h"
#include "level.h"
#include "player.h"
#include "worker.h"
#include "render.h"

static struct worker s_image_worker;
static char *s_image_path;

struct image_job
{
	struct level_t *level;
	struct client_t *client;
};

static void image_worker(void *arg)
{
	struct image_job *job = arg;

	level_render_png(job->level, 0, false, s_image_path);
	level_render_png(job->level, 0, true, s_image_path);

	if (client_is_valid(job->client))
	{
		char buf[64];
		snprintf(buf, sizeof buf, TAG_YELLOW "Image job complete for %s", job->level->name);
		client_notify(job->client, buf);
	}

	level_inuse(job->level, false);
}

static const char help_image[] =
"/image\n"
"Make images of the current level";

static bool cmd_image(struct client_t *c, int params, const char **param)
{
	if (!level_inuse(c->player->level, true)) return false;

	struct image_job *job = malloc(sizeof *job);
	job->level = c->player->level;
	job->client = c;

	char buf[64];
	snprintf(buf, sizeof buf, TAG_YELLOW "Image job queued for %s", job->level->name);
	client_notify(job->client, buf);
	worker_queue(&s_image_worker, job);
	return false;
}

void module_init(void **data)
{
	if (!config_get_string("image.path", &s_image_path))
	{
		s_image_path = ".";
	}
	worker_init(&s_image_worker, "image", 30000, &image_worker);
	register_command("image", RANK_OP, &cmd_image, help_image);
}

void module_deinit(void *data)
{
	deregister_command("image");
	worker_deinit(&s_image_worker);
}
