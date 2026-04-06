/*-
 * Copyright (c) 2026 Lucas Holt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#define _XOPEN_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sqlite3.h>
#include <time.h>
#include <poll.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <syslog.h>

#include <sys/capsicum.h>
#include <signal.h>

#define SOCKET_PATH "/var/run/aged/aged.sock"
#define DB_PATH "/var/db/aged/aged.db"
#define RUN_USER "aged"
#define MAX_REGION_LEN 10

static int calculate_age(const char *);
static void get_range(int, char *);
static void init_db(void);
static int is_valid_region(const char *);
static void load_region_setting(sqlite3 *);
static void cleanup(int);

static char *current_region = NULL;
static sqlite3 *db = NULL;
static sqlite3_stmt *stmt_set = NULL;
static sqlite3_stmt *stmt_reg = NULL;
static sqlite3_stmt *stmt_get = NULL;
static sqlite3_stmt *stmt_load_reg = NULL;

static void
cleanup(int sig __unused)
{
	if (stmt_set)
		sqlite3_finalize(stmt_set);
	if (stmt_reg)
		sqlite3_finalize(stmt_reg);
	if (stmt_get)
		sqlite3_finalize(stmt_get);
	if (stmt_load_reg)
		sqlite3_finalize(stmt_load_reg);
	if (db)
		sqlite3_close(db);
	if (current_region)
		free(current_region);

	unlink(SOCKET_PATH);
	unlink("/var/run/aged/aged.pid");
	exit(0);
}

static int
safe_atoi(const char *str, int *val)
{
	char *endptr;
	long lval;

	if (str == NULL || *str == '\0')
		return -1;

	errno = 0;
	lval = strtol(str, &endptr, 10);
	if (errno != 0 || *endptr != '\0' || lval < INT_MIN || lval > INT_MAX)
		return -1;

	*val = (int)lval;
	return 0;
}

int
main(void)
{
	int server_fd;
	int client_fd;
	struct sockaddr_un addr;
	uid_t client_uid;
	gid_t client_gid;
	struct passwd *pw;

	umask(0077);
	openlog("aged", LOG_PID, LOG_DAEMON);

	if (daemon(0, 0) == -1) {
		syslog(LOG_ERR, "daemon: %m");
		exit(1);
	}

	signal(SIGTERM, cleanup);
	signal(SIGINT, cleanup);

	if ((pw = getpwnam(RUN_USER)) == NULL) {
		syslog(LOG_ERR, "getpwnam failed for %s", RUN_USER);
		exit(1);
	}

	if (mkdir("/var/run/aged", 0755) == -1 && errno != EEXIST) {
		syslog(LOG_ERR, "mkdir /var/run/aged: %m");
		exit(1);
	}
	lchown("/var/run/aged", pw->pw_uid, pw->pw_gid);

	if (mkdir("/var/db/aged", 0700) == -1 && errno != EEXIST) {
		syslog(LOG_ERR, "mkdir /var/db/aged: %m");
		exit(1);
	}
	lchown("/var/db/aged", pw->pw_uid, pw->pw_gid);

	if (setgid(pw->pw_gid) != 0 || setuid(pw->pw_uid) != 0) {
		syslog(LOG_ERR, "setgid/setuid failed");
		exit(1);
	}

	FILE *fp = fopen("/var/run/aged/aged.pid", "w");

	if (fp) {
		fprintf(fp, "%d\n", getpid());
		fclose(fp);
	}

	init_db();

	int rc = sqlite3_open(DB_PATH, &db);
	if (rc != SQLITE_OK) {
		syslog(LOG_ERR, "Cannot open database: %s", sqlite3_errmsg(db));
		exit(1);
	}

	/* Disable journaling to disk to allow Capsicum sandboxing and improve performance */
	sqlite3_exec(db, "PRAGMA journal_mode=MEMORY;", NULL, NULL, NULL);

	if (sqlite3_prepare_v2(db, "INSERT OR REPLACE INTO users (uid, dob, age) VALUES (?, ?, ?);", -1, &stmt_set, NULL) != SQLITE_OK ||
	    sqlite3_prepare_v2(db, "UPDATE settings SET value = ? WHERE key = 'region';", -1, &stmt_reg, NULL) != SQLITE_OK ||
	    sqlite3_prepare_v2(db, "SELECT age FROM users WHERE uid = ?;", -1, &stmt_get, NULL) != SQLITE_OK ||
	    sqlite3_prepare_v2(db, "SELECT value FROM settings WHERE key = 'region';", -1, &stmt_load_reg, NULL) != SQLITE_OK) {
		syslog(LOG_ERR, "Failed to prepare SQL statements: %s", sqlite3_errmsg(db));
		exit(1);
	}

	load_region_setting(db);

	if ((server_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		syslog(LOG_ERR, "socket error: %m");
		exit(1);
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
	unlink(SOCKET_PATH);

	if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		syslog(LOG_ERR, "bind error: %m");
		exit(1);
	}

	chmod(SOCKET_PATH, 0666); /* intentional for agectl to work */

	if (listen(server_fd, 5) == -1) {
		syslog(LOG_ERR, "listen error: %m");
		exit(1);
	}

	if (cap_enter() < 0) {
		syslog(LOG_ERR, "cap_enter failed: %m");
		exit(1);
	}

	struct pollfd pfd;
	pfd.fd = server_fd;
	pfd.events = POLLIN;

	syslog(LOG_NOTICE, "aged daemon started");

	while (1) {
		if (poll(&pfd, 1, -1) <= 0) continue;

		if ((client_fd = accept(server_fd, NULL, NULL)) == -1) {
			continue;
		}

		struct pollfd cpfd;
		cpfd.fd = client_fd;
		cpfd.events = POLLIN;
		if (poll(&cpfd, 1, 100) <= 0) {
			close(client_fd);
			continue;
		}

		if (getpeereid(client_fd, &client_uid, &client_gid) == -1) {
			close(client_fd);
			continue;
		}

		char buf[256];
		int n = read(client_fd, buf, sizeof(buf) - 1);

		if (n > 0) {
			buf[n] = '\0';
			char *newline = strchr(buf, '\n');
			if (newline) *newline = '\0';
			newline = strchr(buf, '\r');
			if (newline) *newline = '\0';

			if (client_uid == 0 && strncmp(buf, "SET ", 4) == 0) {
				char *p = buf + 4;
				char *token;
				int target_uid;
				char *type;
				char *val_str;

				token = strsep(&p, " ");
				if (token == NULL || safe_atoi(token, &target_uid) != 0) {
					close(client_fd);
					continue;
				}

				type = strsep(&p, " ");
				if (type == NULL) {
					close(client_fd);
					continue;
				}

				val_str = strsep(&p, " ");
				if (val_str == NULL) {
					close(client_fd);
					continue;
				}

				if (target_uid > 0) {
					int final_age = -1;

					if (strcmp(type, "age") == 0) {
						safe_atoi(val_str, &final_age);
					} else if (strcmp(type, "dob") == 0) {
						final_age = calculate_age(val_str);
					}

					if (final_age < 2 || final_age > 125) {
						syslog(LOG_WARNING, "Invalid age %d for uid %d", final_age, target_uid);
						write(client_fd, "ERR\n", 4);
						close(client_fd);
						continue;
					}

					sqlite3_reset(stmt_set);
					sqlite3_bind_int(stmt_set, 1, target_uid);
					sqlite3_bind_text(stmt_set, 2, val_str, -1, SQLITE_STATIC);
					sqlite3_bind_int(stmt_set, 3, final_age);
					if (sqlite3_step(stmt_set) != SQLITE_DONE) {
						syslog(LOG_ERR, "Failed to update user: %s", sqlite3_errmsg(db));
						write(client_fd, "ERR\n", 4);
					} else {
						syslog(LOG_INFO, "User information updated for uid %d", target_uid);
						write(client_fd, "OK\n", 3);
					}
				}
			} else if (client_uid == 0 && strncmp(buf, "REG ", 4) == 0) {
				char *region = buf + 4;

				if (is_valid_region(region)) {
					sqlite3_reset(stmt_reg);
					if (strcmp(region, "null") == 0) {
						sqlite3_bind_null(stmt_reg, 1);
					} else {
						sqlite3_bind_text(stmt_reg, 1, region, -1, SQLITE_STATIC);
					}
					if (sqlite3_step(stmt_reg) != SQLITE_DONE) {
						syslog(LOG_ERR, "Failed to update region: %s", sqlite3_errmsg(db));
						write(client_fd, "ERR\n", 4);
					} else {
						syslog(LOG_INFO, "Region updated to %s", region);
						load_region_setting(db);
						write(client_fd, "OK\n", 3);
					}
				} else {
					syslog(LOG_ERR, "Invalid region value: %s", region);
					write(client_fd, "ERR\n", 4);
				}
			} else {
				int region_allowed = 0;
				if (current_region != NULL) {
					/* 
					These are places that age attenstion is required or will be. 
					It does not include locales with required ID checks since we don't provide that level of verification.
					Not legal advice, best effort. 
					*/
					if (strcmp(current_region, "US-CA") == 0 ||
						strcmp(current_region, "US-CO") == 0 ||
						strcmp(current_region, "US-IL") == 0 ||
						strcmp(current_region, "BR") == 0 ||  // Doing nothing appears worse in brazil legally so we keep it on.
					    strcmp(current_region, "parental") == 0) {
						region_allowed = 1;
					}
				}

				if (!region_allowed) {
					write(client_fd, "-2,-2\n", 6);
				} else {
					sqlite3_reset(stmt_get);
					sqlite3_bind_int(stmt_get, 1, (int)client_uid);
					int age = (sqlite3_step(stmt_get) == SQLITE_ROW) ? sqlite3_column_int(stmt_get, 0) : -1;

					/* user accounts start at 1000 */
					if (client_uid < 1000 && age == -1) {
						age = 18; /* assume 18+ for root and service accounts if undefined. With root, kids could circumvent any protections anyway.  */
					}

					char response[16];

					get_range(age, response);
					write(client_fd, response, strlen(response));
				}
			}
		}
		close(client_fd);
	}

	closelog();
	return 0;
}

int
calculate_age(const char *dob_str)
{
	struct tm tm_dob = {0};

	if (strptime(dob_str, "%Y-%m-%d", &tm_dob) == NULL)
		return -1;

	time_t t = time(NULL);
	struct tm *tm_now = localtime(&t);
	if (tm_now == NULL) return -1;

	int age = (tm_now->tm_year + 1900) - (tm_dob.tm_year + 1900);

	if (tm_now->tm_mon < tm_dob.tm_mon ||
	    (tm_now->tm_mon == tm_dob.tm_mon && tm_now->tm_mday < tm_dob.tm_mday)) {
		age--;
	}
	return age;
}

static int
is_valid_region(const char *region)
{
	/* places that require age verification or will be. Not all can be supported */
	const char *valid_regions[] = {
		"US-AL", "US-CA", "US-CO", "US-IL", "BR", "US-NY", "US-MI", "US-WA",
		"US-LA", "US-UT", "US-TX", "US-FL", "DE", "EU", "UK", "AU",
		"JP", "null", "parental", NULL
	};

	if (region == NULL)
		return 0;

	for (int i = 0; valid_regions[i] != NULL; i++) {
		if (strcmp(region, valid_regions[i]) == 0) {
			return 1;
		}
	}
	return 0;
}

static void
load_region_setting(sqlite3 *db_handle __unused)
{
	if (current_region) {
		free(current_region);
		current_region = NULL;
	}

	sqlite3_reset(stmt_load_reg);
	if (sqlite3_step(stmt_load_reg) == SQLITE_ROW) {
		const unsigned char *value = sqlite3_column_text(stmt_load_reg, 0);
		if (value) {
			current_region = strdup((const char *)value);
		}
	}
}


void
init_db(void)
{
	sqlite3 *db_local;
	char *err_msg = 0;
	int rc = sqlite3_open(DB_PATH, &db_local);

	if (rc != SQLITE_OK) {
		syslog(LOG_ERR, "Cannot open database: %s", sqlite3_errmsg(db_local));
		exit(1);
	}

	const char *sql = "CREATE TABLE IF NOT EXISTS users("
	"uid INTEGER PRIMARY KEY, "
	"dob TEXT, "
	"age INTEGER);";

	rc = sqlite3_exec(db_local, sql, 0, 0, &err_msg);
	if (rc != SQLITE_OK) {
		syslog(LOG_ERR, "SQL error: %s", err_msg);
		sqlite3_free(err_msg);
	}

	const char *sql_settings = "CREATE TABLE IF NOT EXISTS settings(key TEXT NOT NULL PRIMARY KEY, value TEXT);";
	rc = sqlite3_exec(db_local, sql_settings, 0, 0, &err_msg);
	if (rc != SQLITE_OK) {
		syslog(LOG_ERR, "SQL error: %s", err_msg);
		sqlite3_free(err_msg);
		sqlite3_close(db_local);
		exit(1);
	}

	const char *sql_insert_region = "INSERT OR IGNORE INTO settings (key, value) VALUES ('region', 'parental');";
	rc = sqlite3_exec(db_local, sql_insert_region, 0, 0, &err_msg);
	if (rc != SQLITE_OK) {
		syslog(LOG_ERR, "SQL error: %s", err_msg);
		sqlite3_free(err_msg);
	}

	sqlite3_close(db_local);
	db_local = NULL;
}

void
get_range(int age, char *buffer)
{
	if (age < 0) {
		snprintf(buffer, 16, "-1,-1\n");
		return;
	}
	if (age < 13) {
		snprintf(buffer, 16, "0,12\n");
		return;
	}
	if (age >= 13 && age < 16) {
		snprintf(buffer, 16, "13,15\n");
		return;
	}
	if (age >= 16 && age < 18) {
		snprintf(buffer, 16, "16,17\n");
		return;
	}
	if (age >= 18) {
		snprintf(buffer, 16, "18,-1\n");
		return;
	}
	snprintf(buffer, 16, "-1,-1\n");
}
