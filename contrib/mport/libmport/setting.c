/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2011 Lucas Holt
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
 *
 */

#include "mport.h"
#include "mport_private.h"
#include <stdlib.h>
#include <string.h>

MPORT_PUBLIC_API char *
mport_setting_get(mportInstance *mport, const char *name) {
    sqlite3_stmt *stmt;
   char *val = NULL;

    if (name == NULL)
        return NULL;

    if (mport_db_prepare(mport->db, &stmt, "SELECT val FROM settings WHERE name=%Q", name) != MPORT_OK) {
		sqlite3_finalize(stmt);
		return NULL;
	}

	switch (sqlite3_step(stmt)) {
		case SQLITE_ROW:
			val = strdup((const char *) sqlite3_column_text(stmt, 0));
			sqlite3_finalize(stmt);
			break;
		case SQLITE_DONE:
			SET_ERROR(MPORT_ERR_FATAL, "Setting not found.");
			sqlite3_finalize(stmt);
			break;
		default:
			SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(mport->db));
			sqlite3_finalize(stmt);
	}

    return val;
}

MPORT_PUBLIC_API int
mport_setting_set(mportInstance *mport, const char *name, const char *val) {
    char *tmpval;

    tmpval = mport_setting_get(mport, name);
    if (tmpval == NULL) {
        if (mport_db_do(mport->db, "INSERT INTO settings (name, val) VALUES(%Q, %Q)", name, val) != MPORT_OK)
            RETURN_CURRENT_ERROR;
    } else {
        free(tmpval);
        if (mport_db_do(mport->db, "UPDATE settings set val=%Q where name=%Q", val, name) != MPORT_OK)
            RETURN_CURRENT_ERROR;
    }

    return MPORT_OK;
}

