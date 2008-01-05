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
 * $MidnightBSD: src/lib/libmport/install_pkg.c,v 1.4 2007/12/01 06:21:37 ctriv Exp $
 */



#include "mport.h"
#include <sys/cdefs.h>
#include <sys/stat.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <archive.h>
#include <archive_entry.h>

static int do_pre_install(mportInstance *, mportPackageMeta *, const char *);
static int do_actual_install(mportInstance *, struct archive *, struct archive_entry *, mportPackageMeta *, const char *);
static int do_post_install(mportInstance *, mportPackageMeta *, const char *);
static int run_pkg_install(mportInstance *, const char *, mportPackageMeta *, const char *);
static int run_mtree(mportInstance *, const char *, mportPackageMeta *);
static int display_pkg_msg(mportInstance *, mportPackageMeta *, const char *);
static int clean_up(mportInstance *, const char *);
static int rollback(void);

int mport_install_primative(mportInstance *mport, const char *filename, const char *prefix) 
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
  sqlite3 *db = mport->db;
  mportPackageMeta **packs;
  mportPackageMeta *pack;
  int i;
  
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
      (void)snprintf(filepath, FILENAME_MAX, "%s/%s", tmpdir, file);
      archive_entry_set_pathname(entry, filepath);
      archive_read_extract(a, entry, 0);
    } else {
      break;
    }
  }

  /* Attach the stub db */
  if (mport_attach_stub_db(db, tmpdir) != MPORT_OK) 
    RETURN_CURRENT_ERROR;

  /* get the meta objects from the stub database */  
  if (mport_get_meta_from_stub(db, &packs) != MPORT_OK)
    RETURN_CURRENT_ERROR;

  for (i=0; *(packs + i) != NULL; i++) {
    pack  = *(packs + i);    

    if (prefix != NULL) {
      free(pack->prefix);
      pack->prefix = strdup(prefix);
    }
    
    /* check if this is installed already, depends, and conflicts */
    if (mport_check_install_preconditions(mport, pack) != MPORT_OK)
      RETURN_CURRENT_ERROR;

    /* Run mtree.  Run pkg-install. Etc... */
    if (do_pre_install(mport, pack, tmpdir) != MPORT_OK)
      RETURN_CURRENT_ERROR;

    if (do_actual_install(mport, a, entry, pack, tmpdir) != MPORT_OK)
      RETURN_CURRENT_ERROR;
    
    archive_read_finish(a);
    
    if (do_post_install(mport, pack, tmpdir) != MPORT_OK)
      RETURN_CURRENT_ERROR;
  } 
  
  mport_packagemeta_vec_free(packs);
  
  if (clean_up(mport, tmpdir) != MPORT_OK)
    RETURN_CURRENT_ERROR;
  
  return MPORT_OK;
}

/* This does everything that has to happen before we start installing files.
 * We run mtree, pkg-install PRE-INSTALL, etc... 
 */
static int do_pre_install(mportInstance *mport, mportPackageMeta *pack, const char *tmpdir)
{
  int ret = MPORT_OK;
  
  /* run mtree */
  if ((ret = run_mtree(mport, tmpdir, pack)) != MPORT_OK)
    return ret;
  
  /* run pkg-install PRE-INSTALL */
  if ((ret = run_pkg_install(mport, tmpdir, pack, "PRE-INSTALL")) != MPORT_OK)
    return ret;

  return ret;    
}


static int do_actual_install(
      mportInstance *mport, 
      struct archive *a, 
      struct archive_entry *entry,
      mportPackageMeta *pack, 
      const char *tmpdir
    )
{
  int ret, file_total;
  int file_count = 0;
  mportPlistEntryType type;
  char *data; 
  char file[FILENAME_MAX], last[FILENAME_MAX], cwd[FILENAME_MAX];
  sqlite3_stmt *assets, *count;
  sqlite3 *db;
  
 
  db = mport->db;

  /* get the file count for the progress meter */
  if (mport_db_prepare(db, &count, "SELECT COUNT(*) FROM stub.assets WHERE type=%i", PLIST_FILE) != MPORT_OK)
    RETURN_CURRENT_ERROR;

  switch (sqlite3_step(count)) {
    case SQLITE_ROW:
      file_total = sqlite3_column_int(count, 0);
      sqlite3_finalize(count);
      break;
    default:
      SET_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
      sqlite3_finalize(count);
      RETURN_CURRENT_ERROR;
  }
  
  (mport->progress_init_cb)();
  
  if (mport_db_do(db, "BEGIN TRANSACTION") != MPORT_OK) 
    goto ERROR;

  /* Insert the package meta row into the packages table (We use pack here because things might have been twiddled) */  
  if ((ret = mport_db_do(db, "INSERT INTO packages (pkg, version, origin, prefix, lang, options) VALUES (%Q,%Q,%Q,%Q,%Q,%Q)", pack->name, pack->version, pack->origin, pack->prefix, pack->lang, pack->options)) != MPORT_OK)
    goto ERROR;
  /* Insert the assets into the master table */
  if ((ret = mport_db_do(db, "INSERT INTO assets (pkg, type, data, checksum) SELECT pkg,type,data,checksum FROM stub.assets WHERE pkg=%Q", pack->name)) != MPORT_OK)
    goto ERROR;  
  /* Insert the depends into the master table */
  if ((ret = mport_db_do(db, "INSERT INTO depends (pkg, depend_pkgname, depend_pkgversion, depend_port) SELECT pkg,depend_pkgname,depend_pkgversion,depend_port FROM stub.depends WHERE pkg=%Q", pack->name)) != MPORT_OK) 
    goto ERROR;
  
  if ((ret = mport_db_prepare(db, &assets, "SELECT type,data FROM stub.assets WHERE pkg=%Q", pack->name)) != MPORT_OK) 
    goto ERROR;

  (void)strlcpy(cwd, pack->prefix, sizeof(cwd));

  while ((ret = sqlite3_step(assets)) == SQLITE_ROW) {
    type = (mportPlistEntryType)sqlite3_column_int(assets, 0);
    data = (char *)sqlite3_column_text(assets, 1);
      
    switch (type) {
      case PLIST_CWD:      
        (void)strlcpy(cwd, data == NULL ? pack->prefix : data, sizeof(cwd));
        break;
      case PLIST_EXEC:
        if ((ret = mport_run_plist_exec(mport, data, cwd, last)) != MPORT_OK)
          goto ERROR;
        break;
      case PLIST_FILE:
        /* Our logic here is a bit backwards, because the first real file
         * in the archive was read in the loop in mport_install_pkg.  we
         * use the current entry and then update it. */
        if (entry == NULL) {
          ret = SET_ERROR(MPORT_ERR_INTERNAL, "Plist to arhive mismatch!");
          goto ERROR; 
        } 
        
        (void)strlcpy(last, data, sizeof(last));
        (void)snprintf(file, FILENAME_MAX, "%s%s/%s", mport->root, cwd, data);

        archive_entry_set_pathname(entry, file);

        if ((ret = archive_read_extract(a, entry, ARCHIVE_EXTRACT_OWNER|ARCHIVE_EXTRACT_PERM|ARCHIVE_EXTRACT_TIME|ARCHIVE_EXTRACT_ACL|ARCHIVE_EXTRACT_FFLAGS)) != ARCHIVE_OK) {
          ret = SET_ERROR(MPORT_ERR_ARCHIVE, archive_error_string(a));
          goto ERROR;
        }
        
        (mport->progress_step_cb)(++file_count, file_total, file);
        
        /* we only look for fatal, because EOF is only an error if we come
        back around. */
        if (archive_read_next_header(a, &entry) == ARCHIVE_FATAL) {
          ret = SET_ERROR(MPORT_ERR_ARCHIVE, archive_error_string(a));
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
    
  (mport->progress_free_cb)();
  
  return MPORT_OK;
  
  ERROR:
    (mport->progress_free_cb)();
    rollback();
    return ret;
}           

static int do_post_install(mportInstance *mport, mportPackageMeta *pack, const char *tmpdir)
{
  char to[FILENAME_MAX], from[FILENAME_MAX];
  (void)snprintf(from, FILENAME_MAX, "%s/%s/%s-%s/%s", tmpdir, MPORT_STUB_INFRA_DIR, pack->name, pack->version, MPORT_DEINSTALL_FILE);
  
  if (mport_file_exists(from)) {
    (void)snprintf(to, FILENAME_MAX, "%s%s/%s-%s/%s", mport->root, MPORT_INST_INFRA_DIR, pack->name, pack->version, MPORT_DEINSTALL_FILE);
    
    if (mport_mkdir(dirname(to)) != MPORT_OK)
      RETURN_CURRENT_ERROR;
  
    if (mport_copy_file(from, to) != MPORT_OK)
      RETURN_CURRENT_ERROR;
  }

  if (display_pkg_msg(mport, pack, tmpdir) != MPORT_OK)
    RETURN_CURRENT_ERROR;
  
  return run_pkg_install(mport, tmpdir, pack, "POST-INSTALL");
}


      
static int run_mtree(mportInstance *mport, const char *tmpdir, mportPackageMeta *pack)
{
  char file[FILENAME_MAX];
  int ret;
  
  (void)snprintf(file, FILENAME_MAX, "%s/%s/%s-%s/%s", tmpdir, MPORT_STUB_INFRA_DIR, pack->name, pack->version, MPORT_MTREE_FILE);
  
  if (mport_file_exists(file)) {
    if ((ret = mport_xsystem(mport, "%s -U -f %s -d -e -p %s >/dev/null", MPORT_MTREE_BIN, file, pack->prefix)) != 0) 
      RETURN_ERRORX(MPORT_ERR_SYSCALL_FAILED, "%s returned non-zero: %i", MPORT_MTREE_BIN, ret);
  }
  
  return MPORT_OK;
}



static int run_pkg_install(mportInstance *mport, const char *tmpdir, mportPackageMeta *pack, const char *mode)
{
  char file[FILENAME_MAX];
  int ret;
  
  (void)snprintf(file, FILENAME_MAX, "%s/%s/%s-%s/%s", tmpdir, MPORT_STUB_INFRA_DIR, pack->name, pack->version, MPORT_INSTALL_FILE);    
 
  if (mport_file_exists(file)) {
   if ((ret = mport_xsystem(mport, "PKG_PREFIX=%s %s %s %s %s", pack->prefix, MPORT_SH_BIN, file, pack->name, mode)) != 0)
     RETURN_ERRORX(MPORT_ERR_SYSCALL_FAILED, "%s %s returned non-zero: %i", MPORT_INSTALL_FILE, mode, ret);
  }
  
 return MPORT_OK;
}
 


static int display_pkg_msg(mportInstance *mport, mportPackageMeta *pack, const char *tmpdir)
{
  char filename[FILENAME_MAX];
  char *buf;
  struct stat st;
  FILE *file;
  
  (void)snprintf(filename, FILENAME_MAX, "%s/%s/%s-%s/%s", tmpdir, MPORT_STUB_INFRA_DIR, pack->name, pack->version, MPORT_MESSAGE_FILE);
  
  if (stat(filename, &st) == -1) 
    /* if we couldn't stat the file, we assume there isn't a pkg-msg */
    return MPORT_OK;
    
  if ((file = fopen(filename, "r")) == NULL) 
    RETURN_ERRORX(MPORT_ERR_FILEIO, "Couldn't open %s: %s", filename, strerror(errno));
  
  if ((buf = (char *)malloc(st.st_size * sizeof(char))) == NULL)
    return MPORT_ERR_NO_MEM;

  
  if (fread(buf, 1, st.st_size, file) != st.st_size) {
    free(buf);
    RETURN_ERRORX(MPORT_ERR_FILEIO, "Read error: %s", strerror(errno));
  }
  
  (mport->msg_cb)(buf);
  
  free(buf);
  
  return MPORT_OK;
}
 
 

static int clean_up(mportInstance *mport, const char *tmpdir) 
{
  if (mport_detach_stub_db(mport->db) != MPORT_OK)
    RETURN_CURRENT_ERROR;
    
#ifdef DEBUG
  return MPORT_OK;
#else
  return mport_rmtree(tmpdir);
#endif
}


static int rollback()
{
  fprintf(stderr, "Rollback called!\n");
  return 0;
}

