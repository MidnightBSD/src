/* $MidnightBSD$ */
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

#ifndef MSEARCH_H
#define MSEARCH_H

#include <sys/types.h>
#include <sqlite3.h>

#define MSEARCH_DEFAULT_INDEX_FILE "/var/db/msearch.db"

#define MSEARCH_QUERY_TYPE_FILE 1

typedef struct _msearch_query {
	char **	terms;
	int 	term_count;
	int	type;
} msearch_query;

typedef struct _msearch_result {
	char *	filename;
	char *	path;
	size_t	size;
	int	uid;
	char *	owner;
	time_t	created;
	time_t	modified;
	struct _msearch_result *	next;
} msearch_result;

typedef struct _msearch_index {
	char *	index_file;
	sqlite3 *db; 
	int	state;
} msearch_index;

/* Index */
msearch_index * msearch_index_open(const char *filename);
int msearch_index_close(msearch_index *);
int msearch_index_create(msearch_index *);
int msearch_index_path(msearch_index *, const char *);
int msearch_index_file(msearch_index *, const char *, int flag);

/* Search */
int msearch(msearch_query *, msearch_result *);
void msearch_free(msearch_result *result);

#endif
