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
 * $MidnightBSD: src/lib/libmport/inst_init.c,v 1.1 2007/11/22 08:00:32 ctriv Exp $
 */



#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <sqlite3.h>
#include <md5.h>
#include <stdlib.h>
#include "mport.h"

__MBSDID("$MidnightBSD: src/lib/libmport/inst_init.c,v 1.1 2007/11/22 08:00:32 ctriv Exp $");



static int run_pkg_deinstall(mportInstance *, mportPackageMeta *, const char *);
static int delete_pkg_infra(mportInstance *, mportPackageMeta *);
static int check_for_upwards_depends(mportInstance *, mportPackageMeta *);


int mport_delete_primative(mportInstance *mport, mportPackageMeta *pack, int force) 
{
  sqlite3_stmt *stmt;
  int ret;
  mportPlistEntryType type;
  char *data, *checksum, *cwd;
  struct stat st;
  char md5[33], file[FILENAME_MAX];
  
  if (force == 0) {
    if (check_for_upwards_depends(mport, pack) != MPORT_OK)
      RETURN_CURRENT_ERROR;
  }

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
      SET_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(mport->db));
      sqlite3_finalize(stmt);
      RETURN_CURRENT_ERROR;
    }
    
    type     = (mportPlistEntryType)sqlite3_column_int(stmt, 0);
    data     = (char *)sqlite3_column_text(stmt, 1);
    checksum = (char *)sqlite3_column_text(stmt, 2);
    
    switch (type) {
      case PLIST_CWD:
        cwd = data == NULL ? pack->prefix : data;
        break;
      case PLIST_FILE:
        (void)snprintf(file, FILENAME_MAX, "%s%s/%s", mport->root, cwd, data);
        
        if (lstat(file, &st) != 0) {
          char *msg;
          (void)asprintf(&msg, "Can't stat %s: %s", file, strerror(errno));
          (mport->msg_cb)(msg);
          free(msg);
          break; /* next asset */
        } 
        
        
        if (S_ISREG(st.st_mode)) {
          if (MD5File(file, md5) == NULL) {
            sqlite3_finalize(stmt);
            RETURN_ERRORX(MPORT_ERR_FILE_NOT_FOUND, "File not found: %s", file);
          }
          
          if (strcmp(md5, checksum) != 0) {
            char *msg;
            (void)asprintf(&msg, "Checksum mismatch: %s", file);
            (mport->msg_cb)(msg);
            free(msg);
          }
        }
        
        if (unlink(file) != 0) {
          sqlite3_finalize(stmt);
          RETURN_ERROR(MPORT_ERR_SYSCALL_FAILED, strerror(errno));
        }

        break;
      case PLIST_UNEXEC:
        if (mport_run_plist_exec(mport, data, cwd, file) != MPORT_OK) {
          sqlite3_finalize(stmt);
          RETURN_CURRENT_ERROR;
        }
        break;
      case PLIST_DIRRM:
      case PLIST_DIRRMTRY:
        (void)snprintf(file, FILENAME_MAX, "%s%s/%s", mport->root, cwd, data);
        
        if (mport_rmdir(file, type == PLIST_DIRRMTRY ? 1 : 0) != MPORT_OK) {
          sqlite3_finalize(stmt);
          RETURN_CURRENT_ERROR;
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
    
  if (mport_db_do(mport->db, "DELETE FROM assets WHERE pkg=%Q", pack->name) != MPORT_OK)
    RETURN_CURRENT_ERROR;
  
  if (mport_db_do(mport->db, "DELETE FROM depends WHERE pkg=%Q", pack->name) != MPORT_OK)
    RETURN_CURRENT_ERROR;
  
  if (mport_db_do(mport->db, "DELETE FROM packages WHERE pkg=%Q", pack->name) != MPORT_OK)
    RETURN_CURRENT_ERROR;
    
  if (delete_pkg_infra(mport, pack) != MPORT_OK)
    RETURN_CURRENT_ERROR;
  
  return MPORT_OK;
    
} 
  

static int run_pkg_deinstall(mportInstance *mport, mportPackageMeta *pack, const char *mode)
{
  char file[FILENAME_MAX];
  int ret;
  
  (void)snprintf(file, FILENAME_MAX, "%s/%s-%s/%s", MPORT_INST_INFRA_DIR, pack->name, pack->version, MPORT_DEINSTALL_FILE);    

  if (mport_file_exists(file)) {
    if ((ret = mport_xsystem(mport, "PKG_PREFIX=%s %s %s %s %s", pack->prefix, MPORT_SH_BIN, file, pack->name, mode)) != 0)
      RETURN_ERRORX(MPORT_ERR_SYSCALL_FAILED, "%s %s returned non-zero: %i", MPORT_INSTALL_FILE, mode, ret);
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
      RETURN_ERRORX(MPORT_ERR_SYSCALL_FAILED, "mport_rmtree(%s) failed.",  dir);
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
          RETURN_ERRORX(MPORT_ERR_UPWARDS_DEPENDS, "%s depend on %s", depends, pack->name);
        }
        free(msg);
      }
      
      break;    
    default:
      SET_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(mport->db));
      sqlite3_finalize(stmt);
      RETURN_CURRENT_ERROR;
  }
  
  sqlite3_finalize(stmt);
  return MPORT_OK;
} 
      
  
