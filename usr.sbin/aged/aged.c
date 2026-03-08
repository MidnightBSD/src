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
#include <errno.h>

#define SOCKET_PATH "/var/run/aged/aged.sock"
#define DB_PATH "/var/db/aged/aged.db"

static void get_range(int, char *);
static void init_db(void);

int
main(void)
{
    int server_fd, client_fd;
    struct sockaddr_un addr;
    uid_t client_uid;
    gid_t client_gid;

    if (daemon(0, 0) == -1) {
        perror("daemon");
        exit(1);
    }

    FILE *fp = fopen("/var/run/aged/aged.pid", "w");
    if (fp) {
        fprintf(fp, "%d\n", getpid());
        fclose(fp);
    }

    mkdir("/var/run/aged", 0755);
    mkdir("/var/db/aged", 0700);

    init_db();

    if ((server_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket error");
        exit(1);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    unlink(SOCKET_PATH);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind error");
        exit(1);
    }

    chmod(SOCKET_PATH, 0666);

    if (listen(server_fd, 5) == -1) {
        perror("listen error");
        exit(1);
    }

    printf("aged daemon started...\n");

    while (1) {
        if ((client_fd = accept(server_fd, NULL, NULL)) == -1) {
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
            sqlite3_open(DB_PATH, &db);

            if (client_uid == 0 && strncmp(buf, "SET", 3) == 0) {
                int target_uid, val;
                char type[4]; // "dob" or "age"
                sscanf(buf, "SET %d %s %d", &target_uid, type, &val);
                
                char sql[256];
                if (strcmp(type, "age") == 0)
                    snprintf(sql, sizeof(sql), "INSERT OR REPLACE INTO users (uid, age) VALUES (%d, %d);", target_uid, val);
                else
                    snprintf(sql, sizeof(sql), "INSERT OR REPLACE INTO users (uid, dob) VALUES (%d, '%d');", target_uid, val);

                sqlite3_exec(db, sql, 0, 0, NULL);
                write(client_fd, "OK\n", 3);
            } else {
                sqlite3_stmt *res;
                char sql[128];
                snprintf(sql, sizeof(sql), "SELECT age FROM users WHERE uid = %d;", client_uid);
                
                int age = -1;
                if (sqlite3_prepare_v2(db, sql, -1, &res, 0) == SQLITE_OK) {
                    if (sqlite3_step(res) == SQLITE_ROW) {
                        age = sqlite3_column_int(res, 0);
                    }
                    sqlite3_finalize(res);
                }

                char response[16];
                get_range(age, response);
                write(client_fd, response, strlen(response));
            }
            sqlite3_close(db);
        }
        close(client_fd);
    }

    return 0;
}

void
init_db(void)
{
    sqlite3 *db;
    char *err_msg = 0;
    int rc = sqlite3_open(DB_PATH, &db);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        exit(1);
    }

    const char *sql = "CREATE TABLE IF NOT EXISTS users("
                "uid INTEGER PRIMARY KEY, "
                "dob TEXT, "
                "age INTEGER);";

    rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
    }
    sqlite3_close(db);
}

void
get_range(int age, char *buffer)
{
    if (age < 0) { strcpy(buffer, "-1,-1"); return; }
    if (age < 13) { strcpy(buffer, "0,12"); return; }
    if (age >= 13 && age < 16) { strcpy(buffer, "13,15"); return; }
    if (age >= 16 && age < 18) { strcpy(buffer, "16,17"); return; }
    if (age >= 18) { strcpy(buffer, "18,-1"); return; }
    strcpy(buffer, "-1,-1");
}

