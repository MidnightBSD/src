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
__MBSDID("$MidnightBSD: src/lib/libmsearch/msearch_search.c,v 1.5 2011/08/06 23:02:51 laffer1 Exp $");

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <libgen.h>

#include "msearch_private.h"

static sqlite3_stmt *msearch_search_prepare(sqlite3 *, msearch_query *);
static int msearch_search_bind(sqlite3_stmt *, msearch_query *);

int
msearch(msearch_query *query, msearch_result *result) {

	if (query == NULL)
		return -1;

	if (query->type == MSEARCH_QUERY_TYPE_FILE)
		return msearch_search(query, result);
	if (query->type == MSEARCH_QUERY_TYPE_FULL)
		return msearch_fulltext_search(query, result);

	return -1; /* not yet implemented */
}

int 
msearch_search(msearch_query *query, msearch_result *result) {
	sqlite3_stmt *stmt = NULL;
	int ret;
	int i = 0;
	msearch_index *idx;
	msearch_result *current;
	struct stat sb;
	char buf[1024]; /* XXX _SC_GETPW_R_SIZE_MAX ? */
	struct passwd pwd;
	struct passwd* pwdbuf;
	uid_t uid = 0;

	if (query == NULL)
		return -1;

	idx = msearch_index_open(MSEARCH_DEFAULT_INDEX_FILE);
	if (idx == NULL)
		return -1;
	current = result;

	stmt = msearch_search_prepare(idx->db, query);
	if (stmt == NULL) {
		msearch_index_close(idx);
		return -1;
	}

	ret = msearch_search_bind(stmt, query);
	if (ret != 0) {
		sqlite3_finalize(stmt);
		msearch_index_close(idx);
		return -1;
	}

	while (1) {
		ret = sqlite3_step(stmt);
		if (ret == SQLITE_ROW) {
			if (i > 0) {
				current->next = malloc(sizeof(msearch_result));
				if (current->next == NULL) {
					i = -1; 
					break;
				}
				current = current->next;
			}
			current->filename = NULL;
			if (lstat(sqlite3_column_text(stmt, 0), &sb) == 0) {
				if (S_ISREG(sb.st_mode)) {
					current->filename = strdup(basename((char *)sqlite3_column_text(stmt, 0)));
				}
			}
			current->path = strdup(sqlite3_column_text(stmt, 0));
			current->size = sqlite3_column_int64(stmt, 1);
			current->uid = sqlite3_column_int(stmt, 2);
			if ((getpwuid_r(uid, &pwd, buf, sizeof(buf), &pwdbuf)) == 0 && pwdbuf != NULL) {
				current->owner = strdup(pwdbuf->pw_name);				
			} else
				current->owner = NULL;
			current->created =  sqlite3_column_int64(stmt, 3);
			current->modified = sqlite3_column_int64(stmt, 4);
			current->next = NULL;
			i++;
		} else if (ret == SQLITE_DONE) {
			break;
		} else {
			i = -1;
			break;
		}
	}
	sqlite3_finalize(stmt);
	msearch_index_close(idx);

	return i;
}

static sqlite3_stmt *
msearch_search_prepare(sqlite3 *db, msearch_query *query) {
	sqlite3_stmt *stmt = NULL;
	char *sql;
	size_t sql_len;
	size_t limit_len = 0;
	int i;

	if (db == NULL || query == NULL || query->term_count < 1)
		return NULL;

	sql_len = sizeof("SELECT * FROM files WHERE ");
	for (i = 0; i < query->term_count; i++) {
		sql_len += sizeof("path LIKE ?");
		if (i < query->term_count - 1)
			sql_len += sizeof(" AND ");
	}
	if (query->limit > 0) {
		limit_len = sizeof(" LIMIT ");
		limit_len += 11;
	}
	sql_len += limit_len;

	sql = calloc(sql_len, sizeof(char));
	if (sql == NULL)
		return NULL;

	strlcpy(sql, "SELECT * FROM files WHERE ", sql_len);
	for (i = 0; i < query->term_count; i++) {
		strlcat(sql, "path LIKE ?", sql_len);
		if (i < query->term_count - 1)
			strlcat(sql, " AND ", sql_len);
	}
	if (query->limit > 0) {
		char limit_buf[12];

		snprintf(limit_buf, sizeof(limit_buf), "%d", query->limit);
		strlcat(sql, " LIMIT ", sql_len);
		strlcat(sql, limit_buf, sql_len);
	}

	if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
		stmt = NULL;

	free(sql);

	return stmt;
}

static int
msearch_search_bind(sqlite3_stmt *stmt, msearch_query *query) {
	char *pattern;
	size_t pattern_len;
	int i;

	if (stmt == NULL || query == NULL)
		return 1;

	for (i = 0; i < query->term_count; i++) {
		pattern_len = strlen(query->terms[i]) + 3;
		pattern = malloc(pattern_len);
		if (pattern == NULL)
			return 1;

		snprintf(pattern, pattern_len, "%%%s%%", query->terms[i]);
		if (sqlite3_bind_text(stmt, i + 1, pattern, -1, SQLITE_TRANSIENT) != SQLITE_OK) {
			free(pattern);
			return 1;
		}
		free(pattern);
	}

	return 0;
}

void
msearch_free(msearch_result *result) {
	msearch_result *current, *next;

	if (result == NULL)
		return;
	current = result;

	while (current != NULL) {
		next = current->next;
		free(current->filename);
		free(current->path);
		free(current->owner);
		free(current);
		current = next;
	}
}

char *
msearch_query_expand(msearch_query *query) {
	int i;
	size_t rlen = 1;
	char *result;
	char like[14] = "path like '%%"; 
	char like2[5] = "%%' ";
	char like3[5] = "and ";

	if (query == NULL)
		return NULL;

	for (i = 0; i < query->term_count; i++) {
		rlen += strlen(query->terms[i]);
		rlen += sizeof(like) -1;
		rlen += sizeof(like2) -1;
	}
	if (query->term_count > 1) {
		rlen += (query->term_count - 1) * sizeof(like3);
	}

	result = calloc(rlen, sizeof(char));
	if (result == NULL)
		return result;

	for (i = 0; i < query->term_count; i++) {
		strcat(result, like);
		strcat(result, query->terms[i]);
		strcat(result, like2);
		if (i < query->term_count -1)
			strcat(result, like3);
	}

	return result;
}
