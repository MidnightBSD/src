/*-
 * Copyright (c) 2009 Chris Reinhardt
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

#include "mport.h"
#include "mport_private.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stddef.h>

static int index_is_recentish(void);

static int index_last_checked_recentish(mportInstance *);

static int index_update_last_checked(mportInstance *);

static time_t get_time(void);

static int lookup_alias(mportInstance *, const char *, char **);

static int attach_index_db(sqlite3 *db);

static void populate_row(sqlite3_stmt *stmt, mportIndexEntry *e);

/*
 * Loads the index database.  The index contains a list of bundles that are
 * available for download, a list of aliases (apache is aliased to apache22 for 
 * example), and a list of mirrors.
 *
 * This function will use the current local index if it is present and younger
 * than the max index age.  Otherwise, it will download the index.  If any 
 * index is present, the mirror list will be used; otherwise the bootstrap
 * url will be used.
 */
MPORT_PUBLIC_API int
mport_index_load(mportInstance *mport)
{

	if (mport_file_exists(MPORT_INDEX_FILE)) {

		if (attach_index_db(mport->db) != MPORT_OK)
			RETURN_CURRENT_ERROR;

		mport->flags |= MPORT_INST_HAVE_INDEX;

		if (!index_is_recentish()) {
			if (index_last_checked_recentish(mport))
				return MPORT_OK;

			return mport_index_get(mport);
		}
	} else {
		if (mport_fetch_bootstrap_index(mport) != MPORT_OK)
			RETURN_CURRENT_ERROR;

		if (!mport_file_exists(MPORT_INDEX_FILE))
			RETURN_ERROR(MPORT_ERR_FATAL, "Index file could not be extracted");

		if (attach_index_db(mport->db) != MPORT_OK)
			RETURN_CURRENT_ERROR;

		mport->flags |= MPORT_INST_HAVE_INDEX;
		if (index_update_last_checked(mport) != MPORT_OK)
			RETURN_CURRENT_ERROR;
	}

	return (MPORT_OK);
}

static int
attach_index_db(sqlite3 *db)
{

	if (mport_db_do(db, "ATTACH %Q AS idx", MPORT_INDEX_FILE) != MPORT_OK) {
		RETURN_CURRENT_ERROR;
	}

	return (MPORT_OK);
}


/**
 * mport_index_load is typically preferred.  This function is only used to force
 * a download of the index manually by the user.
 */
MPORT_PUBLIC_API int
mport_index_get(mportInstance *mport)
{

	if (!(mport->flags & MPORT_INST_HAVE_INDEX)) {
		if (mport_fetch_bootstrap_index(mport) != MPORT_OK)
			RETURN_CURRENT_ERROR;
	} else {
		if (mport_fetch_index(mport) != MPORT_OK) {
			SET_ERROR(MPORT_ERR_WARN, "Could not fetch updated index; previous index used.");
			RETURN_CURRENT_ERROR;
		}
	}

	/* if we were already attached, reconnect refreshed index. */
	if (mport->flags & MPORT_INST_HAVE_INDEX) {
		if (mport_db_do(mport->db, "DETACH idx") != MPORT_OK)
			RETURN_CURRENT_ERROR;

		mport->flags &= ~MPORT_INST_HAVE_INDEX;

		if (attach_index_db(mport->db) != MPORT_OK)
			RETURN_CURRENT_ERROR;

		mport->flags |= MPORT_INST_HAVE_INDEX;
	}

	if (index_update_last_checked(mport) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	return (MPORT_OK);
}


/* return 1 if the index is younger than the max age, 0 otherwise */
static int
index_is_recentish(void)
{
	struct stat st;

	if (stat(MPORT_INDEX_FILE, &st) != 0)
		return 0;

	if ((st.st_birthtime + MPORT_MAX_INDEX_AGE) < get_time())
		return 0;

	return 1;
}

static int
index_last_checked_recentish(mportInstance *mport)
{
	char *recent;
	int ret;

	recent = mport_setting_get(mport, MPORT_SETTING_INDEX_LAST_CHECKED);
	if (recent && get_time() < atoi(recent) + MPORT_DAY)
		ret = 1;
	else
		ret = 0;

	free(recent);

	return ret;
}

static int
index_update_last_checked(mportInstance *mport)
{
	char *utime;
	int ret;

	asprintf(&utime, "%jd", (intmax_t) get_time());
	if (utime)
		ret = mport_setting_set(mport, MPORT_SETTING_INDEX_LAST_CHECKED, utime);
	else
		RETURN_CURRENT_ERROR;
	free(utime);

	return ret;
}

static time_t
get_time(void)
{
	struct timespec now;

	if (clock_gettime(CLOCK_REALTIME, &now) != 0)
		RETURN_ERROR(MPORT_ERR_FATAL, strerror(errno));

	return now.tv_sec;
}


/*
 * Fills the string vector with the list of the mirrors for the current
 * country.  
 * 
 * XXX - The country is currently hardcoded to the US.
 */
int mport_index_get_mirror_list(mportInstance *mport, char ***list_p, int *list_size)
{
	char **list;
	int ret, i;
	int len;
	sqlite3_stmt *stmt;
	char *mirror_region;

	mirror_region = mport_setting_get(mport, MPORT_SETTING_MIRROR_REGION);
	if (mirror_region == NULL) {
		mirror_region = "us";
	}

	/* XXX the country is hard coded until a configuration system is created */
	if (mport_db_count(mport->db, &len, "SELECT COUNT(*) FROM idx.mirrors WHERE country=%Q", mirror_region) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	*list_size = len;
	list = calloc((size_t) len + 1, sizeof(char *));
	*list_p = list;
	i = 0;

	if (mport_db_prepare(mport->db, &stmt, "SELECT mirror FROM idx.mirrors WHERE country=%Q", mirror_region) !=
	    MPORT_OK) {
		sqlite3_finalize(stmt);
		RETURN_CURRENT_ERROR;
	}

	while (1) {
		ret = sqlite3_step(stmt);

		if (ret == SQLITE_ROW) {
			list[i] = strdup((const char *) sqlite3_column_text(stmt, 0));

			if (list[i] == NULL) {
				sqlite3_finalize(stmt);
				RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");
			}

			i++;
		} else if (ret == SQLITE_DONE) {
			list[i] = NULL;
			break;
		} else {
			list[i] = NULL;
			sqlite3_finalize(stmt);
			RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(mport->db));
		}
	}

	sqlite3_finalize(stmt);
	return MPORT_OK;
}

/*
 * Looks up a pkgname from the index and fills a vector of index entries
 * with the result.
 *
 * Globbing is supported, and the alias list is consulted.  The calling code
 * is responsible for freeing the memory allocated.  See
 * mport_index_entry_free_vec()
 */
MPORT_PUBLIC_API int
mport_index_lookup_pkgname(mportInstance *mport, const char *pkgname, mportIndexEntry ***entry_vec)
{
	char *lookup = NULL;
	int count;
	int i = 0, step;
	sqlite3_stmt *stmt;
	int ret = MPORT_OK;
	mportIndexEntry **e = NULL;

	MPORT_CHECK_FOR_INDEX(mport, "mport_index_lookup_pkgname()")

	if (lookup_alias(mport, pkgname, &lookup) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	if (mport_db_count(mport->db, &count, "SELECT count(*) FROM idx.packages  WHERE pkg GLOB %Q", lookup) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	e = (mportIndexEntry **) calloc((size_t) count + 1, sizeof(mportIndexEntry *));
	if (e == NULL) {
		free(lookup);
		RETURN_ERROR(MPORT_ERR_FATAL, "Could not allocate memory for index entries");
	}
	*entry_vec = e;

	if (count == 0) {
		free(lookup);
		return MPORT_OK;
	}

	if (mport_db_prepare(mport->db, &stmt,
	                     "SELECT pkg, version, comment, bundlefile, license, hash FROM idx.packages WHERE pkg GLOB %Q",
	                     lookup) != MPORT_OK) {
		ret = mport_err_code();
		goto DONE;
	}

	while (1) {
		step = sqlite3_step(stmt);

		if (step == SQLITE_ROW) {
			if ((e[i] = (mportIndexEntry *) calloc(1, sizeof(mportIndexEntry))) == NULL) {
				ret = MPORT_ERR_FATAL;
				goto DONE;
			}

			populate_row(stmt, e[i]);

			if (e[i]->pkgname == NULL || e[i]->version == NULL || e[i]->comment == NULL || e[i]->license == NULL ||
			    e[i]->bundlefile == NULL) {
				ret = MPORT_ERR_FATAL;
				goto DONE;
			}

			i++;
		} else if (step == SQLITE_DONE) {
			e[i] = NULL;
			goto DONE;
		} else {
			ret = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(mport->db));
			goto DONE;
		}
	}

	DONE:
	free(lookup);
	sqlite3_finalize(stmt);
	return ret;
}


/* mport_index_search(mportInstance *mport, mportIndexEntry ***entry_vec, const char *where, ...)
 *
 * Allocate and populate the index meta for the given package in the index.
 *
 * 'where' and the vargs are used to be build a where clause.  For example to search by
 * name:
 *
 * mport_index_search(mport, &indexEntries, "pkg=%Q", name);
 *
 * indexEntries is set to an empty allocated list and MPORT_OK is returned if no packages where found.
 */
MPORT_PUBLIC_API int
mport_index_search(mportInstance *mport, mportIndexEntry ***entry_vec, const char *fmt, ...)
{
	va_list args;
	sqlite3_stmt *stmt;
	int ret = MPORT_OK;
	int len;
	int i = 0, step;
	char *where;
	sqlite3 *db = mport->db;
	mportIndexEntry **e;

	va_start(args, fmt);
	where = sqlite3_vmprintf(fmt, args);
	va_end(args);

	if (where == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Could not build where clause");

	if (mport_db_count(mport->db, &len, "SELECT count(*) FROM idx.packages  WHERE %s", where) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	e = (mportIndexEntry **) calloc((size_t) len + 1, sizeof(mportIndexEntry *));
	if (e == NULL) {
		sqlite3_free(where);
		RETURN_ERROR(MPORT_ERR_FATAL, "Could not allocate memory");
	}
	*entry_vec = e;

	if (len == 0) {
		sqlite3_free(where);
		return MPORT_OK;
	}

	if (mport_db_prepare(db, &stmt,
	                     "SELECT pkg, version, comment, bundlefile, license, hash FROM idx.packages WHERE %s", where) !=
	    MPORT_OK) {
		sqlite3_free(where);
		sqlite3_finalize(stmt);
		RETURN_CURRENT_ERROR;
	}

	while (1) {
		step = sqlite3_step(stmt);

		if (step == SQLITE_ROW) {
			if ((e[i] = (mportIndexEntry *) calloc(1, sizeof(mportIndexEntry))) == NULL) {
				ret = MPORT_ERR_FATAL;
				break;
			}

			populate_row(stmt, e[i]);

			if (e[i]->pkgname == NULL || e[i]->version == NULL || e[i]->comment == NULL || e[i]->license == NULL ||
			    e[i]->bundlefile == NULL) {
				ret = MPORT_ERR_FATAL;
				break;
			}

			i++;
		} else if (step == SQLITE_DONE) {
			e[i] = NULL;
			break;
		} else {
			ret = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(mport->db));
			break;
		}
	}

	sqlite3_free(where);
	sqlite3_finalize(stmt);

	return ret;
}


MPORT_PUBLIC_API int
mport_index_list(mportInstance *mport, mportIndexEntry ***entry_vec)
{
	sqlite3_stmt *stmt;
	int ret = MPORT_OK;
	int len;
	int i = 0, step;
	sqlite3 *db = mport->db;
	mportIndexEntry **e;

	if (mport_db_count(mport->db, &len, "SELECT count(*) FROM idx.packages") != MPORT_OK)
		RETURN_CURRENT_ERROR;

	e = (mportIndexEntry **) calloc((size_t) len + 1, sizeof(mportIndexEntry *));
	*entry_vec = e;

	if (len == 0) {
		return MPORT_OK;
	}

	if (mport_db_prepare(db, &stmt, "SELECT pkg, version, comment, bundlefile, license, hash FROM idx.packages") !=
	    MPORT_OK) {
		sqlite3_finalize(stmt);
		RETURN_CURRENT_ERROR;
	}

	while (1) {
		step = sqlite3_step(stmt);

		if (step == SQLITE_ROW) {
			if ((e[i] = (mportIndexEntry *) calloc(1, sizeof(mportIndexEntry))) == NULL) {
				ret = MPORT_ERR_FATAL;
				break;
			}

			populate_row(stmt, e[i]);

			if (e[i]->pkgname == NULL || e[i]->version == NULL || e[i]->comment == NULL || e[i]->license == NULL ||
			    e[i]->bundlefile == NULL) {
				ret = MPORT_ERR_FATAL;
				break;
			}

			i++;
		} else if (step == SQLITE_DONE) {
			e[i] = NULL;
			break;
		} else {
			ret = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(mport->db));
			break;
		}
	}

	sqlite3_finalize(stmt);

	return ret;
}

static void
populate_row(sqlite3_stmt *stmt, mportIndexEntry *e)
{

	e->pkgname = strdup((const char *) sqlite3_column_text(stmt, 0));
	e->version = strdup((const char *) sqlite3_column_text(stmt, 1));
	e->comment = strdup((const char *) sqlite3_column_text(stmt, 2));
	e->bundlefile = strdup((const char *) sqlite3_column_text(stmt, 3));
	e->license = strdup((const char *) sqlite3_column_text(stmt, 4));
	e->hash = strdup((const char *) sqlite3_column_text(stmt, 5));
}


static int
lookup_alias(mportInstance *mport, const char *query, char **result)
{
	sqlite3_stmt *stmt;
	int ret = MPORT_OK;

	if (mport_db_prepare(mport->db, &stmt, "SELECT pkg FROM idx.aliases WHERE alias=%Q", query) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	switch (sqlite3_step(stmt)) {
		case SQLITE_ROW:
			*result = strdup((const char *) sqlite3_column_text(stmt, 0));
			break;
		case SQLITE_DONE:
			*result = strdup(query);
			break;
		default:
			ret = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(mport->db));
			break;
	}

	sqlite3_finalize(stmt);

	return ret;
}


/* free a vector of mportIndexEntry structs */
MPORT_PUBLIC_API void
mport_index_entry_free_vec(mportIndexEntry **e)
{

	if (e == NULL)
		return;

	for (int i = 0; e[i] != NULL; i++)
		mport_index_entry_free(e[i]);
}


/* free a mportIndexEntry struct */
MPORT_PUBLIC_API void
mport_index_entry_free(mportIndexEntry *e)
{
	if (e == NULL)
		return;

	free(e->pkgname);
	free(e->comment);
	free(e->version);
	free(e->bundlefile);
	free(e->license);
	free(e->hash);
	free(e);
}
