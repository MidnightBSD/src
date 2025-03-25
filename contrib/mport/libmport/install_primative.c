/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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
#include <string.h>

static char ** get_dependencies(mportInstance *mport, mportPackageMeta *pack);

static char **
get_dependencies(mportInstance *mport, mportPackageMeta *pkg)
{
	sqlite3_stmt *stmt;
	int ret, count = 0;
	char **dependencies;

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
		"SELECT depend_pkgname, depend_pkgversion FROM stub.depends WHERE pkg=%Q",
		pkg->name) != MPORT_OK) {
		sqlite3_finalize(stmt);
		return NULL;
	}

    int i = 0;
	while (1) {
		const char *depend_pkg, *depend_version;

		ret = sqlite3_step(stmt);

		if (ret == SQLITE_ROW) {
			depend_pkg = sqlite3_column_text(stmt, 0);
			depend_version = sqlite3_column_text(stmt, 1);
			asprintf(&dependencies[i], "%s-%s", depend_pkg, depend_version);
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

		if (i == count)
			break;	
	}

	return dependencies;
}



MPORT_PUBLIC_API int
mport_install_primative(mportInstance *mport, const char *filename, const char *prefix, mportAutomatic automatic)
{
	mportBundleRead *bundle = NULL;
	mportPackageMeta **pkgs = NULL;
	mportPackageMeta *pkg = NULL;
	int i;
	bool error = false;
	char **dependencies = NULL;
	char **deps = NULL;

		/* There are two scenarios here.  
		   1. We are installing online with an index and have already fetched dependencies. 
		   2. We are installing from a local package file. Check if the dependencies are availaible in the
		   same directory that are missing.
		*/
	if (mport->offline) {
		// temporarily open pkg file to get metadata.
		if ((bundle = mport_bundle_read_new()) == NULL)
			RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");

		if (mport_bundle_read_init(bundle, filename) != MPORT_OK)
			RETURN_CURRENT_ERROR;

		if (mport_bundle_read_prep_for_install(mport, bundle) != MPORT_OK)
			RETURN_CURRENT_ERROR;

		if (mport_pkgmeta_read_stub(mport, &pkgs) != MPORT_OK)
			RETURN_CURRENT_ERROR;

		if (mport_check_preconditions(mport, pkg, MPORT_PRECHECK_INSTALLED) != MPORT_OK) {
			if (mport->force) {
				mport_delete_primative(mport, pkg, 1);
			} else {
				mport_call_msg_cb(mport, "%s-%s: already installed.", pkg->name, pkg->version);
				return MPORT_OK;
			}
		}

		if (mport_check_preconditions(mport, pkg, MPORT_PRECHECK_CONFLICTS) != MPORT_OK) {
			mport_call_msg_cb(mport, "Unable to install %s-%s: %s", pkg->name, pkg->version,
			                  mport_err_string());
			return MPORT_ERR_FATAL;
		}
		dependencies = get_dependencies(mport, pkg);

		// close so we can safely process depdendencies.
		if (mport_bundle_read_finish(mport, bundle) != MPORT_OK)
			return MPORT_ERR_FATAL;

		deps = dependencies;
		char *dir = mport_directory(filename);
		while (deps!= NULL) {
			char *dep_filename = NULL; 
			asprintf(&dep_filename, "%s/%s.mport", dir, *deps);
			if (dep_filename != NULL && mport_install_primative(mport, dep_filename, prefix, MPORT_AUTOMATIC)!= MPORT_OK) {
		                mport_call_msg_cb(mport, "Unable to install %s: %s", *deps, mport_err_string());
                		return MPORT_ERR_FATAL;
			}
			free(dep_filename);
			deps++;
		}
		free(dir);
		free(dependencies);
	}

	if ((bundle = mport_bundle_read_new()) == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");

	if (mport_bundle_read_init(bundle, filename) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	if (mport_bundle_read_prep_for_install(mport, bundle) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	if (mport_pkgmeta_read_stub(mport, &pkgs) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	for (i = 0; *(pkgs + i) != NULL; i++) {
		pkg = pkgs[i];
        pkg->automatic = automatic;
		pkg->install_date = mport_get_time();
		pkg->action = MPORT_ACTION_INSTALL;

		if (prefix != NULL) {
			/* override the default prefix with the given prefix */
			free(pkg->prefix);
			if ((pkg->prefix = strdup(prefix)) == NULL) /* all hope is lost! bail */
				RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");
		}

		if ((mport_check_preconditions(mport, pkg, MPORT_PRECHECK_INSTALLED | MPORT_PRECHECK_DEPENDS | MPORT_PRECHECK_CONFLICTS) != MPORT_OK)
                   || (mport_bundle_read_install_pkg(mport, bundle, pkg) != MPORT_OK)) {
			mport_call_msg_cb(mport, "Unable to install %s-%s: %s", pkg->name, pkg->version,
			                  mport_err_string());
			error = true;
			break; /* do not keep going if we have a package failure! */
		}
	}

	if (mport_bundle_read_finish(mport, bundle) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	if (error)
		return MPORT_ERR_FATAL;

	return MPORT_OK;
}
