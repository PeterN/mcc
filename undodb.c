#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sqlite3.h>
#include "mcc.h"
#include "undodb.h"
#include "util.h"

struct undodb_t
{
	sqlite3 *db;
	sqlite3_stmt *insert_stmt;
};

struct undodb_t *undodb_init(const char *name)
{
	sqlite3 *db;
	sqlite3_stmt *insert_stmt;

	char buf[256];
	snprintf(buf, sizeof buf, "undo_%s.db", name);
	lcase(buf);

	int res;
	res = sqlite3_open(buf, &db);
	if (res != SQLITE_OK)
	{
		LOG("Can't open database: %s", sqlite3_errmsg(db));
		sqlite3_close(db);
		return NULL;
	}

	char *err;
	sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS undo (id INTEGER PRIMARY KEY AUTOINCREMENT, playerid INT, x INT, y INT, z INT, oldtype INT, olddata INT, newtype INT, time DATETIME)", NULL, NULL, &err);
	if (err != NULL)
	{
		LOG("Errrrr: %s", err);
		sqlite3_free(err);
	}

	res = sqlite3_prepare_v2(db, "INSERT INTO undo (playerid, x, y, z, oldtype, olddata, newtype, time) VALUES (?, ?, ?, ?, ?, ?, ?, date('now'))", -1, &insert_stmt, NULL);
	if (res != SQLITE_OK)
	{
		LOG("Can't prepare statement: %s", sqlite3_errmsg(db));
		sqlite3_close(db);
		return NULL;
	}

	struct undodb_t *u = malloc(sizeof *u);
	u->db = db;
	u->insert_stmt = insert_stmt;

	return u;
}

void undodb_close(struct undodb_t *u)
{
	if (u == NULL) return;

	sqlite3_finalize(u->insert_stmt);
	sqlite3_close(u->db);
	free(u);
}

void undodb_log(struct undodb_t *u, int playerid, int16_t x, int16_t y, int16_t z, int oldtype, int olddata, int newtype)
{
	if (u == NULL) return;

	sqlite3_reset(u->insert_stmt);
	sqlite3_bind_int(u->insert_stmt, 1, playerid);
	sqlite3_bind_int(u->insert_stmt, 2, x);
	sqlite3_bind_int(u->insert_stmt, 3, y);
	sqlite3_bind_int(u->insert_stmt, 4, z);
	sqlite3_bind_int(u->insert_stmt, 5, oldtype);
	sqlite3_bind_int(u->insert_stmt, 6, olddata);
	sqlite3_bind_int(u->insert_stmt, 7, newtype);

	int res = sqlite3_step(u->insert_stmt);
	if (res != SQLITE_DONE)
	{
		LOG("Undo log failed\n");
	}
}

