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
#include <sys/wait.h>
#include <fcntl.h>

#define SOCKET_PATH "/var/run/aged/aged.sock"
#define DB_PATH "/var/db/aged/aged.db"
#define RUN_USER "aged"

static int calculate_age(const char *);
static void get_range(int, char *);
static void init_db(void);
static void update_age_groups(const char *, int);
static int run_pw_command(const char *, const char *, const char *);

int
main(void)
{
	int server_fd;
	int client_fd;
	struct sockaddr_un addr;
	uid_t client_uid;
	gid_t client_gid;
	struct passwd *pw;

	openlog("aged", LOG_PID, LOG_DAEMON);

	if (daemon(0, 0) == -1) {
		syslog(LOG_ERR, "daemon: %m");
		exit(1);
	}

	FILE *fp = fopen("/var/run/aged/aged.pid", "w");

	if (fp) {
		fprintf(fp, "%d\n", getpid());
		fclose(fp);
	}

	if ((pw = getpwnam(RUN_USER)) == NULL) {
		syslog(LOG_ERR, "getpwnam failed for %s", RUN_USER);
		exit(1);
	}

	if (mkdir("/var/run/aged", 0755) == -1 && errno != EEXIST) {
		syslog(LOG_ERR, "mkdir /var/run/aged: %m");
		exit(1);
	}

	if (mkdir("/var/db/aged", 0700) == -1 && errno != EEXIST) {
		syslog(LOG_ERR, "mkdir /var/db/aged: %m");
		exit(1);
	}

	init_db();

	lchown(DB_PATH, pw->pw_uid, pw->pw_gid);
	chmod(DB_PATH, 0600);

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

	if (setgid(pw->pw_gid) != 0 || setuid(pw->pw_uid) != 0) {
		syslog(LOG_ERR, "setgid/setuid failed");
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
		if (poll(&cpfd, 1, 1000) <= 0) {
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
			sqlite3 *db;

			int rc = sqlite3_open(DB_PATH, &db);
			if (rc != SQLITE_OK) {
				syslog(LOG_ERR, "Cannot open database: %s", sqlite3_errmsg(db));
				close(client_fd);
				continue;
			}

			if (client_uid == 0 && strncmp(buf, "SET ", 4) == 0) {
				char *p = buf + 4;
				char *token;
				int target_uid;
				char *type;
				char *val_str;

				token = strsep(&p, " ");
				if (token == NULL) {
					sqlite3_close(db);
					close(client_fd);
					continue;
				}
				target_uid = atoi(token);

				type = strsep(&p, " ");
				if (type == NULL) {
					sqlite3_close(db);
					close(client_fd);
					continue;
				}
				
				val_str = strsep(&p, " ");
				if (val_str == NULL) {
					sqlite3_close(db);
					close(client_fd);
					continue;
				}

				if (target_uid > 0) {
					int final_age = -1;

					if (strcmp(type, "age") == 0) {
						final_age = atoi(val_str);
					} else if (strcmp(type, "dob") == 0) {
						final_age = calculate_age(val_str);
					}

					if (final_age < 2 || final_age > 125) {
						syslog(LOG_WARNING, "Invalid age %d for uid %d", final_age, target_uid);
						write(client_fd, "ERR\n", 4);
						sqlite3_close(db);
						close(client_fd);
						continue;
					}

					sqlite3_stmt *stmt;
					sqlite3_prepare_v2(db, "INSERT OR REPLACE INTO users (uid, dob, age) VALUES (?, ?, ?);", -1, &stmt, 0);
					sqlite3_bind_int(stmt, 1, target_uid);
					sqlite3_bind_text(stmt, 2, val_str, -1, SQLITE_STATIC);
					sqlite3_bind_int(stmt, 3, final_age);
					sqlite3_step(stmt);
					sqlite3_finalize(stmt);

					struct passwd *pwent = getpwuid(target_uid);
					if (pwent != NULL) {
						update_age_groups(pwent->pw_name, final_age);
					} else {
						syslog(LOG_ERR, "Could not find username for uid %d for group update", target_uid);
					}

					syslog(LOG_INFO, "User information updated for uid %d", target_uid);
					write(client_fd, "OK\n", 3);
				}
			} else {
				sqlite3_stmt *stmt;
				sqlite3_prepare_v2(db, "SELECT age FROM users WHERE uid = ?;", -1, &stmt, 0);
				sqlite3_bind_int(stmt, 1, (int)client_uid);
				int age = (sqlite3_step(stmt) == SQLITE_ROW) ? sqlite3_column_int(stmt, 0) : -1;
				sqlite3_finalize(stmt);

				/* user accounts start at 1000 */
				if (client_uid < 1000 && age == -1) {
					age = 18; /* assume 18+ for root and service accounts if undefined. With root, kids could circumvent any protections anyway.  */
				}

				char response[16];

				get_range(age, response);
				write(client_fd, response, strlen(response));
			}
			sqlite3_close(db);
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

void
init_db(void)
{
	sqlite3 *db;
	char *err_msg = 0;
	int rc = sqlite3_open(DB_PATH, &db);

	if (rc != SQLITE_OK) {
		syslog(LOG_ERR, "Cannot open database: %s", sqlite3_errmsg(db));
		exit(1);
	}

	const char *sql = "CREATE TABLE IF NOT EXISTS users("
	"uid INTEGER PRIMARY KEY, "
	"dob TEXT, "
	"age INTEGER);";

	rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
	if (rc != SQLITE_OK) {
		syslog(LOG_ERR, "SQL error: %s", err_msg);
		sqlite3_free(err_msg);
	}
	sqlite3_close(db);
}

void
get_range(int age, char *buffer)
{
	if (age < 0) {
		snprintf(buffer, 16, "-1,-1");
		return;
	}
	if (age < 13) {
		snprintf(buffer, 16, "0,12");
		return;
	}
	if (age >= 13 && age < 16) {
		snprintf(buffer, 16, "13,15");
		return;
	}
	if (age >= 16 && age < 18) {
		snprintf(buffer, 16, "16,17");
		return;
	}
	if (age >= 18) {
		snprintf(buffer, 16, "18,-1");
		return;
	}
	snprintf(buffer, 16, "-1,-1");
}

static int
run_pw_command(const char *group, const char *user, const char *action)
{
	pid_t pid;
	int status;
	const char *pw_path = "/usr/sbin/pw";
	char *const argv[] = {(char *)pw_path, "groupmod", (char *)group, (char *)action, (char *)user, NULL};
	char *const envp[] = {"PATH=/bin:/usr/bin:/sbin:/usr/sbin", NULL};

	pid = fork();
	if (pid == -1) {
		syslog(LOG_ERR, "fork failed: %m");
		return -1;
	} else if (pid == 0) {
		int fd = open("/dev/null", O_WRONLY);
		if (fd != -1) {
			dup2(fd, STDOUT_FILENO);
			dup2(fd, STDERR_FILENO);
			close(fd);
		}
		
		execve(pw_path, argv, envp);
		_exit(127);
	}

	if (waitpid(pid, &status, 0) == -1) {
		syslog(LOG_ERR, "waitpid failed: %m");
		return -1;
	}

	if (WIFEXITED(status)) {
		return WEXITSTATUS(status);
	}

	return -1;
}

static void
update_age_groups(const char *username, int age)
{
	const char *groups[] = {"age4p", "age13p", "age16p", "age18p"};
	int min_ages[] = {4, 13, 16, 18};
	int num_groups = sizeof(groups) / sizeof(groups[0]);

	for (int i = 0; i < num_groups; i++) {
		struct group *grp = getgrnam(groups[i]);
		if (grp == NULL) {
			syslog(LOG_WARNING, "Group %s not found.", groups[i]);
			continue;
		}

		int in_group = 0;
		for (int j = 0; grp->gr_mem[j] != NULL; j++) {
			if (strcmp(grp->gr_mem[j], username) == 0) {
				in_group = 1;
				break;
			}
		}

		if (age >= min_ages[i]) {
			if (!in_group) {
				if (run_pw_command(groups[i], username, "-m") != 0) {
					syslog(LOG_ERR, "Failed to add user %s to group %s", username, groups[i]);
				} else {
					syslog(LOG_INFO, "Added user %s to group %s", username, groups[i]);
				}
			}
		} else {
			if (in_group) {
				if (run_pw_command(groups[i], username, "-d") != 0) {
					syslog(LOG_ERR, "Failed to remove user %s from group %s", username, groups[i]);
				} else {
					syslog(LOG_INFO, "Removed user %s from group %s", username, groups[i]);
				}
			}
		}
	}
}
