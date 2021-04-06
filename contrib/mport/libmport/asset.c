/*-
 * Copyright (c) 2018 Lucas Holt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *  notice, this list of conditions and the following disclaimer in the
 *  documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.	IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <sqlite3.h>
#include <stdlib.h>
#include "mport.h"
#include "mport_private.h"

MPORT_PUBLIC_API int
mport_asset_get_package_from_file_path(mportInstance *mport, const char *filePath, mportPackageMeta **pack)
{
	sqlite3_stmt *stmt = NULL;
	int result = MPORT_OK;
	char *err;

	if (mport_db_prepare(mport->db, &stmt, "SELECT pkg FROM assets WHERE data=%Q", filePath) != MPORT_OK) {
		sqlite3_finalize(stmt);
		RETURN_CURRENT_ERROR;
	}

	if (stmt == NULL) {
		RETURN_ERROR(MPORT_ERR_FATAL, "Statement was null");
	}

	while (1) {
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

		const unsigned char *pkgName = sqlite3_column_text(stmt, 0);
		if (pkgName != NULL) {
			mportPackageMeta **packs;
			if (mport_pkgmeta_search_master(mport, &packs, "pkg=%Q", pkgName) != MPORT_OK || packs[0] == NULL) {
				err = "Package does not exist despite having assets";
				result = MPORT_ERR_FATAL;
				break; // we finalize below
			} else {
				*pack = packs[0];
				result = MPORT_OK;
				break;
			}
		}
	}

	sqlite3_finalize(stmt);

	if (result == MPORT_ERR_FATAL)
		SET_ERRORX(result, "Error reading assets %s", err);
	return result;
}

MPORT_PUBLIC_API int
mport_asset_get_assetlist(mportInstance *mport, mportPackageMeta *pack, mportAssetList **alist_p)
{
	mportAssetList *alist;
	sqlite3_stmt *stmt = NULL;
	int result = MPORT_OK;
	char *err;

	if ((alist = mport_assetlist_new()) == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");

	*alist_p = alist;

	// pkg text NOT NULL, type int NOT NULL, data text, checksum text, owner text, grp text, mode text)

	if (mport_db_prepare(mport->db, &stmt, "SELECT type,data,checksum,owner,grp,mode FROM assets WHERE pkg=%Q",
	                     pack->name) != MPORT_OK) {
		sqlite3_finalize(stmt);
		RETURN_CURRENT_ERROR;
	}

	if (stmt == NULL) {
		RETURN_ERROR(MPORT_ERR_FATAL, "Statement was null");
	}


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

	if (result == MPORT_ERR_FATAL)
		SET_ERRORX(result, "Error reading assets %s", err);
	return result;
}
