#include <string.h>
#include "mcc.h"

void config_read(struct server_t *server)
{
	g_server.name = "TEST TEST TEST";
	g_server.motd = "Test server for Just Another Minecraft Server";
	g_server.max_players = 20;
	g_server.players = 0;
	g_server.public = true;
	g_server.exit = false;
	g_server.start_time = time(NULL);
	g_server.cpu_start = clock();

	g_server.irc.hostname = "irc.lspace.org";
	g_server.irc.port = 6667;
	g_server.irc.name = "mccbot";
	g_server.irc.channel = "#mc2";
	g_server.irc.pass = NULL;

	FILE *f = fopen("config.txt", "r");
	if (f == NULL) return;

	while (!feof(f))
	{
		char buf[1024];
		fgets(buf, sizeof buf, f);

		if (buf[0] == '#') continue;

		char *k = buf;
		char *n = strchr(buf, ' ');
		if (n == NULL) return;

		*n++ = '\0';

		if (strcasecmp(k, "name") == 0) server->name = strdup(n);
		else if (strcasecmp(k, "motd") == 0) server->motd = strdup(n);
	}
}
