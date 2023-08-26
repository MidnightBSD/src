/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2011, 2023 Lucas Holt
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

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>

MPORT_PUBLIC_API int
mport_clean_database(mportInstance *mport)
{
	int error_code = MPORT_OK;

	if (mport_db_do(mport->db, "vacuum") != MPORT_OK) {
		error_code = mport_err_code();
		mport_call_msg_cb(mport, "Database maintenance failed: %s\n", mport_err_string());
	} else {
		mport_call_msg_cb(mport, "Database maintenance complete.\n");
	}

	return error_code;
}

MPORT_PUBLIC_API int
mport_clean_oldpackages(mportInstance *mport)
{
	int error_code = MPORT_OK;


	int deleted = 0;
	struct dirent *de;
	DIR *d = opendir(MPORT_FETCH_STAGING_DIR);

	if (d == NULL) {
		error_code = SET_ERRORX(MPORT_ERR_FATAL, "Couldn't open directory %s: %s", MPORT_FETCH_STAGING_DIR,
		                        strerror(errno));
		return error_code;
	}

	while ((de = readdir(d)) != NULL) {
		mportIndexEntry **indexEntry;
		char *path;
		if (strcmp(".", de->d_name) == 0 || strcmp("..", de->d_name) == 0)
			continue;

		if (mport_index_search(mport, &indexEntry, "bundlefile=%Q", de->d_name) != MPORT_OK) {
			mport_call_msg_cb(mport, "failed to search index %s: ", mport_err_string());
			continue;
		}

		asprintf(&path, "%s/%s", MPORT_FETCH_STAGING_DIR, de->d_name);
		if (path == NULL) {
			if (indexEntry != NULL) {
				mport_index_entry_free_vec(indexEntry);
				indexEntry = NULL;
			}
			continue;
		}

		if (indexEntry == NULL || *indexEntry == NULL) {
			if (unlink(path) < 0) {
				error_code = SET_ERRORX(MPORT_ERR_FATAL, "Could not delete file %s: %s",
				                        path, strerror(errno));
				mport_call_msg_cb(mport, "%s\n", mport_err_string());
			} else {
				deleted++;
			}
		} else if (mport_verify_hash(path, (*indexEntry)->hash) == 0) {
			if (unlink(path) < 0) {
				error_code = SET_ERRORX(MPORT_ERR_FATAL, "Could not delete file %s: %s", path, strerror(errno));
				mport_call_msg_cb(mport, "%s\n", mport_err_string());
			} else {
				deleted++;
			}
			mport_index_entry_free_vec(indexEntry);
			indexEntry = NULL;
		} else {
			mport_index_entry_free_vec(indexEntry);
			indexEntry = NULL;
		}

		free(path);
		path = NULL;
	}

	closedir(d);

	mport_call_msg_cb(mport, "Cleaned up %d packages.\n", deleted);

	return error_code;
}

MPORT_PUBLIC_API int
mport_clean_oldmtree(mportInstance *mport)
{
	int error_code = MPORT_OK;

	int deleted = 0;
	struct dirent *de;
	DIR *d = opendir(MPORT_INST_INFRA_DIR);
	
	if (d == NULL) {
		error_code = SET_ERRORX(MPORT_ERR_FATAL, "Couldn't open directory %s: %s", MPORT_INST_INFRA_DIR,
		                        strerror(errno));
		return error_code;
	}

	while ((de = readdir(d)) != NULL) {
		mportPackageMeta **packs;
		char *path;
		if (strcmp(".", de->d_name) == 0 || strcmp("..", de->d_name) == 0)
			continue;

		char packageName[128];
		strncpy(packageName, de->d_name, 127);
		packageName[127] = '\0';
		char *dash = strrchr(packageName, '-');
		if (dash != NULL) {
			*dash = '\0';
		}

		if (mport_pkgmeta_search_master(mport, &packs, "pkg=%Q", packageName) != MPORT_OK) {
			mport_call_msg_cb(mport, "failed to search master database for %s: ", mport_err_string());
			continue;
		}

		asprintf(&path, "%s/%s", MPORT_INST_INFRA_DIR, de->d_name);
		if (path == NULL) {
			if (packs != NULL) {
				mport_pkgmeta_vec_free(packs);
				packs = NULL;
			}
			continue;
		}

		if (packs == NULL || *packs == NULL) {
			if (mport_rmtree(path) != MPORT_OK) {
				error_code = SET_ERRORX(MPORT_ERR_FATAL, "Could not delete file %s: %s", path, strerror(errno));
				mport_call_msg_cb(mport, "%s\n", mport_err_string());
			} else {
				deleted++;
			}
		} else {
			mport_pkgmeta_vec_free(packs);
			packs = NULL;
		}

		free(path);
		path = NULL;
	}

	closedir(d);

	mport_call_msg_cb(mport, "Cleaned up %d mtrees.\n", deleted);

	return error_code;
}

MPORT_PUBLIC_API int
mport_clean_tempfiles(mportInstance *mport)
{
	int error_code = MPORT_OK;

	int deleted = 0;
	struct dirent *de;
	DIR *d = opendir("/tmp");
	
	if (d == NULL) {
		error_code = SET_ERRORX(MPORT_ERR_FATAL, "Couldn't open directory %s: %s", MPORT_INST_INFRA_DIR,
		                        strerror(errno));
		return error_code;
	}

	while ((de = readdir(d)) != NULL) {
		char *path;
		if (strcmp(".", de->d_name) == 0 || strcmp("..", de->d_name) == 0)
			continue;

		if (!mport_starts_with("mport.", de->d_name))
	       		continue;


		asprintf(&path, "%s/%s", "/tmp", de->d_name);
		if (path == NULL) {
			continue;
		}

		int result = unlink(path);

		if (result != 0) {
			error_code = SET_ERRORX(MPORT_ERR_FATAL, "Could not delete file %s: %s", path, strerror(errno));
			mport_call_msg_cb(mport, "%s\n", mport_err_string());
		} else {
			deleted++;
		}
	 
		free(path);
		path = NULL;
	}

	closedir(d);

	mport_call_msg_cb(mport, "Cleaned up %d temporary files.\n", deleted);

	return error_code;
}
