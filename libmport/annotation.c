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

    if (mport_db_prepare(mport->db, &stmt, "SELECT val FROM annotation WHERE pkg=%Q AND tag=%Q", pkg, tag) != MPORT_OK) {
        sqlite3_finalize(stmt);
        RETURN_CURRENT_ERROR;
    }

    if (stmt == NULL) {
        RETURN_ERROR(MPORT_ERR_FATAL, "AnnotationStatement was null");
    }

    int ret = sqlite3_step(stmt);

    switch (ret) {
        case SQLITE_ROW:
            *annotation = strdup((const char *)sqlite3_column_text(stmt, 0));
            sqlite3_finalize(stmt);
            break;
        case SQLITE_DONE:
            sqlite3_finalize(stmt);
            RETURN_ERRORX(MPORT_ERR_FATAL, "No annotation found for package %s with tag %s", pkg, tag);
        default:
            sqlite3_finalize(stmt);
            RETURN_ERRORX(MPORT_ERR_FATAL, "Database error: %s", sqlite3_errmsg(mport->db));
    }

    return (MPORT_OK);
}

MPORT_PUBLIC_API int
mport_annotation_set(mportInstance *mport, const char *pkg, const char *tag, const char *annotation)
{

    return mport_db_do(mport->db,
                         "INSERT OR REPLACE INTO annotation (pkg, tag, val) VALUES (%Q, %Q, %Q)",
                         pkg, tag, annotation);
}

MPORT_PUBLIC_API int
mport_annotation_delete(mportInstance *mport, const char *pkg, const char *tag)
{
    return mport_db_do(mport->db,
                         "DELETE FROM annotation WHERE pkg=%Q AND tag=%Q",
                         pkg, tag);
}

MPORT_PUBLIC_API int
mport_annotation_delete_all(mportInstance *mport, const char *pkg)
{
    return mport_db_do(mport->db,
                         "DELETE FROM annotation WHERE pkg=%Q",
                         pkg);
}

MPORT_PUBLIC_API int
mport_annotation_list(mportInstance *mport, const char *pkg, char ***tags, int *tag_count)
{
    sqlite3_stmt *stmt = NULL;
    int count = 0;
    int i = 0;

    if (mport_db_prepare(mport->db, &stmt, "SELECT tag FROM annotation WHERE pkg=%Q", pkg) != MPORT_OK) {
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

    *tags = malloc(count * sizeof(char*));
    if (*tags == NULL) {
        RETURN_ERROR(MPORT_ERR_FATAL, "Failed to allocate memory for tags");
    }

    if (mport_db_prepare(mport->db, &stmt, "SELECT tag FROM annotation WHERE pkg=%Q", pkg) != MPORT_OK) {
        sqlite3_finalize(stmt);
        free(*tags);
        RETURN_CURRENT_ERROR;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW && i < count) {
        (*tags)[i] = strdup((const char *)sqlite3_column_text(stmt, 0));
        i++;
    }

    sqlite3_finalize(stmt);     
    *tag_count = count;

    return MPORT_OK;
}