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
 * $MidnightBSD: src/lib/libmport/install_pkg.c,v 1.2 2007/11/23 05:57:43 ctriv Exp $
 */



#include "mport.h"
#include <sys/cdefs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <archive.h>
#include <archive_entry.h>

__MBSDID("$MidnightBSD: src/lib/libmport/install_pkg.c,v 1.2 2007/11/23 05:57:43 ctriv Exp $");

static int do_pre_install(sqlite3 *, mportPackageMeta *, const char *);
static int do_actual_install(struct archive *, struct archive_entry *, sqlite3 *, mportPackageMeta *, const char *);
static int do_post_install(sqlite3 *, mportPackageMeta *, const char *);
static int check_preconditions(sqlite3 *, mportPackageMeta *);
static int run_pkg_install(const char *, mportPackageMeta *, const char *);
static int run_mtree(const char *, mportPackageMeta *);
static int clean_up(const char *);
static int rollback(void);

int mport_install_pkg(const char *filename, const char *prefix) 
{
  /* 
   * The general strategy here is to extract the meta-files into a tempdir, but
   * extract the real files inplace.  There's huge IO overhead with having a stagging
   * area. 
   */
  struct archive *a = archive_read_new();
  struct archive_entry *entry;
  char filepath[FILENAME_MAX];
  const char *file;
  sqlite3 *db;
  mportPackageMeta *pack;
  int ret = MPORT_OK;
  
  /* initialize our local mport instance */
  if ((ret = mport_inst_init(&db)) != MPORT_OK) 
    return ret;

  /* extract the meta-files into the a temp dir */  
  char dirtmpl[] = "/tmp/mport.XXXXXXXX"; 
  char *tmpdir = mkdtemp(dirtmpl);

  if (tmpdir == NULL)
    RETURN_ERROR(MPORT_ERR_FILEIO, strerror(errno));
    
  archive_read_support_compression_bzip2(a);
  archive_read_support_format_tar(a);

  if (archive_read_open_filename(a, filename, 10240) != ARCHIVE_OK)
    RETURN_ERROR(MPORT_ERR_ARCHIVE, archive_error_string(a));
    
  while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
    file = archive_entry_pathname(entry);
    
    if (*file == '+') {
      snprintf(filepath, FILENAME_MAX, "%s/%s", tmpdir, file);
      archive_entry_set_pathname(entry, filepath);
      archive_read_extract(a, entry, ARCHIVE_EXTRACT_OWNER|ARCHIVE_EXTRACT_PERM);
    } else {
      break;
    }
  }
  
  /* Attach the stub db */
  if ((ret = mport_attach_stub_db(db, tmpdir)) != MPORT_OK) 
    return ret;

  /* get the meta object from the stub database */  
  if ((ret = mport_get_meta_from_db(db, &pack)) != MPORT_OK)
    return ret;

  if (prefix != NULL) {
    free(pack->prefix);
    pack->prefix = strdup(prefix);
  }
  
  /* check if this is installed already, depends, and conflicts */
  if ((ret = check_preconditions(db, pack)) != MPORT_OK)
    return ret;

  /* Run mtree.  Run pkg-install. Etc... */
  if ((ret = do_pre_install(db, pack, tmpdir)) != MPORT_OK)
    return ret;

  if ((ret = do_actual_install(a, entry, db, pack, tmpdir)) != MPORT_OK)
    return ret;
  
  archive_read_finish(a);
  
  if ((ret = do_post_install(db, pack, tmpdir)) != MPORT_OK)
    return ret;
 
  if ((ret = clean_up(tmpdir)) != MPORT_OK)
    return ret;
  
  return ret;
}

/* This does everything that has to happen before we start installing files.
 * We run mtree, pkg-install PRE-INSTALL, etc... 
 */
static int do_pre_install(sqlite3 *db, mportPackageMeta *pack, const char *tmpdir)
{
  int ret = MPORT_OK;
  
  /* run mtree */
  if ((ret = run_mtree(tmpdir, pack)) != MPORT_OK)
    return ret;
  
  /* run pkg-install PRE-INSTALL */
  if ((ret = run_pkg_install(tmpdir, pack, "PRE-INSTALL")) != MPORT_OK)
    return ret;

  return ret;    
}


static int do_actual_install(
      struct archive *a, 
      struct archive_entry *entry,
      sqlite3 *db, 
      mportPackageMeta *pack, 
      const char *tmpdir
    )
{
  const char *err;
  int ret;
  mportPlistEntryType type;
  char *data, *cwd;
  char file[FILENAME_MAX];
  sqlite3_stmt *assets;

  
  if (mport_db_do(db, "BEGIN TRANSACTION") != MPORT_OK) 
    goto ERROR;

  /* Insert the package meta row into the packages table  - XXX Does not honor pack->prefix! */  
  if ((ret = mport_db_do(db, "INSERT INTO packages (pkg, version, origin, prefix, lang, options) VALUES (%Q,%Q,%Q,%Q,%Q,%Q)", pack->name, pack->version, pack->origin, pack->prefix, pack->lang, pack->options)) != MPORT_OK)
    goto ERROR;
  /* Insert the assets into the master table */
  if ((ret = mport_db_do(db, "INSERT INTO assets (pkg, type, data, checksum) SELECT pkg,type,data,checksum FROM stub.assets")) != MPORT_OK)
    goto ERROR;  
  /* Insert the depends into the master table */
  if ((ret = mport_db_do(db, "INSERT INTO depends (pkg, depend_pkgname, depend_pkgversion, depend_port) SELECT pkg,depend_pkgname,depend_pkgversion,depend_port FROM stub.depends")) != MPORT_OK) 
    goto ERROR;
  
  if ((ret = sqlite3_prepare_v2(db, "SELECT type,data FROM stub.assets", -1, &assets, &err)) != SQLITE_OK) {
    mport_set_err(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
    goto ERROR;
  }

  cwd = pack->prefix;

  while ((ret = sqlite3_step(assets)) == SQLITE_ROW) {
    type = (mportPlistEntryType)sqlite3_column_int(assets, 0);
    data = (char *)sqlite3_column_text(assets, 1);
      
    switch (type) {
      case PLIST_CWD:      
        if (data == NULL) {
          cwd = pack->prefix;
        } else {
          cwd = data;
        }
        break;
      case PLIST_EXEC:
        if ((ret = mport_run_plist_exec(data, cwd, file)) != MPORT_OK)
          goto ERROR;
        break;
      case PLIST_FILE:
        /* Our logic here is a bit backwards, because the first real file
         * in the archive was read in the loop in mport_install_pkg.  we
         * use the current entry and then update it. */
        if (entry == NULL) {
          mport_set_err(MPORT_ERR_INTERNAL, "Plist to arhive mismatch!");
          goto ERROR; 
        } 
        snprintf(file, FILENAME_MAX, "%s/%s", cwd, data);
        archive_entry_set_pathname(entry, file);
        if ((ret = archive_read_extract(a, entry, ARCHIVE_EXTRACT_OWNER|ARCHIVE_EXTRACT_PERM)) != ARCHIVE_OK) {
          mport_set_err(MPORT_ERR_ARCHIVE, archive_error_string(a));
          goto ERROR;
        }
        /* we only look for fatal, because EOF is only an error if we come
        back around. */
        if (archive_read_next_header(a, &entry) == ARCHIVE_FATAL) {
          mport_set_err(MPORT_ERR_ARCHIVE, archive_error_string(a));
          goto ERROR;
        }
        break;
      default:
        /* do nothing */
        break;
    }
  }

  sqlite3_finalize(assets);
  
  if (mport_db_do(db, "COMMIT TRANSACTION") != MPORT_OK) 
    goto ERROR;
  
  return MPORT_OK;
  
  ERROR:
    rollback();
    return ret;
}           

static int do_post_install(sqlite3 *db, mportPackageMeta *pack, const char *tmpdir)
{
  return run_pkg_install(tmpdir, pack, "POST-INSTALL");
}


/* check to see if the package is already installed, if it has any
 * conflicts, and that all its depends are installed.
 */
static int check_preconditions(sqlite3 *db, mportPackageMeta *pack) 
{
  sqlite3_stmt *stmt, *lookup;
  int ret, sret, lret;
  const char *inst_version;

  ret = MPORT_OK;

  /* check if the package is already installed */
  if (sqlite3_prepare_v2(db, "SELECT version FROM packages WHERE pkg=?", -1, &lookup, NULL) != SQLITE_OK) 
    RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));

  if (sqlite3_bind_text(lookup, 1, pack->name, -1, SQLITE_STATIC) != SQLITE_OK) {
    ret = SET_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
    goto DONE;
  }
    
  sret = sqlite3_step(lookup);
  
  switch (sret) {
    case SQLITE_DONE:
      /* No row found.  Do nothing */
      break;
    case SQLITE_ROW:
      /* Row was found */
      inst_version = sqlite3_column_text(lookup, 0);
      ret = mport_set_errx(MPORT_ERR_ALREADY_INSTALLED, "%s (version %s) is already installed.", pack->name, inst_version);
      goto DONE;
      break;
    default:
      /* Some sort of sqlite error */
      ret = SET_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
      goto DONE;
  }
  sqlite3_finalize(lookup);

  
  /* Check for conflicts */
  if (sqlite3_prepare_v2(db, "SELECT conflict_pkg, conflict_version FROM stub.conflicts", -1, &stmt, NULL) != SQLITE_OK) {
    ret = SET_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
    goto DONE;
  }
  
  while (1) {
    sret = sqlite3_step(stmt);
    
    if (sret == SQLITE_ROW) {
      const char *conflict_pkg     = sqlite3_column_text(stmt, 0);
      const char *conflict_version = sqlite3_column_text(stmt, 1);
       
      if (sqlite3_prepare_v2(db, "SELECT version FROM packages WHERE pkg GLOB ? AND version GLOB ?", -1, &lookup, NULL) != SQLITE_OK
          ||
          sqlite3_bind_text(lookup, 1, conflict_pkg, -1, SQLITE_STATIC) != SQLITE_OK
          ||
          sqlite3_bind_text(lookup, 2, conflict_version, -1, SQLITE_STATIC) != SQLITE_OK
      ) {
            ret = SET_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
            goto DONE;
      }
      
      lret = sqlite3_step(lookup);

      if (lret == SQLITE_ROW) {
        inst_version = sqlite3_column_text(lookup, 0);
        ret = mport_set_errx(MPORT_ERR_CONFLICTS, "Installed package %s-%s conflicts with %s", conflict_pkg, inst_version, pack->name);
        goto DONE;
      } else if (lret == SQLITE_DONE) {
        /* No hits.  Do nothing */
      } else {
        ret = SET_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
        goto DONE;
      } 
    } else if (sret == SQLITE_DONE) {
      /* No more conflicts to check */
      break;
    } else {
      ret = SET_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
      goto DONE;
    }
  }
  
  /* check for depends */
  if (sqlite3_prepare_v2(db, "SELECT depend_pkgname, depend_pkgversion FROM stub.depends", -1, &stmt, NULL) != SQLITE_OK) {
    ret = SET_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
    goto DONE;
  }
  
  while (1) {
    sret = sqlite3_step(stmt);
  
    if (sret == SQLITE_ROW) {
      const char *depend_pkg     = sqlite3_column_text(stmt, 0);
      const char *depend_version = sqlite3_column_text(stmt, 1);
      
      if (sqlite3_prepare_v2(db, "SELECT version FROM packages WHERE pkg=?", -1, &lookup, NULL) != SQLITE_OK
          ||
          sqlite3_bind_text(lookup, 1, depend_pkg, -1, SQLITE_STATIC) != SQLITE_OK
      ) {
            ret = SET_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
            goto DONE;
      }
      
      lret = sqlite3_step(lookup);
      
      if (lret == SQLITE_ROW) {
        inst_version = sqlite3_column_text(lookup, 0);
        
        if (mport_version_cmp(inst_version, depend_version) < 0) {
          ret = mport_set_errx(MPORT_ERR_MISSING_DEPEND, "%s depends on %s version %s.  Version %s is installed.", pack->name, depend_pkg, depend_version, inst_version);
          goto DONE;
        }
      } else if (lret == SQLITE_DONE) {
        /* this depend isn't installed. */
        ret = mport_set_errx(MPORT_ERR_MISSING_DEPEND, "%s depends on %s, which is not installed.", pack->name, depend_pkg);
        goto DONE;
      } else {
        ret = SET_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
        goto DONE;
      }
    } else if (sret == SQLITE_DONE) {
      /* No more depends to check. */
      break;
    } else {
      ret = SET_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
      goto DONE;
    }
  }        
  
  DONE:
    sqlite3_finalize(stmt);
    sqlite3_finalize(lookup);
    return ret;    
}  
      
static int run_mtree(const char *tmpdir, mportPackageMeta *pack)
{
  char file[FILENAME_MAX];
  int ret;
  
  snprintf(file, FILENAME_MAX, "%s/%s", tmpdir, MPORT_MTREE_FILE);
  
  if (mport_file_exists(file)) {
    if ((ret = mport_xsystem("%s -U -f %s -d -e -p %s >/dev/null", MPORT_MTREE_BIN, file, pack->prefix)) != 0) 
      return mport_set_errx(MPORT_ERR_SYSCALL_FAILED, "%s returned non-zero: %i", MPORT_MTREE_BIN, ret);
  }
  
  return MPORT_OK;
}

static int run_pkg_install(const char *tmpdir, mportPackageMeta *pack, const char *mode)
{
  char file[FILENAME_MAX];
  int ret;
  
  snprintf(file, FILENAME_MAX, "%s/%s", tmpdir, MPORT_INSTALL_FILE);    
  if (mport_file_exists(file)) {
    if ((ret = mport_xsystem("PKG_PREFIX=%s %s %s %s", pack->prefix, MPORT_SH_BIN, file, mode)) != 0)
      return mport_set_errx(MPORT_ERR_SYSCALL_FAILED, "%s %s returned non-zero: %i" MPORT_INSTALL_FILE, mode, ret);
  }
  
  return MPORT_OK;
}


static int clean_up(const char *tmpdir) 
{
  // return mport_rmtree(tmpdir);
  return MPORT_OK;
}


static int rollback()
{
  fprintf(stderr, "Rollback called!\n");
  return 0;
}

