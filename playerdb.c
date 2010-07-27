#include <stdio.h>
#include <time.h>
#include <sqlite3.h>
#include "mcc.h"
#include "player.h"

static sqlite3 *s_db;
static sqlite3_stmt *s_rank_get_stmt;
static sqlite3_stmt *s_rank_set_stmt;
static sqlite3_stmt *s_new_user_stmt;
static sqlite3_stmt *s_globalid_get_stmt;
static sqlite3_stmt *s_username_get_stmt;
static sqlite3_stmt *s_password_stmt;
static sqlite3_stmt *s_set_password_stmt;
static sqlite3_stmt *s_get_last_ip_stmt;
static sqlite3_stmt *s_log_visit_stmt;
static sqlite3_stmt *s_log_identify_stmt;
static sqlite3_stmt *s_check_ban_stmt;
static sqlite3_stmt *s_banip_stmt;
static sqlite3_stmt *s_unbanip_stmt;

void playerdb_init()
{
	int res;

	res = sqlite3_open("player.db", &s_db);
	if (res != SQLITE_OK)
	{
		LOG("Can't open database: %s", sqlite3_errmsg(s_db));
		sqlite3_close(s_db);
		return;
	}

	char *err;
	sqlite3_exec(s_db, "CREATE TABLE IF NOT EXISTS players (id INTEGER PRIMARY KEY AUTOINCREMENT, username TEXT UNIQUE, rank INT, password TEXT, first_visit DATETIME, last_visit DATETIME, last_ip TEXT, identified INT)", NULL, NULL, &err);
	if (err != NULL)
	{
		LOG("Errrrr: %s", err);
		sqlite3_free(err);
	}

	sqlite3_exec(s_db, "CREATE TABLE IF NOT EXISTS bans (net TEXT PRIMARY KEY, date DATETIME)", NULL, NULL, &err);
	if (err != NULL)
	{
		LOG("Errrrr: %s", err);
		sqlite3_free(err);
	}

	res = sqlite3_prepare_v2(s_db, "SELECT rank FROM players WHERE username = lower(?)", -1, &s_rank_get_stmt, NULL);
	if (res != SQLITE_OK)
	{
		LOG("Can't prepare statement: %s", sqlite3_errmsg(s_db));
		sqlite3_close(s_db);
		return;
	}

	res = sqlite3_prepare_v2(s_db, "UPDATE players SET rank = ? WHERE username = lower(?)", -1, &s_rank_set_stmt, NULL);
	if (res != SQLITE_OK)
	{
		LOG("Can't prepare statement: %s", sqlite3_errmsg(s_db));
		sqlite3_close(s_db);
		return;
	}

	res = sqlite3_prepare_v2(s_db, "INSERT INTO players (username, rank) VALUES (lower(?), 1)", -1, &s_new_user_stmt, NULL);
	if (res != SQLITE_OK)
	{
		LOG("Can't prepare statement: %s", sqlite3_errmsg(s_db));
		sqlite3_close(s_db);
		return;
	}

	res = sqlite3_prepare_v2(s_db, "SELECT id FROM players WHERE username = lower(?)", -1, &s_globalid_get_stmt, NULL);
	if (res != SQLITE_OK)
	{
		LOG("Can't prepare statement: %s", sqlite3_errmsg(s_db));
		sqlite3_close(s_db);
		return;
	}

	res = sqlite3_prepare_v2(s_db, "SELECT username FROM players WHERE id = ?", -1, &s_username_get_stmt, NULL);
	if (res != SQLITE_OK)
	{
		LOG("Can't prepare statement: %s", sqlite3_errmsg(s_db));
		sqlite3_close(s_db);
		return;
	}

	res = sqlite3_prepare_v2(s_db, "SELECT COUNT(*) FROM players WHERE username = lower(?) AND (password = ? OR (? = '' AND password IS NULL))", -1, &s_password_stmt, NULL);
	if (res != SQLITE_OK)
	{
		LOG("Can't prepare statement: %s", sqlite3_errmsg(s_db));
		sqlite3_close(s_db);
		return;
	}

	res = sqlite3_prepare_v2(s_db, "UPDATE players SET password = ? WHERE username = lower(?)", -1, &s_set_password_stmt, NULL);
	if (res != SQLITE_OK)
	{
		LOG("Can't prepare statement: %s", sqlite3_errmsg(s_db));
		sqlite3_close(s_db);
		return;
	}

	res = sqlite3_prepare_v2(s_db, "SELECT last_ip FROM players WHERE id = ? AND identified = 1", -1, &s_get_last_ip_stmt, NULL);
	if (res != SQLITE_OK)
	{
		LOG("Can't prepare statement: %s", sqlite3_errmsg(s_db));
		sqlite3_close(s_db);
		return;
	}

	res = sqlite3_prepare_v2(s_db, "UPDATE players SET last_visit = ?, last_ip = ?, identified = ? WHERE id = ?", -1, &s_log_visit_stmt, NULL);
	if (res != SQLITE_OK)
	{
		LOG("Can't prepare statement: %s", sqlite3_errmsg(s_db));
		sqlite3_close(s_db);
		return;
	}

	res = sqlite3_prepare_v2(s_db, "UPDATE players SET identified = ? WHERE id = ?", -1, &s_log_identify_stmt, NULL);
	if (res != SQLITE_OK)
	{
		LOG("Can't prepare statement: %s", sqlite3_errmsg(s_db));
		sqlite3_close(s_db);
		return;
	}

	res = sqlite3_prepare_v2(s_db, "SELECT COUNT(*) FROM bans WHERE net = ?", -1, &s_check_ban_stmt, NULL);
	if (res != SQLITE_OK)
	{
		LOG("Can't prepare statement: %s", sqlite3_errmsg(s_db));
		sqlite3_close(s_db);
		return;
	}

	res = sqlite3_prepare_v2(s_db, "INSERT INTO bans (net, date) VALUES (?, ?)", -1, &s_banip_stmt, NULL);
	if (res != SQLITE_OK)
	{
		LOG("Can't prepare statement: %s", sqlite3_errmsg(s_db));
		sqlite3_close(s_db);
		return;
	}

	res = sqlite3_prepare_v2(s_db, "DELETE FROM bans WHERE net = ?", -1, &s_unbanip_stmt, NULL);
	if (res != SQLITE_OK)
	{
		LOG("Can't prepare statement: %s", sqlite3_errmsg(s_db));
		sqlite3_close(s_db);
		return;
	}
}

void playerdb_close()
{
	sqlite3_finalize(s_rank_get_stmt);
	sqlite3_finalize(s_rank_set_stmt);
	sqlite3_finalize(s_new_user_stmt);
	sqlite3_finalize(s_globalid_get_stmt);
	sqlite3_finalize(s_username_get_stmt);
	sqlite3_finalize(s_password_stmt);
	sqlite3_finalize(s_set_password_stmt);
	sqlite3_finalize(s_get_last_ip_stmt);
	sqlite3_finalize(s_log_visit_stmt);
	sqlite3_finalize(s_log_identify_stmt);
	sqlite3_finalize(s_check_ban_stmt);
	sqlite3_finalize(s_banip_stmt);
	sqlite3_finalize(s_unbanip_stmt);
	sqlite3_close(s_db);
}

int playerdb_get_globalid(const char *username, bool add, bool *added)
{
	int i;
	int res;

	if (added != NULL) *added = false;

	for (i = 0; i < (add ? 2 : 1); i++)
	{
		if (sqlite3_bind_text(s_globalid_get_stmt, 1, username, -1, SQLITE_STATIC) != SQLITE_OK)
		{
			LOG("[playerid_get_globalid] Can't bind text: %s\n", sqlite3_errmsg(s_db));
			sqlite3_reset(s_globalid_get_stmt);
			return -1;
		}
		if (sqlite3_step(s_globalid_get_stmt) == SQLITE_ROW)
		{
			res = sqlite3_column_int(s_globalid_get_stmt, 0);
			sqlite3_reset(s_globalid_get_stmt);
			return res;
		}
		sqlite3_reset(s_globalid_get_stmt);

		if (i == 0 && add)
		{
			/* New user! */
			if (sqlite3_bind_text(s_new_user_stmt, 1, username, -1, SQLITE_STATIC) != SQLITE_OK)
			{
				LOG("[playerdb_get_globalid] Can't bind text: %s\n", sqlite3_errmsg(s_db));
				sqlite3_reset(s_new_user_stmt);
				return -1;
			}
			res = sqlite3_step(s_new_user_stmt);
			if (res != SQLITE_DONE)
			{
				LOG("Unable to create new user '%s'\n", username);
				sqlite3_reset(s_new_user_stmt);
				return -1;
			}
			sqlite3_reset(s_new_user_stmt);

			if (added != NULL) *added = true;
		}
	}

	LOG("Unable to get globalid for '%s'", username);
	return -1;
}

const char *playerdb_get_username(int globalid)
{
	static char buf[64];
	
	if (sqlite3_bind_int(s_username_get_stmt, 1, globalid) != SQLITE_OK)
	{
		LOG("[playerdb_get_username] Can't bind int: %s\n", sqlite3_errmsg(s_db));
	}
	else if (sqlite3_step(s_username_get_stmt) == SQLITE_ROW)
	{
		snprintf(buf, sizeof buf, "%s", sqlite3_column_text(s_username_get_stmt, 0));
		sqlite3_reset(s_username_get_stmt);
		return buf;
	}

	sqlite3_reset(s_username_get_stmt);
	return "unknown";
}

int playerdb_get_rank(const char *username)
{
	enum rank_t res = RANK_GUEST;

	if (sqlite3_bind_text(s_rank_get_stmt, 1, username, -1, SQLITE_STATIC) != SQLITE_OK)
	{
		LOG("[playerdb_get_rank] Can't bind text: %s\n", sqlite3_errmsg(s_db));
	}
	else if (sqlite3_step(s_rank_get_stmt) == SQLITE_ROW)
	{
		res = (enum rank_t) sqlite3_column_int(s_rank_get_stmt, 0);
	}
	else
	{
		LOG("Unable to get rank for '%s'", username);
	}

	sqlite3_reset(s_rank_get_stmt);

	return res;
}

void playerdb_set_rank(const char *username, int rank)
{
	if (sqlite3_bind_int(s_rank_set_stmt, 1, rank) != SQLITE_OK)
	{
		LOG("[playerdb_set_rank] Can't bind int: %s\n", sqlite3_errmsg(s_db));
	}
	else if (sqlite3_bind_text(s_rank_set_stmt, 2, username, -1, SQLITE_STATIC) != SQLITE_OK)
	{
		LOG("[playerdb_set_rank] Can't bind text: %s\n", sqlite3_errmsg(s_db));
	}
	else if (sqlite3_step(s_rank_set_stmt) != SQLITE_DONE)
	{
		LOG("[playerdb_set_rank] %s\n", sqlite3_errmsg(s_db));
	}

	sqlite3_reset(s_rank_set_stmt);
}

int playerdb_password_check(const char *username, const char *password)
{
	int res = 0;
	if (sqlite3_bind_text(s_password_stmt, 1, username, -1, SQLITE_STATIC) != SQLITE_OK)
	{
		LOG("[playerdb_password_check] Can't bind text: %s\n", sqlite3_errmsg(s_db));
	}
	else if (sqlite3_bind_text(s_password_stmt, 2, password, -1, SQLITE_STATIC) != SQLITE_OK)
	{
		LOG("[playerdb_password_check] Can't bind text: %s\n", sqlite3_errmsg(s_db));
	}
	else if (sqlite3_bind_text(s_password_stmt, 3, password, -1, SQLITE_STATIC) != SQLITE_OK)
	{
		LOG("[playerdb_password_check] Can't bind text: %s\n", sqlite3_errmsg(s_db));
	}
	else if (sqlite3_step(s_password_stmt) != SQLITE_ROW)
	{
		LOG("[playerdb_password_check] %s\n", sqlite3_errmsg(s_db));
	}
	else
	{
		res = sqlite3_column_int(s_password_stmt, 0);
	}

	sqlite3_reset(s_password_stmt);

	return res;
}

void playerdb_set_password(const char *username, const char *password)
{
	if (sqlite3_bind_text(s_set_password_stmt, 2, username, -1, SQLITE_STATIC) != SQLITE_OK)
	{
		LOG("[playerdb_set_password] Can't bind text: %s\n", sqlite3_errmsg(s_db));
	}
	else if (sqlite3_bind_text(s_set_password_stmt, 1, password, -1, SQLITE_STATIC) != SQLITE_OK)
	{
		LOG("[playerdb_set_password] Can't bind text: %s\n", sqlite3_errmsg(s_db));
	}
	else if (sqlite3_step(s_set_password_stmt) != SQLITE_DONE)
	{
		LOG("[playerdb_set_password] %s\n", sqlite3_errmsg(s_db));
	}
	sqlite3_reset(s_set_password_stmt);
}

const char *playerdb_get_last_ip(int globalid)
{
	static char buf[64];
	int res;
	if (sqlite3_bind_int(s_get_last_ip_stmt, 1, globalid) != SQLITE_OK)
	{
		LOG("[playerdb_get_last_ip] Can't bind int: %s\n", sqlite3_errmsg(s_db));
		sqlite3_reset(s_get_last_ip_stmt);
		return "unknown";
	}
	res = sqlite3_step(s_get_last_ip_stmt);
	if (res == SQLITE_ROW)
	{
		snprintf(buf, sizeof buf, "%s", sqlite3_column_text(s_get_last_ip_stmt, 0));
		sqlite3_reset(s_get_last_ip_stmt);
		return buf;
	}

	sqlite3_reset(s_get_last_ip_stmt);
	return "";
}

/* UPDATE players SET last_visit = ?, last_ip = ? WHERE id = ? */
void playerdb_log_visit(int globalid, const char *ip, int identified)
{
	sqlite3_bind_int(s_log_visit_stmt, 1, time(NULL));
	sqlite3_bind_text(s_log_visit_stmt, 2, ip, -1, SQLITE_STATIC);
	sqlite3_bind_int(s_log_visit_stmt, 3, identified);
	sqlite3_bind_int(s_log_visit_stmt, 4, globalid);
	sqlite3_step(s_log_visit_stmt);
	sqlite3_reset(s_log_visit_stmt);
}

void playerdb_log_identify(int globalid, int identified)
{
	sqlite3_bind_int(s_log_identify_stmt, 1, identified);
	sqlite3_bind_int(s_log_identify_stmt, 2, globalid);
	sqlite3_step(s_log_identify_stmt);
	sqlite3_reset(s_log_identify_stmt);
}

bool playerdb_check_ban(const char *ip)
{
	int res;

	sqlite3_bind_text(s_check_ban_stmt, 1, ip, -1, SQLITE_STATIC);
	sqlite3_step(s_check_ban_stmt);
	res = sqlite3_column_int(s_check_ban_stmt, 0) > 0;
	sqlite3_reset(s_check_ban_stmt);

	return res;
}

void playerdb_ban_ip(const char *ip)
{
	sqlite3_bind_text(s_banip_stmt, 1, ip, -1, SQLITE_STATIC);
	sqlite3_bind_int(s_log_visit_stmt, 2, time(NULL));
	sqlite3_step(s_banip_stmt);
	sqlite3_reset(s_banip_stmt);
}

void playerdb_unban_ip(const char *ip)
{
	sqlite3_bind_text(s_unbanip_stmt, 1, ip, -1, SQLITE_STATIC);
	sqlite3_step(s_unbanip_stmt);
	sqlite3_reset(s_unbanip_stmt);
}
