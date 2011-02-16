#include "client.h"
#include "colour.h"
#include "commands.h"
#include "level.h"
#include "player.h"

static void reconstruct(char *buf, size_t len, int params, const char **param)
{
	int i;
	for (i = 0; i < params; i++)
	{
		strcat(buf, param[i]);
		if (i < params - 1) strcat(buf, " ");
	}
}

static const char help_8ball[] =
"/8ball <message>\n"
"Ask the Magic 8-Ball a question. It may reply....";

static const char *s_answer[] = {
	"As I see it, yes",
	"It is certain",
	"It is decidedly so",
	"Most likely",
	"Outlook good",
	"Signs point to yes",
	"Without a doubt",
	"Yes",
	"Yes - definitely",
	"You may rely on it",
	"Reply hazy, try again",
	"Ask again later",
	"Better not tell you now",
	"Cannot predict now",
	"Concentrate and ask again",
	"Don't count on it",
	"My reply is no",
	"My sources say no",
	"Outlook not so good",
	"Very doubtful",
};

static bool cmd_8ball(struct client_t *c, int params, const char **param)
{
	if (params < 2) return true;

	char buf[128];

	snprintf(buf, sizeof buf, "%s asks the Magic 8-Ball:" TAG_WHITE " ", playername(c->player, 1));
	reconstruct(buf, sizeof buf, params - 1, param + 1);
	level_notify_all(c->player->level, buf);

	snprintf(buf, sizeof buf, TAG_YELLOW "Magic 8-Ball:" TAG_WHITE " %s", s_answer[rand() % 20]);
	level_notify_all(c->player->level, buf);

	return false;
}

void module_init(void **data)
{
	register_command("8ball", RANK_REGULAR, &cmd_8ball, help_8ball);
}

void module_deinit(void *data)
{
	deregister_command("8ball");
}
