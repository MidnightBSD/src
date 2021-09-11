/*-
 * Copyright (c) 2013 Lucas Holt
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
#include <sys/stat.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>


/*
 * Looks up dependencies list via pkgname and version from the index and fills with a vector of depends entries
 * with the result.
 *
 * The calling code is responsible for freeing the memory allocated.  See
 * mport_index_depends_free_vec()
 */
MPORT_PUBLIC_API int
mport_index_depends_list(mportInstance *mport, const char *pkgname, const char *version, mportDependsEntry ***entry_vec)
{
	int count, i = 0, step;
	sqlite3_stmt *stmt;
	int ret = MPORT_OK;
	mportDependsEntry **e;
  
	MPORT_CHECK_FOR_INDEX(mport, "mport_index_depends_list()")

	if (mport_db_prepare(mport->db, &stmt,
	    "SELECT COUNT(*) FROM idx.depends WHERE pkg = %Q and version = %Q",
	    pkgname, version) != MPORT_OK) {
		sqlite3_finalize(stmt);
		RETURN_CURRENT_ERROR;
	}
 
	switch (sqlite3_step(stmt)) {
	case SQLITE_ROW:
		count = sqlite3_column_int(stmt, 0);
		break;
	case SQLITE_DONE:
		ret = SET_ERROR(MPORT_ERR_FATAL,
		    "No rows returned from a 'SELECT COUNT(*)' query.");
		goto DONE;
	default:
		ret = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(mport->db));
		goto DONE;
	}
  
	sqlite3_finalize(stmt);
  
	e = (mportDependsEntry **)calloc(count + 1, sizeof(mportDependsEntry *));
	*entry_vec = e;
  
	if (count == 0)  {
		return (MPORT_OK);
	}
  
	if (mport_db_prepare(mport->db, &stmt,
	    "SELECT pkg, version, d_pkg, d_version FROM idx.depends WHERE pkg= %Q and version=%Q", pkgname, version) != MPORT_OK) {
		ret = mport_err_code();
		goto DONE;
	}
  
	while (1) {
		step = sqlite3_step(stmt);
    
		if (step == SQLITE_ROW) {
			if ((e[i] = (mportDependsEntry *)calloc(1, sizeof(mportDependsEntry))) == NULL) {
				ret = MPORT_ERR_FATAL;
				goto DONE;
			}
      
			e[i]->pkgname    = strdup(sqlite3_column_text(stmt, 0));
			e[i]->version    = strdup(sqlite3_column_text(stmt, 1));
			e[i]->d_pkgname  = strdup(sqlite3_column_text(stmt, 2));
			e[i]->d_version  = strdup(sqlite3_column_text(stmt, 3));
      
			if (e[i]->pkgname == NULL ||
			    e[i]->version == NULL ||
			    e[i]->d_pkgname == NULL ||
			    e[i]->d_version == NULL) {
				ret = MPORT_ERR_FATAL;
				goto DONE;
			}

			i++;
		} else if (step == SQLITE_DONE) {
			e[i] = NULL;
			goto DONE;
		} else {
			ret = SET_ERROR(MPORT_ERR_FATAL,
			    sqlite3_errmsg(mport->db));
			goto DONE;
		}
	}
      
DONE:
	sqlite3_finalize(stmt);
	return ret; 
}


/* free a vector of mportDependsEntry structs */
MPORT_PUBLIC_API void
mport_index_depends_free_vec(mportDependsEntry **e)
{

	if (e == NULL)
		return;
  
	for (int i = 0; e[i] != NULL; i++) {
		mport_index_depends_free(e[i]);
		e[i] = NULL;
	}

	free(e);
}


/* free a mportDependsEntry struct */
MPORT_PUBLIC_API void
mport_index_depends_free(mportDependsEntry *e) 
{
	if (e == NULL)
		return;

	free(e->pkgname);
	free(e->d_pkgname);
	free(e->version);
	free(e->d_version);
	free(e);
}
