/*-
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
 * $MidnightBSD: src/lib/libmport/create_primative.c,v 1.2 2008/04/26 17:59:26 ctriv Exp $
 */



#include <sys/cdefs.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <md5.h>
#include <archive.h>
#include <archive_entry.h>
#include "mport.h"
#include "mport_private.h"

static int create_stub_db(sqlite3 **, const char *);
static int insert_assetlist(sqlite3 *, mportAssetList *, mportPackageMeta *, mportCreateExtras *);
static int insert_meta(sqlite3 *, mportPackageMeta *, mportCreateExtras *);
static int insert_depends(sqlite3 *, mportPackageMeta *, mportCreateExtras *);
static int insert_conflicts(sqlite3 *, mportPackageMeta *, mportCreateExtras *);
static int insert_categories(sqlite3 *, mportPackageMeta *);
static int archive_files(mportAssetList *, mportPackageMeta *, mportCreateExtras *, const char *);
static int archive_metafiles(mportBundleWrite *, mportPackageMeta *, mportCreateExtras *);
static int archive_assetlistfiles(mportBundleWrite *, mportPackageMeta *, mportCreateExtras *, mportAssetList *);
static int clean_up(const char *);


MPORT_PUBLIC_API int mport_create_primative(mportAssetList *assetlist, mportPackageMeta *pack, mportCreateExtras *extra)
{
  
  int ret;
  sqlite3 *db;

  char dirtmpl[] = "/tmp/mport.XXXXXXXX"; 
  char *tmpdir = mkdtemp(dirtmpl);

  if (tmpdir == NULL) {
    ret = SET_ERROR(MPORT_ERR_FATAL, strerror(errno));
    goto CLEANUP;
  }
  
  if ((ret = create_stub_db(&db, tmpdir)) != MPORT_OK)
    goto CLEANUP;

  if ((ret = insert_assetlist(db, assetlist, pack, extra)) != MPORT_OK)
    goto CLEANUP;

  if ((ret = insert_meta(db, pack, extra)) != MPORT_OK)
    goto CLEANUP;
  
  if (sqlite3_close(db) != SQLITE_OK) {
    ret = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
    goto CLEANUP;
  }
  
  if ((ret = archive_files(assetlist, pack, extra, tmpdir)) != MPORT_OK)
    goto CLEANUP;
  
  CLEANUP:  
    clean_up(tmpdir);
    return ret;
}


static int create_stub_db(sqlite3 **db, const char *tmpdir) 
{
  char file[FILENAME_MAX];
  
  (void)snprintf(file, FILENAME_MAX, "%s/%s", tmpdir, MPORT_STUB_DB_FILE);
  
  if (sqlite3_open(file, db) != SQLITE_OK) {
    sqlite3_close(*db);
    RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(*db));
  }
  
  /* create tables */
  return mport_generate_stub_schema(*db);
}

static int insert_assetlist(sqlite3 *db, mportAssetList *assetlist, mportPackageMeta *pack, mportCreateExtras *extra)
{
  mportAssetListEntry *e;
  sqlite3_stmt *stmnt;
  char sql[]  = "INSERT INTO assets (pkg, type, data, checksum) VALUES (?,?,?,?)";
  char md5[33];
  char file[FILENAME_MAX];
  char cwd[FILENAME_MAX];
  struct stat st;

  strlcpy(cwd, extra->sourcedir, FILENAME_MAX);
  strlcat(cwd, pack->prefix, FILENAME_MAX);
  
  if (mport_db_prepare(db, &stmnt, sql) != MPORT_OK) 
    RETURN_CURRENT_ERROR;
  
  STAILQ_FOREACH(e, assetlist, next) {
    if (e->type == ASSET_COMMENT)
      continue;
  
    if (e->type == ASSET_CWD) {
      strlcpy(cwd, extra->sourcedir, FILENAME_MAX);
      if (e->data == NULL) {
        strlcat(cwd, pack->prefix, FILENAME_MAX);
      } else {
        strlcat(cwd, e->data, FILENAME_MAX);
      }
    }
    
    if (sqlite3_bind_text(stmnt, 1, pack->name, -1, SQLITE_STATIC) != SQLITE_OK) {
      RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
    }
    if (sqlite3_bind_int(stmnt, 2, e->type) != SQLITE_OK) {
      RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
    }
    if (sqlite3_bind_text(stmnt, 3, e->data, -1, SQLITE_STATIC) != SQLITE_OK) {
      RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
    }
    
    if (e->type == ASSET_FILE) {
      /* Don't prepend cwd onto absolute file paths (this is useful for update) */
      if (*(e->data) == '/') {
        (void)strlcpy(file, e->data, FILENAME_MAX);
      } else {
        (void)snprintf(file, FILENAME_MAX, "%s/%s", cwd, e->data);
      }
      
      if (lstat(file, &st) != 0) {
        sqlite3_finalize(stmnt);
        RETURN_ERRORX(MPORT_ERR_FATAL, "Couln't stat %s: %s", file, strerror(errno));
      }

      if (S_ISREG(st.st_mode)) {

        if (MD5File(file, md5) == NULL) 
          RETURN_ERRORX(MPORT_ERR_FATAL, "File not found: %s", file);
      
        if (sqlite3_bind_text(stmnt, 4, md5, -1, SQLITE_STATIC) != SQLITE_OK) 
          RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
      } else {
        sqlite3_bind_null(stmnt, 4);
      }
    } else {
      if (sqlite3_bind_null(stmnt, 4) != SQLITE_OK) {
        RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
      }
    }
    
    if (sqlite3_step(stmnt) != SQLITE_DONE) {
      RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
    }
        
    sqlite3_reset(stmnt);
  } 
  
  sqlite3_finalize(stmnt);
  
  return MPORT_OK;
}     

static int insert_meta(sqlite3 *db, mportPackageMeta *pack, mportCreateExtras *extra)
{
  sqlite3_stmt *stmnt;
  const char *rest  = 0;
  
  char sql[]  = "INSERT INTO packages (pkg, version, origin, lang, prefix, comment) VALUES (?,?,?,?,?,?)";
  
  if (sqlite3_prepare_v2(db, sql, -1, &stmnt, &rest) != SQLITE_OK) {
    RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
  }
  if (sqlite3_bind_text(stmnt, 1, pack->name, -1, SQLITE_STATIC) != SQLITE_OK) {
    RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
  }
  if (sqlite3_bind_text(stmnt, 2, pack->version, -1, SQLITE_STATIC) != SQLITE_OK) {
    RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
  }
  if (sqlite3_bind_text(stmnt, 3, pack->origin, -1, SQLITE_STATIC) != SQLITE_OK) {
    RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
  }
  if (sqlite3_bind_text(stmnt, 4, pack->lang, -1, SQLITE_STATIC) != SQLITE_OK) {
    RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
  }
  if (sqlite3_bind_text(stmnt, 5, pack->prefix, -1, SQLITE_STATIC) != SQLITE_OK) {
    RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
  }
  if (sqlite3_bind_text(stmnt, 6, pack->comment, -1, SQLITE_STATIC) != SQLITE_OK) {
    RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
  }
    
  if (sqlite3_step(stmnt) != SQLITE_DONE) {
    RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
  }
  
  sqlite3_finalize(stmnt);  


  if (insert_depends(db, pack, extra) != MPORT_OK)
    RETURN_CURRENT_ERROR;        
  if (insert_conflicts(db, pack, extra) != MPORT_OK)
    RETURN_CURRENT_ERROR;
  if (insert_categories(db, pack) != MPORT_OK)
    RETURN_CURRENT_ERROR;

  return MPORT_OK;
}


static int insert_categories(sqlite3 *db, mportPackageMeta *pkg)
{
  sqlite3_stmt *stmt;
  
  int i = 0;
  
  if (pkg->categories == NULL)
    return MPORT_OK; // or should this be an error?

  if (mport_db_prepare(db, &stmt, "INSERT INTO categories (pkg, category) VALUES (?,?)") != MPORT_OK)
    RETURN_CURRENT_ERROR;

  while (pkg->categories[i] != NULL) {  
    if (sqlite3_bind_text(stmt, 1, pkg->name, -1, SQLITE_STATIC) != SQLITE_OK) {
      RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
    }
    if (sqlite3_bind_text(stmt, 2, pkg->categories[i], -1, SQLITE_STATIC) != SQLITE_OK) {
      RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
    }

    if (sqlite3_step(stmt) != SQLITE_DONE) {
      RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
    }
    sqlite3_reset(stmt);
    i++;
  }
  
  sqlite3_finalize(stmt);
  return MPORT_OK;
}
    


static int insert_conflicts(sqlite3 *db, mportPackageMeta *pack, mportCreateExtras *extra) 
{
  sqlite3_stmt *stmnt;
  char **conflict  = extra->conflicts;
  char *version;
  
  /* we're done if there are no conflicts to record. */
  if (conflict == NULL) 
    return MPORT_OK;

  if (mport_db_prepare(db, &stmnt, "INSERT INTO conflicts (pkg, conflict_pkg, conflict_version) VALUES (?,?,?)") != MPORT_OK)
    RETURN_CURRENT_ERROR;
    
  /* we have a conflict like apache-1.4.  We want to do a m/(.*)-(.*)/ */
  while (*conflict != NULL) {
    version = rindex(*conflict, '-');
    
    if (sqlite3_bind_text(stmnt, 1, pack->name, -1, SQLITE_STATIC) != SQLITE_OK) {
      RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
    }
    if (sqlite3_bind_text(stmnt, 2, *conflict, -1, SQLITE_STATIC) != SQLITE_OK) {
      RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
    }
    if (version != NULL) {
      *version = '\0';
      version++;
      if (sqlite3_bind_text(stmnt, 3, version, -1, SQLITE_STATIC) != SQLITE_OK) {
        RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
      }
    } else {
      if (sqlite3_bind_text(stmnt, 3, "*", -1, SQLITE_STATIC) != SQLITE_OK) {
        RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
      }
    }
    if (sqlite3_step(stmnt) != SQLITE_DONE) {
      RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
    }
    sqlite3_reset(stmnt);
    conflict++;
  }
  
  sqlite3_finalize(stmnt);
  
  return MPORT_OK;
}
    
  

static int insert_depends(sqlite3 *db, mportPackageMeta *pack, mportCreateExtras *extra) 
{
  sqlite3_stmt *stmnt;
  char **depend    = extra->depends;
  char *pkgversion;
  char *port;
  
  /* we're done if there are no deps to record. */
  if (depend == NULL) 
    return MPORT_OK;

  if (mport_db_prepare(db, &stmnt, "INSERT INTO depends (pkg, depend_pkgname, depend_pkgversion, depend_port) VALUES (?,?,?,?)") != MPORT_OK)
    RETURN_CURRENT_ERROR;
    
  /* depends look like this.  break'em up into port, pkgversion and pkgname
   * perl:lang/perl5.8:>=5.8.3
   */
  while (*depend != NULL) {
    port = index(*depend, ':');
    *port = '\0';
    port++;

    if (*port == 0)
      RETURN_ERRORX(MPORT_ERR_FATAL, "Maformed depend: %s", *depend);

    if (sqlite3_bind_text(stmnt, 1, pack->name, -1, SQLITE_STATIC) != SQLITE_OK) {
      RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
    }
    if (sqlite3_bind_text(stmnt, 2, *depend, -1, SQLITE_STATIC) != SQLITE_OK) {
      RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
    }
    
    pkgversion = index(port, ':');
    
    if (pkgversion != NULL) {
      *pkgversion = '\0';
      pkgversion++;
      if (sqlite3_bind_text(stmnt, 3, pkgversion, -1, SQLITE_STATIC) != SQLITE_OK) {
        RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
      }
    } else {
      if (sqlite3_bind_null(stmnt, 3) != SQLITE_OK) {
        RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
      }
    }
    
    if (sqlite3_bind_text(stmnt, 4, port, -1, SQLITE_STATIC) != SQLITE_OK) {
      RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
    }
    
    if (sqlite3_step(stmnt) != SQLITE_DONE) {
      RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
    }
    sqlite3_reset(stmnt);
    depend++;
  }
    
  sqlite3_finalize(stmnt);

  return MPORT_OK;
}



static int archive_files(mportAssetList *assetlist, mportPackageMeta *pack, mportCreateExtras *extra, const char *tmpdir)
{
  mportBundleWrite *bundle;
  char filename[FILENAME_MAX];
  
  bundle = mport_bundle_write_new();
  
  if (mport_bundle_write_init(bundle, extra->pkg_filename) != MPORT_OK)
    RETURN_CURRENT_ERROR;

  /* First step - +CONTENTS.db ALWAYS GOES FIRST!!! */        
  (void)snprintf(filename, FILENAME_MAX, "%s/%s", tmpdir, MPORT_STUB_DB_FILE);
  if (mport_bundle_write_add_file(bundle, filename, MPORT_STUB_DB_FILE)) 
    RETURN_CURRENT_ERROR;
    
  /* second step - the meta files */
  if (archive_metafiles(bundle, pack, extra) != MPORT_OK)
    RETURN_CURRENT_ERROR;

  /* last step - the real files from the assetlist */
  if (archive_assetlistfiles(bundle, pack, extra, assetlist) != MPORT_OK)
    RETURN_CURRENT_ERROR;
    
  mport_bundle_write_finish(bundle);
  
  return MPORT_OK;    
}


static int archive_metafiles(mportBundleWrite *bundle, mportPackageMeta *pack, mportCreateExtras *extra)
{
  char filename[FILENAME_MAX], dir[FILENAME_MAX];
  
  (void)snprintf(dir, FILENAME_MAX, "%s/%s-%s", MPORT_STUB_INFRA_DIR, pack->name, pack->version);

  if (extra->mtree != NULL && mport_file_exists(extra->mtree)) {
    (void)snprintf(filename, FILENAME_MAX, "%s/%s", dir, MPORT_MTREE_FILE);
    if (mport_bundle_write_add_file(bundle, extra->mtree, filename) != MPORT_OK)
      RETURN_CURRENT_ERROR;
  }
  
  if (extra->pkginstall != NULL && mport_file_exists(extra->pkginstall)) {
    (void)snprintf(filename, FILENAME_MAX, "%s/%s", dir, MPORT_INSTALL_FILE);
    if (mport_bundle_write_add_file(bundle, extra->pkginstall, filename) != MPORT_OK)
      RETURN_CURRENT_ERROR;
  }
  
  if (extra->pkgdeinstall != NULL && mport_file_exists(extra->pkgdeinstall)) {
    (void)snprintf(filename, FILENAME_MAX, "%s/%s", dir, MPORT_DEINSTALL_FILE);
    if (mport_bundle_write_add_file(bundle, extra->pkgdeinstall, filename) != MPORT_OK)
      RETURN_CURRENT_ERROR;
  }
  
  if (extra->pkgmessage != NULL && mport_file_exists(extra->pkgmessage)) {
    (void)snprintf(filename, FILENAME_MAX, "%s/%s", dir, MPORT_MESSAGE_FILE);
    if (mport_bundle_write_add_file(bundle, extra->pkgmessage, filename) != MPORT_OK)
      RETURN_CURRENT_ERROR;
  }
  
  return MPORT_OK;
}

static int archive_assetlistfiles(mportBundleWrite *bundle, mportPackageMeta *pack, mportCreateExtras *extra, mportAssetList *assetlist)
{
  mportAssetListEntry *e;
  char filename[FILENAME_MAX];
  char *cwd = pack->prefix;
  /*int total = 0;
  int cur   = 0;
  
   (mport->progress_init_cb)(); 
  
  STAILQ_FOREACH(e, assetlist, next) {
    total++;
  } */
  
  STAILQ_FOREACH(e, assetlist, next) {
    if (e->type == ASSET_CWD) 
      cwd = e->data == NULL ? pack->prefix : e->data;
    
    if (e->type != ASSET_FILE) {
      continue;
    }

    /* don't prepend the cwd if the path is abs. */
    if (*(e->data) == '/') {   
      (void)snprintf(filename, FILENAME_MAX, "%s%s", extra->sourcedir, e->data);
    } else {
      (void)snprintf(filename, FILENAME_MAX, "%s/%s/%s", extra->sourcedir, cwd, e->data);
    }
    
    if (mport_bundle_write_add_file(bundle, filename, e->data) != MPORT_OK)
      RETURN_CURRENT_ERROR;
    
    /* (mport->progress_step_cb)(++cut, total, e->data);     */
  }    
  
  /* (mport->progress_free_cb)(); */
 
  return MPORT_OK;
}


static int clean_up(const char *tmpdir) 
{
  return mport_rmtree(tmpdir);
}


