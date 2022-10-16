/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2008,2009 Chris Reinhardt
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

#include <archive.h>
#include <archive_entry.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "mport.h"
#include "mport_private.h"

/* build a hashtable with pkgname keys, values are a struct with fields for
 * filename, and boolean is_in_db */
struct table_entry {
  char *file;
  short is_in_db;
  char *name;
  struct table_entry *next;
};

#define TABLE_SIZE 128

static int build_stub_db(mportInstance *, sqlite3 **, const char *, const char *, const char **, struct table_entry **);
static int archive_metafiles(mportBundleWrite *, sqlite3 *, struct table_entry **);
static int archive_package_files(mportBundleWrite *, sqlite3 *, struct table_entry **);
static int extract_stub_db(const char *, const char *);

static struct table_entry * find_in_table(struct table_entry **, const char *);
static int insert_into_table(struct table_entry **, const char *, const char *);
static uint32_t SuperFastHash(const char *);

#include <err.h>

/*
 * mport_merge_primative(filenames, outfile)
 *
 * Takes a list of bundle filenames and an output filename.  This function will create
 * a new bundle file containing all the packages un the different input bundle files,
 * named `outfile`.  Care is taken to not have duplicates, to ensure that the exterior
 * dependencies are correct, and that the packages are in an optimal order for installation.
 */ 
MPORT_PUBLIC_API int
mport_merge_primative(mportInstance *mport, const char **filenames, const char *outfile)
{
  sqlite3 *db;
  mportBundleWrite *bundle;
  struct table_entry **table;
  char tmpdir[] = "/tmp/mport.XXXXXXXX";
  char *dbfile;
  
  if ((table = (struct table_entry **)calloc(TABLE_SIZE, sizeof(struct table_entry *))) == NULL)
    RETURN_ERROR(MPORT_ERR_FATAL, "Couldn't allocate hash table.");
  
  DIAG("mport_merge_primative(%p, %s)", filenames, outfile)
  
  if (mkdtemp(tmpdir) == NULL)
    RETURN_ERROR(MPORT_ERR_FATAL, "Couldn't make temp directory.");
  if (asprintf(&dbfile, "%s/%s", tmpdir, "merged.db") == -1)
    RETURN_ERROR(MPORT_ERR_FATAL, "Couldn't build merge database name.");
  
  DIAG("Building stub")

  /* this function merges the stub databases into one db. */      
  if (build_stub_db(mport, &db, tmpdir, dbfile, filenames, table) != MPORT_OK)
    RETURN_CURRENT_ERROR;
  
  DIAG("Stub complete: %s", dbfile)
    
  /* set up the bundle, and add our new stub database to it. */
  if ((bundle = mport_bundle_write_new()) == NULL)
    RETURN_ERROR(MPORT_ERR_FATAL, "Couldn't alloca bundle struct.");
  if (mport_bundle_write_init(bundle, outfile) != MPORT_OK)
    RETURN_CURRENT_ERROR;
   
  DIAG("Adding %s", dbfile)
    
  if (mport_bundle_write_add_file(bundle, dbfile, MPORT_STUB_DB_FILE) != MPORT_OK)
    RETURN_CURRENT_ERROR;
  
  DIAG("Adding metafiles")
  /* add all the meta files in the correct order */
  if (archive_metafiles(bundle, db, table) != MPORT_OK)
    RETURN_CURRENT_ERROR;

  DIAG("Adding realfiles")
  /* add all the other files */     
  if (archive_package_files(bundle, db, table) != MPORT_OK)
    RETURN_CURRENT_ERROR;

  DIAG("Realfiles complete")
  
  if (mport_bundle_write_finish(bundle) != MPORT_OK)
    RETURN_CURRENT_ERROR;
 
 /*if (mport_rmtree(tmpdir) != MPORT_OK)
   RETURN_CURRENT_ERROR; */
 
 return MPORT_OK;
}


/* This function goes through each file, and builds up the merged database as
 * filename `dbfile`.  It also builds up the hashtable of package -> filename pairs.
 * When this function is done, db points to a readonly sqlite object representing
 * the merged db.
 */
static int build_stub_db(mportInstance *mport, sqlite3 **db,  const char *tmpdir,  const char *dbfile,  const char **filenames, struct table_entry **table)
{
  char *tmpdbfile;
  const char *name;
  const char *file = NULL;
  int made_table = 0, ret;
  sqlite3_stmt *stmt;
  
  if (asprintf(&tmpdbfile, "%s/%s", tmpdir, "pkg.db") == -1)
    RETURN_ERROR(MPORT_ERR_FATAL, "Couldn't make stub db tempfile.");
  
  if (sqlite3_open(dbfile, db) != SQLITE_OK)
    RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(*db));
  
  if (mport_generate_stub_schema(mport, *db) != MPORT_OK)
    RETURN_CURRENT_ERROR;
    
  for (file = *filenames; file != NULL; file = *(++filenames)) {
    DIAG("Visiting %s", file)
    if (extract_stub_db(file, tmpdbfile) != MPORT_OK)
      RETURN_CURRENT_ERROR;

    if (mport_db_do(*db, "ATTACH %Q AS subbundle", tmpdbfile) != MPORT_OK)
      RETURN_CURRENT_ERROR;
    
    if (mport_db_do(*db, "BEGIN TRANSACTION") != MPORT_OK)
      RETURN_CURRENT_ERROR;
      
    if (made_table == 0) { 
      made_table++;
      if (mport_db_do(*db, "CREATE TABLE unsorted AS SELECT * FROM subbundle.packages") != MPORT_OK)
        RETURN_CURRENT_ERROR;
    } else {
      if (mport_db_do(*db, "INSERT INTO unsorted SELECT * FROM subbundle.packages") != MPORT_OK)
        RETURN_CURRENT_ERROR;
    }
    
    if (mport_db_do(*db, "INSERT INTO assets SELECT * FROM subbundle.assets") != MPORT_OK) 
      RETURN_CURRENT_ERROR;
    if (mport_db_do(*db, "INSERT INTO conflicts SELECT * FROM subbundle.conflicts") != MPORT_OK) 
      RETURN_CURRENT_ERROR;
    if (mport_db_do(*db, "INSERT INTO depends SELECT * FROM subbundle.depends") != MPORT_OK) 
      RETURN_CURRENT_ERROR;

    /* build our hashtable (pkgname => metadata) up */      
    if (mport_db_prepare(*db, &stmt, "SELECT pkg FROM subbundle.packages") != MPORT_OK) {
      sqlite3_finalize(stmt);
      RETURN_CURRENT_ERROR;
    }
    
    while (1) {
      ret = sqlite3_step(stmt);
      
      if (ret == SQLITE_ROW) {
        name = sqlite3_column_text(stmt, 0);
        if (insert_into_table(table, name, file) != MPORT_OK) {
          sqlite3_finalize(stmt);
          RETURN_CURRENT_ERROR;
        }
      } else if (ret == SQLITE_DONE) {
        break;
      } else {
        SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(*db));
        sqlite3_finalize(stmt);
        RETURN_CURRENT_ERROR;
      }
    }
        
    sqlite3_finalize(stmt);
    
    if (mport_db_do(*db, "COMMIT TRANSACTION") != MPORT_OK)
      RETURN_CURRENT_ERROR;  
    if (mport_db_do(*db, "DETACH subbundle") != MPORT_OK)
      RETURN_CURRENT_ERROR;    
  }

  /* just have to sort the packages (going from unsorted to packages), no big deal... ;) */
  while (1) {
    if (mport_db_do(*db, "INSERT INTO packages SELECT * FROM unsorted WHERE NOT EXISTS (SELECT 1 FROM packages WHERE packages.pkg=unsorted.pkg) AND (NOT EXISTS (SELECT 1 FROM depends WHERE depends.pkg=unsorted.pkg) OR NOT EXISTS (SELECT 1 FROM depends LEFT JOIN packages ON depends.depend_pkgname=packages.pkg WHERE depends.pkg=unsorted.pkg AND EXISTS (SELECT 1 FROM unsorted AS us2 WHERE us2.pkg=depend_pkgname) AND packages.pkg ISNULL))") != MPORT_OK)
      RETURN_CURRENT_ERROR;
    if (sqlite3_changes(*db) == 0) /* if there is nothing left to insert, we're done */
      break;
  }
      
  /* Check that unsorted and packages have the same number of rows. */     
  if (mport_db_prepare(*db, &stmt, "SELECT COUNT(DISTINCT pkg) FROM packages")) {
    sqlite3_finalize(stmt);
    RETURN_CURRENT_ERROR;  
  }
  if (sqlite3_step(stmt) != SQLITE_ROW) {
    SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(*db));
    sqlite3_finalize(stmt);
    RETURN_CURRENT_ERROR;
  }
  
  int pkgs = sqlite3_column_int(stmt, 0);
  sqlite3_finalize(stmt);
  
  if (mport_db_prepare(*db, &stmt, "SELECT COUNT(DISTINCT pkg) FROM unsorted")) {
    sqlite3_finalize(stmt);
    RETURN_CURRENT_ERROR;
  }
    
  if (sqlite3_step(stmt) != SQLITE_ROW) {
    SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(*db));
    sqlite3_finalize(stmt);
    RETURN_CURRENT_ERROR;
  }
  
  int unsort = sqlite3_column_int(stmt, 0);
  sqlite3_finalize(stmt);
  
  if (pkgs != unsort) 
    RETURN_ERRORX(MPORT_ERR_FATAL, "Sorted (%i) and unsorted (%i) counts do no match.", pkgs, unsort);
    
      
  /* Close the stub database handle, and reopen as read only to ensure that we don't
   * try to change it after this point 
   */    
  if (sqlite3_close(*db) != SQLITE_OK)
    RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(*db));
  if (sqlite3_open_v2(dbfile, db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK)
    RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(*db));
  
  return MPORT_OK;
}


static int archive_metafiles(mportBundleWrite *bundle, sqlite3 *db, struct table_entry **table) 
{
  sqlite3_stmt *stmt;
  int ret, sret;
  char *filename;
  const char *pkgname;
  struct table_entry *match = NULL;
  mportBundleRead *inbundle= NULL;
  struct archive_entry *entry;
  
  ret = MPORT_OK;
        
  if (mport_db_prepare(db, &stmt, "SELECT pkg FROM packages") != MPORT_OK) {
	  sqlite3_finalize(stmt);
	  RETURN_CURRENT_ERROR;
  }
    
  while (1) {
    sret = sqlite3_step(stmt);
    
    if (sret == SQLITE_DONE) {
      goto DONE;
    } else if (sret != SQLITE_ROW) {
      ret = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
      goto DONE;
    } 
    
    /* at this point, ret must be SQLITE_ROW */
    pkgname  = sqlite3_column_text(stmt, 0);
    match = find_in_table(table, pkgname);
    
    if (match == NULL) {
      ret = SET_ERRORX(MPORT_ERR_FATAL, "Couldn't find package '%s' in filename table.", pkgname);
      goto DONE;
    }
    
    filename = match->file;
    
    if ((inbundle = mport_bundle_read_new()) == NULL) {
      SET_ERROR(MPORT_ERR_FATAL, "Couldn't allocate bundle");
      goto DONE;
    }

    
    if (mport_bundle_read_init(inbundle, filename) != MPORT_OK) {
      ret = mport_bundle_read_finish(NULL, inbundle);
      goto DONE;
    }

    
    /* skip the sub db */
    if (mport_bundle_read_next_entry(inbundle, &entry) != MPORT_OK) {
      ret = mport_bundle_read_finish(NULL, inbundle);
      goto DONE;
    }
    if (archive_read_data_skip(inbundle->archive) != ARCHIVE_OK) {
      ret = mport_bundle_read_finish(NULL, inbundle);
      ret = SET_ERRORX(MPORT_ERR_FATAL, "Unable to read %s: %s", filename, archive_error_string(inbundle->archive));
      goto DONE;
    }
    
    
    while (1) {
      if (mport_bundle_read_next_entry(inbundle, &entry) != MPORT_OK) {
        ret = mport_bundle_read_finish(NULL, inbundle);
        goto DONE;
      } 
      
      if (*(archive_entry_pathname(entry)) != '+')
        break;
      
      DIAG("Adding %s", archive_entry_pathname(entry))
      
      if ((ret = mport_bundle_write_add_entry(bundle, inbundle, entry)) != MPORT_OK) {
        DIAG("bundle add entry failed")
        ret = mport_bundle_read_finish(NULL, inbundle);
        goto DONE;
      }
    }

    ret = mport_bundle_read_finish(NULL, inbundle);    
  }
  
  DONE:
    sqlite3_finalize(stmt);
    return ret;
}



static int archive_package_files(mportBundleWrite *bundle, sqlite3 *db, struct table_entry **table)
{
  sqlite3_stmt *stmt, *files;
  int ret;
  struct table_entry *cur;
  const char *pkgname;
  const char *file;
  mportBundleRead *inbundle;
  struct archive_entry *entry;
  
  if (mport_db_prepare(db, &stmt, "SELECT pkg FROM packages") != MPORT_OK) {
	  sqlite3_finalize(stmt);
	  RETURN_CURRENT_ERROR;
  }
  
  while (1) {
    ret = sqlite3_step(stmt);
    
    if (ret == SQLITE_DONE)
      break;
    
    if (ret != SQLITE_ROW) {
      SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
      sqlite3_finalize(stmt);
      RETURN_CURRENT_ERROR;
    }
    
    pkgname = sqlite3_column_text(stmt, 0);
    cur     = find_in_table(table, pkgname);
    
    if (cur == NULL) {
      sqlite3_finalize(stmt);
      RETURN_ERRORX(MPORT_ERR_FATAL, "Couldn't find package '%s' in bundle hash table", pkgname);
    }
    
    file = cur->file;
        
    if ((inbundle = mport_bundle_read_new()) == NULL)
      RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");
        
    
    if (mport_bundle_read_init(inbundle, file) != MPORT_OK) {
      mport_bundle_read_finish(NULL, inbundle);
      RETURN_CURRENT_ERROR;
    }
    
    if (mport_bundle_read_skip_metafiles(inbundle) != MPORT_OK) {
      mport_bundle_read_finish(NULL, inbundle);
      sqlite3_finalize(stmt);
      RETURN_CURRENT_ERROR;
    }

    if (mport_db_prepare(db, &files, "SELECT data FROM assets WHERE pkg=%Q AND (type=%i or type=%i or type=%i)", pkgname, ASSET_FILE, ASSET_SAMPLE, ASSET_SAMPLE_OWNER_MODE) != MPORT_OK) {
      mport_bundle_read_finish(NULL, inbundle);
      sqlite3_finalize(stmt);
      RETURN_CURRENT_ERROR;
    }
    
    while (1) {
      int fret = sqlite3_step(files);

      if (fret == SQLITE_DONE) {
        sqlite3_finalize(files);
        break;
      } else if (fret != SQLITE_ROW) {
        SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        sqlite3_finalize(files);
        mport_bundle_read_finish(NULL, inbundle);
        RETURN_CURRENT_ERROR;
      }
      
      file = sqlite3_column_text(files, 0);
      
      if (mport_bundle_read_next_entry(inbundle, &entry) != MPORT_OK) {
        mport_bundle_read_finish(NULL, inbundle);
        sqlite3_finalize(stmt);
        sqlite3_finalize(files);
        RETURN_CURRENT_ERROR;
      } 
      
      if (strcmp(file, archive_entry_pathname(entry)) != 0) {
        SET_ERRORX(MPORT_ERR_FATAL, "Plist to archive mismatch in package %s: found '%s', expected '%s'", pkgname, archive_entry_pathname(entry), file);
        mport_bundle_read_finish(NULL, inbundle);
        sqlite3_finalize(stmt);
        sqlite3_finalize(files);
        RETURN_CURRENT_ERROR;
      }

      DIAG("Adding realfile: %s", archive_entry_pathname(entry));
  
      if (mport_bundle_write_add_entry(bundle, inbundle, entry) != MPORT_OK) {
        mport_bundle_read_finish(NULL, inbundle);
        sqlite3_finalize(stmt);
        sqlite3_finalize(files);
        RETURN_CURRENT_ERROR;
      } 
    }
  
    /* we're done with this package, onto the next one */
    sqlite3_finalize(files);
    mport_bundle_read_finish(NULL, inbundle);
  } 
  
  sqlite3_finalize(stmt);
  
  return MPORT_OK;   
}



/* get the stub database file out of filename and place it at destfile */
static int extract_stub_db(const char *filename, const char *destfile)
{
  struct archive *a = archive_read_new();
  struct archive_entry *entry;
  
  if (a == NULL)
    RETURN_ERROR(MPORT_ERR_FATAL, "Couldn't allocate read archive struct");

  if (archive_read_support_format_tar(a) != ARCHIVE_OK)
    RETURN_ERROR(MPORT_ERR_FATAL, archive_error_string(a));
  if (archive_read_support_filter_xz(a))
    RETURN_ERROR(MPORT_ERR_FATAL, archive_error_string(a));
    
  if (archive_read_open_filename(a, filename, 10240) != ARCHIVE_OK) {
    SET_ERRORX(MPORT_ERR_FATAL, "Could not open %s: %s", filename, archive_error_string(a));
    archive_read_free(a);
    RETURN_CURRENT_ERROR;
  }
  
  if (archive_read_next_header(a, &entry) != ARCHIVE_OK) {
    SET_ERROR(MPORT_ERR_FATAL, archive_error_string(a));
    archive_read_free(a);
    RETURN_CURRENT_ERROR;
  }
  
  if (strcmp(archive_entry_pathname(entry), MPORT_STUB_DB_FILE) != 0) {
    archive_read_free(a);
    RETURN_ERROR(MPORT_ERR_FATAL, "Invalid bundle file: stub database is not the first file");
  }
    
  archive_entry_set_pathname(entry, destfile);
  
  if (archive_read_extract(a, entry, 0) != ARCHIVE_OK) {
    SET_ERROR(MPORT_ERR_FATAL, archive_error_string(a));
    archive_read_free(a);
    RETURN_CURRENT_ERROR;
  }
  
  if (archive_read_free(a) != ARCHIVE_OK)
    RETURN_ERROR(MPORT_ERR_FATAL, archive_error_string(a));
    
  return MPORT_OK;
}


/* insert into a name => file pair into the given hash table. */
static int insert_into_table(struct table_entry **table, const char *name, const char *file)
{
  struct table_entry *node, *cur;
  int hash = SuperFastHash(name) % TABLE_SIZE;
  
  if ((node = (struct table_entry *)calloc(1, sizeof(struct table_entry))) == NULL)
    RETURN_ERROR(MPORT_ERR_FATAL, "Couldn't allocate table entry");

  node->name     = strdup(name);
  node->file     = strdup(file);
  node->is_in_db = 0;
  node->next     = NULL;
   
  if (table[hash] == NULL) {     
    table[hash] = node;   
  } else {
    cur = table[hash];
    while (cur->next != NULL) 
      cur = cur->next;
  
    cur->next = node;
  }
  
  return MPORT_OK;
}


static struct table_entry * find_in_table(struct table_entry **table, const char *name)
{
  int hash = SuperFastHash(name) % TABLE_SIZE;
  struct table_entry *e = NULL;
  
  e = table[hash];  
  while (e != NULL) {
    if (strcmp(e->name, name) == 0)
      return e;
      
    e = e->next;
  }
  
  return e;
}
      
      
    
      
/* Paul Hsieh's fast hash function, from http://www.azillionmonkeys.com/qed/hash.html */
/* This function has been modified to only work with C strings */
#undef get16bits
#if (defined(__GNUC__) && defined(__i386__)) || defined(__WATCOMC__) \
  || defined(_MSC_VER) || defined (__BORLANDC__) || defined (__TURBOC__)
#define get16bits(d) (*((const uint16_t *) (d)))
#endif

#if !defined (get16bits)
#define get16bits(d) ((((uint32_t)(((const uint8_t *)(d))[1])) << 8)\
                       +(uint32_t)(((const uint8_t *)(d))[0]) )
#endif

static uint32_t SuperFastHash(const char * data) 
{
    int len = strlen(data);
    uint32_t hash = len, tmp;
    int rem;

    if (len <= 0 || data == NULL) return 0;

    rem = len & 3;
    len >>= 2;

    /* Main loop */
    for (;len > 0; len--) {
        hash  += get16bits (data);
        tmp    = (get16bits (data+2) << 11) ^ hash;
        hash   = (hash << 16) ^ tmp;
        data  += 2*sizeof (uint16_t);
        hash  += hash >> 11;
    }

    /* Handle end cases */
    switch (rem) {
        case 3: hash += get16bits (data);
                hash ^= hash << 16;
                hash ^= data[sizeof (uint16_t)] << 18;
                hash += hash >> 11;
                break;
        case 2: hash += get16bits (data);
                hash ^= hash << 11;
                hash += hash >> 17;
                break;
        case 1: hash += *data;
                hash ^= hash << 10;
                hash += hash >> 1;
    }

    /* Force "avalanching" of final 127 bits */
    hash ^= hash << 3;
    hash += hash >> 5;
    hash ^= hash << 4;
    hash += hash >> 17;
    hash ^= hash << 25;
    hash += hash >> 6;

    return hash;
}


