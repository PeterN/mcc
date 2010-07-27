#include "mcc.h"
#include "player.h"
#include "playerdb.h"
#include "string.h"
#include "util.h"

struct server_t g_server;

int main(int argc, char **argv)
{
	g_server.logfile = stderr;

	if (argc != 2)
	{
		LOG("Usage: setrank <rank> < list\n");
		return 0;
	}

	enum rank_t rank = rank_get_by_name(argv[1]);
	if (rank == -1)
	{
		LOG("Unknown rank %s\n", argv[1]);
		return 0;
	}

	playerdb_init();

	char buf[64];
	while (fgets(buf, sizeof buf, stdin) != 0)
	{
		bool added;
		char *nl = strchr(buf, '\n');
		if (nl != NULL) *nl = '\0';

		lcase(buf);

		playerdb_get_globalid(buf, true, &added);
		playerdb_set_rank(buf, rank);
	}

	playerdb_close();

	return 0;
}
