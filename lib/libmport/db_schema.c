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
 * $MidnightBSD$
 */



#include <sqlite3.h>
#include <stdlib.h>
#include "mport.h"

__MBSDID("$MidnightBSD: src/usr.sbin/pkg_install/lib/plist.c,v 1.50.2.1 2006/01/10 22:15:06 krion Exp $");

static void run_sql(sqlite3 *db, const char *sql);

void mport_generate_package_schema(sqlite3 *db) 
{
  run_sql(db, "CREATE TABLE assets (pkg text not NULL, type int NOT NULL, data text, checksum text)");
  run_sql(db, "CREATE TABLE package  (pkg text NOT NULL, version text NOT NULL, lang text, options text, date int NOT NULL)");
  run_sql(db, "CREATE TABLE conflicts (pkg text NOT NULL, conflict text NOT NULL)");
  run_sql(db, "CREATE TABLE depends   (pkg text NOT NULL, depend text NOT NULL)");
}

static void run_sql(sqlite3 *db, const char *sql)
{
  char *error = 0;
  
  sqlite3_exec(
    db,
    sql,
    NULL,
    NULL, 
    &error
  );
}
