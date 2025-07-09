/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2013, 2015, 2025 Lucas Holt
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

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "mport.h"
#include "mport_private.h"


/**
 * @brief Allocates memory for a new mportInstance structure.
 *
 * This function creates a new mportInstance by allocating memory using calloc.
 * The allocated memory is initialized to zero.
 *
 * @return A pointer to the newly allocated mportInstance structure.
 *         Returns NULL if memory allocation fails.
 */
MPORT_PUBLIC_API mportInstance *
mport_instance_new(void) {

    return (mportInstance *) calloc(1, sizeof(mportInstance));
}

/**
 * Set up the master database, and related instance infrastructure.
 */
MPORT_PUBLIC_API int
mport_instance_init(mportInstance *mport, const char *root, const char *outputPath, bool noIndex, mportVerbosity verbosity) {

	char dir[FILENAME_MAX];
	mport->flags = 0;

	mport->noIndex = noIndex;
	mport->verbosity = verbosity;
	mport->offline = false;
	mport->force = false;

	if (root != NULL) {
		mport->root = strdup(root);
    	if ((mport->rootfd = open(mport->root, O_DIRECTORY|O_RDONLY|O_CLOEXEC)) < 0) {
			RETURN_ERROR(MPORT_ERR_FATAL, "unable to open root directory");
    	}
	} else {
		mport->root = strdup("");
		if ((mport->rootfd = open("/", O_DIRECTORY|O_RDONLY|O_CLOEXEC)) < 0) {
			RETURN_ERROR(MPORT_ERR_FATAL, "unable to open root directory");
		}
	}

	if (outputPath == NULL) {
		mport->outputPath = strdup(MPORT_LOCAL_PKG_PATH);
	} else {
		mport->outputPath = strdup(outputPath);
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
 * @brief Retrieves the version of the mport database schema.
 *
 * This function queries the SQLite database to get the user_version PRAGMA,
 * which represents the version of the mport database schema.
 *
 * @param db Pointer to the SQLite database connection.
 *
 * @return The version number of the database schema if successful,
 *         or -1 if an error occurred or the version could not be retrieved.
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
 * @brief Sets the database schema version for the mport database.
 *
 * This function updates the user_version PRAGMA in the SQLite database to
 * reflect the current mport master version. This is typically used when
 * upgrading the mport database schema.
 *
 * @param db Pointer to the SQLite database connection.
 *
 * @return MPORT_OK on successful update of the database version,
 *         or an error code if the operation fails.
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
/**
 * @brief Sets the message callback function for a mport instance.
 *
 * This function allows setting a custom message callback function for the given
 * mport instance. The message callback is used to handle and display messages
 * during mport operations.
 *
 * @param mport Pointer to the mportInstance for which to set the callback.
 * @param cb The message callback function to be set. This function should
 *           conform to the mport_msg_cb type.
 *
 * @return This function does not return a value.
 */
MPORT_PUBLIC_API void
mport_set_msg_cb(mportInstance *mport, mport_msg_cb cb) {
    mport->msg_cb = cb;
}

/**
 * @brief Sets the progress initialization callback function for a mport instance.
 *
 * This function allows setting a custom progress initialization callback function
 * for the given mport instance. The progress initialization callback is typically
 * used to set up or initialize progress reporting for an operation.
 *
 * @param mport Pointer to the mportInstance for which to set the callback.
 * @param cb The progress initialization callback function to be set. This function
 *           should conform to the mport_progress_init_cb type.
 *
 * @return This function does not return a value.
 */
MPORT_PUBLIC_API void
mport_set_progress_init_cb(mportInstance *mport, mport_progress_init_cb cb) {
    mport->progress_init_cb = cb;
}

/**
 * @brief Sets the progress step callback function for a mport instance.
 *
 * This function allows setting a custom progress step callback function for the given
 * mport instance. The progress step callback is used to update the progress of an
 * ongoing operation.
 *
 * @param mport Pointer to the mportInstance for which to set the callback.
 * @param cb The progress step callback function to be set. This function should
 *           conform to the mport_progress_step_cb type.
 *
 * @return This function does not return a value.
 */
MPORT_PUBLIC_API void
mport_set_progress_step_cb(mportInstance *mport, mport_progress_step_cb cb) {
    mport->progress_step_cb = cb;
}

/**
 * @brief Sets the progress free callback function for a mport instance.
 *
 * This function allows setting a custom progress free callback function for the given
 * mport instance. The progress free callback is typically used to clean up or free
 * resources associated with progress reporting.
 *
 * @param mport Pointer to the mportInstance for which to set the callback.
 * @param cb The progress free callback function to be set. This function should
 *           conform to the mport_progress_free_cb type.
 *
 * @return This function does not return a value.
 */
MPORT_PUBLIC_API void
mport_set_progress_free_cb(mportInstance *mport, mport_progress_free_cb cb) {
    mport->progress_free_cb = cb;
}

/**
 * @brief Sets the confirmation callback function for a mport instance.
 *
 * This function allows setting a custom confirmation callback function for the given
 * mport instance. The confirmation callback is used to prompt the user for
 * confirmation during certain operations.
 *
 * @param mport Pointer to the mportInstance for which to set the callback.
 * @param cb The confirmation callback function to be set. This function should
 *           conform to the mport_confirm_cb type.
 *
 * @return This function does not return a value.
 */
MPORT_PUBLIC_API void
mport_set_confirm_cb(mportInstance *mport, mport_confirm_cb cb) {
    mport->confirm_cb = cb;
}

/* callers for the callbacks (only for msg at the moment) */


/**
 * @brief Calls the message callback function with a formatted message.
 *
 * This function formats a message using the provided format string and arguments,
 * then calls the message callback function stored in the mportInstance.
 *
 * @param mport Pointer to the mportInstance containing the message callback.
 * @param fmt Format string for the message.
 * @param ... Variable arguments to be formatted according to fmt.
 *
 * @return MPORT_OK on success, MPORT_ERR_WARN if message formatting fails.
 */
MPORT_PUBLIC_API int
mport_call_msg_cb(mportInstance *mport, const char *fmt, ...) {
    va_list args;

    char *msg;
    va_start(args, fmt);
    (void) vasprintf(&msg, fmt, args);
    va_end(args);

    if (msg == NULL)
        RETURN_ERROR(MPORT_ERR_WARN, "Unable to format message");

    (mport->msg_cb)(msg);

    free(msg);
    msg = NULL;

    return (MPORT_OK);
}

/**
 * @brief Calls the confirmation callback function to prompt the user for confirmation.
 *
 * This function invokes the confirmation callback set in the mportInstance to ask
 * the user for confirmation. It handles special cases such as null messages and
 * environment variable overrides.
 *
 * @param mport Pointer to the mportInstance containing the confirmation callback.
 * @param msg The message to display to the user for confirmation.
 * @param yes The text to display for the affirmative option.
 * @param no The text to display for the negative option.
 * @param def The default option (1 for yes, 0 for no). (an int so custom logic can be expressed)
 *
 * @return true if the user confirms or if ASSUME_ALWAYS_YES is set,
 *         false if the user declines.
 */
MPORT_PUBLIC_API bool
mport_call_confirm_cb(mportInstance *mport, const char *msg, const char *yes, const char *no, int def) {
    if (getenv("ASSUME_ALWAYS_YES") != NULL) {
        return (true);
    }

    return (mport->confirm_cb)(msg, yes, no, def) == MPORT_OK ? true : false;
}


/**
 * @brief Initializes and calls the progress callback with a formatted title.
 *
 * This function formats a title string using the provided format and arguments,
 * then calls the progress initialization callback stored in the mportInstance.
 *
 * @param mport Pointer to the mportInstance containing the progress callback.
 * @param fmt Format string for the progress title.
 * @param ... Variable arguments to be formatted according to fmt.
 *
 * @return MPORT_OK on success, MPORT_ERR_WARN if title formatting fails.
 */
MPORT_PUBLIC_API int
mport_call_progress_init_cb(mportInstance *mport, const char *fmt, ...) {
    va_list args;
    char *title = NULL;

    va_start(args, fmt);
    (void) vasprintf(&title, fmt, args);

    if (title == NULL)
        RETURN_ERROR(MPORT_ERR_WARN, "Unable to format progress title");

    (mport->progress_init_cb)(title);

    free(title);
    title = NULL;

    return (MPORT_OK);
}


/**
 * @brief Frees resources associated with a mportInstance.
 *
 * This function closes the SQLite database connection, closes the root file descriptor,
 * frees allocated memory for root and outputPath, and finally frees the mportInstance itself.
 *
 * @param mport Pointer to the mportInstance to be freed. The pointer becomes invalid after this call.
 *
 * @return MPORT_OK on successful freeing of resources, or MPORT_ERR_FATAL if there's an error closing the database.
 */
MPORT_PUBLIC_API int
mport_instance_free(mportInstance *mport) {
    if (sqlite3_close(mport->db) != SQLITE_OK) {
        RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(mport->db));
    }

    close(mport->rootfd);
    free(mport->root);
    mport->root = NULL;

    free(mport->outputPath);
    mport->outputPath = NULL;
    free(mport);

    return MPORT_OK;
}
