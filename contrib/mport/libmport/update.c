/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021, 2026 Lucas Holt
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
#include <string.h>
#include <stdlib.h>

static int migrate_default_package(/*@notnull@*/ mportInstance *,
    /*@notnull@*/ mportPackageMeta *, /*@notnull@*/ const char *);

MPORT_PUBLIC_API int
mport_update(mportInstance *mport, const char *packageName)
{
	char *path = NULL;
	mportDependsEntry **depends = NULL;
	mportDependsEntry **depends_orig = NULL;
	mportIndexEntry **indexEntries = NULL;
	mportIndexEntry *indexEntry = NULL;
	mportPackageMeta **packs_meta = NULL;
	/*@null@*/ mportIndexMovedEntry **movedEntries = NULL;
	char *replacement_path = NULL;
	char *default_pkgname = NULL;
	char *transition_msg = NULL;
	mportIndexEntry **old_index_entries = NULL;
	char expiry[32];
	mportAutomatic automatic = MPORT_EXPLICIT;
	int result;
	int ret;
	int default_result;

	if (packageName == NULL) {
		return (MPORT_ERR_WARN);
	}

	/* Check for MOVE operations: look up the installed package to get its origin,
	 * then consult the moved table in the index. */
	if (mport_pkgmeta_search_master(mport, &packs_meta, "pkg=%Q", packageName) == MPORT_OK &&
	    packs_meta != NULL && *packs_meta != NULL && (*packs_meta)->origin != NULL) {
		if (mport_moved_lookup(mport, (*packs_meta)->origin, &movedEntries) == MPORT_OK &&
		    movedEntries != NULL && *movedEntries != NULL) {
			if ((*movedEntries)->date[0] != '\0') {
				/* Package is deprecated/expired; save date, then warn and refuse.
				 */
				strlcpy(expiry, (*movedEntries)->date, sizeof(expiry));
				mport_call_msg_cb(mport,
				    "Package %s is deprecated and expired on %s\n", packageName,
				    expiry);
				mport_index_moved_entry_free_vec(movedEntries);
				mport_pkgmeta_vec_free(packs_meta);
				return SET_ERRORX(MPORT_ERR_WARN,
				    "Package %s is deprecated (expiry: %s)", packageName, expiry);
			}

			if ((*movedEntries)->moved_to_pkgname[0] != '\0') {
				/* Package has been moved to a new name; migrate it. */
				automatic = (*packs_meta)->automatic;
				mport_call_msg_cb(mport,
				    "Package %s has moved to %s. Migrating to %s\n", packageName,
				    (*movedEntries)->moved_to_pkgname,
				    (*movedEntries)->moved_to_pkgname);
				ret = mport_download(mport, (*movedEntries)->moved_to_pkgname,
				    false, false, &replacement_path);
				if (ret != MPORT_OK) {
					mport_index_moved_entry_free_vec(movedEntries);
					mport_pkgmeta_vec_free(packs_meta);
					return ret;
				}
				if (replacement_path == NULL) {
					mport_index_moved_entry_free_vec(movedEntries);
					mport_pkgmeta_vec_free(packs_meta);
					return SET_ERROR(
					    MPORT_ERR_FATAL, "Downloaded package path is missing");
				}
				(*packs_meta)->action = MPORT_ACTION_UPGRADE;
				ret = mport_delete_primative(mport, *packs_meta, 1);
				if (ret == MPORT_OK)
					ret = mport_install_primative(
					    mport, replacement_path, NULL, automatic);
				free(replacement_path);
				mport_index_moved_entry_free_vec(movedEntries);
				mport_pkgmeta_vec_free(packs_meta);
				return ret;
			}
		}
		mport_index_moved_entry_free_vec(movedEntries);
		movedEntries = NULL;
	}

	if (packs_meta != NULL && *packs_meta != NULL) {
		default_result = mport_index_resolve_default_pkgname(
		    mport, (*packs_meta)->name, &default_pkgname);
		if (default_result != MPORT_OK) {
			mport_pkgmeta_vec_free(packs_meta);
			return default_result;
		}
	}
	if (default_pkgname != NULL) {
		bool use_default = true;

		if (mport_index_lookup_pkgname(mport, (*packs_meta)->name, &old_index_entries) !=
		    MPORT_OK) {
			result = mport_err_code();
			free(default_pkgname);
			mport_pkgmeta_vec_free(packs_meta);
			return result;
		}

		if (old_index_entries != NULL && *old_index_entries != NULL) {
			if (asprintf(&transition_msg,
				"%s is installed from an older language default. "
				"Switch to %s?",
				(*packs_meta)->name, default_pkgname) == -1) {
				transition_msg = NULL;
				result = SET_ERROR(MPORT_ERR_FATAL, "Out of memory");
				mport_index_entry_free_vec(old_index_entries);
				free(default_pkgname);
				mport_pkgmeta_vec_free(packs_meta);
				return result;
			}
			use_default = mport_call_confirm_cb(
			    mport, transition_msg, "Switch", "Keep current", 0);
			free(transition_msg);
			transition_msg = NULL;
		} else {
			mport_call_msg_cb(mport,
			    "Package %s is not available for the old language default; "
			    "switching to %s\n",
			    (*packs_meta)->name, default_pkgname);
		}
		mport_index_entry_free_vec(old_index_entries);
		old_index_entries = NULL;

		if (use_default) {
			result = migrate_default_package(mport, *packs_meta, default_pkgname);
			free(default_pkgname);
			mport_pkgmeta_vec_free(packs_meta);
			return result;
		}
	}
	free(default_pkgname);
	default_pkgname = NULL;
	mport_pkgmeta_vec_free(packs_meta);
	packs_meta = NULL;

	if (mport_index_select_pkgname(mport, packageName, "Multiple packages match your query.",
		&indexEntries, &indexEntry) != MPORT_OK)
		return mport_err_code();

	result = mport_download(
	    mport, indexEntry == NULL ? packageName : indexEntry->pkgname, false, false, &path);
	if (result != MPORT_OK) {
		mport_index_entry_free_vec(indexEntries);
		return result;
	}

	/* in the event the package is not found in the index, it could be user generated, and we
	   still want to update it if present */
	if (indexEntry == NULL) {
		mport_call_msg_cb(mport, "Package %s not found in the index\n", packageName);
	} else {
		/* get the dependency list and start updating/installing missing entries */
		if (mport_index_depends_list(mport, indexEntry->pkgname, indexEntry->version,
			&depends_orig) != MPORT_OK) {
			mport_call_msg_cb(mport, "Failed to get dependency list for %s: %s\n",
			    packageName, mport_err_string());
			result = mport_err_code();
			mport_index_entry_free_vec(indexEntries);
			free(path);
			return result;
		}

		depends = depends_orig;
		while (depends != NULL && *depends != NULL) {
			if (mport_install_depends(mport, (*depends)->d_pkgname,
				(*depends)->d_version, MPORT_AUTOMATIC) != MPORT_OK) {
				mport_call_msg_cb(mport, "%s", mport_err_string());

				if (mport->ignoreMissing) {
					mport_call_msg_cb(mport,
					    "Ignoring missing dependency %s-%s\n",
					    (*depends)->d_pkgname, (*depends)->d_version);
					depends++;
					continue;
				}

				result = mport_err_code();
				mport_index_depends_free_vec(depends_orig);
				mport_index_entry_free_vec(indexEntries);
				free(path);
				return result;
			}
			depends++;
		}

		mport_index_depends_free_vec(depends_orig);
		depends_orig = NULL;
		depends = NULL;
	}

	if (mport_update_primative(mport, path) != MPORT_OK) {
		mport_call_msg_cb(mport, "%s\n", mport_err_string());
		free(path);
		path = NULL;
		mport_index_entry_free_vec(indexEntries);
		return mport_err_code();
	}

	free(path);
	path = NULL;
	mport_index_entry_free_vec(indexEntries);

	return (MPORT_OK);
}

static int
migrate_default_package(
    /*@notnull@*/ mportInstance *mport, /*@notnull@*/ mportPackageMeta *installed,
    /*@notnull@*/ const char *target_pkgname)
{
	mportDependsEntry **depends = NULL;
	mportDependsEntry **depends_orig = NULL;
	mportIndexEntry **entries = NULL;
	mportIndexEntry *entry = NULL;
	char *path = NULL;
	mportAutomatic automatic;
	int ret;

	if (mport_lock_islocked(installed) == MPORT_LOCKED)
		return SET_ERRORX(
		    MPORT_ERR_WARN, "Unable to replace %s: package is locked", installed->name);

	if (mport_index_select_pkgname(mport, target_pkgname,
		"Multiple packages match the default version target.", &entries,
		&entry) != MPORT_OK)
		return mport_err_code();
	if (entry == NULL) {
		mport_index_entry_free_vec(entries);
		return SET_ERRORX(MPORT_ERR_WARN,
		    "Default version package %s was not found in the index", target_pkgname);
	}

	if (mport_index_depends_list(mport, entry->pkgname, entry->version, &depends_orig) !=
	    MPORT_OK) {
		ret = mport_err_code();
		mport_index_entry_free_vec(entries);
		return ret;
	}
	for (depends = depends_orig; depends != NULL && *depends != NULL; depends++) {
		if (mport_install_depends(mport, (*depends)->d_pkgname, (*depends)->d_version,
			MPORT_AUTOMATIC) != MPORT_OK) {
			ret = mport_err_code();
			mport_index_depends_free_vec(depends_orig);
			mport_index_entry_free_vec(entries);
			return ret;
		}
	}
	mport_index_depends_free_vec(depends_orig);
	depends_orig = NULL;

	if (mport_download(mport, entry->pkgname, false, false, &path) != MPORT_OK) {
		ret = mport_err_code();
		mport_index_entry_free_vec(entries);
		return ret;
	}
	if (path == NULL) {
		mport_index_entry_free_vec(entries);
		return SET_ERROR(MPORT_ERR_FATAL, "Downloaded package path is missing");
	}

	mport_call_msg_cb(mport, "Replacing %s with default version package %s\n", installed->name,
	    entry->pkgname);
	automatic = installed->automatic;
	installed->action = MPORT_ACTION_UPGRADE;
	ret = mport_delete_primative(mport, installed, 1);
	if (ret == MPORT_OK)
		ret = mport_install_primative(mport, path, NULL, automatic);

	free(path);
	mport_index_entry_free_vec(entries);
	return ret;
}
