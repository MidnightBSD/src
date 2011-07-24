/*-
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

#include <sys/cdefs.h>
__MBSDID("$MidnightBSD: src/usr.sbin/mport/mport.c,v 1.27 2011/06/16 03:22:51 laffer1 Exp $");

#include <stdlib.h>
#include <stdarg.h>

#include "msearch_private.h"

int msearch_db_do(sqlite3 *db, const char *fmt, ...)
{
  va_list args;
  char *sql;
  int sqlcode;

  va_start(args, fmt);

  sql = sqlite3_vmprintf(fmt, args);

  va_end(args);

  if (sql == NULL)
    return 2;

  sqlcode = sqlite3_exec(db, sql, 0, 0, 0);
  /* if we get an error code, we want to run it again in some cases */
  if (sqlcode == SQLITE_BUSY || sqlcode == SQLITE_LOCKED) {
    if (sqlite3_exec(db, sql, 0, 0, 0) != SQLITE_OK) {
      sqlite3_free(sql);
      return 1;
    }
  } else if (sqlcode != SQLITE_OK) {
    sqlite3_free(sql);
    return 1;
  }

  sqlite3_free(sql);

  return 0;
}

int msearch_db_prepare(sqlite3 *db, sqlite3_stmt **stmt, const char * fmt, ...)
{
  va_list args;
  char *sql;
 
  va_start(args, fmt);
  sql = sqlite3_vmprintf(fmt, args);
  va_end(args);
 
  if (sql == NULL)
	return 1;
 
  if (sqlite3_prepare_v2(db, sql, -1, stmt, NULL) != SQLITE_OK) {
    sqlite3_free(sql);
    return 2;
  }
 
  sqlite3_free(sql);
 
  return 0;
}

