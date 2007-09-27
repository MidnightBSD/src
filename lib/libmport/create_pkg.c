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
 * $MidnightBSD: src/lib/libmport/create_pkg.c,v 1.4 2007/09/24 20:58:00 ctriv Exp $
 */



#include <sys/cdefs.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sqlite3.h>
#include <errno.h>
#include <md5.h>
#include <archive.h>
#include <archive_entry.h>
#include <mport.h>

__MBSDID("$MidnightBSD: src/lib/libmport/create_pkg.c,v 1.4 2007/09/24 20:58:00 ctriv Exp $");

#define PACKAGE_DB_FILENAME "+CONTENTS.db"

static int create_package_db(sqlite3 **);
static int create_plist(sqlite3 *, mportPlist *, mportPackageMeta *);
static int create_meta(sqlite3 *, mportPackageMeta *);
static int insert_depends(sqlite3 *, mportPackageMeta *);
static int insert_conflicts(sqlite3 *, mportPackageMeta *);
static int copy_metafiles(mportPackageMeta *);
static int archive_files(mportPlist *, mportPackageMeta *);
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
    
  if ((ret = copy_metafiles(pack)) != MPORT_OK) 
    return ret;
    
  if ((ret = archive_files(plist, pack)) != MPORT_OK)
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
  int ret;
  
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

  /* insert depends and conflicts */
  if ((ret = insert_depends(db, pack)) != MPORT_OK)
    return ret;  
  if ((ret = insert_conflicts(db, pack)) != MPORT_OK)
    return ret;
    
  return MPORT_OK;
}


static int insert_conflicts(sqlite3 *db, mportPackageMeta *pack) 
{
  sqlite3_stmt *stmnt;
  char sql[]  = "INSERT INTO conflicts (pkg, conflict_pkg, conflict_version) VALUES (?,?,?)";
  char **conflict  = pack->conflicts;
  char *version;
  const char *rest = 0;
  
  /* we're done if there are no conflicts to record. */
  if (conflict == NULL) 
    return MPORT_OK;
  
  if (sqlite3_prepare_v2(db, sql, -1, &stmnt, &rest) != SQLITE_OK) {
    RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
  }

  /* we have a conflict like apache-1.4.  We want to do a m/(.*)-(.*)/ */
  while (*conflict != NULL) {
    version = rindex(*conflict, '-');
    *version = '\0';
    version++;
    
    if (sqlite3_bind_text(stmnt, 1, pack->name, -1, SQLITE_STATIC) != SQLITE_OK) {
      RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
    }
    if (sqlite3_bind_text(stmnt, 2, *conflict, -1, SQLITE_STATIC) != SQLITE_OK) {
      RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
    }
    if (sqlite3_bind_text(stmnt, 3, version, -1, SQLITE_STATIC) != SQLITE_OK) {
      RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
    }
    if (sqlite3_step(stmnt) != SQLITE_DONE) {
      RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
    }
    sqlite3_reset(stmnt);
    conflict++;
  }
  
  sqlite3_finalize(stmnt);
  
  return MPORT_OK;
}
    
  

static int insert_depends(sqlite3 *db, mportPackageMeta *pack) 
{
  sqlite3_stmt *stmnt;
  char sql[]  = "INSERT INTO depends (pkg, depend_pkgname, depend_pkgversion, depend_port) VALUES (?,?,?,?)";
  char **depend    = pack->depends;
  char *pkgversion;
  char *port;
  const char *rest = 0;
  
  /* we're done if there are no deps to record. */
  if (depend == NULL) 
    return MPORT_OK;
  
  if (sqlite3_prepare_v2(db, sql, -1, &stmnt, &rest) != SQLITE_OK) {
    RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
  }
  
  
  /* depends look like this.  break'em up into port, pkgversion and pkgname
   * perl-5.8.8_1:lang/perl5.8
   */
  while (*depend != NULL) {
    port = rindex(*depend, ':');
    *port = '\0';
    port++;
    
    pkgversion = rindex(*depend, '-');
    *pkgversion = '\0';
    pkgversion++;
    
    if (sqlite3_bind_text(stmnt, 1, pack->name, -1, SQLITE_STATIC) != SQLITE_OK) {
      RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
    }
    if (sqlite3_bind_text(stmnt, 2, *depend, -1, SQLITE_STATIC) != SQLITE_OK) {
      RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
    }
    if (sqlite3_bind_text(stmnt, 3, pkgversion, -1, SQLITE_STATIC) != SQLITE_OK) {
      RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
    }
    if (sqlite3_bind_text(stmnt, 4, port, -1, SQLITE_STATIC) != SQLITE_OK) {
      RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
    }
    if (sqlite3_step(stmnt) != SQLITE_DONE) {
      RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
    }
    sqlite3_reset(stmnt);
    depend++;
  }
    
  sqlite3_finalize(stmnt);
  
  return MPORT_OK;
}



/* this is just to save a lot of typing.  It will only work in the
   copy_metafiles() function.
 */ 
#define COPY_PKG_METAFILE(field, tofile) \
  if (pack->field != NULL && mport_file_exists(pack->field)) { \
    if ((ret = mport_copy_file(pack->field, #tofile)) != MPORT_OK) { \
      return ret; \
    } \
  } \


static int copy_metafiles(mportPackageMeta *pack) 
{
  int ret;
  
  COPY_PKG_METAFILE(mtree, +MTREE);
  COPY_PKG_METAFILE(pkginstall, +INSTALL);
  COPY_PKG_METAFILE(pkgdeinstall, +DEINSTALL);
  COPY_PKG_METAFILE(pkgmessage, +MESSAGE);
  
  return ret;
}


static int archive_files(mportPlist *plist, mportPackageMeta *pack)
{
  struct archive *a;
  struct archive_entry *entry;
  struct stat st;
  DIR *dir;
  struct dirent *diren;
  mportPlistEntry *e;
  char filename[FILENAME_MAX];
  char buff[8192];
  char *cwd;
  int len;
  int fd;
   
  cwd = pack->prefix; 
    
  a = archive_write_new();
  archive_write_set_compression_bzip2(a);
  archive_write_set_format_pax(a);
  
  if (archive_write_open_filename(a, pack->pkg_filename) != ARCHIVE_OK) {
    RETURN_ERROR(MPORT_ERR_ARCHIVE, archive_error_string(a)); 
  }
  
  /* First step - add the files in the tmpdir to the archive. */    
  if ((dir = opendir(".")) == NULL) {
    RETURN_ERROR(MPORT_ERR_SYSCALL_FAILED, strerror(errno));
  }
  
  while ((diren = readdir(dir)) != NULL) {
    if (strcmp(diren->d_name, ".") == 0 || strcmp(diren->d_name, "..") == 0) 
      continue;
    
    if (lstat(diren->d_name, &st) != 0) {
      RETURN_ERROR(MPORT_ERR_SYSCALL_FAILED, strerror(errno));
    }
    
    entry = archive_entry_new();
    archive_entry_copy_stat(entry, &st);
    archive_entry_set_pathname(entry, diren->d_name);
    archive_write_header(a, entry);
    if ((fd = open(diren->d_name, O_RDONLY)) == -1) {
      RETURN_ERROR(MPORT_ERR_SYSCALL_FAILED, strerror(errno));
    }
    
    len = read(fd, buff, sizeof(buff));
    while (len > 0) {
      archive_write_data(a, buff, len);
      len = read(fd, buff, sizeof(buff));
    }
    
    archive_entry_free(entry);
    close(fd);
  }
  
  closedir(dir);
  
  /* second step - all the files in the plist */  
  STAILQ_FOREACH(e, plist, next) {
    if (e->type == PLIST_CWD) {
      if (e->data == NULL) {
        cwd = pack->prefix;
      } else {
        cwd = e->data;
      }
    }
    
    if (e->type != PLIST_FILE) {
      continue;
    }
    
    snprintf(filename, FILENAME_MAX, "%s/%s/%s", pack->sourcedir, cwd, e->data);
    
    if (lstat(filename, &st) != 0) {
      RETURN_ERROR(MPORT_ERR_SYSCALL_FAILED, strerror(errno));
    }
    
    entry = archive_entry_new();
    archive_entry_copy_stat(entry, &st);
    archive_entry_set_pathname(entry, e->data);
    archive_write_header(a, entry);
    if ((fd = open(filename, O_RDONLY)) == -1) {
      RETURN_ERROR(MPORT_ERR_SYSCALL_FAILED, strerror(errno));
    }
    
    len = read(fd, buff, sizeof(buff));
    while (len > 0) {
      archive_write_data(a, buff, len);
      len = read(fd, buff, sizeof(buff));
    }
    
    archive_entry_free(entry);
    close(fd);
  }
  
  archive_write_finish(a);
}

#ifdef DEBUG

static int clean_up(const char *tmpdir)
{
  /* do nothing */
}

#else

static int clean_up(const char *tmpdir) 
{
  return mport_rmtree(tmpdir);
}

#endif

