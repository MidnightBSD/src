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
 * $MidnightBSD: src/lib/libmport/db_schema.c,v 1.3 2007/09/28 03:01:31 ctriv Exp $
 */



#include <sqlite3.h>
#include <stdlib.h>
#include "mport.h"

__MBSDID("$MidnightBSD: src/lib/libmport/db_schema.c,v 1.3 2007/09/28 03:01:31 ctriv Exp $");

static int run_sql(sqlite3 *db, const char *sql);

#define RUN_SQL(db, sql) \
  if (run_sql(db, sql) != SQLITE_OK) \
    RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db))


int mport_generate_stub_schema(sqlite3 *db) 
{
  RUN_SQL(db, "CREATE TABLE assets    (pkg text not NULL, type int NOT NULL, data text, checksum text)");
  RUN_SQL(db, "CREATE TABLE package   (pkg text NOT NULL, version text NOT NULL, origin text NOT NULL, lang text, options text, date int NOT NULL, prefix text NOT NULL)");
  RUN_SQL(db, "CREATE TABLE conflicts (pkg text NOT NULL, conflict_pkg text NOT NULL, conflict_version text NOT NULL)");
  RUN_SQL(db, "CREATE TABLE depends   (pkg text NOT NULL, depend_pkgname text NOT NULL, depend_pkgversion text NOT NULL, depend_port text NOT NULL)");
    
  return MPORT_OK;  
}

int mport_generate_master_schema(sqlite3 *db) 
{
  RUN_SQL(db, "CREATE TABLE IF NOT EXISTS packages (pkg text NOT NULL, version text NOT NULL, origin text NOT NULL, prefix text NOT NULL, lang text, options text, date int)");
  RUN_SQL(db, "CREATE UNIQUE INDEX IF NOT EXISTS packages_pkg ON packages (pkg)");
  RUN_SQL(db, "CREATE INDEX IF NOT EXISTS packages_origin ON packages (origin)");
  RUN_SQL(db, "CREATE TABLE IF NOT EXISTS depends (pkg text NOT NULL, depend_pkgname text NOT NULL, depend_pkgversion text NOT NULL, depend_port text NOT NULL)");
  RUN_SQL(db, "CREATE INDEX IF NOT EXISTS depends_pkg ON depends (pkg)");
  RUN_SQL(db, "CREATE INDEX IF NOT EXISTS depends_dependpkgname ON depends (depend_pkgname)");
  RUN_SQL(db, "CREATE TABLE IF NOT EXISTS assets (pkg text NOT NULL, type int NOT NULL, data text, checksum text)");
  RUN_SQL(db, "CREATE INDEX IF NOT EXISTS assets_pkg ON assets (pkg)");
  RUN_SQL(db, "CREATE INDEX IF NOT EXISTS assets_data ON assets (data)");
  
  return MPORT_OK;
}

static int run_sql(sqlite3 *db, const char *sql)
{
  char *error = 0;
  
  return sqlite3_exec(
    db,
    sql,
    NULL,
    NULL, 
    &error
  );
}
