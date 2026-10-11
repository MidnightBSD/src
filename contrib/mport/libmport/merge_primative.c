/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2008,2009 Chris Reinhardt
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

#include <archive.h>
#include <archive_entry.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "mport.h"
#include "mport_private.h"

/* build a hashtable with pkgname keys, values are a struct with fields for
 * filename, and boolean is_in_db */
struct table_entry {
	char *file;
	int input; /* index of file among the inputs */
	short is_in_db;
	char *name;
	struct table_entry *next;
};

#define TABLE_SIZE 128

/* the asset types mport.create archives as data files */
#define DATA_ASSET_TYPES_SQL "type IN (%i, %i, %i, %i, %i, %i)"
#define DATA_ASSET_TYPES                                                                       \
	ASSET_FILE, ASSET_SAMPLE, ASSET_SHELL, ASSET_FILE_OWNER_MODE, ASSET_SAMPLE_OWNER_MODE, \
	    ASSET_INFO

static int build_stub_db(
    mportInstance *, sqlite3 **, const char *, const char *, const char **, struct table_entry **);
static int archive_metafiles(mportInstance *, mportBundleWrite *, sqlite3 *, struct table_entry **);
static int archive_package_files(
    mportInstance *, mportBundleWrite *, sqlite3 *, struct table_entry **);
static int count_bundle_data_files(sqlite3 *, struct table_entry **, const char *, int *);
static int extract_stub_db(const char *, const char *);
static int make_temp_outfile(const char *, char *, size_t);
static void finish_input_bundle_preserving_error(
    /*@notnull@*/ mportInstance *, /*@only@*/ /*@notnull@*/ mportBundleRead *, int);

static struct table_entry *find_in_table(struct table_entry **, const char *);
static int insert_into_table(struct table_entry **, const char *, const char *, int);
static uint32_t SuperFastHash(const char *);
static void free_table(struct table_entry **);

static void
free_table(struct table_entry **table)
{
	if (table == NULL)
		return;

	for (int i = 0; i < TABLE_SIZE; i++) {
		struct table_entry *node = table[i];
		while (node != NULL) {
			struct table_entry *next = node->next;
			free(node->name);
			free(node->file);
			free(node);
			node = next;
		}
	}
	free(table);
}

#include <err.h>

/*
 * mport_merge_primative(filenames, outfile)
 *
 * Takes a list of bundle filenames and an output filename.  This function will create
 * a new bundle file containing all the packages un the different input bundle files,
 * named `outfile`.  Care is taken to not have duplicates, to ensure that the exterior
 * dependencies are correct, and that the packages are in an optimal order for installation.
 */
MPORT_PUBLIC_API int
mport_merge_primative(mportInstance *mport, const char **filenames, const char *outfile)
{
	sqlite3 *db = NULL;
	mportBundleWrite *bundle = NULL;
	struct table_entry **table = NULL;
	char *dbfile = NULL;
	char dirtmpl[MAXPATHLEN];
	char *tmpdir;
	char *madedir = NULL;
	char tmpout[MAXPATHLEN];
	bool made_tmpout = false;
	int ret = MPORT_OK;

	tmpdir = getenv("TMPDIR");
	if (tmpdir == NULL)
		tmpdir = "/tmp";

	strlcpy(dirtmpl, tmpdir, sizeof(dirtmpl));
	strlcat(dirtmpl, "/mport.XXXXXXXX", sizeof(dirtmpl));

	if ((table = (struct table_entry **)calloc(TABLE_SIZE, sizeof(struct table_entry *))) ==
	    NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Couldn't allocate hash table.");

	DIAG("mport_merge_primative(%p, %s)", filenames, outfile)

	if ((tmpdir = mkdtemp(dirtmpl)) == NULL) {
		ret = SET_ERROR(MPORT_ERR_FATAL, "Couldn't make temp directory.");
		goto DONE;
	}
	/* only this created directory is safe to remove later (not $TMPDIR) */
	madedir = tmpdir;
	if (asprintf(&dbfile, "%s/%s", tmpdir, "merged.db") == -1) {
		dbfile = NULL;
		ret = SET_ERROR(MPORT_ERR_FATAL, "Couldn't build merge database name.");
		goto DONE;
	}

	DIAG("Building stub")

	/* this function merges the stub databases into one db. */
	if (build_stub_db(mport, &db, tmpdir, dbfile, filenames, table) != MPORT_OK) {
		ret = mport_err_code();
		goto DONE;
	}

	DIAG("Stub complete: %s", dbfile)

	/* set up the bundle, and add our new stub database to it. */
	if ((bundle = mport_bundle_write_new()) == NULL) {
		ret = SET_ERROR(MPORT_ERR_FATAL, "Couldn't alloca bundle struct.");
		goto DONE;
	}
	/* build the bundle beside outfile and rename it into place when it is
	 * complete, so a failed or concurrent merge never exposes a partial
	 * bundle or removes an existing one */
	if (make_temp_outfile(outfile, tmpout, sizeof(tmpout)) != MPORT_OK) {
		ret = mport_err_code();
		goto DONE;
	}
	made_tmpout = true;
	if (mport_bundle_write_init(bundle, tmpout) != MPORT_OK) {
		ret = mport_err_code();
		goto DONE;
	}

	DIAG("Adding %s", dbfile)

	if (mport_bundle_write_add_file(bundle, dbfile, MPORT_STUB_DB_FILE) != MPORT_OK) {
		ret = mport_err_code();
		goto DONE;
	}

	DIAG("Adding metafiles")
	/* add all the meta files in the correct order */
	if (archive_metafiles(mport, bundle, db, table) != MPORT_OK) {
		ret = mport_err_code();
		goto DONE;
	}

	DIAG("Adding realfiles")
	/* add all the other files */
	if (archive_package_files(mport, bundle, db, table) != MPORT_OK) {
		ret = mport_err_code();
		goto DONE;
	}

	DIAG("Realfiles complete")

	ret = mport_bundle_write_finish(bundle);
	bundle = NULL; /* finish frees the bundle on success and failure */

	if (ret == MPORT_OK && rename(tmpout, outfile) != 0)
		ret = SET_ERRORX(MPORT_ERR_FATAL, "Couldn't rename %s to %s: %s", tmpout, outfile,
		    strerror(errno));
	else if (ret == MPORT_OK)
		made_tmpout = false;

DONE:
	if (bundle != NULL)
		(void)mport_bundle_write_finish(bundle);
	if (db != NULL)
		sqlite3_close(db);
	free_table(table);
	free(dbfile);
	/* don't leave a truncated bundle behind for someone to install */
	if (made_tmpout)
		(void)unlink(tmpout);
	/* attempt removal of the created tmpdir, ignore errors. */
	if (madedir != NULL)
		mport_rmtree(madedir);

	return ret;
}

/* This function goes through each file, and builds up the merged database as
 * filename `dbfile`.  It also builds up the hashtable of package -> filename pairs.
 * When this function is done, db points to a readonly sqlite object representing
 * the merged db.
 */
static int
build_stub_db(mportInstance *mport, sqlite3 **db, const char *tmpdir, const char *dbfile,
    const char **filenames, struct table_entry **table)
{
	char tmpdbfile[MAXPATHLEN];
	const char *name;
	const char *file = NULL;
	struct table_entry *dup;
	int made_table = 0, ret, input;
	bool same, skip;
	sqlite3_stmt *stmt;

	if (snprintf(tmpdbfile, sizeof(tmpdbfile), "%s/%s", tmpdir, "pkg.db") >=
	    (int)sizeof(tmpdbfile))
		RETURN_ERROR(MPORT_ERR_FATAL, "Couldn't make stub db tempfile.");

	if (sqlite3_open(dbfile, db) != SQLITE_OK)
		RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(*db));

	/* the stub databases ATTACHed below come from the bundles being merged */
	if (mport_db_harden(*db) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	if (mport_generate_stub_schema(mport, *db) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	for (input = 0, file = *filenames; file != NULL; input++, file = *(++filenames)) {
		DIAG("Visiting %s", file)
		if (extract_stub_db(file, tmpdbfile) != MPORT_OK)
			RETURN_CURRENT_ERROR;

		if (mport_db_do(*db, "ATTACH %Q AS subbundle", tmpdbfile) != MPORT_OK)
			RETURN_CURRENT_ERROR;

		if (mport_db_do(*db, "BEGIN TRANSACTION") != MPORT_OK)
			RETURN_CURRENT_ERROR;

		if (made_table == 0) {
			made_table++;
			if (mport_db_do(
				*db, "CREATE TABLE unsorted AS SELECT * FROM subbundle.packages") !=
			    MPORT_OK)
				goto rollback;
		} else {
			if (mport_db_do(
				*db, "INSERT INTO unsorted SELECT * FROM subbundle.packages") !=
			    MPORT_OK)
				goto rollback;
		}

		if (mport_db_do(*db, "INSERT INTO assets SELECT * FROM subbundle.assets") !=
		    MPORT_OK)
			goto rollback;
		if (mport_db_do(*db, "INSERT INTO conflicts SELECT * FROM subbundle.conflicts") !=
		    MPORT_OK)
			goto rollback;
		if (mport_db_do(*db, "INSERT INTO depends SELECT * FROM subbundle.depends") !=
		    MPORT_OK)
			goto rollback;

		/* build our hashtable (pkgname => metadata) up */
		if (mport_db_prepare(*db, &stmt, "SELECT pkg FROM subbundle.packages") !=
		    MPORT_OK) {
			sqlite3_finalize(stmt);
			goto rollback;
		}

		skip = false;
		while (1) {
			ret = sqlite3_step(stmt);

			if (ret == SQLITE_ROW) {
				name = sqlite3_column_text(stmt, 0);
				if ((dup = find_in_table(table, name)) != NULL) {
					/* a byte-identical copy of an earlier input adds
					 * nothing; its first package is already a dup */
					if (dup->input != input) {
						if (mport_same_file_contents(
							dup->file, file, &same) != MPORT_OK) {
							sqlite3_finalize(stmt);
							goto rollback;
						}
						if (same) {
							DIAG("Skipping %s, identical to %s", file,
							    dup->file)
							skip = true;
							break;
						}
					}
					/* its assets would be merged twice; refuse rather
					 * than pick one */
					SET_ERRORX(MPORT_ERR_FATAL,
					    "Package %s is in both %s and %s", name, dup->file,
					    file);
					sqlite3_finalize(stmt);
					goto rollback;
				}
				if (insert_into_table(table, name, file, input) != MPORT_OK) {
					sqlite3_finalize(stmt);
					goto rollback;
				}
			} else if (ret == SQLITE_DONE) {
				break;
			} else {
				SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(*db));
				sqlite3_finalize(stmt);
				goto rollback;
			}
		}

		sqlite3_finalize(stmt);

		if (skip) {
			if (mport_db_do(*db, "ROLLBACK TRANSACTION") != MPORT_OK)
				RETURN_CURRENT_ERROR;
			if (mport_db_do(*db, "DETACH subbundle") != MPORT_OK)
				RETURN_CURRENT_ERROR;
			continue;
		}

		if (mport_db_do(*db, "COMMIT TRANSACTION") != MPORT_OK)
			goto rollback;
		if (mport_db_do(*db, "DETACH subbundle") != MPORT_OK)
			RETURN_CURRENT_ERROR;
		continue;

	rollback:
		/* sqlite3_exec directly so the rollback cannot clobber the error */
		(void)sqlite3_exec(*db, "ROLLBACK", NULL, NULL, NULL);
		RETURN_CURRENT_ERROR;
	}

	/* just have to sort the packages (going from unsorted to packages), no big deal... ;) */
	while (1) {
		if (mport_db_do(*db,
			"INSERT INTO packages SELECT * FROM unsorted WHERE NOT EXISTS (SELECT 1 FROM packages WHERE packages.pkg=unsorted.pkg) AND (NOT EXISTS (SELECT 1 FROM depends WHERE depends.pkg=unsorted.pkg) OR NOT EXISTS (SELECT 1 FROM depends LEFT JOIN packages ON depends.depend_pkgname=packages.pkg WHERE depends.pkg=unsorted.pkg AND EXISTS (SELECT 1 FROM unsorted AS us2 WHERE us2.pkg=depend_pkgname) AND packages.pkg ISNULL))") !=
		    MPORT_OK)
			RETURN_CURRENT_ERROR;
		if (sqlite3_changes(*db) == 0) /* if there is nothing left to insert, we're done */
			break;
	}

	/* Check that unsorted and packages have the same number of rows. */
	if (mport_db_prepare(*db, &stmt, "SELECT COUNT(DISTINCT pkg) FROM packages")) {
		sqlite3_finalize(stmt);
		RETURN_CURRENT_ERROR;
	}
	if (sqlite3_step(stmt) != SQLITE_ROW) {
		SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(*db));
		sqlite3_finalize(stmt);
		RETURN_CURRENT_ERROR;
	}

	int pkgs = sqlite3_column_int(stmt, 0);
	sqlite3_finalize(stmt);

	if (mport_db_prepare(*db, &stmt, "SELECT COUNT(DISTINCT pkg) FROM unsorted")) {
		sqlite3_finalize(stmt);
		RETURN_CURRENT_ERROR;
	}

	if (sqlite3_step(stmt) != SQLITE_ROW) {
		SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(*db));
		sqlite3_finalize(stmt);
		RETURN_CURRENT_ERROR;
	}

	int unsort = sqlite3_column_int(stmt, 0);
	sqlite3_finalize(stmt);

	if (pkgs != unsort)
		RETURN_ERRORX(MPORT_ERR_FATAL, "Sorted (%i) and unsorted (%i) counts do no match.",
		    pkgs, unsort);

	/* Close the stub database handle, and reopen as read only to ensure that we don't
	 * try to change it after this point
	 */
	if (sqlite3_close(*db) != SQLITE_OK)
		RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(*db));
	if (sqlite3_open_v2(dbfile, db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK)
		RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(*db));
	if (mport_db_harden(*db) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	return MPORT_OK;
}

/* Closing a failed input bundle can replace the error that caused the merge
 * to stop.  Keep the first error for the caller. */
static void
finish_input_bundle_preserving_error(/*@notnull@*/ mportInstance *mport,
    /*@only@*/ /*@notnull@*/ mportBundleRead *bundle, int error_code)
/*@requires error_code != MPORT_OK @*/
{
	char original_error[MPORT_ERROR_MESSAGE_MAX];

	(void)strlcpy(original_error, mport_err_string(), sizeof(original_error));
	(void)mport_bundle_read_finish(mport, bundle);
	(void)mport_set_err(error_code, original_error);
}

static int
archive_metafiles(
    mportInstance *mport, mportBundleWrite *bundle, sqlite3 *db, struct table_entry **table)
{
	sqlite3_stmt *stmt = NULL;
	int ret = MPORT_OK, sret;
	char *filename;
	const char *pkgname;
	const char *path;
	struct table_entry *match = NULL;
	mportBundleRead *inbundle = NULL;
	struct archive_entry *entry;

	if (mport_db_prepare(db, &stmt, "SELECT pkg FROM packages") != MPORT_OK) {
		sqlite3_finalize(stmt);
		RETURN_CURRENT_ERROR;
	}

	while ((sret = sqlite3_step(stmt)) == SQLITE_ROW) {
		pkgname = sqlite3_column_text(stmt, 0);
		match = find_in_table(table, pkgname);

		if (match == NULL) {
			ret = SET_ERRORX(MPORT_ERR_FATAL,
			    "Couldn't find package '%s' in filename table.", pkgname);
			goto DONE;
		}

		filename = match->file;

		if ((inbundle = mport_bundle_read_new()) == NULL) {
			ret = SET_ERROR(MPORT_ERR_FATAL, "Couldn't allocate bundle");
			goto DONE;
		}

		if (mport_bundle_read_init(inbundle, filename) != MPORT_OK) {
			ret = mport_err_code();
			goto DONE;
		}

		/* skip the sub db */
		if (mport_bundle_read_next_entry(inbundle, &entry) != MPORT_OK) {
			ret = mport_err_code();
			goto DONE;
		}
		if (entry == NULL) {
			ret = SET_ERRORX(
			    MPORT_ERR_FATAL, "Corrupt bundle %s: archive is empty", filename);
			goto DONE;
		}
		if (archive_read_data_skip(inbundle->archive) != ARCHIVE_OK) {
			ret = SET_ERRORX(MPORT_ERR_FATAL, "Unable to read %s: %s", filename,
			    archive_error_string(inbundle->archive));
			goto DONE;
		}

		while (1) {
			if (mport_bundle_read_next_entry(inbundle, &entry) != MPORT_OK) {
				ret = mport_err_code();
				goto DONE;
			}

			/* a package without data files ends after its metafiles */
			if (entry == NULL)
				break;
			path = archive_entry_pathname(entry);
			if (path == NULL || *path != '+')
				break;

			DIAG("Adding %s", path)

			if (mport_bundle_write_add_entry(bundle, inbundle, entry) != MPORT_OK) {
				DIAG("bundle add entry failed")
				ret = mport_err_code();
				goto DONE;
			}
		}

		ret = mport_bundle_read_finish(mport, inbundle);
		inbundle = NULL;
		if (ret != MPORT_OK)
			goto DONE;
	}

	if (sret != SQLITE_DONE)
		ret = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));

DONE:
	if (inbundle != NULL)
		finish_input_bundle_preserving_error(mport, inbundle, ret);
	sqlite3_finalize(stmt);
	return ret;
}

static int
archive_package_files(
    mportInstance *mport, mportBundleWrite *bundle, sqlite3 *db, struct table_entry **table)
{
	sqlite3_stmt *stmt = NULL, *files = NULL;
	int ret = MPORT_OK, sret, fret, own, total;
	struct table_entry *cur;
	const char *pkgname;
	const char *file;
	const char *path;
	mportBundleRead *inbundle = NULL;
	struct archive_entry *entry;

	if (mport_db_prepare(db, &stmt, "SELECT pkg FROM packages") != MPORT_OK) {
		sqlite3_finalize(stmt);
		RETURN_CURRENT_ERROR;
	}

	while ((sret = sqlite3_step(stmt)) == SQLITE_ROW) {
		pkgname = sqlite3_column_text(stmt, 0);
		cur = find_in_table(table, pkgname);

		if (cur == NULL) {
			ret = SET_ERRORX(MPORT_ERR_FATAL,
			    "Couldn't find package '%s' in bundle hash table", pkgname);
			goto DONE;
		}

		/* the same asset types mport.create archives, in the same order */
		if (mport_db_prepare(db, &files,
			"SELECT data FROM assets WHERE pkg=%Q AND " DATA_ASSET_TYPES_SQL, pkgname,
			DATA_ASSET_TYPES) != MPORT_OK) {
			ret = mport_err_code();
			goto DONE;
		}

		/* an input bundle can hold several packages (e.g. an earlier
		 * merge's output); count the data files of all of them */
		if (count_bundle_data_files(db, table, cur->file, &total) != MPORT_OK) {
			ret = mport_err_code();
			goto DONE;
		}

		/* read the bundle even for a package without data files (e.g. a
		 * meta-port), so a data file without an asset row is caught */
		if ((inbundle = mport_bundle_read_new()) == NULL) {
			ret = SET_ERROR(MPORT_ERR_FATAL, "Out of memory.");
			goto DONE;
		}

		if (mport_bundle_read_init(inbundle, cur->file) != MPORT_OK) {
			ret = mport_err_code();
			goto DONE;
		}

		/* skip the stub database and metafiles; entry is the first data
		 * file, or NULL at the end of the archive */
		do {
			if (mport_bundle_read_next_entry(inbundle, &entry) != MPORT_OK) {
				ret = mport_err_code();
				goto DONE;
			}
			path = entry == NULL ? NULL : archive_entry_pathname(entry);
		} while (path != NULL && *path == '+');

		own = 0;
		while ((fret = sqlite3_step(files)) == SQLITE_ROW) {
			file = sqlite3_column_text(files, 0);

			if (entry == NULL) {
				ret = SET_ERRORX(MPORT_ERR_FATAL,
				    "Corrupt bundle %s: archive ends before '%s'", cur->file, file);
				goto DONE;
			}

			path = archive_entry_pathname(entry);
			if (file == NULL || path == NULL || strcmp(file, path) != 0) {
				ret = SET_ERRORX(MPORT_ERR_FATAL,
				    "Plist to archive mismatch in package %s: found '%s', expected '%s'",
				    pkgname, path == NULL ? "(null)" : path,
				    file == NULL ? "(null)" : file);
				goto DONE;
			}

			DIAG("Adding realfile: %s", path);

			if (mport_bundle_write_add_entry(bundle, inbundle, entry) != MPORT_OK) {
				ret = mport_err_code();
				goto DONE;
			}
			own++;

			if (mport_bundle_read_next_entry(inbundle, &entry) != MPORT_OK) {
				ret = mport_err_code();
				goto DONE;
			}
		}

		if (fret != SQLITE_DONE) {
			ret = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
			goto DONE;
		}

		/* the rest belong to the other packages in this bundle, which
		 * are archived on their own turn */
		for (int i = own; i < total; i++) {
			if (entry == NULL) {
				ret = SET_ERRORX(MPORT_ERR_FATAL,
				    "Corrupt bundle %s: archive ends early", cur->file);
				goto DONE;
			}
			if (mport_bundle_read_next_entry(inbundle, &entry) != MPORT_OK) {
				ret = mport_err_code();
				goto DONE;
			}
		}

		/* every data file must belong to an asset row, or it would be
		 * silently left out of the merged bundle */
		if (entry != NULL) {
			path = archive_entry_pathname(entry);
			ret = SET_ERRORX(MPORT_ERR_FATAL, "Unexpected file '%s' in %s",
			    path == NULL ? "(null)" : path, cur->file);
			goto DONE;
		}

		ret = mport_bundle_read_finish(mport, inbundle);
		inbundle = NULL;
		if (ret != MPORT_OK)
			goto DONE;

		/* we're done with this package, onto the next one */
		sqlite3_finalize(files);
		files = NULL;
	}

	if (sret != SQLITE_DONE)
		ret = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));

DONE:
	if (inbundle != NULL)
		finish_input_bundle_preserving_error(mport, inbundle, ret);
	sqlite3_finalize(files);
	sqlite3_finalize(stmt);
	return ret;
}

/* set count to the number of data file asset rows of the packages that came
 * from the input bundle file */
static int
count_bundle_data_files(sqlite3 *db, struct table_entry **table, const char *file, int *count)
{
	sqlite3_stmt *stmt = NULL;
	struct table_entry *e;

	*count = 0;
	for (int i = 0; i < TABLE_SIZE; i++) {
		for (e = table[i]; e != NULL; e = e->next) {
			if (strcmp(e->file, file) != 0)
				continue;

			if (mport_db_prepare(db, &stmt,
				"SELECT COUNT(*) FROM assets WHERE pkg=%Q AND " DATA_ASSET_TYPES_SQL,
				e->name, DATA_ASSET_TYPES) != MPORT_OK) {
				sqlite3_finalize(stmt);
				RETURN_CURRENT_ERROR;
			}
			if (sqlite3_step(stmt) != SQLITE_ROW) {
				SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
				sqlite3_finalize(stmt);
				RETURN_CURRENT_ERROR;
			}
			*count += sqlite3_column_int(stmt, 0);
			sqlite3_finalize(stmt);
			stmt = NULL;
		}
	}

	return MPORT_OK;
}

/* create an empty, uniquely named file next to outfile and put its name in
 * tmpout; created like the final bundle would be (0666 less the umask) */
static int
make_temp_outfile(const char *outfile, char *tmpout, size_t len)
{
	int fd;

	for (int tries = 0; tries < 100; tries++) {
		if (snprintf(tmpout, len, "%s.%08x", outfile, arc4random()) >= (int)len)
			RETURN_ERRORX(MPORT_ERR_FATAL, "Output filename too long: %s", outfile);

		if ((fd = open(tmpout, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0666)) != -1) {
			(void)close(fd);
			return MPORT_OK;
		}
		if (errno != EEXIST)
			RETURN_ERRORX(
			    MPORT_ERR_FATAL, "Couldn't create %s: %s", tmpout, strerror(errno));
	}

	RETURN_ERRORX(MPORT_ERR_FATAL, "Couldn't create a temporary file next to %s", outfile);
}

/* get the stub database file out of filename and place it at destfile */
static int
extract_stub_db(const char *filename, const char *destfile)
{
	struct archive *a = archive_read_new();
	struct archive_entry *entry;
	/*@observer@*/ /*@null@*/ const char *path;

	if (a == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Couldn't allocate read archive struct");

	if (archive_read_support_format_tar(a) != ARCHIVE_OK ||
	    archive_read_support_filter_xz(a) != ARCHIVE_OK) {
		SET_ERROR(MPORT_ERR_FATAL, archive_error_string(a));
		archive_read_free(a);
		RETURN_CURRENT_ERROR;
	}

	if (archive_read_open_filename(a, filename, 10240) != ARCHIVE_OK) {
		SET_ERRORX(
		    MPORT_ERR_FATAL, "Could not open %s: %s", filename, archive_error_string(a));
		archive_read_free(a);
		RETURN_CURRENT_ERROR;
	}

	if (archive_read_next_header(a, &entry) != ARCHIVE_OK) {
		SET_ERROR(MPORT_ERR_FATAL, archive_error_string(a));
		archive_read_free(a);
		RETURN_CURRENT_ERROR;
	}

	path = archive_entry_pathname(entry);
	if (path == NULL || strcmp(path, MPORT_STUB_DB_FILE) != 0) {
		archive_read_free(a);
		RETURN_ERROR(
		    MPORT_ERR_FATAL, "Invalid bundle file: stub database is not the first file");
	}

	/* a link here would make the merge read whatever file it points at */
	if (archive_entry_filetype(entry) != AE_IFREG || archive_entry_hardlink(entry) != NULL) {
		archive_read_free(a);
		RETURN_ERRORX(MPORT_ERR_FATAL,
		    "Invalid bundle file %s: stub database is not a regular file", filename);
	}

	archive_entry_set_pathname(entry, destfile);

	if (archive_read_extract(a, entry, 0) != ARCHIVE_OK) {
		SET_ERROR(MPORT_ERR_FATAL, archive_error_string(a));
		archive_read_free(a);
		RETURN_CURRENT_ERROR;
	}

	/* a's error string is gone once it is freed */
	if (archive_read_free(a) != ARCHIVE_OK)
		RETURN_ERRORX(MPORT_ERR_FATAL, "Couldn't close %s", filename);

	return MPORT_OK;
}

/* insert into a name => file pair into the given hash table. */
static int
insert_into_table(struct table_entry **table, const char *name, const char *file, int input)
{
	struct table_entry *node, *cur;
	int hash = SuperFastHash(name) % TABLE_SIZE;

	if ((node = (struct table_entry *)calloc(1, sizeof(struct table_entry))) == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Couldn't allocate table entry");

	node->name = strdup(name);
	node->file = strdup(file);
	node->input = input;
	node->is_in_db = 0;
	node->next = NULL;

	if (table[hash] == NULL) {
		table[hash] = node;
	} else {
		cur = table[hash];
		while (cur->next != NULL)
			cur = cur->next;

		cur->next = node;
	}

	return MPORT_OK;
}

static struct table_entry *
find_in_table(struct table_entry **table, const char *name)
{
	int hash = SuperFastHash(name) % TABLE_SIZE;
	struct table_entry *e = NULL;

	e = table[hash];
	while (e != NULL) {
		if (strcmp(e->name, name) == 0)
			return e;

		e = e->next;
	}

	return e;
}

/* Paul Hsieh's fast hash function, from http://www.azillionmonkeys.com/qed/hash.html */
/* This function has been modified to only work with C strings */
#undef get16bits
#if (defined(__GNUC__) && defined(__i386__)) || defined(__WATCOMC__) || defined(_MSC_VER) || \
    defined(__BORLANDC__) || defined(__TURBOC__)
#define get16bits(d) (*((const uint16_t *)(d)))
#endif

#if !defined(get16bits)
#define get16bits(d) \
	((((uint32_t)(((const uint8_t *)(d))[1])) << 8) + (uint32_t)(((const uint8_t *)(d))[0]))
#endif

static uint32_t
SuperFastHash(const char *data)
{
	if (data == NULL)
		return 0;

	int len = strlen(data);
	uint32_t hash = len, tmp;
	int rem;

	if (len <= 0)
		return 0;

	rem = len & 3;
	len >>= 2;

	/* Main loop */
	for (; len > 0; len--) {
		hash += get16bits(data);
		tmp = (get16bits(data + 2) << 11) ^ hash;
		hash = (hash << 16) ^ tmp;
		data += 2 * sizeof(uint16_t);
		hash += hash >> 11;
	}

	/* Handle end cases */
	switch (rem) {
	case 3:
		hash += get16bits(data);
		hash ^= hash << 16;
		hash ^= data[sizeof(uint16_t)] << 18;
		hash += hash >> 11;
		break;
	case 2:
		hash += get16bits(data);
		hash ^= hash << 11;
		hash += hash >> 17;
		break;
	case 1:
		hash += *data;
		hash ^= hash << 10;
		hash += hash >> 1;
	}

	/* Force "avalanching" of final 127 bits */
	hash ^= hash << 3;
	hash += hash >> 5;
	hash ^= hash << 4;
	hash += hash >> 17;
	hash ^= hash << 25;
	hash += hash >> 6;

	return hash;
}
