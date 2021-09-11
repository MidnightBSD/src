/*-
 * Copyright (c) 2009 Chris Reinhardt
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
 */

#include <sys/cdefs.h>
__MBSDID("$MidnightBSD$");

#include "mport.h"
#include "mport_private.h"
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

static int make_backup_bundle(mportInstance *, mportPackageMeta *, char *);
static int install_backup_bundle(mportInstance *, char *);
static int build_create_extras(mportInstance *, mportPackageMeta *, char *, mportCreateExtras **);
static int build_create_extras_copy_metafiles(const mportPackageMeta *, mportCreateExtras *);
static int build_create_extras_depends(mportInstance *, mportPackageMeta *, mportCreateExtras *);

int mport_bundle_read_update_pkg(mportInstance *mport, mportBundleRead *bundle, mportPackageMeta *pkg)
{
  char tmpfile2[] = "/tmp/mport.XXXXXXXX";
  int fd;

  mport_pkgmeta_logevent(mport, pkg, "Begining update");

  if ((fd = mkstemp(tmpfile2)) == -1) {
    RETURN_ERRORX(MPORT_ERR_FATAL, "Couldn't make tmp file: %s", strerror(errno));
  }
  
  close(fd);

  if (make_backup_bundle(mport, pkg, tmpfile2) != MPORT_OK) {
    RETURN_CURRENT_ERROR;
  }

  if (
        (mport_delete_primative(mport, pkg, 1) != MPORT_OK)
                          ||
        (mport_bundle_read_install_pkg(mport, bundle, pkg) != MPORT_OK)
  ) 
  {
    if (install_backup_bundle(mport, tmpfile2) == MPORT_OK) {
      (void)mport_rmtree(tmpfile2);
    }
    RETURN_CURRENT_ERROR;
  }           
  
  /* if we can't delete the tmpfile, just move on. */
  (void)mport_rmtree(tmpfile2);
  
  return MPORT_OK;
}
  
  
static int make_backup_bundle(mportInstance *mport, mportPackageMeta *pkg, char *tempfile)
{
  mportAssetList *alist;
  mportCreateExtras *extra;
  int ret;
 
  if (mport_asset_get_assetlist(mport, pkg, &alist) != MPORT_OK)
    RETURN_CURRENT_ERROR;

  if (build_create_extras(mport, pkg, tempfile, &extra) != MPORT_OK) 
    RETURN_CURRENT_ERROR;

  extra->is_backup = true;

  ret = mport_create_primative(alist, pkg, extra);

  mport_assetlist_free(alist);
  mport_createextras_free(extra);

  return ret;
}        


static int install_backup_bundle(mportInstance *mport, char *filename) 
{
  /* at some point we might want to look into making this more forceful, but
   * this will do for the moment.  Wrap in a function for this future. */
  
  return mport_install_primative(mport, filename, NULL);
}



static int build_create_extras(mportInstance *mport, mportPackageMeta *pkg, char *tempfile, mportCreateExtras **extra_p)
{
  mportCreateExtras *extra;
 
  extra = mport_createextras_new();
  *extra_p = extra;
 
  extra->pkg_filename = strdup(tempfile); /* this MUST be on the heap, as it will be freed */
  extra->sourcedir = strdup("");
 
  if (build_create_extras_depends(mport, pkg, extra) != MPORT_OK)
    RETURN_CURRENT_ERROR;
 
  if (build_create_extras_copy_metafiles(pkg, extra) != MPORT_OK)
    RETURN_CURRENT_ERROR;

  extra->is_backup = false;
  
  return MPORT_OK;
}

static int build_create_extras_copy_metafiles(const mportPackageMeta *pkg, mportCreateExtras *extra) 
{
  char file[FILENAME_MAX];
  
  if (snprintf(file, FILENAME_MAX, "%s/%s-%s/%s", MPORT_INST_INFRA_DIR, pkg->name, pkg->version, MPORT_MTREE_FILE) < 0)
    RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");
   
  if (mport_file_exists(file)) {
    if ((extra->mtree = strdup(file)) == NULL) 
      RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");
  }
    
  if (snprintf(file, FILENAME_MAX, "%s/%s-%s/%s", MPORT_INST_INFRA_DIR, pkg->name, pkg->version, MPORT_INSTALL_FILE) < 0)
    RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");
   
  if (mport_file_exists(file)) {
    if ((extra->pkginstall = strdup(file)) == NULL) 
      RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");
  }
    
  if (snprintf(file, FILENAME_MAX, "%s/%s-%s/%s", MPORT_INST_INFRA_DIR, pkg->name, pkg->version, MPORT_DEINSTALL_FILE) < 0)
    RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");
   
  if (mport_file_exists(file)) {
    if ((extra->pkgdeinstall = strdup(file)) == NULL) 
      RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");
  }
    
  if (snprintf(file, FILENAME_MAX, "%s/%s-%s/%s", MPORT_INST_INFRA_DIR, pkg->name, pkg->version, MPORT_MESSAGE_FILE) < 0)
    RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");
   
  if (mport_file_exists(file)) {
    if ((extra->pkgmessage = strdup(file)) == NULL) 
      RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");
  }

  return 0;
}

static int build_create_extras_depends(mportInstance *mport, mportPackageMeta *pkg, mportCreateExtras *extra) 
{
  int count, ret, i;
  sqlite3_stmt *stmt;
  char *entry;
 
  if (mport_db_prepare(mport->db, &stmt, "SELECT COUNT(*) FROM depends WHERE pkg=%Q", pkg->name) != MPORT_OK) {
	  sqlite3_finalize(stmt);
	  RETURN_CURRENT_ERROR;
  }
    
  ret = sqlite3_step(stmt);
   
  switch (ret) {
    case SQLITE_ROW:
      count = sqlite3_column_int(stmt, 0);
      sqlite3_finalize(stmt);
      break;
    case SQLITE_DONE:
      sqlite3_finalize(stmt);
      RETURN_ERROR(MPORT_ERR_FATAL, "SQLite returned no rows for a COUNT(*) select.");
    default:
      sqlite3_finalize(stmt);
      RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(mport->db));
  }

  if ((extra->depends = (char **)calloc(count + 1, sizeof(char *))) == NULL)
    RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");
 
  if (mport_db_prepare(mport->db, &stmt, "SELECT depend_pkgname, depend_pkgversion, depend_port FROM depends WHERE pkg=%Q", pkg->name) != MPORT_OK) {
	  sqlite3_finalize(stmt);
	  RETURN_CURRENT_ERROR;
  }
  
  i = 0;
  while (1) {
    ret = sqlite3_step(stmt);
   
    if (ret == SQLITE_ROW) {
      if (asprintf(&entry, "%s:%s:%s", sqlite3_column_text(stmt, 0), sqlite3_column_text(stmt, 2), sqlite3_column_text(stmt, 1)) == -1) {
        sqlite3_finalize(stmt);
        RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");  
      }
        
      extra->depends[i] = entry;
      i++;
    } else if (ret == SQLITE_DONE) {
      extra->depends[i] = NULL;
      break;
    } else {
      sqlite3_finalize(stmt);
      RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(mport->db));
    }
  }
  
  sqlite3_finalize(stmt);
  
  return MPORT_OK;
}
