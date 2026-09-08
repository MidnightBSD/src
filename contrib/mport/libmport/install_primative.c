/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2015 Lucas Holt
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

#include "mport.h"
#include "mport_private.h"
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <sys/statvfs.h>
#include <libutil.h>

static char **get_dependencies(mportInstance *mport, mportPackageMeta *pack);
static char *find_file_with_prefix(const char *dir, const char *prefix);
static /*@null@*/ /*@only@*/ mportPackageMeta **lookup_current_os_installed(
    /*@notnull@*/ mportInstance *, /*@notnull@*/ mportPackageMeta *,
    /*@out@*/ mportPackageMeta **);
static int remove_stale_os_release_copy(/*@notnull@*/ mportInstance *,
    /*@notnull@*/ mportPackageMeta *);
static int purge_orphaned_rows(/*@notnull@*/ mportInstance *, /*@notnull@*/ const char *);

#define GOTO_CLEANUP_ON_MPORT_ERR(expr)         \
	do {                                    \
		if ((expr) != MPORT_OK) {       \
			ret = mport_err_code(); \
			goto cleanup;           \
		}                               \
	} while (0)

static char **
get_dependencies(mportInstance *mport, mportPackageMeta *pkg)
{
	sqlite3_stmt *stmt;
	int ret, count = 0;
	char **dependencies;
	int i = 0;

	if (mport_db_prepare(mport->db, &stmt, "SELECT COUNT(*) FROM stub.depends WHERE pkg=%Q",
		pkg->name) != MPORT_OK) {
		sqlite3_finalize(stmt);
		return NULL;
	}

	ret = sqlite3_step(stmt);

	switch (ret) {
	case SQLITE_ROW:
		count = sqlite3_column_int(stmt, 0);
		sqlite3_finalize(stmt);
		break;
	case SQLITE_DONE:
		sqlite3_finalize(stmt);
		SET_ERROR(MPORT_ERR_FATAL, "SQLite returned no rows for a COUNT(*) select.");
		return NULL;
	default:
		sqlite3_finalize(stmt);
		SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(mport->db));
		return NULL;
	}

	if ((dependencies = (char **)calloc(count + 1, sizeof(char *))) == NULL)
		return NULL;

	if (mport_db_prepare(mport->db, &stmt,
		"SELECT depend_pkgname FROM stub.depends WHERE pkg=%Q", pkg->name) != MPORT_OK) {
		sqlite3_finalize(stmt);
		return NULL;
	}

	while (1) {
		const char *depend_pkg;

		ret = sqlite3_step(stmt);

		if (ret == SQLITE_ROW) {
			depend_pkg = (const char *)sqlite3_column_text(stmt, 0);
			if (asprintf(&dependencies[i], "%s", depend_pkg) == -1) {
				sqlite3_finalize(stmt);
				return NULL;
			}
			i++;
		} else if (ret == SQLITE_DONE) {
			/* No more dependencies to check. */
			sqlite3_finalize(stmt);
			break;
		} else {
			SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(mport->db));
			sqlite3_finalize(stmt);
			return NULL;
		}

		if (i == count) {
			sqlite3_finalize(stmt);
			break;
		}
	}

	return dependencies;
}

char *
find_file_with_prefix(const char *dir, const char *prefix)
{
	DIR *d;
	struct dirent *dir_entry;
	char *found_file = NULL;
	size_t prefix_len = strlen(prefix);

	d = opendir(dir);
	if (d) {
		while ((dir_entry = readdir(d)) != NULL) {
			if (strncmp(dir_entry->d_name, prefix, prefix_len) == 0) {
				// Found a file with the correct prefix
				found_file = malloc(strlen(dir) + strlen(dir_entry->d_name) +
				    2); // +2 for '/' and null terminator
				if (found_file) {
					sprintf(found_file, "%s/%s", dir, dir_entry->d_name);
				}
				break;
			}
		}
		closedir(d);
	}

	return found_file;
}

/*
 * Return the installed package(s) matching pkg->name, and set *match to the
 * single entry whose os_release equals the running system's. The package name
 * is not unique in the master DB (the same name can be installed under a
 * different os_release), so scan every returned row rather than assuming the
 * current-OS entry is first. Returns NULL (and sets *match to NULL) when the
 * package is not installed under the current OS release; the caller owns the
 * returned vector, and *match is an observer into it.
 */
static mportPackageMeta **
lookup_current_os_installed(mportInstance *mport, mportPackageMeta *pkg, mportPackageMeta **match)
{
	mportPackageMeta **installed = NULL;
	char *system_os_release;
	int i;

	*match = NULL;

	if (mport_pkgmeta_search_master(mport, &installed, "pkg=%Q", pkg->name) != MPORT_OK)
		return NULL;
	if (installed == NULL)
		return NULL;

	system_os_release = mport_get_osrelease(mport);
	if (system_os_release == NULL) {
		mport_pkgmeta_vec_free(installed);
		return NULL;
	}

	for (i = 0; installed[i] != NULL; i++) {
		if (installed[i]->os_release != NULL &&
		    strcmp(installed[i]->os_release, system_os_release) == 0) {
			*match = installed[i];
			break;
		}
	}

	free(system_os_release);

	if (*match == NULL) {
		mport_pkgmeta_vec_free(installed);
		return NULL;
	}

	return installed;
}

/*
 * Remove a copy of pkg that is registered under a different os_release than the
 * running system.  check_if_installed() considers such a copy to be a distinct
 * package, so without this the install proceeds while the old copy still owns
 * the same files on disk: the file conflict precheck reports them as "not
 * managed by mport" and, when forced, the registry ends up with two rows and
 * two identical asset sets for one package.
 *
 * The os_release difference is itself the reason to replace, so the versions
 * are not compared here -- rebuilding the same version against a new OS release
 * is the common case.  Nothing is removed when a copy is registered under the
 * current os_release; that copy is handled by the existing update/force paths.
 */
static int
remove_stale_os_release_copy(mportInstance *mport, mportPackageMeta *pkg)
{
	mportPackageMeta **installed = NULL;
	mportPackageMeta *stale = NULL;
	char *system_os_release;
	int ret = MPORT_OK;
	int i;

	if (mport_pkgmeta_search_master(mport, &installed, "pkg=%Q", pkg->name) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	if (installed == NULL)
		return MPORT_OK;

	if ((system_os_release = mport_get_osrelease(mport)) == NULL) {
		mport_pkgmeta_vec_free(installed);
		RETURN_ERROR(MPORT_ERR_FATAL, "Unable to determine OS release");
	}

	for (i = 0; installed[i] != NULL; i++) {
		if (installed[i]->os_release != NULL &&
		    strcmp(installed[i]->os_release, system_os_release) == 0) {
			/* installed under the current OS release; not our problem */
			stale = NULL;
			break;
		}
		if (stale == NULL)
			stale = installed[i];
	}

	free(system_os_release);

	if (stale != NULL) {
		/* mport_pkgmeta_search_master() always fills os_release in, but be
		 * defensive since this string is only used for messages */
		const char *stale_os = stale->os_release == NULL ? "unknown" : stale->os_release;

		/* delete_primative stops the package's service before it checks the
		 * lock, so refuse here rather than leaving a locked package stopped */
		if (mport_lock_islocked(stale) == MPORT_LOCKED) {
			mport_pkgmeta_vec_free(installed);
			RETURN_ERRORX(MPORT_ERR_FATAL,
			    "%s is installed under OS release %s and is locked.", stale->name,
			    stale_os);
		}

		mport_call_msg_cb(mport, "Replacing %s-%s installed under OS release %s.",
		    stale->name, stale->version, stale_os);

		/* honor the old install's automatic flag, as the update path does */
		pkg->automatic = stale->automatic;
		stale->action = MPORT_ACTION_UPGRADE;

		/* delete_primative works by package name, so this removes every
		 * stale row for pkg, not just the one we matched. */
		if (mport_delete_primative(mport, stale, 1) != MPORT_OK)
			ret = mport_err_code();
	}

	mport_pkgmeta_vec_free(installed);

	return ret;
}

/*
 * Drop every registry row for a package that has no packages row.  A failed
 * install or a partially completed delete can leave assets, depends,
 * categories, conflicts or annotation rows behind; the fresh inserts made by
 * the install collide with them (annotation has a primary key on pkg,tag).
 * Only called when --force is set and no packages row exists, so no installed
 * package is unregistered here.
 */
static int
purge_orphaned_rows(mportInstance *mport, const char *pkg_name)
{
	static const char *const tables[] = { "assets", "depends", "categories", "conflicts",
		"annotation" };
	size_t i;

	if (mport_db_do(mport->db, "BEGIN TRANSACTION") != MPORT_OK)
		RETURN_CURRENT_ERROR;

	for (i = 0; i < sizeof(tables) / sizeof(tables[0]); i++) {
		if (mport_db_do(mport->db, "DELETE FROM %s WHERE pkg=%Q", tables[i], pkg_name) !=
		    MPORT_OK) {
			(void)mport_db_do(mport->db, "ROLLBACK");
			RETURN_CURRENT_ERROR;
		}
	}

	if (mport_db_do(mport->db, "COMMIT TRANSACTION") != MPORT_OK) {
		(void)mport_db_do(mport->db, "ROLLBACK");
		RETURN_CURRENT_ERROR;
	}

	return MPORT_OK;
}

static int
mport_install_primative_impl(
    /*@notnull@*/ mportInstance *mport, /*@null@*/ const char *filename, int fd,
    /*@null@*/ const char *prefix, mportAutomatic automatic)
{
	mportBundleRead *bundle = NULL;
	mportPackageMeta **already_installed = NULL;
	mportPackageMeta *current_installed = NULL;
	mportPackageMeta **pkgs = NULL;
	mportPackageMeta *pkg = NULL;
	int i;
	int ret = MPORT_OK;
	char **dependencies = NULL;
	char **deps = NULL;
	char *dir = NULL;
	long precheck_flags;

	if (mport == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "mport not initialized");

	/* There are two scenarios here.
	   1. We are installing online with an index and have already fetched dependencies.
	   2. We are installing from a local package file. Check if the dependencies are availaible
	   in the same directory that are missing.
	*/
	if (mport->offline) {
		// temporarily open pkg file to get metadata.
		if ((bundle = mport_bundle_read_new()) == NULL)
			RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");

		if (fd >= 0)
			GOTO_CLEANUP_ON_MPORT_ERR(mport_bundle_read_init_fd(bundle, fd));
		else
			GOTO_CLEANUP_ON_MPORT_ERR(mport_bundle_read_init(bundle, filename));
		GOTO_CLEANUP_ON_MPORT_ERR(mport_bundle_read_prep_for_install(mport, bundle));
		GOTO_CLEANUP_ON_MPORT_ERR(mport_pkgmeta_read_stub(mport, &pkgs));

		if (!mport_is_age_verified(mport, pkgs[0])) {
			mport_call_msg_cb(mport,
			    "Unable to install %s-%s: package is age restricted and user does not meet the requirements.",
			    pkgs[0]->name, pkgs[0]->version);
			ret = MPORT_ERR_FATAL;
			goto cleanup;
		}

		/*
		 * Only packages installed under the current OS release are treated
		 * as update/reinstall candidates here. A copy installed under a
		 * different os_release is left for the normal install path below,
		 * where MPORT_PRECHECK_INSTALLED considers a differing os_release to
		 * be a distinct package (matching pre-existing --force behavior).
		 */
		already_installed = lookup_current_os_installed(mport, pkgs[0], &current_installed);
		if (current_installed != NULL) {
			int version_cmp =
			    mport_version_cmp(current_installed->version, pkgs[0]->version);

			if (mport->force || version_cmp < 0) {
				automatic = current_installed->automatic;
				if (version_cmp < 0) {
					mport_call_msg_cb(mport, "Updating %s from %s to %s.",
					    pkgs[0]->name, current_installed->version,
					    pkgs[0]->version);
				}
				if (mport_delete_primative(mport, current_installed, 1) !=
				    MPORT_OK) {
					ret = mport_err_code();
					goto cleanup;
				}
			} else {
				mport_call_msg_cb(mport, "%s-%s: already installed.", pkgs[0]->name,
				    pkgs[0]->version);
				ret = MPORT_OK;
				goto cleanup;
			}

			mport_pkgmeta_vec_free(already_installed);
			already_installed = NULL;
		}

		if (mport_check_preconditions(mport, pkgs[0], MPORT_PRECHECK_CONFLICTS) !=
		    MPORT_OK) {
			mport_call_msg_cb(mport, "Unable to install %s-%s: %s", pkgs[0]->name,
			    pkgs[0]->version, mport_err_string());
			ret = MPORT_ERR_FATAL;
			goto cleanup;
		}
		dependencies = get_dependencies(mport, pkgs[0]);

		if (mport_bundle_read_finish(mport, bundle) != MPORT_OK) {
			ret = mport_err_code();
			bundle = NULL;
			goto cleanup;
		}
		bundle = NULL;

		if (pkgs != NULL) {
			mport_pkgmeta_vec_free(pkgs);
			pkgs = NULL;
		}

		deps = dependencies;
		dir = mport_directory(filename);
		while (deps != NULL && *deps != NULL) {
			char *dep_filename = NULL;
			if (asprintf(&dep_filename, "%s/%s.mport", dir, *deps) == -1) {
				deps++;
				continue;
			}

			if (!mport_file_exists(dep_filename)) {
				char *prefix_search = NULL;
				free(dep_filename);
				if (asprintf(&prefix_search, "%s-", *deps) == -1) {
					deps++;
					continue;
				}
				dep_filename = find_file_with_prefix(dir, prefix_search);
				free(prefix_search);
				if (dep_filename == NULL) {
					mport_call_msg_cb(
					    mport, "Dependency %s not found in %s", *deps, dir);
					deps++;
					continue;
				}
			}

			if (mport_install_primative(mport, dep_filename, prefix, MPORT_AUTOMATIC) !=
			    MPORT_OK) {
				mport_call_msg_cb(
				    mport, "Unable to install %s: %s", *deps, mport_err_string());
				if (!mport->ignoreMissing) {
					free(dep_filename);
					ret = MPORT_ERR_FATAL;
					goto cleanup;
				}
			}
			free(dep_filename);
			deps++;
		}
	}

	if ((bundle = mport_bundle_read_new()) == NULL) {
		ret = mport_set_errx(
		    MPORT_ERR_FATAL, "Error at %s:(%d): %s", __FILE__, __LINE__, "Out of memory.");
		goto cleanup;
	}

	if (fd >= 0)
		GOTO_CLEANUP_ON_MPORT_ERR(mport_bundle_read_init_fd(bundle, fd));
	else
		GOTO_CLEANUP_ON_MPORT_ERR(mport_bundle_read_init(bundle, filename));
	GOTO_CLEANUP_ON_MPORT_ERR(mport_bundle_read_prep_for_install(mport, bundle));
	GOTO_CLEANUP_ON_MPORT_ERR(mport_pkgmeta_read_stub(mport, &pkgs));

	for (i = 0; *(pkgs + i) != NULL; i++) {
		pkg = pkgs[i];
		pkg->automatic = automatic;
		pkg->install_date = mport_get_time();
		pkg->action = MPORT_ACTION_INSTALL;

		if (!mport_is_age_verified(mport, pkg)) {
			mport_call_msg_cb(mport,
			    "Unable to install %s-%s: package is age restricted and user does not meet the requirements.",
			    pkg->name, pkg->version);
			ret = MPORT_ERR_FATAL;
			break; /* do not keep going if we have an age verification failure! */
		}

		if (mport_pkgmeta_search_master(mport, &already_installed, "pkg=%Q", pkg->name) ==
		    MPORT_OK) {
			if (already_installed != NULL && already_installed[0] != NULL) {
				if (mport->force) {
					pkg->automatic =
					    already_installed[0]->automatic; // honor old flag
				}
				mport_pkgmeta_vec_free(already_installed);
				already_installed = NULL;
			} else if (mport->force) {
				/* re-register: clear rows a failed install or delete left
				 * behind, or the fresh inserts below fail */
				if (purge_orphaned_rows(mport, pkg->name) != MPORT_OK) {
					mport_call_msg_cb(mport, "Unable to install %s-%s: %s",
					    pkg->name, pkg->version, mport_err_string());
					ret = MPORT_ERR_FATAL;
					break;
				}
			}
		}

		if (prefix != NULL) {
			/* override the default prefix with the given prefix */
			free(pkg->prefix);
			if ((pkg->prefix = strdup(prefix)) == NULL) { /* all hope is lost! bail */
				ret = mport_set_errx(MPORT_ERR_FATAL, "Error at %s:(%d): %s",
				    __FILE__, __LINE__, "Out of memory.");
				goto cleanup;
			}
		}

		if (pkg->flatsize > 0) {
			struct statvfs sfs;
			const char *check_path = (mport->root != NULL && mport->root[0] != '\0') ?
			    mport->root :
			    pkg->prefix;
			if (statvfs(check_path, &sfs) == 0) {
				int64_t avail_bytes;
				if (sfs.f_frsize != 0 &&
				    (uintmax_t)sfs.f_bavail >
					(uintmax_t)INT64_MAX / (uintmax_t)sfs.f_frsize) {
					avail_bytes = INT64_MAX;
				} else {
					avail_bytes = (int64_t)((uintmax_t)sfs.f_bavail *
					    (uintmax_t)sfs.f_frsize);
				}
				if (pkg->flatsize > avail_bytes) {
					char avail_str[32], need_str[32];
					humanize_number(avail_str, sizeof(avail_str), avail_bytes,
					    "B", HN_AUTOSCALE, HN_DECIMAL | HN_IEC_PREFIXES);
					humanize_number(need_str, sizeof(need_str), pkg->flatsize,
					    "B", HN_AUTOSCALE, HN_DECIMAL | HN_IEC_PREFIXES);
					mport_call_msg_cb(mport,
					    "Warning: %s-%s requires %s but only %s is available on %s.",
					    pkg->name, pkg->version, need_str, avail_str,
					    check_path);
				}
			}
		}

		if (remove_stale_os_release_copy(mport, pkg) != MPORT_OK) {
			mport_call_msg_cb(mport, "Unable to install %s-%s: %s", pkg->name,
			    pkg->version, mport_err_string());
			ret = MPORT_ERR_FATAL;
			break;
		}

		precheck_flags =
		    MPORT_PRECHECK_INSTALLED | MPORT_PRECHECK_DEPENDS | MPORT_PRECHECK_CONFLICTS;
		if (!mport->force)
			precheck_flags |= MPORT_PRECHECK_FILE_CONFLICTS;
		if ((mport_check_preconditions(mport, pkg, precheck_flags) != MPORT_OK) ||
		    (mport_bundle_read_install_pkg(mport, bundle, pkg) != MPORT_OK)) {
			mport_call_msg_cb(mport, "Unable to install %s-%s: %s", pkg->name,
			    pkg->version, mport_err_string());
			ret = MPORT_ERR_FATAL;
			break; /* do not keep going if we have a package failure! */
		}
	}

	if (mport_bundle_read_finish(mport, bundle) != MPORT_OK) {
		ret = mport_err_code();
		bundle = NULL;
		goto cleanup;
	}
	bundle = NULL;

cleanup:
	if (bundle != NULL) {
		if (mport_bundle_read_finish(mport, bundle) != MPORT_OK && ret == MPORT_OK)
			ret = mport_err_code();
		bundle = NULL;
	}

	if (already_installed != NULL) {
		mport_pkgmeta_vec_free(already_installed);
		already_installed = NULL;
	}
	if (pkgs != NULL) {
		mport_pkgmeta_vec_free(pkgs);
		pkgs = NULL;
	}
	free(dir);
	dir = NULL;
	if (dependencies != NULL) {
		char **dptr = dependencies;
		while (*dptr != NULL)
			free(*dptr++);
		free(dependencies);
		dependencies = NULL;
	}

	return ret;
}

MPORT_PUBLIC_API int
mport_install_primative(
    mportInstance *mport, const char *filename, const char *prefix, mportAutomatic automatic)
{
	return mport_install_primative_impl(mport, filename, -1, prefix, automatic);
}

int
mport_install_primative_fd(
    /*@notnull@*/ mportInstance *mport, int fd, /*@null@*/ const char *prefix,
    mportAutomatic automatic)
{
	return mport_install_primative_impl(mport, NULL, fd, prefix, automatic);
}
