/*-
 * Copyright (c) 2007 Chris Reinhardt
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
 * $MidnightBSD: src/lib/libmport/util.c,v 1.6 2007/09/28 03:01:31 ctriv Exp $
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "mport.h"

__MBSDID("$MidnightBSD: src/lib/libmport/util.c,v 1.6 2007/09/28 03:01:31 ctriv Exp $");

/* mport_db_do(sqlite3 *db, const char *sql, ...)
 * 
 * A wrapper for doing executing a single sql query.  Takes a sqlite3 struct
 * pointer, a format string and a list of args.  See the documentation for 
 * sqlite3_vmprintf() for format information.
 */
int mport_db_do(sqlite3 *db, const char *fmt, ...) 
{
  va_list args;
  char *sql;
  
  va_start(args, fmt);
  
  if ((sql = sqlite3_vmprintf(fmt, args)) == NULL) {
    return MPORT_ERR_NO_MEM;
  }
  
  if (sqlite3_exec(db, sql, 0, 0, 0) != SQLITE_OK) {
    sqlite3_free(sql);
    RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
  }
  
  sqlite3_free(sql);
  
  return MPORT_OK;
}
  



