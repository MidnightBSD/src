/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Lucas Holt
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
#include <ctype.h>
#include <assert.h>
#include "mport.h"
#include "mport_private.h"

/* allocate mem for a mportInstance */
MPORT_PUBLIC_API mportStats *
mport_stats_new(void)
{
	return (mportStats *)calloc(1, sizeof(mportStats));
}

MPORT_PUBLIC_API int
mport_stats_free(mportStats *stats)
{
	free(stats);
	return MPORT_OK;
}

MPORT_PUBLIC_API int
mport_stats(mportInstance *mport, mportStats **stats)
{
	sqlite3_stmt *stmt = NULL;
	sqlite3 *db = mport->db;
	mportStats *s;
	int result = MPORT_OK;

	if (stats == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "stats pointer is NULL");

	if ((s = mport_stats_new()) == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");

	if (mport_db_prepare(db, &stmt, "SELECT COUNT(*) FROM packages") != MPORT_OK) {
		result = mport_err_code();
		goto cleanup;
	}

	if (sqlite3_step(stmt) != SQLITE_ROW) {
		SET_ERRORX(MPORT_ERR_FATAL, "%s", sqlite3_errmsg(db));
		result = MPORT_ERR_FATAL;
		goto cleanup;
	}

	s->pkg_installed = (unsigned int)sqlite3_column_int(stmt, 0);
	sqlite3_finalize(stmt);
	stmt = NULL;

	if (mport_db_prepare(db, &stmt, "SELECT sum(flatsize) FROM packages") != MPORT_OK) {
		result = mport_err_code();
		goto cleanup;
	}

	if (sqlite3_step(stmt) != SQLITE_ROW) {
		SET_ERRORX(MPORT_ERR_FATAL, "%s", sqlite3_errmsg(db));
		result = MPORT_ERR_FATAL;
		goto cleanup;
	}

	s->pkg_installed_size = sqlite3_column_int64(stmt, 0);
	sqlite3_finalize(stmt);
	stmt = NULL;

	if (mport_db_prepare(db, &stmt, "SELECT COUNT(*) FROM idx.packages") != MPORT_OK) {
		int errcode = sqlite3_errcode(db);
		/* If the index is missing or detached, we still want to return what we have for
		 * local */
		if (errcode == SQLITE_ERROR &&
		    strstr(sqlite3_errmsg(db), "no such table") != NULL) {
			s->pkg_available = 0;
			sqlite3_finalize(stmt);
			stmt = NULL;
		} else {
			result = MPORT_ERR_FATAL;
			goto cleanup;
		}
	} else {
		if (sqlite3_step(stmt) != SQLITE_ROW) {
			s->pkg_available = 0;
		} else {
			s->pkg_available = (unsigned int)sqlite3_column_int(stmt, 0);
		}
		sqlite3_finalize(stmt);
		stmt = NULL;
	}

	*stats = s;
	return MPORT_OK;

cleanup:
	if (stmt != NULL)
		sqlite3_finalize(stmt);
	mport_stats_free(s);
	*stats = NULL;
	return result;
}
