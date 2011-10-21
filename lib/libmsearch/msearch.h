/* $MidnightBSD: src/lib/libmsearch/msearch.h,v 1.8 2011/10/15 04:29:35 laffer1 Exp $ */
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

#define MSEARCH_DEFAULT_INDEX_FILE "/var/db/msearch/msearch.db"
#define MSEARCH_DEFAULT_FULLTEXT_FILE "/var/db/msearch/msearch_full.db"

#define MSEARCH_QUERY_TYPE_FILE 1
#define MSEARCH_QUERY_TYPE_FULL 2
#define MSEARCH_QUERY_TYPE_FILE_FULL 3

typedef struct _msearch_query {
	char **	terms;
	int 	term_count;
	int	type;
	int	limit; /* limit results */
} msearch_query;

typedef struct _msearch_result {
	double	weight;
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
	char *restrict	index_file;
	sqlite3 *	db; 
	int	state;
} msearch_index;

typedef struct _msearch_fulltext {
        char *restrict  index_file;
        sqlite3 *	db;
        int     state;
} msearch_fulltext;

/* Index */
msearch_index * msearch_index_open(const char *);
int msearch_index_close(msearch_index *restrict);
int msearch_index_create(msearch_index *restrict);
int msearch_index_path(msearch_index *restrict, const char *);
int msearch_index_file(msearch_index *restrict, const char *, int);

/* Full text */
msearch_fulltext * msearch_fulltext_open(const char *);
int msearch_fulltext_close(msearch_fulltext *restrict);
int msearch_fulltext_create(msearch_fulltext *restrict);
int msearch_fulltext_index_file(msearch_fulltext *restrict, const char *);
int msearch_fulltext_index(msearch_fulltext *restrict, msearch_index *restrict);
int msearch_fulltext_search(msearch_query *, msearch_result *);

/* Search */
int msearch(msearch_query *, msearch_result *);
int msearch_search(msearch_query *, msearch_result *);
void msearch_free(msearch_result *);

#endif
