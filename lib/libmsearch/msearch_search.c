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
	sqlite3_stmt *stmt;
	int ret;
	int i = 0;
	msearch_index *idx;
	msearch_result *current;
	struct stat sb;
	char buf[1024]; /* XXX _SC_GETPW_R_SIZE_MAX ? */
	struct passwd pwd;
	struct passwd* pwdbuf;
	uid_t uid = 0;
	char *params;

	if (query == NULL)
		return -1;

	idx = msearch_index_open(MSEARCH_DEFAULT_INDEX_FILE);
	current = result;
	params = msearch_query_expand(query);

	if (query->limit < 1)
		ret = msearch_db_prepare(idx->db, &stmt, "SELECT * FROM files where %s", params);
	else
		ret = msearch_db_prepare(idx->db, &stmt, "SELECT * FROM files where %s limit %d", params, query->limit);

	if (ret == 0) {
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
			}
		}
	}
	sqlite3_finalize(stmt);
	msearch_index_close(idx);
	free(params);

	return i;
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
