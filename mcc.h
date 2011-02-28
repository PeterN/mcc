#ifndef MCC_H
#define MCC_H

#include <stdio.h>
#include <stdbool.h>
#include <time.h>

struct server_t
{
	char *name;
	char *motd;
	char *salt;
	char *old_salt;
	int max_players;
	int players;
	int port;
	int public;
	bool exit;
	time_t start_time;
	clock_t cpu_start;
	double cpu_time;
	int pos_interval;
	int cuboid_max;
	int usleep;
	int physics_usleep;

	FILE *logfile;
};

extern struct server_t g_server;

#define LOG(args...) { fprintf(g_server.logfile, "%lld: ", (long long int)time(NULL)); fprintf(g_server.logfile, args); fflush(g_server.logfile); }

#endif /* MCC_H */
