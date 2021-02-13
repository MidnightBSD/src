/*-
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

#include <stdlib.h>
#include <string.h>

#include "mport.h"
#include "mport_private.h"


static int check_if_installed(sqlite3 *, mportPackageMeta *);
static int check_conflicts(sqlite3 *, mportPackageMeta *);
static int check_depends(mportInstance *mport, mportPackageMeta *);
static int check_if_older_installed(mportInstance *, mportPackageMeta *);
static int check_if_older_os(mportInstance *, mportPackageMeta *);

/* Run the checks requested by the flags given.
 *
 * Flags:
 *   MPORT_PRECHECK_INSTALLED  -- Fail if the package is installed
 *   MPORT_PRECHECK_UPGRADABLE -- Fail if an older version is not installed
 *   MPORT_PRECHECK_CONFLICTS  -- Fail if the package has a conflict
 *   MPORT_PRECHECK_DEPENDS    -- Fail if the the depends are no resolved
 *   MPORT_PRECHECK_OS	       -- Fail if the os version of the installed is older
 *
 * The checks are run in the order listed above.  The first failure
 * encountered is the one reported.   
 *
 * This function expects that the stub database for the given package is
 * connected.
 */

int mport_check_preconditions(mportInstance *mport, mportPackageMeta *pack, long flags)
{
	if (flags & MPORT_PRECHECK_INSTALLED && check_if_installed(mport->db, pack) != MPORT_OK)
		RETURN_CURRENT_ERROR;
	if (flags & MPORT_PRECHECK_UPGRADEABLE && check_if_older_installed(mport, pack) != MPORT_OK)
		RETURN_CURRENT_ERROR;
	if (flags & MPORT_PRECHECK_CONFLICTS && check_conflicts(mport->db, pack) != MPORT_OK)
		RETURN_CURRENT_ERROR;
	if (flags & MPORT_PRECHECK_DEPENDS && check_depends(mport, pack) != MPORT_OK)
		RETURN_CURRENT_ERROR;
	if (flags & MPORT_PRECHECK_OS && check_if_older_os(mport, pack) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	return MPORT_OK;
}

static int check_if_installed(sqlite3 *db, mportPackageMeta *pack)
{
	sqlite3_stmt *stmt;
	const char *inst_version;
	const char *os_release;
	char *system_os_release;

	/* check if the package is already installed */
	if (mport_db_prepare(db, &stmt, "SELECT version, os_release FROM packages WHERE pkg=%Q", pack->name) != MPORT_OK) {
		sqlite3_finalize(stmt);
		RETURN_CURRENT_ERROR;
	}

	/* it's possible that the default package with a flavor does not have a prefix, but will appear that way during
	 * dependency calculation.
	 */
	int step = sqlite3_step(stmt);
	if (step == SQLITE_DONE) {
		if (pack->flavor != NULL && !mport_starts_with(pack->flavor, pack->name)) {
			char *full_name;
			asprintf(&full_name, "%s-%s", pack->flavor, pack->name);
			if (full_name != NULL) {
				if (mport_db_prepare(db, &stmt, "SELECT version, os_release FROM packages WHERE pkg=%Q", full_name) !=
				    MPORT_OK) {
					sqlite3_finalize(stmt);
					RETURN_CURRENT_ERROR;
				}
				free(full_name);
				step = sqlite3_step(stmt);
			}
		}
	}

	switch (step) {
		case SQLITE_DONE:
			/* No row found. */
			break;
		case SQLITE_ROW:
			/* Row was found */
			inst_version = sqlite3_column_text(stmt, 0);
			os_release = sqlite3_column_text(stmt, 1);
			system_os_release = (char *) mport_get_osrelease();

			/* Different os release version should not be considered the same package */
			if (strcmp(os_release, system_os_release) != 0) {
				free(system_os_release);
				break;
			}
			free(system_os_release);

			SET_ERRORX(MPORT_ERR_FATAL, "%s (version %s) is already installed.", pack->name, inst_version);
			sqlite3_finalize(stmt);
			RETURN_CURRENT_ERROR;

			break;
		default:
			/* Some sort of sqlite error */
			SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
			sqlite3_finalize(stmt);
			RETURN_CURRENT_ERROR;
	}

	sqlite3_finalize(stmt);
	return MPORT_OK;
}

static int check_conflicts(sqlite3 *db, mportPackageMeta *pack)
{
	sqlite3_stmt *stmt;
	int ret;
	const char *inst_name, *inst_version;

	if (mport_db_prepare(db, &stmt,
	                     "SELECT packages.pkg, packages.version FROM stub.conflicts LEFT JOIN packages ON packages.pkg GLOB stub.conflicts.conflict_pkg AND packages.version GLOB stub.conflicts.conflict_version WHERE stub.conflicts.pkg=%Q AND packages.pkg IS NOT NULL",
	                     pack->name) != MPORT_OK) {
		sqlite3_finalize(stmt);
		RETURN_CURRENT_ERROR;
	}


	while (1) {
		ret = sqlite3_step(stmt);

		if (ret == SQLITE_ROW) {
			inst_name = (const char *) sqlite3_column_text(stmt, 0);
			inst_version = (const char *) sqlite3_column_text(stmt, 1);

			SET_ERRORX(MPORT_ERR_FATAL, "Installed package %s-%s conflicts with %s", inst_name, inst_version,
			           pack->name);
			sqlite3_finalize(stmt);
			RETURN_CURRENT_ERROR;
		} else if (ret == SQLITE_DONE) {
			/* No conflicts */
			break;
		} else {
			SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
			sqlite3_finalize(stmt);
			RETURN_CURRENT_ERROR;
		}
	}

	sqlite3_finalize(stmt);
	return MPORT_OK;
}


static int check_depends(mportInstance *mport, mportPackageMeta *pack)
{
	sqlite3 *db = mport->db;
	sqlite3_stmt *stmt, *lookup;
	const char *depend_pkg, *depend_version, *inst_version;
	const char *os_release;
	char *system_os_release;
	int ret;

	/* check for depends */
	if (mport_db_prepare(db, &stmt, "SELECT depend_pkgname, depend_pkgversion FROM stub.depends WHERE pkg=%Q",
	                     pack->name) != MPORT_OK) {
		sqlite3_finalize(stmt);
		RETURN_CURRENT_ERROR;
	}

	/* package name on dependencies can contain the flavor prefix. native-binutils but there is no guarnatee we stored it as native-bintuils in master. check for binutils also. */
	if (mport_db_prepare(db, &lookup, "SELECT version, os_release, flavor FROM packages WHERE (pkg=? or (flavor is not null and flavor != '' and pkg=substr(?, length(flavor) + 2) )) AND status='clean'") !=
	    MPORT_OK) {
		sqlite3_finalize(stmt);
		RETURN_CURRENT_ERROR;
	}

	system_os_release = (char *) mport_get_osrelease();

	while (1) {
		ret = sqlite3_step(stmt);

		if (ret == SQLITE_ROW) {
			depend_pkg = sqlite3_column_text(stmt, 0);
			depend_version = sqlite3_column_text(stmt, 1);

			if (sqlite3_bind_text(lookup, 1, depend_pkg, -1, SQLITE_STATIC) != SQLITE_OK || sqlite3_bind_text(lookup, 2, depend_pkg, -1, SQLITE_STATIC) != SQLITE_OK) {
				SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
				sqlite3_finalize(lookup);
				sqlite3_finalize(stmt);

				free(system_os_release);

				RETURN_CURRENT_ERROR;
			}

			switch (sqlite3_step(lookup)) {
				case SQLITE_ROW:
					inst_version = sqlite3_column_text(lookup, 0);
					os_release = sqlite3_column_text(lookup, 1);
					int ok;

					if (strcmp(os_release, system_os_release) != 0) {
						SET_ERRORX(MPORT_ERR_FATAL,
						           "%s depends on %s version %s.  Version %s for MidnightBSD %s is installed.",
						           pack->name, depend_pkg, depend_version == NULL ? "<any>" : depend_version,
						           inst_version, os_release);
						sqlite3_finalize(lookup);
						sqlite3_finalize(stmt);
						free(system_os_release);
						RETURN_CURRENT_ERROR;
					}

					if (depend_version == NULL)
						/* no minimum version */
						break;

					ok = mport_version_require_check(inst_version, depend_version);

					if (ok > 0) {
						sqlite3_finalize(lookup);
						sqlite3_finalize(stmt);
						free(system_os_release);
						RETURN_CURRENT_ERROR;
					} else if (ok == -1) {
						SET_ERRORX(MPORT_ERR_FATAL, "%s depends on %s version %s.  Version %s is installed.",
						           pack->name, depend_pkg, depend_version, inst_version);
						sqlite3_finalize(lookup);
						sqlite3_finalize(stmt);
						free(system_os_release);
						RETURN_CURRENT_ERROR;
					}

					break;
				case SQLITE_DONE:
					/* this depend isn't installed. */
					SET_ERRORX(MPORT_ERR_FATAL, "%s depends on %s, which is not installed.", pack->name, depend_pkg);
					sqlite3_finalize(lookup);
					sqlite3_finalize(stmt);
					free(system_os_release);
					RETURN_CURRENT_ERROR;
					break;
				default:
					SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
					sqlite3_finalize(lookup);
					sqlite3_finalize(stmt);
					free(system_os_release);
					RETURN_CURRENT_ERROR;
			}

			sqlite3_reset(lookup);
			sqlite3_clear_bindings(lookup);
		} else if (ret == SQLITE_DONE) {
			/* No more depends to check. */
			sqlite3_finalize(lookup);
			sqlite3_finalize(stmt);
			break;
		} else {
			SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
			sqlite3_finalize(lookup);
			sqlite3_finalize(stmt);
			free(system_os_release);
			RETURN_CURRENT_ERROR;
		}
	}

	free(system_os_release);

	return MPORT_OK;
}

/* check to see if an older version of a package is installed. */
static int
check_if_older_installed(mportInstance *mport, mportPackageMeta *pkg)
{
	sqlite3_stmt *stmt;
	int ret;
	const char *os_release;

	os_release = mport_get_osrelease();

	if (mport_db_prepare(mport->db, &stmt,
	                     "SELECT os_release FROM packages WHERE pkg=%Q and ((mport_version_cmp(version, %Q) < 0 and os_release=%Q) or os_release != %Q)",
	                     pkg->name, pkg->version, os_release, os_release) != MPORT_OK) {
		sqlite3_finalize(stmt);
		RETURN_CURRENT_ERROR;
	}

	switch (sqlite3_step(stmt)) {
		case SQLITE_ROW:
			ret = MPORT_OK;
			break;
		case SQLITE_DONE:
			ret = SET_ERRORX(MPORT_ERR_FATAL, "No older version of %s installed", pkg->name);
			break;
		default:
			ret = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(mport->db));
			break;
	}

	sqlite3_finalize(stmt);
	return ret;
}

static int
check_if_older_os(mportInstance *mport, mportPackageMeta *pkg)
{
	sqlite3_stmt *stmt;
	int ret;
	const char *os_release;

	os_release = mport_get_osrelease();
	if (mport_db_prepare(mport->db, &stmt,
	                     "SELECT os_release FROM packages WHERE pkg=%Q and mport_version_cmp(os_release, %Q) < 0",
	                     pkg->name, os_release) != MPORT_OK) {
		sqlite3_finalize(stmt);
		RETURN_CURRENT_ERROR;
	}

	switch (sqlite3_step(stmt)) {
		case SQLITE_ROW:
			ret = MPORT_OK;
			break;
		case SQLITE_DONE:
			ret = SET_ERRORX(MPORT_ERR_FATAL, "No older os release version of %s installed", pkg->name);
			break;
		default:
			ret = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(mport->db));
			break;
	}

	sqlite3_finalize(stmt);
	return ret;
}
