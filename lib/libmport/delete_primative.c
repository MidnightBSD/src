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
 * $MidnightBSD: src/lib/libmport/delete_primative.c,v 1.2 2008/04/26 17:59:26 ctriv Exp $
 */



#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <sqlite3.h>
#include <md5.h>
#include <stdlib.h>
#include "mport.h"
#include "mport_private.h"



static int run_pkg_deinstall(mportInstance *, mportPackageMeta *, const char *);
static int delete_pkg_infra(mportInstance *, mportPackageMeta *);
static int check_for_upwards_depends(mportInstance *, mportPackageMeta *);


MPORT_PUBLIC_API int mport_delete_primative(mportInstance *mport, mportPackageMeta *pack, int force) 
{
  sqlite3_stmt *stmt;
  int ret, current, total;
  mportAssetListEntryType type;
  char *data, *checksum, *cwd;
  struct stat st;
  char md5[33];
  
  if (force == 0) {
    if (check_for_upwards_depends(mport, pack) != MPORT_OK)
      RETURN_CURRENT_ERROR;
  }

  /* get the file count for the progress meter */
  if (mport_db_prepare(mport->db, &stmt, "SELECT COUNT(*) FROM assets WHERE type=%i AND pkg=%Q", ASSET_FILE, pack->name) != MPORT_OK)
    RETURN_CURRENT_ERROR;

  switch (sqlite3_step(stmt)) {
    case SQLITE_ROW:
      total   = sqlite3_column_int(stmt, 0) + 1;
      current = 0;
      sqlite3_finalize(stmt);
      break;
    default:
      SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(mport->db));
      sqlite3_finalize(stmt);
      RETURN_CURRENT_ERROR;
  }
  
  mport_call_progress_init_cb(mport, "Deleteing %s-%s", pack->name, pack->version);

  if (mport_db_do(mport->db, "UPDATE packages SET status='dirty' WHERE pkg=%Q", pack->name) != MPORT_OK)
    RETURN_CURRENT_ERROR;

  if (run_pkg_deinstall(mport, pack, "DEINSTALL") != MPORT_OK)
    RETURN_CURRENT_ERROR;
  
  if (mport_db_prepare(mport->db, &stmt, "SELECT type,data,checksum FROM assets WHERE pkg=%Q", pack->name) != MPORT_OK)
    RETURN_CURRENT_ERROR;  
  
  cwd = pack->prefix;

  while (1) {
    ret = sqlite3_step(stmt);
    
    if (ret == SQLITE_DONE)
      break;
      
    if (ret != SQLITE_ROW) {
      /* some error occured */
      SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(mport->db));
      sqlite3_finalize(stmt);
      RETURN_CURRENT_ERROR;
    }
    
    type     = (mportAssetListEntryType)sqlite3_column_int(stmt, 0);
    data     = (char *)sqlite3_column_text(stmt, 1);
    checksum = (char *)sqlite3_column_text(stmt, 2);

    char file[FILENAME_MAX];
    /* XXX TMP */
    if (*data == '/') {
      snprintf(file, sizeof(file), "%s%s", mport->root, data);
    } else {
      snprintf(file, sizeof(file), "%s%s/%s", mport->root, pack->prefix, data);
    }

    
    switch (type) {
      case ASSET_FILE:
        (mport->progress_step_cb)(++current, total, file);
        
      
        if (lstat(file, &st) != 0) {
          mport_call_msg_cb(mport, "Can't stat %s: %s", file, strerror(errno));
          break; /* next asset */
        } 
        
        
        if (S_ISREG(st.st_mode)) {
          if (MD5File(file, md5) == NULL) 
            mport_call_msg_cb(mport, "Can't md5 %s: %s", file, strerror(errno));
          
          if (strcmp(md5, checksum) != 0) 
            mport_call_msg_cb(mport, "Checksum mismatch: %s", file);
        }
        
        if (unlink(file) != 0) 
          mport_call_msg_cb(mport, "Could not unlink %s: %s", file, strerror(errno));

        break;
      case ASSET_UNEXEC:
        if (mport_run_asset_exec(mport, data, cwd, file) != MPORT_OK) {
          mport_call_msg_cb(mport, "Could not execute %s: %s", data, mport_err_string());
        }
        break;
      case ASSET_DIRRM:
      case ASSET_DIRRMTRY:
        if (mport_rmdir(file, type == ASSET_DIRRMTRY ? 1 : 0) != MPORT_OK) {
          mport_call_msg_cb(mport, "Could not remove directory '%s': %s", file, mport_err_string());
        }
        
        break;
      default:
        /* do nothing */
        break;
    }
  }
    
  sqlite3_finalize(stmt);
  
  if (run_pkg_deinstall(mport, pack, "POST-DEINSTALL") != MPORT_OK)
    RETURN_CURRENT_ERROR;
  
  if (mport_db_do(mport->db, "BEGIN TRANSACTION") != MPORT_OK)
    RETURN_CURRENT_ERROR; 
    
  if (mport_db_do(mport->db, "DELETE FROM assets WHERE pkg=%Q", pack->name) != MPORT_OK)
    RETURN_CURRENT_ERROR;
  
  if (mport_db_do(mport->db, "DELETE FROM depends WHERE pkg=%Q", pack->name) != MPORT_OK)
    RETURN_CURRENT_ERROR;
  
  if (mport_db_do(mport->db, "DELETE FROM packages WHERE pkg=%Q", pack->name) != MPORT_OK)
    RETURN_CURRENT_ERROR;
    
  if (mport_db_do(mport->db, "DELETE FROM categories WHERE pkg=%Q", pack->name) != MPORT_OK)
    RETURN_CURRENT_ERROR;

  if (delete_pkg_infra(mport, pack) != MPORT_OK)
    RETURN_CURRENT_ERROR;

  if (mport_db_do(mport->db, "COMMIT TRANSACTION") != MPORT_OK)
    RETURN_CURRENT_ERROR; 
  
  (mport->progress_step_cb)(++current, total, "DB Updated");

  
  (mport->progress_free_cb)();

  mport_pkgmeta_logevent(mport, pack, "Package deleted");
  
  return MPORT_OK;  
} 
  

static int run_pkg_deinstall(mportInstance *mport, mportPackageMeta *pack, const char *mode)
{
  char file[FILENAME_MAX];
  int ret;
  
  (void)snprintf(file, FILENAME_MAX, "%s/%s-%s/%s", MPORT_INST_INFRA_DIR, pack->name, pack->version, MPORT_DEINSTALL_FILE);    

  if (mport_file_exists(file)) {
    if (chmod(file, 755) != 0)
      RETURN_ERRORX(MPORT_ERR_FATAL, "chmod(%s, 755): %s", file, strerror(errno));
      
    if ((ret = mport_xsystem(mport, "PKG_PREFIX=%s %s %s %s", pack->prefix, file, pack->name, mode)) != 0)
      RETURN_ERRORX(MPORT_ERR_FATAL, "%s %s returned non-zero: %i", MPORT_INSTALL_FILE, mode, ret);
  }
  
  return MPORT_OK;
}


/* delete this package's infrastructure dir. */
static int delete_pkg_infra(mportInstance *mport, mportPackageMeta *pack)
{
  char dir[FILENAME_MAX];
  int ret;

  (void)snprintf(dir, FILENAME_MAX, "%s%s/%s-%s", mport->root, MPORT_INST_INFRA_DIR, pack->name, pack->version);
  
  if (mport_file_exists(dir)) {
    if ((ret = mport_rmtree(dir)) != MPORT_OK) 
      RETURN_ERRORX(MPORT_ERR_FATAL, "mport_rmtree(%s) failed.",  dir);
  }
  
  return MPORT_OK;
}



static int check_for_upwards_depends(mportInstance *mport, mportPackageMeta *pack)
{
  sqlite3_stmt *stmt;
  char *depends, *msg;
  int count;
      
  if (mport_db_prepare(mport->db, &stmt, "SELECT group_concat(packages.pkg),count(packages.pkg) FROM depends JOIN packages ON depends.pkg=packages.pkg WHERE depend_pkgname=%Q", pack->name) != MPORT_OK)
    RETURN_CURRENT_ERROR;
  
  switch (sqlite3_step(stmt)) {
    case SQLITE_ROW:
      depends = (char *)sqlite3_column_text(stmt, 0);
      count   = sqlite3_column_int(stmt, 1);
      
      if (count != 0) {
        (void)asprintf(&msg, "%s depend on %s, delete anyway?", depends, pack->name);
        if ((mport->confirm_cb)(msg, "Delete", "Don't delete", 0) != MPORT_OK) {
          sqlite3_finalize(stmt);
          free(msg);
          RETURN_ERRORX(MPORT_ERR_FATAL, "%s depend on %s", depends, pack->name);
        }
        free(msg);
      }
      
      break;    
    default:
      SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(mport->db));
      sqlite3_finalize(stmt);
      RETURN_CURRENT_ERROR;
  }
  
  sqlite3_finalize(stmt);
  return MPORT_OK;
} 
      
  
