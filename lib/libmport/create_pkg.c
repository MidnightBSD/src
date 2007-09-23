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



#include <sys/cdefs.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sqlite3.h>
#include <errno.h>
#include <md5.h>
#include "mport.h"

__MBSDID("$MidnightBSD: src/usr.sbin/pkg_install/lib/plist.c,v 1.50.2.1 2006/01/10 22:15:06 krion Exp $");

#define PACKAGE_DB_FILENAME "+CONTENTS.db"

static int create_package_db(sqlite3 **);
static int create_plist(sqlite3 *, mportPlist *, mportPackageMeta *);
static int create_meta(sqlite3 *, mportPackageMeta *);
static int tar_files(mportPlist *, mportPackageMeta *);
static int clean_up(const char *);

int mport_create_pkg(mportPlist *plist, mportPackageMeta *pack)
{
  /* create a temp dir to hold our meta files. */
  char dirtmpl[] = "/tmp/mport.XXXXXXXX"; 
  char *tmpdir   = mkdtemp(dirtmpl);
  
  int ret = 0;
  sqlite3 *db;
  
  if (tmpdir == NULL) 
    RETURN_ERROR(MPORT_ERR_FILEIO, strerror(errno));
    
  if (chdir(tmpdir) != 0) 
    RETURN_ERROR(MPORT_ERR_FILEIO, strerror(errno));

  if ((ret = create_package_db(&db)) != MPORT_OK)
    return ret;
    
  if ((ret = create_plist(db, plist, pack)) != MPORT_OK)
    return ret;
  
  if ((ret = create_meta(db, pack)) != MPORT_OK)
    return ret;
    
  if (sqlite3_close(db) != SQLITE_OK)
    RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
    
  if ((ret = tar_files(plist, pack)) != MPORT_OK)
    return ret;
    
  if ((ret = clean_up(tmpdir)) != MPORT_OK)
    return ret;
  
  return MPORT_OK;    
}


static int create_package_db(sqlite3 **db) 
{
  if (sqlite3_open(PACKAGE_DB_FILENAME, db) != 0) {
    sqlite3_close(*db);
    RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(*db));
  }
  
  /* create tables */
  mport_generate_package_schema(*db);
  
  return MPORT_OK;
}

static int create_plist(sqlite3 *db, mportPlist *plist, mportPackageMeta *pack)
{
  mportPlistEntry *e;
  sqlite3_stmt *stmnt;
  const char *rest  = 0;
  int ret;
  char sql[]  = "INSERT INTO assets (pkg, type, data, checksum) VALUES (?,?,?,?)";
  char md5[33];
  char file[FILENAME_MAX];
  char cwd[FILENAME_MAX];
  
  strlcpy(cwd, pack->sourcedir, FILENAME_MAX);
  strlcat(cwd, pack->prefix, FILENAME_MAX);
  
  if (sqlite3_prepare_v2(db, sql, -1, &stmnt, &rest) != SQLITE_OK) {
    RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
  }
  
  STAILQ_FOREACH(e, plist, next) {
    if (e->type == PLIST_CWD) {
      strlcpy(cwd, pack->sourcedir, FILENAME_MAX);
      if (e->data != NULL) 
        strlcat(cwd, e->data, FILENAME_MAX);
    }
    
    if (sqlite3_bind_text(stmnt, 1, pack->name, -1, SQLITE_STATIC) != SQLITE_OK) {
      RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
    }
    if (sqlite3_bind_int(stmnt, 2, e->type) != SQLITE_OK) {
      RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
    }
    if (sqlite3_bind_text(stmnt, 3, e->data, -1, SQLITE_STATIC) != SQLITE_OK) {
      RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
    }
    
    if (e->type == PLIST_FILE) {
      snprintf(file, FILENAME_MAX, "%s/%s", cwd, e->data);
      
      if (MD5File(file, md5) == NULL) {
        char *error;
        asprintf(&error, "File not found: %s", file);
        RETURN_ERROR(MPORT_ERR_FILE_NOT_FOUND, error);
      }
      
      if (sqlite3_bind_text(stmnt, 4, md5, -1, SQLITE_STATIC) != SQLITE_OK) {
        RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
      }
    } else {
      if (sqlite3_bind_null(stmnt, 4) != SQLITE_OK) {
        RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
      }
    }
    if ((ret = sqlite3_step(stmnt)) != SQLITE_DONE) {
      RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
    }
        
    sqlite3_reset(stmnt);
  } 
  
  sqlite3_finalize(stmnt);
  
  return MPORT_OK;
}     

static int create_meta(sqlite3 *db, mportPackageMeta *pack)
{
  sqlite3_stmt *stmnt;
  const char *rest  = 0;
  struct timespec now;
  
  char sql[]  = "INSERT INTO package (pkg, version, lang, date) VALUES (?,?,?,?)";
  
  if (sqlite3_prepare_v2(db, sql, -1, &stmnt, &rest) != SQLITE_OK) {
    RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
  }

  if (sqlite3_bind_text(stmnt, 1, pack->name, -1, SQLITE_STATIC) != SQLITE_OK) {
    RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
  }
  
  if (sqlite3_bind_text(stmnt, 2, pack->version, -1, SQLITE_STATIC) != SQLITE_OK) {
    RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
  }
  
  if (sqlite3_bind_text(stmnt, 3, pack->lang, -1, SQLITE_STATIC) != SQLITE_OK) {
    RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
  }
  
  if (clock_gettime(CLOCK_REALTIME, &now) != 0) {
    RETURN_ERROR(MPORT_ERR_SYSCALL_FAILED, strerror(errno));
  }
  
  if (sqlite3_bind_int(stmnt, 4, now.tv_sec) != SQLITE_OK) {
    RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
  }
    
  if (sqlite3_step(stmnt) != SQLITE_DONE) {
    RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
  }
  
  sqlite3_finalize(stmnt);  
  
  return MPORT_OK;
}

static int tar_files(mportPlist *plist, mportPackageMeta *pack)
{

}

static int clean_up(const char *tmpdir)
{
}

