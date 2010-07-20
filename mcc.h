#ifndef MCC_H
#define MCC_H

#include <stdio.h>
#include <stdbool.h>
#include <time.h>

struct server_t
{
	char *name;
	char *motd;
	int max_players;
	int players;
	bool public;
	bool exit;
	time_t start_time;
	clock_t cpu_start;
	double cpu_time;

	FILE *logfile;

	struct
	{
		char *hostname;
		int port;
		char *name;
		char *channel;
		char *pass;
	} irc;
};

extern struct server_t g_server;

#define LOG(args...) { fprintf(g_server.logfile, "%lu: ", time(NULL)); fprintf(g_server.logfile, args); fflush(g_server.logfile); }

#endif /* MCC_H */
