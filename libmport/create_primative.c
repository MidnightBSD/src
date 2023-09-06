/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2015, 2022, 2023 Lucas Holt
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

#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <sha256.h>
#include <archive.h>
#include <archive_entry.h>
#include <assert.h>
#include "mport.h"
#include "mport_private.h"

static int create_stub_db(mportInstance *, sqlite3 **, const char *);

static int insert_assetlist(sqlite3 *, mportAssetList *, mportPackageMeta *, mportCreateExtras *);

static int insert_meta(mportInstance *, sqlite3 *, mportPackageMeta *, mportCreateExtras *);

static int insert_depends(sqlite3 *, mportPackageMeta *, mportCreateExtras *);

static int insert_conflicts(sqlite3 *, mportPackageMeta *, mportCreateExtras *);

static int insert_categories(sqlite3 *, mportPackageMeta *);

static int archive_files(mportAssetList *, mportPackageMeta *, mportCreateExtras *, const char *);

static int archive_metafiles(mportBundleWrite *, mportPackageMeta *, mportCreateExtras *);

static int archive_assetlistfiles(mportBundleWrite *, mportPackageMeta *, mportCreateExtras *, mportAssetList *);

static int clean_up(const char *);

MPORT_PUBLIC_API int
mport_create_primative(mportInstance *mport, mportAssetList *assetlist, mportPackageMeta *pack, mportCreateExtras *extra)
{
	int error_code = MPORT_OK;

	sqlite3 *db = NULL;

	char dirtmpl[] = "/tmp/mport.XXXXXXXX";
	char *tmpdir = mkdtemp(dirtmpl);

	if (tmpdir == NULL) {
		error_code = SET_ERROR(MPORT_ERR_FATAL, strerror(errno));
		goto CLEANUP;
	}

	if ((error_code = create_stub_db(mport, &db, tmpdir)) != MPORT_OK)
		goto CLEANUP;

	if ((error_code = insert_assetlist(db, assetlist, pack, extra)) != MPORT_OK)
		goto CLEANUP;

	if ((error_code = insert_meta(mport, db, pack, extra)) != MPORT_OK)
		goto CLEANUP;

	if (sqlite3_close(db) != SQLITE_OK) {
		error_code = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
		goto CLEANUP;
	}

	error_code = archive_files(assetlist, pack, extra, tmpdir); /* cleanup will run next which is the desired action */

	CLEANUP:
	clean_up(tmpdir);

	return error_code;
}


static int
create_stub_db(mportInstance *mport, sqlite3 **db, const char *tmpdir)
{
	int error_code = MPORT_OK;

	char file[FILENAME_MAX];
	(void) snprintf(file, FILENAME_MAX, "%s/%s", tmpdir, MPORT_STUB_DB_FILE);
	if (sqlite3_open(file, db) != SQLITE_OK) {
		sqlite3_close(*db);
		error_code = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(*db));
	}

	if (error_code != MPORT_OK)
		return error_code;

	/* create tables */
	return mport_generate_stub_schema(mport, *db);
}

static int
insert_assetlist(sqlite3 *db, mportAssetList *assetlist, mportPackageMeta *pack, mportCreateExtras *extra)
{
	mportAssetListEntry *e = NULL;
	sqlite3_stmt *stmnt = NULL;
	char sql[] = "INSERT INTO assets (pkg, type, data, checksum, owner, grp, mode) VALUES (?,?,?,?,?,?,?)";
	char hash[65];
	char file[FILENAME_MAX];
	char cwd[FILENAME_MAX];
	struct stat st;

	strlcpy(cwd, extra->sourcedir, FILENAME_MAX);
	strlcat(cwd, pack->prefix, FILENAME_MAX);

	if (mport_db_prepare(db, &stmnt, sql) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	STAILQ_FOREACH(e, assetlist, next)
	{
		if (e->type == ASSET_COMMENT)
			continue;

		if (e->type == ASSET_CWD) {
			strlcpy(cwd, extra->sourcedir, FILENAME_MAX);
			if (e->data == NULL) {
				strlcat(cwd, pack->prefix, FILENAME_MAX);
			} else {
				strlcat(cwd, e->data, FILENAME_MAX);
			}
		}

		if (sqlite3_bind_text(stmnt, 1, pack->name, -1, SQLITE_STATIC) != SQLITE_OK) {
			RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
		}
		if (sqlite3_bind_int(stmnt, 2, e->type) != SQLITE_OK) {
			RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
		}
		if (sqlite3_bind_text(stmnt, 3, e->data, -1, SQLITE_STATIC) != SQLITE_OK) {
			RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
		}
		// 4 is computed below
		if (sqlite3_bind_text(stmnt, 5, e->owner, -1, SQLITE_STATIC) != SQLITE_OK) {
			RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
		}
		if (sqlite3_bind_text(stmnt, 6, e->group, -1, SQLITE_STATIC) != SQLITE_OK) {
			RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
		}
		if (sqlite3_bind_text(stmnt, 7, e->mode, -1, SQLITE_STATIC) != SQLITE_OK) {
			RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
		}

		if (e->type == ASSET_FILE || e->type == ASSET_SAMPLE || e->type == ASSET_SHELL ||
		    e->type == ASSET_FILE_OWNER_MODE || e->type == ASSET_SAMPLE_OWNER_MODE) {
			/* Don't prepend cwd onto absolute file paths (this is useful for update) */
			if (e->data[0] == '/') {
				(void) snprintf(file, FILENAME_MAX, "%s%s", extra->sourcedir, e->data);
			} else {
				(void) snprintf(file, FILENAME_MAX, "%s/%s", cwd, e->data);
			}

			if (e->type == ASSET_SAMPLE || e->type == ASSET_SAMPLE_OWNER_MODE) {
				for (int ch = 0; ch < FILENAME_MAX; ch++) {
					if (file[ch] == '\0')
						break;
					if (file[ch] == ' ' || file[ch] == '\t')
						file[ch] = '\0';
				}
			}

			if (lstat(file, &st) != 0) {
				// if we have a backup, we can safely ignore some missing files
				if (extra->is_backup) {
					// hack: mark it as a comment, so it gets ignored later
					e->type = ASSET_COMMENT;
					goto reset;
				}
				sqlite3_finalize(stmnt);
				RETURN_ERRORX(MPORT_ERR_FATAL, "Could not stat %s: %s", file, strerror(errno));
			}

			if (S_ISREG(st.st_mode)) {
				if (SHA256_File(file, hash) == NULL)
					RETURN_ERRORX(MPORT_ERR_FATAL, "File not found: %s", file);

				if (sqlite3_bind_text(stmnt, 4, hash, -1, SQLITE_STATIC) != SQLITE_OK)
					RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));

				pack->flatsize += st.st_size;
			} else {
				sqlite3_bind_null(stmnt, 4);
			}
		} else {
			if (sqlite3_bind_null(stmnt, 4) != SQLITE_OK) {
				RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
			}
		}

		if (sqlite3_step(stmnt) != SQLITE_DONE) {
			RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
		}

		reset:
		sqlite3_clear_bindings(stmnt);
		sqlite3_reset(stmnt);
	}

	sqlite3_finalize(stmnt);

	return MPORT_OK;
}

static int
insert_meta(mportInstance *mport, sqlite3 *db, mportPackageMeta *pack, mportCreateExtras *extra)
{
	int error_code = MPORT_OK;

	sqlite3_stmt *stmnt = NULL;
	const char *rest = 0;
	char sql[] = "INSERT INTO packages (pkg, version, origin, lang, prefix, comment, os_release, cpe, deprecated, expiration_date, no_provide_shlib, flavor, type, flatsize) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?)";

	char *os_release = mport_get_osrelease(mport);
	if (pack->cpe == NULL) {
		pack->cpe = malloc(1 * sizeof(char));
		pack->cpe[0] = '\0';
	}
	if (pack->deprecated == NULL) {
		pack->deprecated = malloc(1 * sizeof(char));
		pack->deprecated[0] = '\0';
	}
	if (pack->flavor == NULL) {
		pack->flavor = malloc(1 * sizeof(char));
		pack->flavor[0] = '\0';
	}

	if (sqlite3_prepare_v2(db, sql, -1, &stmnt, &rest) != SQLITE_OK) {
		error_code = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
		return error_code;
	}
	if (sqlite3_bind_text(stmnt, 1, pack->name, -1, SQLITE_STATIC) != SQLITE_OK) {
		error_code = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
		return error_code;
	}
	if (sqlite3_bind_text(stmnt, 2, pack->version, -1, SQLITE_STATIC) != SQLITE_OK) {
		error_code = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
		return error_code;
	}
	if (sqlite3_bind_text(stmnt, 3, pack->origin, -1, SQLITE_STATIC) != SQLITE_OK) {
		error_code = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
		return error_code;
	}
	if (sqlite3_bind_text(stmnt, 4, pack->lang, -1, SQLITE_STATIC) != SQLITE_OK) {
		error_code = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
		return error_code;
	}
	if (sqlite3_bind_text(stmnt, 5, pack->prefix, -1, SQLITE_STATIC) != SQLITE_OK) {
		error_code = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
		return error_code;
	}
	if (sqlite3_bind_text(stmnt, 6, pack->comment, -1, SQLITE_STATIC) != SQLITE_OK) {
		error_code = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
		return error_code;
	}
	if (sqlite3_bind_text(stmnt, 7, os_release, -1, SQLITE_STATIC) != SQLITE_OK) {
		free(os_release);
		error_code = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
		return error_code;
	}
	if (sqlite3_bind_text(stmnt, 8, pack->cpe, -1, SQLITE_STATIC) != SQLITE_OK) {
		error_code = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
		return error_code;
	}
	if (sqlite3_bind_text(stmnt, 9, pack->deprecated, -1, SQLITE_STATIC) != SQLITE_OK) {
		error_code = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
		return error_code;
	}
	if (sqlite3_bind_int64(stmnt, 10, pack->expiration_date) != SQLITE_OK) {
		error_code = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
		return error_code;
	}
	if (sqlite3_bind_int(stmnt, 11, pack->no_provide_shlib) != SQLITE_OK) {
		error_code = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
		return error_code;
	}
	if (sqlite3_bind_text(stmnt, 12, pack->flavor, -1, SQLITE_STATIC) != SQLITE_OK) {
		error_code = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
		return error_code;
	}
	if (sqlite3_bind_int(stmnt, 13, pack->type) != SQLITE_OK) {
		error_code = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
		return error_code;
	}
	if (sqlite3_bind_int(stmnt, 14, pack->flatsize) != SQLITE_OK) {
		error_code = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
		return error_code;
	}

	if (sqlite3_step(stmnt) != SQLITE_DONE) {
		error_code = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
	}

	sqlite3_finalize(stmnt);
	free(os_release);

	if (error_code != MPORT_OK)
		return error_code;

	if (insert_depends(db, pack, extra) != MPORT_OK)
		RETURN_CURRENT_ERROR;
	if (insert_conflicts(db, pack, extra) != MPORT_OK)
		RETURN_CURRENT_ERROR;
	if (insert_categories(db, pack) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	return error_code;
}


static int
insert_categories(sqlite3 *db, mportPackageMeta *pkg)
{
	sqlite3_stmt *stmt = NULL;
	int error_code = MPORT_OK;

	int i = 0;
	assert(pkg != NULL);
	if (pkg->categories == NULL)
		return MPORT_OK;

	if (mport_db_prepare(db, &stmt, "INSERT INTO categories (pkg, category) VALUES (?,?)") != MPORT_OK)
		RETURN_CURRENT_ERROR;

	while (pkg->categories[i] != NULL) {
		if (sqlite3_bind_text(stmt, 1, pkg->name, -1, SQLITE_STATIC) != SQLITE_OK) {
			error_code = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
		}
		if (sqlite3_bind_text(stmt, 2, pkg->categories[i], -1, SQLITE_STATIC) != SQLITE_OK) {
			error_code = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
		}

		if (sqlite3_step(stmt) != SQLITE_DONE) {
			error_code = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
		}
		sqlite3_clear_bindings(stmt);
		sqlite3_reset(stmt);
		i++;
	}

	sqlite3_finalize(stmt);
	return error_code;
}


static int
insert_conflicts(sqlite3 *db, mportPackageMeta *pack, mportCreateExtras *extra)
{
	int error_code = MPORT_OK;
	sqlite3_stmt *stmnt = NULL;
	assert(extra != NULL);
	char **conflict = extra->conflicts;

	/* we're done if there are no conflicts to record. */
	if (conflict == NULL)
		return MPORT_OK;

	if (mport_db_prepare(db, &stmnt, "INSERT INTO conflicts (pkg, conflict_pkg, conflict_version) VALUES (?,?,?)") !=
	    MPORT_OK)
		RETURN_CURRENT_ERROR;

	/* we have a conflict like apache-1.4.  We want to do a m/(.*)-(.*)/ */
	while (*conflict != NULL) {

		char *version = rindex(*conflict, '-');

		if (sqlite3_bind_text(stmnt, 1, pack->name, -1, SQLITE_STATIC) != SQLITE_OK) {
			error_code = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
			return error_code;
		}
		if (sqlite3_bind_text(stmnt, 2, *conflict, -1, SQLITE_STATIC) != SQLITE_OK) {
			error_code = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
			return error_code;
		}
		if (version != NULL) {
			*version = '\0';
			version++;
			if (sqlite3_bind_text(stmnt, 3, version, -1, SQLITE_STATIC) != SQLITE_OK) {
				error_code = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
				return error_code;
			}
		} else {
			if (sqlite3_bind_text(stmnt, 3, "*", -1, SQLITE_STATIC) != SQLITE_OK) {
				error_code = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
				return error_code;
			}
		}
		if (sqlite3_step(stmnt) != SQLITE_DONE) {
			error_code = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
			return error_code;
		}
		sqlite3_clear_bindings(stmnt);
		sqlite3_reset(stmnt);
		conflict++;
	}

	sqlite3_finalize(stmnt);

	return error_code;
}


static int
insert_depends(sqlite3 *db, mportPackageMeta *pack, mportCreateExtras *extra)
{
	int error_code = MPORT_OK;
	sqlite3_stmt *stmnt = NULL;
	assert(extra != NULL);
	char **depend = extra->depends;

	/* we're done if there are no deps to record. */
	if (depend == NULL)
		return MPORT_OK;

	if (mport_db_prepare(db, &stmnt,
	                     "INSERT INTO depends (pkg, depend_pkgname, depend_pkgversion, depend_port) VALUES (?,?,?,?)") !=
	    MPORT_OK)
		RETURN_CURRENT_ERROR;

	/* dependencies look like this.  break'em up into port, pkgversion and pkgname
	 * perl:lang/perl5.8:>=5.8.3
	 */
	while (*depend != NULL) {
		char *port = NULL;
		char *pkgversion = NULL;

		port = strchr(*depend, ':');
		if (port == NULL) {
			error_code = SET_ERRORX(MPORT_ERR_FATAL, "Malformed depend: %s", *depend);
			return error_code;
		}

		*port = '\0';
		port++;

		if (*port == 0) {
			error_code = SET_ERRORX(MPORT_ERR_FATAL, "Malformed depend: %s", *depend);
			return error_code;
		}

		if (sqlite3_bind_text(stmnt, 1, pack->name, -1, SQLITE_STATIC) != SQLITE_OK) {
			error_code = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
			return error_code;
		}
		if (sqlite3_bind_text(stmnt, 2, *depend, -1, SQLITE_STATIC) != SQLITE_OK) {
			error_code = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
			return error_code;
		}

		pkgversion = index(port, ':');

		if (pkgversion != NULL) {
			*pkgversion = '\0';
			pkgversion++;
			if (sqlite3_bind_text(stmnt, 3, pkgversion, -1, SQLITE_STATIC) != SQLITE_OK) {
				error_code = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
				return error_code;
			}
		} else {
			if (sqlite3_bind_null(stmnt, 3) != SQLITE_OK) {
				error_code = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
				return error_code;
			}
		}

		if (sqlite3_bind_text(stmnt, 4, port, -1, SQLITE_STATIC) != SQLITE_OK) {
			error_code = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
			return error_code;
		}

		if (sqlite3_step(stmnt) != SQLITE_DONE) {
			error_code = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
			return error_code;
		}

		sqlite3_clear_bindings(stmnt);
		sqlite3_reset(stmnt);
		depend++;
	}

	sqlite3_finalize(stmnt);

	return error_code;
}


static int
archive_files(mportAssetList *assetlist, mportPackageMeta *pack, mportCreateExtras *extra, const char *tmpdir)
{
	mportBundleWrite *bundle;
	char filename[FILENAME_MAX];

	bundle = mport_bundle_write_new();

	if (mport_bundle_write_init(bundle, extra->pkg_filename) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	/* First step - +CONTENTS.db ALWAYS GOES FIRST!!! */
	(void) snprintf(filename, FILENAME_MAX, "%s/%s", tmpdir, MPORT_STUB_DB_FILE);
	if (mport_bundle_write_add_file(bundle, filename, MPORT_STUB_DB_FILE))
		RETURN_CURRENT_ERROR;

	/* second step - the meta files */
	if (archive_metafiles(bundle, pack, extra) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	/* last step - the real files from the assetlist */
	if (archive_assetlistfiles(bundle, pack, extra, assetlist) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	mport_bundle_write_finish(bundle);

	return MPORT_OK;
}


static int
archive_metafiles(mportBundleWrite *bundle, mportPackageMeta *pack, mportCreateExtras *extra)
{
	char filename[FILENAME_MAX];
	char dir[FILENAME_MAX];

	(void) snprintf(dir, FILENAME_MAX, "%s/%s-%s", MPORT_STUB_INFRA_DIR, pack->name, pack->version);

	if (extra->mtree != NULL && mport_file_exists(extra->mtree)) {
		(void) snprintf(filename, FILENAME_MAX, "%s/%s", dir, MPORT_MTREE_FILE);
		if (mport_bundle_write_add_file(bundle, extra->mtree, filename) != MPORT_OK)
			RETURN_CURRENT_ERROR;
	}

	if (extra->pkginstall != NULL && mport_file_exists(extra->pkginstall)) {
		(void) snprintf(filename, FILENAME_MAX, "%s/%s", dir, MPORT_INSTALL_FILE);
		if (mport_bundle_write_add_file(bundle, extra->pkginstall, filename) != MPORT_OK)
			RETURN_CURRENT_ERROR;
	}

	if (extra->pkgdeinstall != NULL && mport_file_exists(extra->pkgdeinstall)) {
		(void) snprintf(filename, FILENAME_MAX, "%s/%s", dir, MPORT_DEINSTALL_FILE);
		if (mport_bundle_write_add_file(bundle, extra->pkgdeinstall, filename) != MPORT_OK)
			RETURN_CURRENT_ERROR;
	}

	if (extra->pkgmessage != NULL && mport_file_exists(extra->pkgmessage)) {
		(void) snprintf(filename, FILENAME_MAX, "%s/%s", dir, MPORT_MESSAGE_FILE);
		if (mport_bundle_write_add_file(bundle, extra->pkgmessage, filename) != MPORT_OK)
			RETURN_CURRENT_ERROR;
	}

	return MPORT_OK;
}

static int
archive_assetlistfiles(mportBundleWrite *bundle, mportPackageMeta *pack, mportCreateExtras *extra,
                       mportAssetList *assetlist)
{
	mportAssetListEntry *e = NULL;
	char filename[FILENAME_MAX];
	char *cwd = pack->prefix;

	STAILQ_FOREACH(e, assetlist, next)
	{
		if (e->type == ASSET_CWD)
			cwd = e->data == NULL ? pack->prefix : e->data;

		if (e->type != ASSET_FILE && e->type != ASSET_SAMPLE && e->type != ASSET_SAMPLE_OWNER_MODE &&
		    e->type != ASSET_SHELL && e->type != ASSET_FILE_OWNER_MODE) {
			continue;
		}

		/* don't prepend the cwd if the path is abs. */
		if (*(e->data) == '/') {
			(void) snprintf(filename, FILENAME_MAX, "%s%s", extra->sourcedir, e->data);
		} else {
			(void) snprintf(filename, FILENAME_MAX, "%s/%s/%s", extra->sourcedir, cwd, e->data);
		}

		if (e->type == ASSET_SAMPLE || e->type == ASSET_SAMPLE_OWNER_MODE) {
			// eat the second filename if it exists.
			for (int ch = 0; ch < FILENAME_MAX; ch++) {
				if (filename[ch] == '\0')
					break;
				if (filename[ch] == ' ' || filename[ch] == '\t') {
					filename[ch] = '\0';
					break;
				}
			}
		}

		if (mport_bundle_write_add_file(bundle, filename, e->data) != MPORT_OK)
			RETURN_CURRENT_ERROR;
	}

	return MPORT_OK;
}


static int
clean_up(const char *tmpdir)
{
	return mport_rmtree(tmpdir);
}


