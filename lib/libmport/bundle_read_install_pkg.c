/*-
 * Copyright (c) 2013-2015 Lucas Holt
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
__MBSDID("$MidnightBSD$");

#include "mport.h"
#include "mport_private.h"

#include <sys/stat.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <stdarg.h>
#include <archive_entry.h>


enum phase { PREINSTALL, ACTUALINSTALL, POSTINSTALL};

static int do_pre_install(mportInstance *, mportBundleRead *, mportPackageMeta *);
static int do_actual_install(mportInstance *, mportBundleRead *, mportPackageMeta *);
static int do_post_install(mportInstance *, mportBundleRead *, mportPackageMeta *);
static int run_postexec(mportInstance *, mportPackageMeta *);
static int run_pkg_install(mportInstance *, mportBundleRead *, mportPackageMeta *, const char *);
static int run_mtree(mportInstance *, mportBundleRead *, mportPackageMeta *);
static int display_pkg_msg(mportInstance *, mportBundleRead *, mportPackageMeta *);
static int get_file_count(mportInstance *, char *, int *);
static int create_package_row(mportInstance *, mportPackageMeta *);
static int create_categories(mportInstance *mport, mportPackageMeta *pkg);
static int create_depends(mportInstance *mport, mportPackageMeta *pkg);
static int create_sample_file(mportInstance *mport, char *cwd, const char *file);
static char** parse_sample(char *input);
static int mark_complete(mportInstance *, mportPackageMeta *);
static int mport_bundle_read_get_assetlist(mportInstance *mport, mportPackageMeta *pkg, mportAssetList **alist_p, enum phase);

/**
 * This is a wrapper for all bund read install operations
 */
int
mport_bundle_read_install_pkg(mportInstance *mport, mportBundleRead *bundle, mportPackageMeta *pkg)
{
	if (do_pre_install(mport, bundle, pkg) != MPORT_OK) {
		RETURN_CURRENT_ERROR;
	}

	if (do_actual_install(mport, bundle, pkg) != MPORT_OK) {
		RETURN_CURRENT_ERROR;
	}

	if (do_post_install(mport, bundle, pkg) != MPORT_OK) {
		RETURN_CURRENT_ERROR;
	}

	syslog(LOG_NOTICE, "%s-%s installed", pkg->name, pkg->version);

	return MPORT_OK;
}  


/* This does everything that has to happen before we start installing files.
 * We run mtree, pkg-install PRE-INSTALL, etc... 
 */
static int
do_pre_install(mportInstance *mport, mportBundleRead *bundle, mportPackageMeta *pkg)
{
	char cwd[FILENAME_MAX];
	char file[FILENAME_MAX];
	mportAssetList *alist;
	mportAssetListEntry *e = NULL;

	/* run mtree */
	if (run_mtree(mport, bundle, pkg) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	/* run pkg-install PRE-INSTALL */
	if (run_pkg_install(mport, bundle, pkg, "PRE-INSTALL") != MPORT_OK)
		RETURN_CURRENT_ERROR;

	/* Process @preexec steps */
	if (mport_bundle_read_get_assetlist(mport, pkg, &alist, PREINSTALL) != MPORT_OK)
		goto ERROR;

	(void) strlcpy(cwd, pkg->prefix, sizeof(cwd));

	if (mport_chdir(mport, cwd) != MPORT_OK)
		goto ERROR;

	STAILQ_FOREACH(e, alist, next) {
		switch (e->type) {
			case ASSET_CWD:
				(void) strlcpy(cwd, e->data == NULL ? pkg->prefix : e->data, sizeof(cwd));
				if (mport_chdir(mport, cwd) != MPORT_OK)
					goto ERROR;

				break;
			case ASSET_PREEXEC:
				if (mport_run_asset_exec(mport, e->data, cwd, file) != MPORT_OK)
					goto ERROR;
				break;
			default:
				/* do nothing */
				break;
		}
	}

	mport_assetlist_free(alist);
	mport_pkgmeta_logevent(mport, pkg, "preexec");

	return MPORT_OK;

	ERROR:
	// TODO: asset list free
	RETURN_CURRENT_ERROR;
}

/* get the file count for the progress meter */
static int
get_file_count(mportInstance *mport, char *pkg_name, int *file_total)
{
	__block sqlite3_stmt *count;
	__block int result = MPORT_OK;
	__block char *err;

	if (mport_db_prepare(mport->db, &count,
	                     "SELECT COUNT(*) FROM stub.assets WHERE (type=%i or type=%i or type=%i or type=%i or type=%i) AND pkg=%Q",
	                     ASSET_FILE, ASSET_SAMPLE, ASSET_SHELL, ASSET_FILE_OWNER_MODE, ASSET_SAMPLE_OWNER_MODE,
                             pkg_name) != MPORT_OK) {
		sqlite3_finalize(count);
		RETURN_CURRENT_ERROR;
	}

	dispatch_sync(mportSQLSerial, ^{
	    switch (sqlite3_step(count)) {
		    case SQLITE_ROW:
			    *file_total = sqlite3_column_int(count, 0);
			    sqlite3_finalize(count);
			    break;
		    default:
			    err = (char *) sqlite3_errmsg(mport->db);
			    result = MPORT_ERR_FATAL;
			    sqlite3_finalize(count);
	    }
	});

	if (result == MPORT_ERR_FATAL)
		SET_ERRORX(result, "Error reading file count %s", err);
	return result;
}

static int
create_package_row(mportInstance *mport, mportPackageMeta *pkg)
{
	/* Insert the package meta row into the packages table (We use pack here because things might have been twiddled) */
	/* Note that this will be marked as dirty by default */
	if (mport_db_do(mport->db,
	                "INSERT INTO packages (pkg, version, origin, prefix, lang, options, comment, os_release, cpe, locked, deprecated, expiration_date, no_provide_shlib, flavor) VALUES (%Q,%Q,%Q,%Q,%Q,%Q,%Q,%Q,%Q,0,%Q,%ld,%d,%Q)",
	                pkg->name, pkg->version, pkg->origin, pkg->prefix, pkg->lang, pkg->options, pkg->comment,
	                pkg->os_release, pkg->cpe, pkg->deprecated, pkg->expiration_date, pkg->no_provide_shlib, pkg->flavor) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	return MPORT_OK;
}

static int
create_depends(mportInstance *mport, mportPackageMeta *pkg)
{
	/* Insert the depends into the master table */
	if (mport_db_do(mport->db,
	                "INSERT INTO depends (pkg, depend_pkgname, depend_pkgversion, depend_port) SELECT pkg,depend_pkgname,depend_pkgversion,depend_port FROM stub.depends WHERE pkg=%Q",
	                pkg->name) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	return MPORT_OK;
}

static int
create_categories(mportInstance *mport, mportPackageMeta *pkg)
{
	/* Insert the categories into the master table */
	if (mport_db_do(mport->db,
	                "INSERT INTO categories (pkg, category) SELECT pkg, category FROM stub.categories WHERE pkg=%Q",
	                pkg->name) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	return MPORT_OK;
}

static char**
parse_sample(char *input)
{
	char **ap, **argv;
	argv = calloc(3, sizeof(char *));

	if (argv == NULL)
		return NULL;

	for (ap = argv; (*ap = strsep(&input, " \t")) != NULL;) {
		if (**ap != '\0') {
			if (++ap >= &argv[3])
				break;
		}
	}

	return argv;
}

static int
create_sample_file(mportInstance *mport, char *cwd, const char *file)
{
	char nonSample[FILENAME_MAX * 2];
	char secondFile[FILENAME_MAX];

	if (file[0] != '/')
		(void) snprintf(nonSample, FILENAME_MAX, "%s%s/%s", mport->root, cwd, file);
	else
		strlcpy(nonSample, file, FILENAME_MAX * 2);
	char** fileargv = parse_sample(nonSample);

	if (fileargv[1] != '\0') {
		if (fileargv[1][0] == '/')
			strlcpy(secondFile, fileargv[1], FILENAME_MAX);
		else
			(void) snprintf(secondFile, FILENAME_MAX, "%s%s/%s", mport->root, cwd, fileargv[1]);

		if (!mport_file_exists(secondFile)) {
			if (mport_copy_file(fileargv[0], secondFile) != MPORT_OK)
				RETURN_CURRENT_ERROR;
		}
	} else {
		/* single file */
		char *sptr = strcasestr(nonSample, ".sample");
		if (sptr != NULL) {
			sptr[0] = '\0'; /* hack off .sample */
			if (!mport_file_exists(nonSample)) {
				if (mport_copy_file(file, nonSample) != MPORT_OK)
					RETURN_CURRENT_ERROR;
			}
		}
	}

	free(fileargv);

	return MPORT_OK;
}

/**
 * Get the list of assets (plist entries) from the stub attached database (package we are installing)
 * filtered on entries that are not pre/post exec.
 */
static int
mport_bundle_read_get_assetlist(mportInstance *mport, mportPackageMeta *pkg, mportAssetList **alist_p, enum phase state)
{
	__block mportAssetList *alist;
	__block sqlite3_stmt *stmt = NULL;
	__block int result = MPORT_OK;
	__block char *err;

	if ((alist = mport_assetlist_new()) == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");

	*alist_p = alist;

	if (state == PREINSTALL) {
		if (mport_db_prepare(mport->db, &stmt,
		                     "SELECT type,data,checksum,owner,grp,mode FROM stub.assets WHERE pkg=%Q and type in (%d, %d)",
		                     pkg->name, ASSET_CWD, ASSET_PREEXEC) != MPORT_OK) {
			sqlite3_finalize(stmt);
			RETURN_CURRENT_ERROR;
		}
	} else if (state == ACTUALINSTALL) {
		if (mport_db_prepare(mport->db, &stmt,
		                     "SELECT type,data,checksum,owner,grp,mode FROM stub.assets WHERE pkg=%Q and type not in (%d, %d)",
		                     pkg->name, ASSET_PREEXEC, ASSET_POSTEXEC) != MPORT_OK) {
			sqlite3_finalize(stmt);
			RETURN_CURRENT_ERROR;
		}
	} else if (state == POSTINSTALL) {
		if (mport_db_prepare(mport->db, &stmt,
		                     "SELECT type,data,checksum,owner,grp,mode FROM stub.assets WHERE pkg=%Q and type in (%d, %d)",
		                     pkg->name, ASSET_CWD, ASSET_POSTEXEC) != MPORT_OK) {
			sqlite3_finalize(stmt);
			RETURN_CURRENT_ERROR;
		}
	}

	if (stmt == NULL) {
		RETURN_ERROR(MPORT_ERR_FATAL, "Statement was null");
	}

	dispatch_sync(mportSQLSerial, ^{
	    while (1) {
		    mportAssetListEntry *e;

		    int ret = sqlite3_step(stmt);

		    if (ret == SQLITE_BUSY || ret == SQLITE_LOCKED) {
			    sleep(1);
			    ret = sqlite3_step(stmt);
		    }

		    if (ret == SQLITE_DONE)
			    break;

		    if (ret != SQLITE_ROW) {
			    err = (char *) sqlite3_errmsg(mport->db);
			    result = MPORT_ERR_FATAL;
			    break; // we finalize below
		    }

		    e = (mportAssetListEntry *) calloc(1, sizeof(mportAssetListEntry));

		    if (e == NULL) {
			    err = "Out of memory";
			    result = MPORT_ERR_FATAL;
			    break; // we finalize below
		    }

		    e->type = (mportAssetListEntryType) sqlite3_column_int(stmt, 0);
		    const unsigned char *data = sqlite3_column_text(stmt, 1);
		    const unsigned char *checksum = sqlite3_column_text(stmt, 2);
		    const unsigned char *owner = sqlite3_column_text(stmt, 3);
		    const unsigned char *group = sqlite3_column_text(stmt, 4);
		    const unsigned char *mode = sqlite3_column_text(stmt, 5);

		    e->data = data == NULL ? NULL : strdup((char *) data);
		    if (checksum != NULL)
			    e->checksum = strdup((char *) checksum);
		    if (owner != NULL)
			    e->owner = strdup((char *) owner);
		    if (group != NULL)
			    e->group = strdup((char *) group);
		    if (mode != NULL)
			    e->mode = strdup((char *) mode);

		    STAILQ_INSERT_TAIL(alist, e, next);
	    }

	    sqlite3_finalize(stmt);
	});

	if (result == MPORT_ERR_FATAL)
		SET_ERRORX(result, "Error reading assets %s", err);
	return result;
}

static int
do_actual_install(mportInstance *mport, mportBundleRead *bundle, mportPackageMeta *pkg)
{
	mportAssetList *alist = NULL;
	__block mportAssetListEntry *e = NULL;
	int file_total;
	int file_count = 0;
	struct archive_entry *entry;
	char *orig_cwd = NULL;
	uid_t owner = 0; /* root */
	gid_t group = 0; /* wheel */
	mode_t *set = NULL;
	mode_t newmode;
	mode_t *dirset = NULL;
	mode_t dirnewmode;
	char *mode = NULL;
	char *mkdirp = NULL;
	struct stat sb;
	__block char file[FILENAME_MAX], cwd[FILENAME_MAX];
	__block sqlite3_stmt *insert = NULL;

	/* sadly, we can't just use abs pathnames, because it will break hardlinks */
	orig_cwd = getcwd(NULL, 0);

	if (get_file_count(mport, pkg->name, &file_total) != MPORT_OK)
		goto ERROR;

	mport_call_progress_init_cb(mport, "Installing %s-%s", pkg->name, pkg->version);

	if (mport_bundle_read_get_assetlist(mport, pkg, &alist, ACTUALINSTALL) != MPORT_OK)
		goto ERROR;

	if (create_package_row(mport, pkg) != MPORT_OK)
		goto ERROR;

	if (create_depends(mport, pkg) != MPORT_OK)
		goto ERROR;

	if (create_categories(mport, pkg) != MPORT_OK)
		goto ERROR;

	/* Insert the assets into the master table (We do this one by one because we want to insert file
	* assets as absolute paths. */
	if (mport_db_prepare(mport->db, &insert,
	                     "INSERT INTO assets (pkg, type, data, checksum, owner, grp, mode) values (%Q,?,?,?,?,?,?)",
	                     pkg->name) != MPORT_OK)
		goto ERROR;

	(void) strlcpy(cwd, pkg->prefix, sizeof(cwd));

	if (mport_chdir(mport, cwd) != MPORT_OK)
		goto ERROR;

	mport_db_do(mport->db, "BEGIN TRANSACTION");

	STAILQ_FOREACH(e, alist, next) {
		switch (e->type) {
			case ASSET_CWD:
				(void) strlcpy(cwd, e->data == NULL ? pkg->prefix : e->data, sizeof(cwd));
				if (mport_chdir(mport, cwd) != MPORT_OK)
					goto ERROR;
				break;
			case ASSET_CHMOD:
				if (mode != NULL)
					free(mode);
				/* TODO: should we reset the mode rather than NULL here */
				if (e->data == NULL)
					mode = NULL;
				else
					mode = strdup(e->data);
				break;
			case ASSET_CHOWN:
				owner = mport_get_uid(e->data);
				break;
			case ASSET_CHGRP:
				group = mport_get_gid(e->data);
				break;
			case ASSET_DIR:
			case ASSET_DIRRM:
			case ASSET_DIRRMTRY:
			case ASSET_DIR_OWNER_MODE:
				mkdirp = strdup( e->data == NULL ? "" : e->data); /* need a char * here */
				if (mkdirp == NULL || mport_mkdirp(mkdirp, S_IRWXU | S_IRWXG | S_IRWXO) == 0) {
					free(mkdirp);
					SET_ERRORX(MPORT_ERR_FATAL, "Unable to create directory %s", e->data);
					goto ERROR;
				}
				free(mkdirp);

				if (e->mode != NULL && e->mode[0] != '\0') {
					if ((dirset = setmode(e->mode)) == NULL)
						goto ERROR;
					dirnewmode = getmode(dirset, sb.st_mode);
					free(dirset);
					if (chmod(e->data, dirnewmode))
						goto ERROR;
				}
				if (e->owner != NULL && e->group != NULL && e->owner[0] != '\0' &&
				    e->group[0] != '\0') {
					if (chown(e->data, mport_get_uid(e->owner), mport_get_gid(e->group)) == -1) {
						SET_ERROR(MPORT_ERR_FATAL, "Unable to change owner");
						goto ERROR;
					}
				} else if (e->owner != NULL && e->owner[0] != '\0') {
					if (chown(e->data, mport_get_uid(e->owner), group) == -1) {
						SET_ERROR(MPORT_ERR_FATAL, "Unable to change owner");
						goto ERROR;
					}
				} else if (e->group != NULL && e->group[0] != '\0') {
					if (chown(e->data, owner, mport_get_gid(e->group)) == -1) {
						SET_ERROR(MPORT_ERR_FATAL, "Unable to change owner");
						goto ERROR;
					}
				}

				break;
			case ASSET_EXEC:
				if (mport_run_asset_exec(mport, e->data, cwd, file) != MPORT_OK)
					goto ERROR;
				break;
			case ASSET_FILE_OWNER_MODE:
				/* FALLS THROUGH */
			case ASSET_FILE:
				/* FALLS THROUGH */
			case ASSET_SHELL:
				/* FALLS THROUGH */
			case ASSET_SAMPLE:
				/* FALLS THROUGH */
			case ASSET_SAMPLE_OWNER_MODE:
				if (mport_bundle_read_next_entry(bundle, &entry) != MPORT_OK)
					goto ERROR;

				if (e->data[0] == '/') {
					(void) snprintf(file, FILENAME_MAX, "%s", e->data);
				} else {
					(void) snprintf(file, FILENAME_MAX, "%s%s/%s", mport->root, cwd, e->data);
				}

				if (e->type == ASSET_SAMPLE || e->type == ASSET_SAMPLE_OWNER_MODE)
					for (int ch = 0; ch < FILENAME_MAX; ch++) {
						if (file[ch] == '\0')
							break;
						if (file[ch] == ' ' || file[ch] == '\t') {
							file[ch] = '\0';
							break;
						}
					}
						
				if (entry == NULL) {
					SET_ERROR(MPORT_ERR_FATAL, "Unexpected EOF with archive file");
					goto ERROR;
				}

				archive_entry_set_pathname(entry, file);

				if (mport_bundle_read_extract_next_file(bundle, entry) != MPORT_OK)
					goto ERROR;

				if (lstat(file, &sb)) {
					SET_ERRORX(MPORT_ERR_FATAL, "Unable to stat file %s", file);
					goto ERROR;
				}

				if (S_ISREG(sb.st_mode)) {
					if (e->type == ASSET_FILE_OWNER_MODE) {
						/* Test for owner and group settings, otherwise roll with our default. */
						if (e->owner != NULL && e->group != NULL && e->owner[0] != '\0' &&
						    e->group[0] != '\0') {
#ifdef DEBUG
							fprintf(stderr, "owner %s and group %s\n", e->owner, e->group);
#endif
							if (chown(file, mport_get_uid(e->owner),
							          mport_get_gid(e->group)) == -1) {
								SET_ERROR(MPORT_ERR_FATAL, "Unable to change owner");
								goto ERROR;
							}
						} else if (e->owner != NULL && e->owner[0] != '\0') {
#ifdef DEBUG
							fprintf(stderr, "owner %s\n", e->owner);
#endif
							if (chown(file, mport_get_uid(e->owner), group) == -1) {
								SET_ERROR(MPORT_ERR_FATAL, "Unable to change owner");
								goto ERROR;
							}
						} else if (e->group != NULL && e->group[0] != '\0') {
#ifdef DEBUG
							fprintf(stderr, "group %s\n", e->group);
#endif
							if (chown(file, owner, mport_get_gid(e->group)) == -1) {
								SET_ERROR(MPORT_ERR_FATAL, "Unable to change owner");
								goto ERROR;
							}
						} else {
							// use default.
							if (chown(file, owner, group) == -1) {
								SET_ERROR(MPORT_ERR_FATAL, "Unable to change owner");
								goto ERROR;
							}
						}
					} else {
						/* Set the owner and group */
						if (chown(file, owner, group) == -1) {
							SET_ERRORX(MPORT_ERR_FATAL,
							           "Unable to set permissions on file %s", file);
							goto ERROR;
						}
					}

					/* Set the file permissions, assumes non NFSv4 */
					if (mode != NULL) {
						if (stat(file, &sb)) {
							SET_ERRORX(MPORT_ERR_FATAL, "Unable to stat file %s", file);
							goto ERROR;
						}
						if (e->type == ASSET_FILE_OWNER_MODE && e->mode != NULL) {
							if ((set = setmode(e->mode)) == NULL) {
								SET_ERROR(MPORT_ERR_FATAL, "Unable to set mode");
								goto ERROR;
							}
						} else {
							if ((set = setmode(mode)) == NULL) {
								SET_ERROR(MPORT_ERR_FATAL, "Unable to set mode");
								goto ERROR;
							}
						}
						newmode = getmode(set, sb.st_mode);
						free(set);
						if (chmod(file, newmode)) {
							SET_ERROR(MPORT_ERR_FATAL, "Unable to set file permissions");
							goto ERROR;
						}
					}

					/* shell registration */
					if (e->type == ASSET_SHELL && mport_shell_register(file) != MPORT_OK) {
						goto ERROR;
					}

					/* for sample files, if we don't have an existing file, make a new one */
					if ((e->type == ASSET_SAMPLE || e->type == ASSET_SAMPLE_OWNER_MODE) && 
					    create_sample_file(mport, cwd, e->data) != MPORT_OK) {
						SET_ERRORX(MPORT_ERR_FATAL, "Unable to create sample file from %s",
						           file);
						goto ERROR;
					}
				}

				(mport->progress_step_cb)(++file_count, file_total, file);

				break;
			default:
				/* do nothing */
				break;
		}

		/* insert this asset into the master database */
		__block int code = MPORT_OK;
		__block const char *err;
		__block char *filePtr = strdup(file);
		__block char *cwdPtr = strdup(cwd);
		dispatch_sync(mportSQLSerial, ^{
		    char dir[FILENAME_MAX];
		    if (sqlite3_bind_int(insert, 1, (int) e->type) != SQLITE_OK) {
			    code = MPORT_ERR_FATAL;
			    err = sqlite3_errmsg(mport->db);
			    return;
		    }
		    if (e->type == ASSET_FILE || e->type == ASSET_SAMPLE || e->type == ASSET_SHELL ||
		        e->type == ASSET_FILE_OWNER_MODE || e->type == ASSET_SAMPLE_OWNER_MODE) {
			    /* don't put the root in the database! */
			    if (sqlite3_bind_text(insert, 2, filePtr + strlen(mport->root), -1, SQLITE_STATIC) !=
			        SQLITE_OK) {
				    code = MPORT_ERR_FATAL;
				    err = sqlite3_errmsg(mport->db);
				    return;
			    }

			    if (sqlite3_bind_text(insert, 3, e->checksum, -1, SQLITE_STATIC) != SQLITE_OK) {
				    code = MPORT_ERR_FATAL;
				    err = sqlite3_errmsg(mport->db);
				    return;
			    }

			    if (e->owner != NULL) {
				    if (sqlite3_bind_text(insert, 4, e->owner, -1, SQLITE_STATIC) != SQLITE_OK) {
					    code = MPORT_ERR_FATAL;
					    err = sqlite3_errmsg(mport->db);
					    return;
				    }
			    } else {
				    if (sqlite3_bind_null(insert, 4) != SQLITE_OK) {
					    code = MPORT_ERR_FATAL;
					    err = sqlite3_errmsg(mport->db);
					    return;
				    }
			    }

			    if (e->group != NULL) {
				    if (sqlite3_bind_text(insert, 5, e->group, -1, SQLITE_STATIC) != SQLITE_OK) {
					    code = MPORT_ERR_FATAL;
					    err = sqlite3_errmsg(mport->db);
					    return;
				    }
			    } else {
				    if (sqlite3_bind_null(insert, 5) != SQLITE_OK) {
					    code = MPORT_ERR_FATAL;
					    err = sqlite3_errmsg(mport->db);
					    return;
				    }
			    }

			    if (e->mode != NULL) {
				    if (sqlite3_bind_text(insert, 6, e->mode, -1, SQLITE_STATIC) != SQLITE_OK) {
					    code = MPORT_ERR_FATAL;
					    err = sqlite3_errmsg(mport->db);
					    return;
				    }
			    } else {
				    if (sqlite3_bind_null(insert, 6) != SQLITE_OK) {
					    code = MPORT_ERR_FATAL;
					    err = sqlite3_errmsg(mport->db);
					    return;
				    }
			    }
		    } else if (e->type == ASSET_DIR || e->type == ASSET_DIRRM || e->type == ASSET_DIRRMTRY) {
			    /* if data starts with /, it's most likely an absolute path. Don't prepend cwd */
			    if (e->data != NULL && e->data[0] == '/')
				    (void) snprintf(dir, FILENAME_MAX, "%s", e->data);
			    else
				    (void) snprintf(dir, FILENAME_MAX, "%s/%s", cwdPtr, e->data);

			    if (sqlite3_bind_text(insert, 2, dir, -1, SQLITE_STATIC) != SQLITE_OK) {
				    code = MPORT_ERR_FATAL;
				    err = sqlite3_errmsg(mport->db);
				    return;
			    }

			    if (sqlite3_bind_null(insert, 3) != SQLITE_OK) {
				    code = MPORT_ERR_FATAL;
				    err = sqlite3_errmsg(mport->db);
				    return;
			    }

			    if (sqlite3_bind_null(insert, 4) != SQLITE_OK) {
				    code = MPORT_ERR_FATAL;
				    err = sqlite3_errmsg(mport->db);
				    return;
			    }

			    if (sqlite3_bind_null(insert, 5) != SQLITE_OK) {
				    code = MPORT_ERR_FATAL;
				    err = sqlite3_errmsg(mport->db);
				    return;
			    }

			    if (sqlite3_bind_null(insert, 6) != SQLITE_OK) {
				    code = MPORT_ERR_FATAL;
				    err = sqlite3_errmsg(mport->db);
				    return;
			    }
		    } else {
			    if (sqlite3_bind_text(insert, 2, e->data, -1, SQLITE_STATIC) != SQLITE_OK) {
				    code = MPORT_ERR_FATAL;
				    err = sqlite3_errmsg(mport->db);
				    return;
			    }

			    if (sqlite3_bind_null(insert, 3) != SQLITE_OK) {
				    code = MPORT_ERR_FATAL;
				    err = sqlite3_errmsg(mport->db);
				    return;
			    }

			    if (sqlite3_bind_null(insert, 4) != SQLITE_OK) {
				    code = MPORT_ERR_FATAL;
				    err = sqlite3_errmsg(mport->db);
				    return;
			    }

			    if (sqlite3_bind_null(insert, 5) != SQLITE_OK) {
				    code = MPORT_ERR_FATAL;
				    err = sqlite3_errmsg(mport->db);
				    return;
			    }

			    if (sqlite3_bind_null(insert, 6) != SQLITE_OK) {
				    code = MPORT_ERR_FATAL;
				    err = sqlite3_errmsg(mport->db);
				    return;
			    }
		    }

		    if (sqlite3_step(insert) != SQLITE_DONE) {
			    code = MPORT_ERR_FATAL;
			    err = sqlite3_errmsg(mport->db);
			    return;
		    }

		    sqlite3_clear_bindings(insert);
		    sqlite3_reset(insert);
		});

		free(filePtr);
		free(cwdPtr);
		if (code != MPORT_OK) {
			SET_ERROR(code, err);
			goto ERROR;
		}
	}

	if (mport_db_do(mport->db, "COMMIT") != MPORT_OK) {
		SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(mport->db));
		goto ERROR;
	}
	sqlite3_finalize(insert);

	mport_pkgmeta_logevent(mport, pkg, "Installed");

	(mport->progress_free_cb)();
	(void) mport_chdir(NULL, orig_cwd);
	free(orig_cwd);
	mport_assetlist_free(alist);
	return (MPORT_OK);

	ERROR:
	sqlite3_finalize(insert);
	(mport->progress_free_cb)();
	free(orig_cwd);
	mport_assetlist_free(alist);
	RETURN_CURRENT_ERROR;
}           


#define COPY_METAFILE(TYPE)	(void)snprintf(from, FILENAME_MAX, "%s/%s/%s-%s/%s", bundle->tmpdir, MPORT_STUB_INFRA_DIR, pkg->name, pkg->version, TYPE); \
                                if (mport_file_exists(from)) { \
                                  (void)snprintf(to, FILENAME_MAX, "%s%s/%s-%s/%s", mport->root, MPORT_INST_INFRA_DIR, pkg->name, pkg->version, TYPE); \
                                  if (mport_mkdir(dirname(to)) != MPORT_OK) \
                                    RETURN_CURRENT_ERROR; \
                                  if (mport_copy_file(from, to) != MPORT_OK) \
                                    RETURN_CURRENT_ERROR; \
                                }

static int
mark_complete(mportInstance *mport, mportPackageMeta *pkg)
{
	if (mport_db_do(mport->db, "UPDATE packages SET status='clean' WHERE pkg=%Q", pkg->name) != MPORT_OK) {
		SET_ERROR(MPORT_ERR_FATAL, "Unable to mark package clean");
		RETURN_CURRENT_ERROR;
	}

	return MPORT_OK;
}


static int
do_post_install(mportInstance *mport, mportBundleRead *bundle, mportPackageMeta *pkg)
{
	char to[FILENAME_MAX], from[FILENAME_MAX]; /* keep these for COPY_METAFILE */

	COPY_METAFILE(MPORT_MTREE_FILE);
	COPY_METAFILE(MPORT_INSTALL_FILE);
	COPY_METAFILE(MPORT_DEINSTALL_FILE);
	COPY_METAFILE(MPORT_MESSAGE_FILE);

	if (run_postexec(mport, pkg) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	if (display_pkg_msg(mport, bundle, pkg) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	if (run_pkg_install(mport, bundle, pkg, "POST-INSTALL") != MPORT_OK)
		RETURN_CURRENT_ERROR;

	return mark_complete(mport, pkg);
}


static int
run_postexec(mportInstance *mport, mportPackageMeta *pkg)
{
	mportAssetList *alist;
	mportAssetListEntry *e = NULL;
	char cwd[FILENAME_MAX];

	/* Process @postexec steps */
	if (mport_bundle_read_get_assetlist(mport, pkg, &alist, POSTINSTALL) != MPORT_OK)
		goto ERROR;

	(void) strlcpy(cwd, pkg->prefix, sizeof(cwd));

	if (mport_chdir(mport, cwd) != MPORT_OK)
		goto ERROR;

	STAILQ_FOREACH(e, alist, next) {
		char file[FILENAME_MAX];
		if (e->data == NULL) {
			snprintf(file, sizeof(file), "%s", mport->root);
		} else if (e->data[0] == '/') {
			snprintf(file, sizeof(file), "%s%s", mport->root, e->data);
		} else {
			snprintf(file, sizeof(file), "%s%s/%s", mport->root, pkg->prefix, e->data);
		}

		switch (e->type) {
			case ASSET_CWD:
				(void) strlcpy(cwd, e->data == NULL ? pkg->prefix : e->data, sizeof(cwd));
				if (mport_chdir(mport, cwd) != MPORT_OK)
					goto ERROR;
				break;
			case ASSET_POSTEXEC:
				if (mport_run_asset_exec(mport, e->data, cwd, file) != MPORT_OK)
					goto ERROR;
				break;
			default:
				/* do nothing */
				break;
		}
	}

	mport_assetlist_free(alist);
	mport_pkgmeta_logevent(mport, pkg, "postexec");

	return MPORT_OK;

	ERROR:
	// TODO: asset list free
	RETURN_CURRENT_ERROR;
}


static int
run_mtree(mportInstance *mport, mportBundleRead *bundle, mportPackageMeta *pkg)
{
	char file[FILENAME_MAX];
	int ret;

	(void) snprintf(file, FILENAME_MAX, "%s/%s/%s-%s/%s", bundle->tmpdir, MPORT_STUB_INFRA_DIR, pkg->name,
	                pkg->version,
	                MPORT_MTREE_FILE);

	if (mport_file_exists(file)) {
		if ((ret = mport_xsystem(mport, "%s -U -f %s -d -e -p %s >/dev/null", MPORT_MTREE_BIN, file,
		                         pkg->prefix)) != 0)
			RETURN_ERRORX(MPORT_ERR_FATAL, "%s returned non-zero: %i", MPORT_MTREE_BIN, ret);
	}

	return MPORT_OK;
}


static int
run_pkg_install(mportInstance *mport, mportBundleRead *bundle, mportPackageMeta *pkg, const char *mode)
{
	char file[FILENAME_MAX];
	int ret;

	(void) snprintf(file, FILENAME_MAX, "%s/%s/%s-%s/%s", bundle->tmpdir, MPORT_STUB_INFRA_DIR, pkg->name,
	                pkg->version,
	                MPORT_INSTALL_FILE);

	if (mport_file_exists(file)) {
		if (chmod(file, 755) != 0)
			RETURN_ERRORX(MPORT_ERR_FATAL, "chmod(%s, 755): %s", file, strerror(errno));

		if ((ret = mport_xsystem(mport, "PKG_PREFIX=%s %s %s %s", pkg->prefix, file, pkg->name, mode)) != 0)
			RETURN_ERRORX(MPORT_ERR_FATAL, "%s %s returned non-zero: %i", MPORT_INSTALL_FILE, mode, ret);
	}

	return MPORT_OK;
}


static int
display_pkg_msg(mportInstance *mport, mportBundleRead *bundle, mportPackageMeta *pkg)
{
	char filename[FILENAME_MAX];
	char *buf;
	struct stat st;
	FILE *file;

	(void) snprintf(filename, FILENAME_MAX, "%s/%s/%s-%s/%s", bundle->tmpdir, MPORT_STUB_INFRA_DIR, pkg->name,
	                pkg->version, MPORT_MESSAGE_FILE);

	if (stat(filename, &st) == -1)
		/* if we couldn't stat the file, we assume there isn't a pkg-msg */
		return MPORT_OK;

	if ((file = fopen(filename, "r")) == NULL)
		RETURN_ERRORX(MPORT_ERR_FATAL, "Couldn't open %s: %s", filename, strerror(errno));

	if ((buf = (char *) calloc((size_t) (st.st_size + 1), sizeof(char))) == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");

	if (fread(buf, sizeof(char), (size_t) st.st_size, file) != (size_t) st.st_size) {
		free(buf);
		RETURN_ERRORX(MPORT_ERR_FATAL, "Read error: %s", strerror(errno));
	}

	buf[st.st_size] = '\0';

	if (buf[0] != '\0')
		mport_call_msg_cb(mport, "%s", buf);

	free(buf);

	return MPORT_OK;
}
