/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Lucas Holt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *  notice, this list of conditions and the following disclaimer in the
 *  documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.	IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <sqlite3.h>
#include <stdlib.h>
#include "mport.h"
#include "mport_private.h"

MPORT_PUBLIC_API int
mport_annotation_get(mportInstance *mport, const char *pkg, const char *tag, char **annotation)
{
	sqlite3_stmt *stmt = NULL;

	if (mport_db_prepare(mport->db, &stmt, "SELECT val FROM annotation WHERE pkg=%Q AND tag=%Q",
		pkg, tag) != MPORT_OK) {
		sqlite3_finalize(stmt);
		RETURN_CURRENT_ERROR;
	}

	if (stmt == NULL) {
		RETURN_ERROR(MPORT_ERR_FATAL, "AnnotationStatement was null");
	}

	int ret = sqlite3_step(stmt);

	switch (ret) {
	case SQLITE_ROW: {
		const unsigned char *value = sqlite3_column_text(stmt, 0);
		*annotation = (value == NULL) ? NULL : strdup((const char *)value);
	}
		sqlite3_finalize(stmt);
		if (*annotation == NULL)
			RETURN_ERROR(MPORT_ERR_FATAL, "Malformed annotation row.");
		break;
	case SQLITE_DONE:
		sqlite3_finalize(stmt);
		RETURN_ERRORX(
		    MPORT_ERR_FATAL, "No annotation found for package %s with tag %s", pkg, tag);
	default:
		sqlite3_finalize(stmt);
		RETURN_ERRORX(MPORT_ERR_FATAL, "Database error: %s", sqlite3_errmsg(mport->db));
	}

	return (MPORT_OK);
}

/*
 * mport_stub_annotation_get(mport, tag, annotation)
 *
 * Look up an annotation of the bundle whose stub database is attached, i.e.
 * of a package that is being installed and is not registered yet.  Sets
 * *annotation to a copy of the value, or to NULL when the bundle has none.
 */
int
mport_stub_annotation_get(
    mportInstance *mport, const char *tag, /*@out@*/ /*@null@*/ char **annotation)
{
	sqlite3_stmt *stmt = NULL;
	const unsigned char *value;
	int ret;

	*annotation = NULL;

	if (mport_db_prepare(mport->db, &stmt, "SELECT value FROM stub.meta WHERE field=%Q", tag) !=
	    MPORT_OK) {
		sqlite3_finalize(stmt);
		RETURN_CURRENT_ERROR;
	}

	if (stmt == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Stub annotation statement was null");

	ret = sqlite3_step(stmt);
	if (ret == SQLITE_ROW) {
		value = sqlite3_column_text(stmt, 0);
		if (value != NULL && (*annotation = strdup((const char *)value)) == NULL) {
			sqlite3_finalize(stmt);
			RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");
		}
	} else if (ret != SQLITE_DONE) {
		sqlite3_finalize(stmt);
		RETURN_ERRORX(MPORT_ERR_FATAL, "Database error: %s", sqlite3_errmsg(mport->db));
	}

	sqlite3_finalize(stmt);
	return (MPORT_OK);
}

MPORT_PUBLIC_API int
mport_annotation_set(mportInstance *mport, const char *pkg, const char *tag, const char *annotation)
{

	return mport_db_do(mport->db,
	    "INSERT OR REPLACE INTO annotation (pkg, tag, val) VALUES (%Q, %Q, %Q)", pkg, tag,
	    annotation);
}

MPORT_PUBLIC_API int
mport_annotation_delete(mportInstance *mport, const char *pkg, const char *tag)
{
	return mport_db_do(mport->db, "DELETE FROM annotation WHERE pkg=%Q AND tag=%Q", pkg, tag);
}

MPORT_PUBLIC_API int
mport_annotation_delete_all(mportInstance *mport, const char *pkg)
{
	return mport_db_do(mport->db, "DELETE FROM annotation WHERE pkg=%Q", pkg);
}

MPORT_PUBLIC_API int
mport_annotation_list(mportInstance *mport, const char *pkg, char ***tags, int *tag_count)
{
	sqlite3_stmt *stmt = NULL;
	int count = 0;
	int i = 0;

	if (mport_db_prepare(mport->db, &stmt, "SELECT tag FROM annotation WHERE pkg=%Q", pkg) !=
	    MPORT_OK) {
		sqlite3_finalize(stmt);
		RETURN_CURRENT_ERROR;
	}

	if (stmt == NULL) {
		RETURN_ERROR(MPORT_ERR_FATAL, "AnnotationStatement was null");
	}

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		count++;
	}

	sqlite3_finalize(stmt);

	if (count == 0) {
		*tags = NULL;
		*tag_count = 0;
		return MPORT_OK;
	}

	*tags = malloc(count * sizeof(char *));
	if (*tags == NULL) {
		RETURN_ERROR(MPORT_ERR_FATAL, "Failed to allocate memory for tags");
	}

	if (mport_db_prepare(mport->db, &stmt, "SELECT tag FROM annotation WHERE pkg=%Q", pkg) !=
	    MPORT_OK) {
		sqlite3_finalize(stmt);
		free(*tags);
		RETURN_CURRENT_ERROR;
	}

	while (sqlite3_step(stmt) == SQLITE_ROW && i < count) {
		const unsigned char *tag = sqlite3_column_text(stmt, 0);
		if (tag == NULL) {
			while (i > 0) {
				i--;
				free((*tags)[i]);
			}
			free(*tags);
			*tags = NULL;
			sqlite3_finalize(stmt);
			RETURN_ERROR(MPORT_ERR_FATAL, "Malformed annotation row.");
		}
		(*tags)[i] = strdup((const char *)tag);
		if ((*tags)[i] == NULL) {
			while (i > 0) {
				i--;
				free((*tags)[i]);
			}
			free(*tags);
			*tags = NULL;
			sqlite3_finalize(stmt);
			RETURN_ERROR(MPORT_ERR_FATAL, "Failed to allocate memory for tag.");
		}
		i++;
	}

	sqlite3_finalize(stmt);
	/* report the number actually populated; if rows were deleted between
	   the COUNT and this SELECT, i < count and the remaining malloc'd slots
	   are uninitialized and must not be handed to the caller. */
	*tag_count = i;

	return MPORT_OK;
}
