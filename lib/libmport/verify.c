/*-
 * Copyright (c) 2012 Lucas Holt
 * Copyright (c) 2007-2009 Chris Reinhardt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	  notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	  notice, this list of conditions and the following disclaimer in the
 *	  documentation and/or other materials provided with the distribution.
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
__MBSDID("$MidnightBSD: src/lib/libmport/verify.c,v 1.1 2012/01/25 03:43:11 laffer1 Exp $");

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <sqlite3.h>
#include <md5.h>
#include <stdlib.h>
#include "mport.h"
#include "mport_private.h"

MPORT_PUBLIC_API int
mport_verify_package(mportInstance *mport, mportPackageMeta *pack) {
	sqlite3_stmt *stmt;
	mportAssetListEntryType type;
	int current;
	int total;
	int ret;
	const char *data, *checksum;
	struct stat st;
	char md5[33];
	
	/* get the file count for the progress meter */
	if (mport_db_prepare(mport->db, &stmt, "SELECT COUNT(*) FROM assets WHERE type=%i AND pkg=%Q", ASSET_FILE, pack->name) != MPORT_OK)
		RETURN_CURRENT_ERROR;
		
	switch (sqlite3_step(stmt)) {
		case SQLITE_ROW:
			total		= sqlite3_column_int(stmt, 0) + 1;
			current = 0;
			sqlite3_finalize(stmt);
			break;
		default:
			SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(mport->db));
			sqlite3_finalize(stmt);
			RETURN_CURRENT_ERROR;
	}
	
	mport_call_progress_init_cb(mport, "Verifying %s-%s", pack->name, pack->version);
	
	while (1) {
		ret = sqlite3_step(stmt);
		
		if (ret == SQLITE_DONE)
			break;
			
		if (ret != SQLITE_ROW) {
			/* some error occured */
			SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(mport->db));
			sqlite3_finalize(stmt);
			RETURN_CURRENT_ERROR;
		}
		
		type		 = (mportAssetListEntryType)sqlite3_column_int(stmt, 0);
		data		 = sqlite3_column_text(stmt, 1);
		checksum = sqlite3_column_text(stmt, 2);

		char file[FILENAME_MAX];
		/* XXX TMP */
		if (data == NULL) {
				/* XXX data is null when ASSET_CHMOD (mode) or similar commands are in plist */
			snprintf(file, sizeof(file), "%s", mport->root);
		} else if (*data == '/') {
			snprintf(file, sizeof(file), "%s%s", mport->root, data);
		} else {
			snprintf(file, sizeof(file), "%s%s/%s", mport->root, pack->prefix, data);
		}

		switch (type) {
			case ASSET_FILE:
				(mport->progress_step_cb)(++current, total, file);
	
				if (lstat(file, &st) != 0) {
					mport_call_msg_cb(mport, "Can't stat %s: %s", file, strerror(errno));
					break; /* next asset */
				} 				
				
				if (S_ISREG(st.st_mode)) {
					if (MD5File(file, md5) == NULL) 
						mport_call_msg_cb(mport, "Can't md5 %s: %s", file, strerror(errno));
					
					if (strcmp(md5, checksum) != 0) 
						mport_call_msg_cb(mport, "Checksum mismatch: %s", file);
				}
				
				break;
			default:
				/* do nothing */
				break;
		}
	}

	sqlite3_finalize(stmt);
	
	(mport->progress_step_cb)(++current, total, "Complete");
	(mport->progress_free_cb)();

	return MPORT_OK;
}
