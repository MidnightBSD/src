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
 * $MidnightBSD: src/lib/libmport/check_preconditions.c,v 1.3 2008/04/29 01:35:50 ctriv Exp $
 */

#include "mport.h"
#include "mport_private.h"


static int check_if_installed(sqlite3 *, mportPackageMeta *);
static int check_conflicts(sqlite3 *, mportPackageMeta *);
static int check_depends(mportInstance *mport, mportPackageMeta *);
static int check_if_older_installed(mportInstance *, mportPackageMeta *);


/* check to see if the package is already installed, if it has any
 * conflicts, and that all its depends are installed.
 */
int mport_check_install_preconditions(mportInstance *mport, mportPackageMeta *pack) 
{
  if (check_if_installed(mport->db, pack) != MPORT_OK)
    RETURN_CURRENT_ERROR;
  if (check_conflicts(mport->db, pack) != MPORT_OK)
    RETURN_CURRENT_ERROR;
  if (check_depends(mport, pack) != MPORT_OK)
    RETURN_CURRENT_ERROR;
    
  return MPORT_OK;
}


int mport_check_update_preconditions(mportInstance *mport, mportPackageMeta *pack) 
{
  if (
    (check_if_older_installed(mport, pack) != MPORT_OK)
                        ||
    (check_conflicts(mport->db, pack) != MPORT_OK)
                        ||
    (check_depends(mport, pack) != MPORT_OK)
  )
      RETURN_CURRENT_ERROR;
  
  return MPORT_OK;
}


static int check_if_installed(sqlite3 *db, mportPackageMeta *pack)
{
  sqlite3_stmt *stmt;
  const char *inst_version;
  
  /* check if the package is already installed */
  if (mport_db_prepare(db, &stmt, "SELECT version FROM packages WHERE pkg=%Q", pack->name) != MPORT_OK) 
    RETURN_CURRENT_ERROR;

  switch (sqlite3_step(stmt)) {
    case SQLITE_DONE:
      /* No row found.  Do nothing */
      break;
    case SQLITE_ROW:
      /* Row was found */
      inst_version = sqlite3_column_text(stmt, 0);
      
      SET_ERRORX(MPORT_ERR_ALREADY_INSTALLED, "%s (version %s) is already installed.", pack->name, inst_version);
      sqlite3_finalize(stmt);
      RETURN_CURRENT_ERROR;

      break;
    default:
      /* Some sort of sqlite error */
      SET_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
      sqlite3_finalize(stmt);
      RETURN_CURRENT_ERROR;
  }

  sqlite3_finalize(stmt);
  return MPORT_OK;
}  

static int check_conflicts(sqlite3 *db, mportPackageMeta *pack)
{
  sqlite3_stmt *stmt;
  int ret;
  const char *inst_name, *inst_version;
  
  if (mport_db_prepare(db, &stmt, "SELECT packages.pkg, packages.version FROM stub.conflicts LEFT JOIN packages ON packages.pkg GLOB stub.conflicts.conflict_pkg AND packages.version GLOB stub.conflicts.conflict_version WHERE stub.conflicts.pkg=%Q AND packages.pkg IS NOT NULL", pack->name) != MPORT_OK) 
    RETURN_CURRENT_ERROR;
  
  while (1) {
    ret = sqlite3_step(stmt);
    
    if (ret == SQLITE_ROW) {
        inst_name    = sqlite3_column_text(stmt, 0);
        inst_version = sqlite3_column_text(stmt, 1);
        
        SET_ERRORX(MPORT_ERR_CONFLICTS, "Installed package %s-%s conflicts with %s", inst_name, inst_version, pack->name);
        sqlite3_finalize(stmt);
        RETURN_CURRENT_ERROR;
    } else if (ret == SQLITE_DONE) {
      /* No conflicts */
      break;
    } else {
      SET_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
      sqlite3_finalize(stmt);
      RETURN_CURRENT_ERROR;
    }
  }
  
  sqlite3_finalize(stmt);
  return MPORT_OK;
}


static int check_depends(mportInstance *mport, mportPackageMeta *pack)
{
  sqlite3 *db = mport->db;
  sqlite3_stmt *stmt, *lookup;
  const char *depend_pkg, *depend_version, *inst_version;
  int ret;
  
  /* check for depends */
  if (mport_db_prepare(db, &stmt, "SELECT depend_pkgname, depend_pkgversion FROM stub.depends WHERE pkg=%Q", pack->name) != MPORT_OK) 
    RETURN_CURRENT_ERROR;
 
  if (mport_db_prepare(db, &lookup, "SELECT version FROM packages WHERE pkg=? AND status='clean'") != MPORT_OK) {
    sqlite3_finalize(stmt);
    RETURN_CURRENT_ERROR;
  }  
  
  while (1) {
    ret = sqlite3_step(stmt);
  
    if (ret == SQLITE_ROW) {
      depend_pkg     = sqlite3_column_text(stmt, 0);
      depend_version = sqlite3_column_text(stmt, 1);
      
      if (sqlite3_bind_text(lookup, 1, depend_pkg, -1, SQLITE_STATIC) != SQLITE_OK) {
        SET_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
        sqlite3_finalize(lookup); sqlite3_finalize(stmt);
        RETURN_CURRENT_ERROR;
      }
      
      switch (sqlite3_step(lookup)) {
        case SQLITE_ROW:
          inst_version = sqlite3_column_text(lookup, 0);
          int ok;
                    
          if (depend_version == NULL)
            /* no minimum version */
            break;

          /* break out the compare op at the front of the version string */          
          if (depend_version[0] == '<') {
            if (depend_version[1] == '=') {
              depend_version += 2;
              ok = (mport_version_cmp(inst_version, depend_version) <= 0);
            } else {
              depend_version++;
              ok = (mport_version_cmp(inst_version, depend_version) < 0);
            }
          } else if (depend_version[0] == '>') {
            if (depend_version[1] == '=') {
              depend_version += 2;
              ok = (mport_version_cmp(inst_version, depend_version) >= 0);
            } else {
              depend_version++;
              ok = (mport_version_cmp(inst_version, depend_version) > 0);
            }
          } else {
            sqlite3_finalize(lookup); sqlite3_finalize(stmt);
            RETURN_ERRORX(MPORT_ERR_MALFORMED_DEPEND, "Maformed depend version for %s: %s", depend_pkg, depend_version);
          }

          if (!ok) {
            SET_ERRORX(MPORT_ERR_MISSING_DEPEND, "%s depends on %s version %s.  Version %s is installed.", pack->name, depend_pkg, depend_version, inst_version);
            sqlite3_finalize(lookup); sqlite3_finalize(stmt);
            RETURN_CURRENT_ERROR;
          }
          
          break;
        case SQLITE_DONE:
          /* this depend isn't installed. */
           /* this depend isn't installed. */
           SET_ERRORX(MPORT_ERR_MISSING_DEPEND, "%s depends on %s, which is not installed.", pack->name, depend_pkg);
           sqlite3_finalize(lookup); sqlite3_finalize(stmt);
           RETURN_CURRENT_ERROR;
          break;
        default:
          SET_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
          sqlite3_finalize(lookup); sqlite3_finalize(stmt);
          RETURN_CURRENT_ERROR;
      }
      
      sqlite3_reset(lookup);
      sqlite3_clear_bindings(lookup);
    } else if (ret == SQLITE_DONE) {
      /* No more depends to check. */
      break;
    } else {
      SET_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
      sqlite3_finalize(lookup); sqlite3_finalize(stmt);
      RETURN_CURRENT_ERROR;
    }
  }        
  
  return MPORT_OK;    
}  

/* check to see if an older version of a package is installed. */
static int check_if_older_installed(mportInstance *mport, mportPackageMeta *pkg)
{
  sqlite3_stmt *stmt;
  int ret;
    
  if (mport_db_prepare(mport->db, &stmt, "SELECT 1 FROM packages WHERE pkg=%Q and mport_version_cmp(version, %Q) < 0", pkg->name, pkg->version) != MPORT_OK)
    RETURN_CURRENT_ERROR;
  
  switch (sqlite3_step(stmt)) {
    case SQLITE_ROW:
      ret = MPORT_OK;
      break;
    case SQLITE_DONE:
      ret = SET_ERRORX(MPORT_ERR_NOT_UPGRADABLE, "No older version of %s installed", pkg->name);
      break;
    default:
      ret = SET_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(mport->db));
      break;
  }
  
  sqlite3_finalize(stmt);
  return ret;
}
