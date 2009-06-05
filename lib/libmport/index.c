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
 *
 * $MidnightBSD: src/lib/libmport/error.c,v 1.7 2008/04/26 17:59:26 ctriv Exp $
 */


#include "mport.h"
#include "mport_private.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

static int index_is_recentish(mportInstance *);
static int lookup_alias(mportInstance *, const char *, char **);

/*
 * Loads the index database.  The index contains a list of bundles that are
 * available for download, a list of aliases (apache is aliased to apache22 for 
 * example), and a list of mirrors.
 *
 * This function will use the current local index if it is present and younger
 * than the max index age.  Otherwise, it will download the index.  If any 
 * index is present, the mirror list will be used; otherwise the bootstrap
 * url will be used.
 */
MPORT_PUBLIC_API int mport_index_load(mportInstance *mport)
{
  if (mport_file_exists(MPORT_INDEX_FILE)) {
    if (mport_db_do(mport->db, "ATTACH %Q AS index", MPORT_INDEX_FILE) != MPORT_OK)
        RETURN_CURRENT_ERROR;
        
    mport->flags |= MPORT_INST_HAVE_INDEX;
  
    if (!index_is_recentish(mport)) {
      if (mport_fetch_index(mport) != MPORT_OK)
        RETURN_CURRENT_ERROR;
        
      if (mport_db_do(mport->db, "DETACH index") != MPORT_OK)
        RETURN_CURRENT_ERROR;
        
      mport->flags &= ~MPORT_INST_HAVE_INDEX;
        
      if (mport_db_do(mport->db, "ATTACH %Q AS index", MPORT_INDEX_FILE) != MPORT_OK)
        RETURN_CURRENT_ERROR;
        
      mport->flags |= MPORT_INST_HAVE_INDEX;
    }
  } else {
    if (mport_fetch_bootstrap_index(mport) != MPORT_OK)
      RETURN_CURRENT_ERROR;
    
    if (mport_db_do(mport->db, "ATTACH %Q AS index", MPORT_INDEX_FILE) != MPORT_OK)
      RETURN_CURRENT_ERROR;
      
    mport->flags |= MPORT_INST_HAVE_INDEX;
  }
  

  return MPORT_OK;
}


/* return 1 if the index is younger than the max age, 0 otherwise */
static int index_is_recentish(mportInstance *mport) 
{
  struct stat st;
  struct timespec now;
  
  if (stat(MPORT_INDEX_FILE, &st) != 0) 
    return 0;
   
  if (clock_gettime(CLOCK_REALTIME, &now) != 0) 
    RETURN_ERROR(MPORT_ERR_FATAL, strerror(errno));
      
  if ((st.st_birthtime + MPORT_MAX_INDEX_AGE) < now.tv_sec) 
    return 0;
    
  return 1;
}  


/*
 * Fills the string vector with the list of the mirrors for the current
 * country.  
 * 
 * XXX - The country is currently hardcoded to the US.
 */
int mport_index_get_mirror_list(mportInstance *mport, char ***list_p)
{
  char **list;
  int len, ret, i;
  sqlite3_stmt *stmt;
  
  /* XXX the country is hard coded until a configuration system is created */    
  if (mport_db_prepare(mport->db, &stmt, "SELECT COUNT(*) FROM index.mirrors WHERE country='us'") != MPORT_OK)
    RETURN_CURRENT_ERROR;

  switch (sqlite3_step(stmt)) {
    case SQLITE_ROW:
      len = sqlite3_column_int(stmt, 0);
      sqlite3_finalize(stmt);
      break;
    case SQLITE_DONE:
      SET_ERROR(MPORT_ERR_FATAL, "A 'SELECT COUNT(*)...' statement returned no rows.");
      sqlite3_finalize(stmt);
      RETURN_CURRENT_ERROR;
      break;
    default:
      SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(mport->db));
      sqlite3_finalize(stmt);
      RETURN_CURRENT_ERROR;
  }
  
  list = calloc(len + 1, sizeof(char *));
  *list_p = list;  
  i = 0;
    
  if (mport_db_prepare(mport->db, &stmt, "SELECT mirror FROM index.mirrors WHERE country='us'") != MPORT_OK)
    RETURN_CURRENT_ERROR;
    
  while (1) {
    ret = sqlite3_step(stmt);
    
    if (ret == SQLITE_ROW) {
      list[i] = strdup(sqlite3_column_text(stmt, 0));
      
      if (list[i] == NULL) {
        sqlite3_finalize(stmt);
        RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");
      }
      
      i++;
    } else if (ret == SQLITE_DONE) {
      list[i] = NULL;
      break;
    } else {
      sqlite3_finalize(stmt);
      RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(mport->db));
    }
  }
  
  sqlite3_finalize(stmt);
  return MPORT_OK;
}

/*
 * Looks up a pkgname from the index and fills a vector of index entries
 * with the result.
 *
 * Globbing is supported, and the alias list is consulted.  The calling code
 * is responsible for freeing the memory allocated.  See
 * mport_index_entry_free_vec()
 */
MPORT_PUBLIC_API int mport_index_lookup_pkgname(mportInstance *mport, const char *pkgname, mportIndexEntry ***entry_vec)
{
  char *lookup;
  int count, i, step;
  sqlite3_stmt *stmt;
  int ret = MPORT_OK;
  mportIndexEntry **e;
  
  MPORT_CHECK_FOR_INDEX(mport, "mport_index_lookup_pkgname()")
  
  if (lookup_alias(mport, pkgname, &lookup) != MPORT_OK)
    RETURN_CURRENT_ERROR;

  if (mport_db_prepare(mport->db, &stmt, "SELECT COUNT(*) FROM index.packages WHERE pkg GLOB %Q", lookup) != MPORT_OK)
    RETURN_CURRENT_ERROR;
    
  switch (sqlite3_step(stmt)) {
    case SQLITE_ROW:
      count = sqlite3_column_int(stmt, 0);
      break;
    case SQLITE_DONE:
      ret = SET_ERROR(MPORT_ERR_FATAL, "No rows returned from a 'SELECT COUNT(*)' query.");
      goto DONE;
      break;
    default:
      ret = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(mport->db));
      goto DONE;
      break;
  }
  
  sqlite3_finalize(stmt);
  
  e = (mportIndexEntry **)calloc(count + 1, sizeof(mportIndexEntry *));
  *entry_vec = e;
  
  if (count == 0) 
    return MPORT_OK;
  
  if (mport_db_prepare(mport->db, &stmt, "SELECT pkg, version, comment, www, bundlefile FROM index.packages WHERE pkg GLOB %Q", lookup) != MPORT_OK) {
    ret = mport_err_code();
    goto DONE;
  }
  
  while (1) {
    step = sqlite3_step(stmt);
    
    if (step == SQLITE_ROW) {
      if ((e[i] = (mportIndexEntry *)malloc(sizeof(mportIndexEntry))) == NULL) {
        ret = MPORT_ERR_FATAL;
        goto DONE;
      }
      
      e[i]->pkgname    = strdup(sqlite3_column_text(stmt, 0));
      e[i]->version    = strdup(sqlite3_column_text(stmt, 1));
      e[i]->comment    = strdup(sqlite3_column_text(stmt, 2));
      e[i]->www        = strdup(sqlite3_column_text(stmt, 3));
      e[i]->bundlefile = strdup(sqlite3_column_text(stmt, 4));
      
      if (e[i]->pkgname == NULL || e[i]->version == NULL || e[i]->comment == NULL || e[i]->www == NULL || e[i]->bundlefile == NULL) {
        ret = MPORT_ERR_FATAL;
        goto DONE;
      }
      
      i++;
    } else if (step == SQLITE_DONE) {
      e[i] = NULL;
      goto DONE;
    } else {
      ret = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(mport->db));
      goto DONE;
    }
  }
      
  DONE:
    free(lookup);
    sqlite3_finalize(stmt);
    return ret; 
}


static int lookup_alias(mportInstance *mport, const char *query, char **result) 
{
  sqlite3_stmt *stmt;
  int ret = MPORT_OK;
  
  if (mport_db_prepare(mport->db, &stmt, "SELECT pkg FROM index.aliases WHERE alias=%Q", query) != MPORT_OK)
    RETURN_CURRENT_ERROR;
  
  switch (sqlite3_step(stmt)) {
    case SQLITE_ROW:
      *result = strdup(sqlite3_column_text(stmt, 0));
      break;
    case SQLITE_DONE:
      *result = strdup(query);
      break;
    default:
      ret = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(mport->db));
      break;
  }
  
  sqlite3_finalize(stmt);
  return ret;   
}


/* free a vector of mportIndexEntry structs */
MPORT_PUBLIC_API void mport_index_entry_free_vec(mportIndexEntry **e)
{
  int i;
  
  for (i=0; e[i] != NULL; i++) 
    mport_index_entry_free(e[i]);
  
  free(e);
}


/* free a mportIndexEntry struct */
MPORT_PUBLIC_API void mport_index_entry_free(mportIndexEntry *e) 
{
  free(e->pkgname);
  free(e->comment);
  free(e->version);
  free(e->www);
  free(e->bundlefile);
  free(e);
}
