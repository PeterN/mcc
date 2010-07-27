#include "mcc.h"
#include "playerdb.h"
#include "string.h"

struct server_t g_server;

int main(int argc, char **argv)
{
	g_server.logfile = stderr;

	if (argc != 1)
	{
		LOG("Usage: %s < list\n", argv[0]);
		return 0;
	}

	int ban = strstr(argv[0], "unbanip") == NULL;

	playerdb_init();

	char buf[64];
	while (fgets(buf, sizeof buf, stdin) != 0)
	{
		char *nl = strchr(buf, '\n');
		if (nl != NULL) *nl = '\0';

		if (ban)
		{
			playerdb_ban_ip(buf);
		}
		else
		{
			playerdb_unban_ip(buf);
		}
	}

	playerdb_close();

	return 0;
}
