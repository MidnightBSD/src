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
#include <stdlib.h>
#include <stdio.h>
#include <sys/param.h>
#include <fetch.h>
#include <string.h>
#include <errno.h>

#define BUFFSIZE	1024 * 8

static int fetch(mportInstance *, const char *, const char *);


/* mport_fetch_index(mport)
 *
 * Fetch the index from a remote, or the bootstrap if we don't currently
 * have an index.  If the current index is recentish, then don't do
 * anything.
 */
int mport_fetch_index(mportInstance *mport)
{
  char **mirrors;
  char *url;
  char *dest;
  int i;
  
  MPORT_CHECK_FOR_INDEX(mport, "mport_fetch_index()");
  
  asprintf(&dest, "%s/%s.bz2", MPORT_FETCH_STAGING_DIR, MPORT_INDEX_FILE);
  
  if (dest == NULL)
    RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");

  if (mport_index_get_mirror_list(mport, &mirrors) != MPORT_OK)
    RETURN_CURRENT_ERROR;
    
  while (mirrors[i] != NULL) {
    asprintf(&url, "%s/%s", mirrors[i], MPORT_INDEX_URL_PATH);

    if (url == NULL) {
      free(dest);
      mport_free_vec(mirrors);
      RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");
    }

    if (fetch(mport, url, MPORT_INDEX_FILE) == MPORT_OK) {
      free(url);
      free(dest);
      mport_free_vec(mirrors);
      return MPORT_OK;
    } 
    
    free(url);
  }
    
  free(dest);
  mport_free_vec(mirrors);
  RETURN_ERRORX(MPORT_ERR_FATAL, "Unable to fetch index file: %s", mport_err_string());
}



/* mport_fetch_bootstrap_index(mportInstance *mport)
 *
 * Fetches the index for the bootstrap site.  The index need not be loaded for this 
 * to be used.
 */
int mport_fetch_bootstrap_index(mportInstance *mport)
{
  return fetch(mport, MPORT_BOOTSTRAP_INDEX_URL, MPORT_INDEX_FILE);
}

/* mport_fetch_bundle(mport, filename)
 *
 * Fetch a given bundle from a remote.  If there is no loaded index, then
 * an error is thrown.  The file will be downloaded to the MPORT_FETCH_STAGING_DIR
 * directory.
 */
int mport_fetch_bundle(mportInstance *mport, const char *filename)
{
  char **mirrors;
  char *url;
  char *dest;
  int i;

  MPORT_CHECK_FOR_INDEX(mport, "mport_fetch_bundle()");
  
  if (mport_index_get_mirror_list(mport, &mirrors) != MPORT_OK)
    RETURN_CURRENT_ERROR;
    
  asprintf(&dest, "%s/%s", MPORT_FETCH_STAGING_DIR, filename);
  
  while (mirrors[i] != NULL) {
    asprintf(&url, "%s/%s/%s", mirrors[i], MPORT_URL_PATH, filename);

    if (fetch(mport, url, dest) == MPORT_OK) {
      free(url);
      free(dest);
      mport_free_vec(mirrors);
      return MPORT_OK;
    } 
    
    free(url);
  }
  
  free(dest);
  mport_free_vec(mirrors); 
  RETURN_ERRORX(MPORT_ERR_FATAL, "Unable to fetch %s: %s", filename, mport_err_string());
}



static int fetch(mportInstance *mport, const char *url, const char *dest) 
{
  FILE *remote;
  FILE *local;
  struct url_stat stat;
  char buffer[BUFFSIZE];
  char *ptr;
  size_t size;                                  
  size_t got;
  size_t wrote;
  
  if ((local = fopen(dest, "w")) == NULL) {
    RETURN_ERRORX(MPORT_ERR_FATAL, "Unable to open %s: %s", dest, strerror(errno));
  }

  mport_call_progress_init_cb(mport, "Downloading %s", url);
  
  if ((remote = fetchXGetURL(url, &stat, "p")) == NULL) {
    fclose(local);
    unlink(dest);
    RETURN_ERRORX(MPORT_ERR_FATAL, "Fetch error: %s: %s", url, fetchLastErrString);
  }
  
  while (1) {
    size = fread(buffer, 1, BUFFSIZE, remote);
    
    if (size < BUFFSIZE) {
      if (ferror(remote)) {
        fclose(local);
        fclose(remote);
        unlink(dest);
        RETURN_ERRORX(MPORT_ERR_FATAL, "Fetch error: %s: %s", url, fetchLastErrString);  
      } else if (feof(remote)) {
        /* do nothing */
      } 
    } 
  
    got += size;
  
    (mport->progress_step_cb)(got, stat.size, "XXX Rate");

    for (ptr = buffer; size > 0; ptr += wrote, size -= wrote) {
      wrote = fwrite(ptr, 1, size, local);
      if (wrote < size) {
        fclose(local); fclose(remote);
        unlink(dest);
        RETURN_ERRORX(MPORT_ERR_FATAL, "Write error %s: %s", dest, strerror(errno));
      }
    }

    if (feof(remote))
      break;
  }
  
  fclose(local);
  fclose(remote);
  (mport->progress_free_cb)();

  return MPORT_OK;
}

