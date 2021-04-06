/*-
 * Copyright (c) 2013, 2015 Lucas Holt
 * Copyright (c) 2007-2009 Chris Reinhardt
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

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "mport.h"
#include "mport_private.h"

/**
 * allocate memory for a mportInstance
 */
MPORT_PUBLIC_API mportInstance *
mport_instance_new(void) {

    return (mportInstance *) calloc(1, sizeof(mportInstance));
}


/**
 * Set up the master database, and related instance infrastructure.
 */
MPORT_PUBLIC_API int
mport_instance_init(mportInstance *mport, const char *root) {

	char dir[FILENAME_MAX];
	mport->flags = 0;

	if (root != NULL) {
		mport->root = strdup(root);
	} else {
		mport->root = strdup("");
	}

	(void) snprintf(dir, FILENAME_MAX, "%s/%s", mport->root, MPORT_INST_DIR);

	if (mport_mkdir(dir) != MPORT_OK) {
		RETURN_CURRENT_ERROR;
	}

	(void) snprintf(dir, FILENAME_MAX, "%s/%s", mport->root, MPORT_INST_INFRA_DIR);

	if (mport_mkdir(dir) != MPORT_OK) {
		RETURN_CURRENT_ERROR;
	}

	/* dir is a file here, just trying to save memory */
	(void) snprintf(dir, FILENAME_MAX, "%s/%s", mport->root, MPORT_MASTER_DB_FILE);
	if (sqlite3_open(dir, &(mport->db)) != 0) {
		sqlite3_close(mport->db);
		RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(mport->db));
	}

	if (sqlite3_create_function(mport->db, "mport_version_cmp", 2, SQLITE_ANY, NULL, &mport_version_cmp_sqlite,
								NULL,
								NULL) != SQLITE_OK) {
		sqlite3_close(mport->db);
		RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(mport->db));
	}


	/* set the default UI callbacks */
	mport->msg_cb = &mport_default_msg_cb;
	mport->progress_init_cb = &mport_default_progress_init_cb;
	mport->progress_step_cb = &mport_default_progress_step_cb;
	mport->progress_free_cb = &mport_default_progress_free_cb;
	mport->confirm_cb = &mport_default_confirm_cb;

	int db_version = mport_get_database_version(mport->db);
	if (db_version < 1) {
		/* new, create tables */
		mport_generate_master_schema(mport->db);
		db_version = mport_get_database_version(mport->db);
	}

	mport_db_do(mport->db, "PRAGMA journal_mode=WAL");

	return mport_upgrade_master_schema(mport->db, db_version);
}

/**
 * Get the mport database schema version.
 */
int
mport_get_database_version(sqlite3 *db) {
    int databaseVersion = -1; /* ERROR condition */

		sqlite3_stmt *stmt_version;
        if (sqlite3_prepare_v2(db, "PRAGMA user_version;", -1, &stmt_version, NULL) == SQLITE_OK) {
            while (sqlite3_step(stmt_version) == SQLITE_ROW) {
                databaseVersion = sqlite3_column_int(stmt_version, 0);
            }
        }
        sqlite3_finalize(stmt_version);

    return databaseVersion;
}

/**
 * Set the mport database schema version for when we upgrade mport
 */
int
mport_set_database_version(sqlite3 *db) {
    char *sql;

    asprintf(&sql, "PRAGMA user_version=%d", MPORT_MASTER_VERSION);

    if (mport_db_do(db, sql) != MPORT_OK) {
        free(sql);
        RETURN_CURRENT_ERROR;
    }

    free(sql);

    return (MPORT_OK);
}

/* Setters for the variable UI callbacks. */
MPORT_PUBLIC_API void
mport_set_msg_cb(mportInstance *mport, mport_msg_cb cb) {
    mport->msg_cb = cb;
}

MPORT_PUBLIC_API void
mport_set_progress_init_cb(mportInstance *mport, mport_progress_init_cb cb) {
    mport->progress_init_cb = cb;
}

MPORT_PUBLIC_API void
mport_set_progress_step_cb(mportInstance *mport, mport_progress_step_cb cb) {
    mport->progress_step_cb = cb;
}

MPORT_PUBLIC_API void
mport_set_progress_free_cb(mportInstance *mport, mport_progress_free_cb cb) {
    mport->progress_free_cb = cb;
}


MPORT_PUBLIC_API void
mport_set_confirm_cb(mportInstance *mport, mport_confirm_cb cb) {
    mport->confirm_cb = cb;
}


/* callers for the callbacks (only for msg at the moment) */
void
mport_call_msg_cb(mportInstance *mport, const char *fmt, ...) {
    va_list args;

    char *msg;
    va_start(args, fmt);
    (void) vasprintf(&msg, fmt, args);
    va_end(args);

    if (msg == NULL)
        return; /* No message for you! */


        (mport->msg_cb)(msg);

        free(msg);
}


void
mport_call_progress_init_cb(mportInstance *mport, const char *fmt, ...) {
    va_list args;
    char *title;

    va_start(args, fmt);
    (void) vasprintf(&title, fmt, args);

    if (title == NULL)
        return;

        (mport->progress_init_cb)(title);

        free(title);
}


MPORT_PUBLIC_API int
mport_instance_free(mportInstance *mport) {
    if (sqlite3_close(mport->db) != SQLITE_OK) {
        RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(mport->db));
    }

    free(mport->root);
    free(mport);
    return MPORT_OK;
}
