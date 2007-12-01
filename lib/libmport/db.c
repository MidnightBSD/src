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
 * $MidnightBSD: src/lib/libmport/db_schema.c,v 1.4 2007/11/22 08:00:32 ctriv Exp $
 */



#include <sqlite3.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "mport.h"

__MBSDID("$MidnightBSD: src/lib/libmport/db_schema.c,v 1.4 2007/11/22 08:00:32 ctriv Exp $");


static int populate_meta_from_stmt(mportPackageMeta *, sqlite3 *, sqlite3_stmt *);


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



/* mport_db_prepare(sqlite3 *, sqlite3_stmt **, const char *, ...)
 * 
 * A wrapper for preparing sqlite statements into statement structs.
 * This function returns MPORT_OK on success.  The sqlite3_stmt pointer 
 * may be null if this function does not return MPORT_OK.
 */
int mport_db_prepare(sqlite3 *db, sqlite3_stmt **stmt, const char * fmt, ...)
{
  va_list args;
  char *sql;
  
  va_start(args, fmt);
  
  if ((sql = sqlite3_vmprintf(fmt, args)) == NULL) {
    return MPORT_ERR_NO_MEM;
  }
  
  if (sqlite3_prepare_v2(db, sql, -1, stmt, NULL) != SQLITE_OK) {
    sqlite3_free(sql);
    RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
  }
  
  sqlite3_free(sql);
  
  return MPORT_OK;
}

  

/* mport_attach_stub_db(sqlite *db, const char *tmpdir) 
 *
 * Attaches tmpdir/MPORT_STUB_DB_FILE to the given database handle as 
 * 'stub'.  (stub.table to access a table in the stub db)
 *
 * Returns MPORT_OK on success.
 */
int mport_attach_stub_db(sqlite3 *db, const char *dir)
{
  char *file;
  asprintf(&file, "%s/%s", dir, MPORT_STUB_DB_FILE);
  
  if (mport_db_do(db, "ATTACH %Q AS stub", file) != MPORT_OK) { 
    RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
  }
  
  free(file);
  
  return MPORT_OK;
}


/* mport_get_meta_from_stub(sqlite *db, mportPackageMeta ***pack)
 *
 * Allocates and populates a vector of mportPackageMeta structs from the stub database
 * connected to db. These structs represent all the packages in the stub database.
 * This does not populate the conflicts and depends fields.
 */
int mport_get_meta_from_stub(sqlite3 *db, mportPackageMeta ***ref)
{
  sqlite3_stmt *stmt;
  int len;
  mportPackageMeta **vec;
  
  if (mport_db_prepare(db, &stmt, "SELECT COUNT(*) FROM stub.packages") != MPORT_OK)
    RETURN_CURRENT_ERROR;
  
  len = sqlite3_column_int(stmt, 0);
  sqlite3_finalize(stmt);
  
  vec = (mportPackageMeta**)malloc((1+len) * sizeof(mportPackageMeta *));
  *ref = vec;
  
  if (vec == NULL) 
    return MPORT_ERR_NO_MEM;
    
  if (mport_db_prepare(db, &stmt, "SELECT pkg, version, origin, lang, prefix FROM stub.packages") != MPORT_OK)
    RETURN_CURRENT_ERROR;
  
  while (*vec != NULL) { 
    switch (sqlite3_step(stmt)) {
      case SQLITE_ROW:
        *vec = mport_new_packagemeta();
        if (*vec == NULL)
          return MPORT_ERR_NO_MEM;
        if (populate_meta_from_stmt(*vec, db, stmt) != MPORT_OK)
          RETURN_CURRENT_ERROR;
        vec++;
        break;
      case SQLITE_DONE:
        /* set the last cell in the array to null */
        *vec = NULL;
        break;
      default:
        sqlite3_finalize(stmt);
        RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
        break; /* not reached */
    }
  }
  
  sqlite3_finalize(stmt);
  
  return MPORT_OK;
}


/* mport_get_meta_from_master(sqlite3 *db, mportPacakgeMeta **pack, const char *name)
 *
 * Allocate and populate the package meta for the given package from the
 * master database.
 * 
 * pack is set to NULL if package 'name' is not installed
 */
int mport_get_meta_from_master(sqlite3 *db, mportPackageMeta **pack, const char *name)
{
  sqlite3_stmt *stmt;

  if (mport_db_prepare(db, &stmt, "SELECT pkg, version, lang, prefix FROM packages WHERE pkg=%Q", name) != MPORT_OK)
    RETURN_CURRENT_ERROR;
  
  switch (sqlite3_step(stmt)) {
    case SQLITE_ROW:
      *pack = mport_new_packagemeta();
      if (*pack == NULL)
        return MPORT_ERR_NO_MEM;
      return populate_meta_from_stmt(*pack, db, stmt);
      break;
    case SQLITE_DONE:
      *pack = NULL;
      sqlite3_finalize(stmt);
      return MPORT_OK;
      break;
    default:
      sqlite3_finalize(stmt);
      RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
      break;
  }
  
  /* no reached */
  return MPORT_OK;
}
  
 
 
static int populate_meta_from_stmt(mportPackageMeta *pack, sqlite3 *db, sqlite3_stmt *stmt) 
{  
  const char *tmp = 0;

  /* Copy pkg to pack->name */
  if ((tmp = sqlite3_column_text(stmt, 0)) == NULL) 
    RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));

  if ((pack->name = strdup(tmp)) == NULL)
    return MPORT_ERR_NO_MEM;

  /* Copy version to pack->version */
  if ((tmp = sqlite3_column_text(stmt, 1)) == NULL) 
    RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
  
  if ((pack->version = strdup(tmp)) == NULL)
    return MPORT_ERR_NO_MEM;
  
  /* Copy origin to pack->origin */
  if ((tmp = sqlite3_column_text(stmt, 2)) == NULL) 
    RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
  
  if ((pack->origin = strdup(tmp)) == NULL)
    return MPORT_ERR_NO_MEM;

  /* Copy lang to pack->lang */
  if ((tmp = sqlite3_column_text(stmt, 3)) == NULL) 
    RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
  
  if ((pack->lang = strdup(tmp)) == NULL)
    return MPORT_ERR_NO_MEM;

  /* Copy prefix to pack->prefix */
  if ((tmp = sqlite3_column_text(stmt, 4)) == NULL) 
    RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
  
  if ((pack->prefix = strdup(tmp)) == NULL)
    return MPORT_ERR_NO_MEM;

  
  return MPORT_OK;
}



#define RUN_SQL(db, sql) \
  if (mport_db_do(db, sql) != MPORT_OK) \
    RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db))


int mport_generate_stub_schema(sqlite3 *db) 
{
  RUN_SQL(db, "CREATE TABLE assets    (pkg text not NULL, type int NOT NULL, data text, checksum text)");
  RUN_SQL(db, "CREATE TABLE packages  (pkg text NOT NULL, version text NOT NULL, origin text NOT NULL, lang text, options text, date int NOT NULL, prefix text NOT NULL)");
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
