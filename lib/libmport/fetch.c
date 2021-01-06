/*-
 * Copyright (c) 2011 Lucas Holt
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
#include <stdlib.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <fetch.h>
#include <string.h>
#include <errno.h>

#define BUFFSIZE 1024 * 8

static int fetch(mportInstance *, const char *, const char *);


/* mport_fetch_index(mport)
 *
 * Fetch the index from a remote, or the bootstrap if we don't currently
 * have an index. If the current index is recentish, then don't do
 * anything.
 */
int
mport_fetch_index(mportInstance *mport)
{
	char **mirrors = NULL;
	char **mirrorsPtr = NULL;
	char *url = NULL;
	int mirrorCount = 0;
	
	MPORT_CHECK_FOR_INDEX(mport, "mport_fetch_index()");
 
	if (mport_index_get_mirror_list(mport, &mirrors, &mirrorCount) != MPORT_OK)
		RETURN_CURRENT_ERROR;

#ifdef DEBUGGING 
	fprintf(stderr, "Mirror count is %d\n", mirrorCount);
#endif
 
	mirrorsPtr = mirrors;
	 
	while (mirrorsPtr != NULL) {
		if (*mirrorsPtr == NULL)
				break;
		asprintf(&url, "%s/%s", *mirrorsPtr, MPORT_INDEX_URL_PATH);

		if (url == NULL) {
			for (int mi = 0; mi < mirrorCount; mi++)
				free(mirrors[mi]);
			RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");
		}

		if (fetch(mport, url, MPORT_INDEX_FILE_BZ2) == MPORT_OK) {
			mport_decompress_bzip2(MPORT_INDEX_FILE_BZ2, MPORT_INDEX_FILE);
			free(url);
			for (int mi = 0; mi < mirrorCount; mi++)
				free(mirrors[mi]);
			return MPORT_OK;
		}
		free(url);
		mirrorsPtr++;
	}

	/* fallback to mport bootstrap site in a pinch */
	if (mport_fetch_bootstrap_index(mport) == MPORT_OK)
		return MPORT_OK;
	
	for (int mi = 0; mi < mirrorCount; mi++) 
		free(mirrors[mi]);

	free(mirrors);
	RETURN_ERRORX(MPORT_ERR_FATAL, "Unable to fetch index file: %s", mport_err_string());
}



/* mport_fetch_bootstrap_index(mportInstance *mport)
 *
 * Fetches the index for the bootstrap site. The index need 
 * not be loaded for this to be used.
 */
int
mport_fetch_bootstrap_index(mportInstance *mport)
{
	int result;
	result = fetch(mport, MPORT_BOOTSTRAP_INDEX_URL, MPORT_INDEX_FILE_BZ2);
	mport_decompress_bzip2(MPORT_INDEX_FILE_BZ2, MPORT_INDEX_FILE);
	return result;
}

/* mport_fetch_bundle(mport, filename)
 *
 * Fetch a given bundle from a remote.	If there is no loaded index, then
 * an error is thrown.	The file will be downloaded to the 
 * MPORT_FETCH_STAGING_DIR directory.
 */
int
mport_fetch_bundle(mportInstance *mport, const char *filename)
{
	char **mirrors;
	char **mirrorsPtr;
	char *url;
	char *dest;
	int mirrorCount = 0;
	struct stat sb;

	MPORT_CHECK_FOR_INDEX(mport, "mport_fetch_bundle()");
	
	if (mport_index_get_mirror_list(mport, &mirrors, &mirrorCount) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	if (stat(MPORT_FETCH_STAGING_DIR, &sb) != 0 || ! S_ISDIR(sb.st_mode)) {
		if (mkdir(MPORT_FETCH_STAGING_DIR, S_IRWXU | S_IRWXG)) {
			RETURN_CURRENT_ERROR;
		}
	}
		
	asprintf(&dest, "%s/%s", MPORT_FETCH_STAGING_DIR, filename);
 
	mirrorsPtr = mirrors;
 
	while (mirrorsPtr != NULL) {
		if (*mirrorsPtr == NULL)
			break;
		asprintf(&url, "%s/%s/%s", *mirrorsPtr, MPORT_URL_PATH, filename);

		if (fetch(mport, url, dest) == MPORT_OK) {
			free(url);
			url = NULL;
			free(dest);
			dest = NULL;
			for (int mi = 0; mi < mirrorCount; mi++)
				free(mirrors[mi]);
			return MPORT_OK;
		} 
		
		free(url);
		url = NULL;
		mirrorsPtr++;
	}

	free(dest);
	for (int mi = 0; mi < mirrorCount; mi++)
		free(mirrors[mi]);

	RETURN_CURRENT_ERROR; 
}


static int
fetch(mportInstance *mport, const char *url, const char *dest) 
{
	FILE *remote = NULL;
	FILE *local = NULL;
	struct url_stat ustat;
	char buffer[BUFFSIZE];
	char *ptr = NULL;
	size_t size;																	
	size_t got = 0;
	size_t wrote;
	
	if ((local = fopen(dest, "w")) == NULL) {
		RETURN_ERRORX(MPORT_ERR_FATAL, "Unable to open %s: %s", dest, strerror(errno));
	}

	mport_call_progress_init_cb(mport, "Downloading %s", url);
	
	if ((remote = fetchXGetURL(url, &ustat, "p")) == NULL) {
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
	
		(mport->progress_step_cb)(got, ustat.size, "XXX Rate");

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

/**
 * Download a package. Top level, public method.
 */
int
mport_download(mportInstance *mport, const char *packageName) {
	mportIndexEntry **indexEntry;
	char *path;
	bool existed = true;

	if (mport_index_lookup_pkgname(mport, packageName, &indexEntry) != MPORT_OK) {
		RETURN_CURRENT_ERROR;
	}
	
	if (indexEntry == NULL || *indexEntry == NULL)
		SET_ERRORX(1, "Package %s not found in index.\n", packageName);
	
	asprintf(&path, "%s/%s", MPORT_LOCAL_PKG_PATH, (*indexEntry)->bundlefile);
	if (path == NULL)
		SET_ERRORX(1, "%s", "Unable to allocate memory for path.");

	if (!mport_file_exists(path)) {
		if (mport_fetch_bundle(mport, (*indexEntry)->bundlefile) != MPORT_OK) {
			fprintf(stderr, "%s\n", mport_err_string());
			free(path);
			return mport_err_code();
			
		}
		existed = false;
	}

	if (!mport_verify_hash(path, (*indexEntry)->hash)) {
		free(path);
		SET_ERRORX(1, "Package %s fails hash verification.", packageName);
	}

	if (!existed)
		mport_call_msg_cb(mport, "Package %s saved as %s\n", packageName, path);
	else
		mport_call_msg_cb(mport, "Package %s exists at %s\n", packageName, path);

	free(path);
	mport_index_entry_free_vec(indexEntry);

	return (0);
}

