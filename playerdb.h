#ifndef PLAYERDB_H
#define PLAYERDB_H

void playerdb_init(void);
void playerdb_close(void);

int playerdb_get_globalid(const char *username, bool add, bool *added);
const char *playerdb_get_username(int globalid);
int playerdb_get_rank(const char *username);
void playerdb_set_rank(const char *username, int rank, const char *changedby);
int playerdb_password_check(const char *username, const char *password);
const char *playerdb_get_last_ip(int globalid);
void playerdb_log_visit(int globalid, const char *ip, int identified);
void playerdb_log_identify(int globalid, int idenfied);
bool playerdb_check_ban(const char *ip);
void playerdb_ban_ip(const char *ip);
void playerdb_unban_ip(const char *ip);

#endif /* PLAYERDB_H */
