/*-
 * Copyright (c) 2011 Lucas Holt
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

#include <sys/cdefs.h>
__MBSDID("$MidnightBSD: src/lib/libmsearch/msearch_index.c,v 1.2 2011/07/31 21:27:08 laffer1 Exp $");

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ftw.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "msearch_private.h"

static msearch_index *mindex;
static int msearch_index_path_file(const char *, const struct stat *, int);
static int msearch_index_exists(msearch_index *, const char *);

msearch_index * 
msearch_index_open(const char *filename) {
	msearch_index *idx;

	if ((idx = calloc(1, sizeof(msearch_index))) == NULL) {
		return NULL;
	}
	idx->index_file = strdup(filename);

	if (sqlite3_open(filename, &(idx->db)) != SQLITE_OK) {
		sqlite3_close(idx->db);
		return NULL;
	}

	return idx;
}

int
msearch_index_close(msearch_index *idx) {
	if (idx == NULL)
		return -1;
	sqlite3_close(idx->db);
	free(idx->index_file);
	free(idx);

	return 0;
}

int
msearch_index_create(msearch_index *idx) {
	msearch_db_do(idx->db, "CREATE TABLE IF NOT EXISTS files (path text NOT NULL, size int64, owner int, created int64, modified int64)");
  	msearch_db_do(idx->db, "CREATE INDEX IF NOT EXISTS pathidx ON files (path)");
	return 0;
}

int
msearch_index_file(msearch_index *idx, const char *file, int /* NOTUSED */ flag) {
	struct stat st;
	sqlite3_stmt *stmt;

	if (lstat(file, &st) != 0)
		return 1;

	if (S_ISREG(st.st_mode)) {
		if (msearch_index_exists(idx, file) == 0) {
			if (sqlite3_prepare_v2(idx->db, "INSERT INTO files (path, size, owner, created, modified) VALUES(?,?,?,?,?)", -1, &stmt, 0) != SQLITE_OK)
				return 3;

			sqlite3_bind_text(stmt, 1, file, strlen(file), SQLITE_TRANSIENT);
			sqlite3_bind_int64(stmt, 2, (sqlite3_int64) st.st_size);
			sqlite3_bind_int(stmt, 3, st.st_uid);
			sqlite3_bind_int64(stmt, 4, (sqlite3_int64) st.st_birthtime);
			sqlite3_bind_int64(stmt, 5, (sqlite3_int64) st.st_mtime);
		} else {
			if (sqlite3_prepare_v2(idx->db, "UPDATE files set size=?, owner=?, created=?, modified=? where path=?", -1, &stmt, 0) != SQLITE_OK)
				return 3;

			sqlite3_bind_text(stmt, 5, file, strlen(file), SQLITE_TRANSIENT);
			sqlite3_bind_int64(stmt, 1, (sqlite3_int64) st.st_size);
			sqlite3_bind_int(stmt, 2, st.st_uid);
			sqlite3_bind_int64(stmt, 3, (sqlite3_int64) st.st_birthtime);
			sqlite3_bind_int64(stmt, 4, (sqlite3_int64) st.st_mtime);
		}

		if (sqlite3_step(stmt) != SQLITE_DONE) {
			sqlite3_finalize(stmt);
			return 4;
		}
		sqlite3_finalize(stmt);
	} else {
		return 2;
	}
	return 0;
}

static int 
msearch_index_path_file(const char *file, const struct stat *fst, int flag) {
	sqlite3_stmt *stmt;

	if (flag == FTW_F) {
		if (msearch_index_exists(mindex, file) == 0) {
                	if (sqlite3_prepare_v2(mindex->db, "INSERT INTO files (path, size, owner, created, modified) VALUES(?,?,?,?,?)", -1, &stmt, 0) != SQLITE_OK)
				return 3;
			sqlite3_bind_text(stmt, 1, file, strlen(file), SQLITE_TRANSIENT);
			sqlite3_bind_int64(stmt, 2, (sqlite3_int64) fst->st_size);
			sqlite3_bind_int(stmt, 3, fst->st_uid);
			sqlite3_bind_int64(stmt, 4, (sqlite3_int64) fst->st_birthtime);
			sqlite3_bind_int64(stmt, 5, (sqlite3_int64) fst->st_mtime);
		} else {
			if (sqlite3_prepare_v2(mindex->db, "UPDATE files set size=?, owner=?, created=?, modified=? where path=?", -1, &stmt, 0) != SQLITE_OK)
				return 3;
			sqlite3_bind_text(stmt, 5, file, strlen(file), SQLITE_TRANSIENT);
			sqlite3_bind_int64(stmt, 1, (sqlite3_int64) fst->st_size);
			sqlite3_bind_int(stmt, 2, fst->st_uid);
			sqlite3_bind_int64(stmt, 3, (sqlite3_int64) fst->st_birthtime);
			sqlite3_bind_int64(stmt, 4, (sqlite3_int64) fst->st_mtime);
		}

                if (sqlite3_step(stmt) != SQLITE_DONE) {
			sqlite3_finalize(stmt);
                        return 4;
		}
                sqlite3_finalize(stmt);
	}
	return 0;
}

int
msearch_index_path(msearch_index *idx, const char *path) {
	mindex = idx;
	return ftw(path, &msearch_index_path_file, 5);
}

static int
msearch_index_exists(msearch_index *idx, const char *file) {
	sqlite3_stmt *stmt;
	int ret;

	if (msearch_db_prepare(idx->db, &stmt, "SELECT * FROM files where path=%s", file) == 0) {
                        ret = sqlite3_step(stmt);
                        if (ret == SQLITE_ROW) {
				sqlite3_finalize(stmt);
				return 1;
                        }
        }
        sqlite3_finalize(stmt);
	return 0;
}
