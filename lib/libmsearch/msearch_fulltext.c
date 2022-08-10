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
__MBSDID("$MidnightBSD: src/lib/libmsearch/msearch_fulltext.c,v 1.12 2011/08/09 12:50:17 laffer1 Exp $");

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <libgen.h>
#include <magic.h>

#include "msearch_private.h"

#define MAX_INDEX_SIZE 1048576
#define DEFAULT_RESULT_LIMIT 15

int
msearch_fulltext_search(msearch_query *query, msearch_result *result) {
        sqlite3_stmt *stmt;
        int ret, limit;
        int i = 0;
        msearch_fulltext *idx;
        msearch_result *current;
        struct stat sb;
        char *params;

	if (query == NULL)
		return -1;

	if (query->type != MSEARCH_QUERY_TYPE_FULL && query->type != MSEARCH_QUERY_TYPE_FILE_FULL)
		return -1;

	idx = msearch_fulltext_open(MSEARCH_DEFAULT_FULLTEXT_FILE);
	if (idx == NULL)
		return -1;
	
	current = result;
	params = msearch_fulltext_query_expand(query);
	if (params == NULL)
		return -1;

	if (query->limit < 1)
		limit = DEFAULT_RESULT_LIMIT;
	else
		limit = query->limit;

	ret = msearch_db_prepare(idx->db, &stmt, "SELECT path, msearch_rank(matchinfo(data)) FROM data where %s ORDER BY msearch_rank(matchinfo(data)) DESC limit %d OFFSET 0", 	
		params, limit);

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
				current->path = strdup(sqlite3_column_text(stmt, 0));
				current->weight = sqlite3_column_double(stmt, 1);
                                current->filename = NULL;
                                if (lstat(sqlite3_column_text(stmt, 0), &sb) == 0) {
                                        if (S_ISREG(sb.st_mode)) {
                                                current->filename = strdup(basename((char *) sqlite3_column_text(stmt, 0)));
                                        }
					current->size = sb.st_size;
					current->uid = sb.st_uid;
					current->created =  sb.st_birthtime;
                                	current->modified = sb.st_mtime;
					current->owner = NULL;
                                } else {
                                	current->size = 0; 
                                	current->uid = -1;
                                	current->owner = NULL;
                                	current->created =  0;
                                	current->modified = 0;
				}
                                current->next = NULL;
                                i++;
                        } else if (ret == SQLITE_DONE) {
                                break;
                        }
                }
        }
        sqlite3_finalize(stmt);
        msearch_fulltext_close(idx);
        free(params);

        return i;
}

msearch_fulltext *
msearch_fulltext_open(const char *filename) {
	msearch_fulltext *idx;

	if (filename == NULL)
		return NULL;

	if ((idx = calloc(1, sizeof(msearch_fulltext))) == NULL) {
		return NULL;
	}
	idx->index_file = strdup(filename);

	if (sqlite3_open(filename, &(idx->db)) != SQLITE_OK) {
		free(idx->index_file);
		free(idx);
		sqlite3_close(idx->db);
		return NULL;
	}

	sqlite3_enable_load_extension(idx->db, 1);
	msearch_db_do(idx->db, "SELECT load_extension('/usr/lib/libmsearch.so')");
	sqlite3_enable_load_extension(idx->db, 0);

	return idx;
}

int
msearch_fulltext_close(msearch_fulltext *restrict idx) {
	if (idx == NULL)
		return 1;
	sqlite3_close(idx->db);
	free(idx->index_file);
	free(idx);

	return 0;
}

int
msearch_fulltext_create(msearch_fulltext *restrict idx) {

	if (idx == NULL)
		return 1;

	msearch_db_do(idx->db, "CREATE VIRTUAL TABLE data using fts4 (path, textdata, compress=msearch_compress, uncompress=msearch_uncompress, tokenize=porter)");
	return 0;
}

int
msearch_fulltext_index(msearch_fulltext *restrict idx, msearch_index *restrict iidx) {
	sqlite3_stmt *stmt;
	int ret;
	char *pathname;

	if (idx == NULL || iidx == NULL)
		return 1;

	ret = msearch_db_prepare(iidx->db, &stmt, "SELECT path FROM files");

	if (ret == 0) {
		while (1) {
			ret = sqlite3_step(stmt);
			if (ret == SQLITE_ROW) {
				if (strncmp(sqlite3_column_text(stmt, 0), "/tmp/", 5) == 0 ||
				    strncmp(sqlite3_column_text(stmt, 0), "/var/", 5) == 0 ||
				    strncmp(sqlite3_column_text(stmt, 0), "/dev/", 5) == 0 ||
				    strncmp(sqlite3_column_text(stmt, 0), "/proc/", 6) == 0 ||
			 	    strncmp(sqlite3_column_text(stmt, 0), "/usr/obj/", 9) == 0)
					continue;
				pathname = strdup(sqlite3_column_text(stmt, 0));
				msearch_fulltext_index_file(idx, pathname);				
				free(pathname);		
			} else if (ret == SQLITE_DONE)
				break;
		}
	}
	sqlite3_finalize(stmt);

	return 0;
}

int
msearch_fulltext_index_file(msearch_fulltext *restrict idx, const char *path) {
	sqlite3_stmt *stmt;
	size_t len;
	char *filedata;
	FILE *fp;
	struct stat st;
	magic_t magic;
	const char *mimetype;

	if (idx == NULL || path == NULL)
		return 1;

	if (stat(path, &st) != 0)
		return 1;

	if (st.st_size > MAX_INDEX_SIZE)
		return 2;

	magic = magic_open(MAGIC_MIME);
	magic_load(magic, NULL);
	mimetype = magic_file(magic, path);
	magic_close(magic);
#ifdef DEBUG
	if (mimetype != NULL)
		fprintf(stderr, "magic mime type is %s %s\n", path, mimetype);
	else
		fprintf(stderr, "null mimetype for %s\n", path);
#endif
	if (strcmp("text/plain", mimetype) != 0)
		return 0;

	fp = fopen(path, "r");
	if (fp == NULL) {
		return 1;
	}

	filedata = malloc(st.st_size * sizeof(char));
	if (filedata == NULL) {
		fclose(fp);
		return 1;
	}
	len = fread(filedata, st.st_size, 1, fp);
	if (ferror(fp) != 0 || len == 0) {
		free(filedata);
		fclose(fp);
		return 1;
	}
	filedata[st.st_size -1] = '\0'; 
	fclose(fp);

	if (filedata != NULL && *filedata != '\0') {
		if (sqlite3_prepare_v2(idx->db, "INSERT OR REPLACE INTO data VALUES(?,?)", -1, &stmt, 0) != SQLITE_OK) {
				free(filedata);
                                return 4;
		}

		sqlite3_bind_text(stmt, 1, path, strlen(path), SQLITE_STATIC);
		sqlite3_bind_text(stmt, 2, filedata, st.st_size, SQLITE_STATIC);
		if (sqlite3_step(stmt) != SQLITE_DONE) {
			free(filedata);
                        sqlite3_finalize(stmt);
                        return 4;
                }
                sqlite3_finalize(stmt);
	}

	free(filedata);
	return 0;
}

char *
msearch_fulltext_query_expand(msearch_query *restrict query) {
	int i;
	size_t rlen = 1;
	char *result;
	char like[17] = "textdata match '";
	char like2[3] = "' ";
	char like3[2] = " ";

	if (query == NULL)
		return NULL;

	rlen += sizeof(like) -1;
	for (i = 0; i < query->term_count; i++) {
		rlen += strlen(query->terms[i]);
	}
	rlen += sizeof(like2) -1;

	if (query->term_count > 1) {
		rlen += (query->term_count - 1) * sizeof(like3);
	}

	result = calloc(rlen, sizeof(char));
	if (result == NULL)
		return result;

	strcat(result, like);
	for (i = 0; i < query->term_count; i++) {
		strcat(result, query->terms[i]);
		if (i < query->term_count -1)
			strcat(result, like3);
        }
	strcat(result, like2);

	return result;
}

