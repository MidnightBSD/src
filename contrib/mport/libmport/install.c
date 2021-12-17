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

#include "mport.h"
#include "mport_private.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

MPORT_PUBLIC_API int
mport_install(mportInstance *mport, const char *pkgname, const char *version, const char *prefix, mportAutomatic automatic)
{
  mportIndexEntry **e;
  char *filename;
  int ret = MPORT_OK;
  int e_loc = 0;

  MPORT_CHECK_FOR_INDEX(mport, "mport_install()");
  
  if (mport_index_lookup_pkgname(mport, pkgname, &e) != MPORT_OK) {
  	RETURN_CURRENT_ERROR;
  }

  /* we don't support installing more than one top-level package at a time.
   * Consider a situation like this:
   *
   * mport_install(mport, "p5-Class-DBI*");
   *
   * Say this matches p5-Class-DBI and p5-Class-DBI-AbstractSearch
   * and say the order from the index puts p5-Class-DBI-AbstractSearch 
   * first.
   * 
   * p5-Class-DBI-AbstractSearch is installed, and its dependencies installed.
   * However, p5-Class-DBI is a dependency of p5-Class-DBI-AbstractSearch, so
   * when it comes time to install p5-Class-DBI, we can't - because it is
   * already installed.
   *
   * If a user facing application wants this functionality, it would be
   * easy to piece together with mport_index_lookup_pkgname(), a
   * check for already installed packages, and mport_install().
   */

  if (e[1] != NULL) {
    if (version != NULL) {
        while (e[e_loc] != NULL) {
          if (strcmp(e[e_loc]->version, version) == 0) {
            break;
          }
          e_loc++;
        }
        if (e[e_loc] == NULL) {
          mport_index_entry_free_vec(e);
          RETURN_ERRORX(MPORT_ERR_FATAL, "Could not resolve '%s-%s'.", pkgname, version);
        }
    } else {
      mport_index_entry_free_vec(e);
      RETURN_ERRORX(MPORT_ERR_FATAL, "Could not resolve '%s' to a single package.", pkgname);
    }
  }
 
  asprintf(&filename, "%s/%s", MPORT_FETCH_STAGING_DIR, e[e_loc]->bundlefile);
  if (filename == NULL) {
    mport_index_entry_free_vec(e);
    RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");
  }

  if (!mport_file_exists(filename)) {
    if (mport_fetch_bundle(mport, e[e_loc]->bundlefile) != MPORT_OK) {
      free(filename);
      mport_index_entry_free_vec(e);
      RETURN_CURRENT_ERROR;
    }
  }

  if (mport_verify_hash(filename, e[e_loc]->hash) == 0) {
  	mport_index_entry_free_vec(e);

  	if (unlink(filename) == 0) {
	    free(filename);
  		RETURN_ERROR(MPORT_ERR_FATAL, "Package failed hash verification and was removed.\n");
  	} else {
	    free(filename);
  		RETURN_ERROR(MPORT_ERR_FATAL, "Package failed hash verification, but could not be removed.\n");
  	}
  }
 
  ret = mport_install_primative(mport, filename, prefix, automatic);

  free(filename);
  mport_index_entry_free_vec(e);
  
  return ret;
}

/* recursive function */
int
mport_install_depends(mportInstance *mport, const char *packageName, const char *version, mportAutomatic automatic) {
	mportPackageMeta **packs;
	mportDependsEntry **depends;
	mportDependsEntry **depends_orig;

	if (packageName == NULL || version == NULL) {
		RETURN_ERROR(MPORT_ERR_WARN, "Dependency name or version is null");
	}

	mport_index_depends_list(mport, packageName, version, &depends_orig);
	depends = depends_orig;
 
	if (mport_pkgmeta_search_master(mport, &packs, "pkg=%Q", packageName) != MPORT_OK) {
		mport_call_msg_cb(mport, "%s", mport_err_string());
		return mport_err_code();
	}

	if (packs == NULL && depends == NULL) {
		/* Package is not installed and there are no dependencies */
		if (mport_install(mport, packageName, version, NULL, automatic) != MPORT_OK) {
			mport_call_msg_cb(mport, "%s", mport_err_string());
			return mport_err_code();
		}
	} else if (packs == NULL) {
		/* Package is not installed */
		while (*depends != NULL) {
			if (mport_install_depends(mport, (*depends)->d_pkgname, (*depends)->d_version, MPORT_AUTOMATIC) != MPORT_OK) {
     			mport_call_msg_cb(mport, "%s", mport_err_string());
     			mport_index_depends_free_vec(depends_orig);
				return mport_err_code();
			}
			depends++;
		}
		if (mport_install(mport, packageName, version, NULL, automatic) != MPORT_OK) {
			mport_call_msg_cb(mport, "%s", mport_err_string());
			mport_index_depends_free_vec(depends_orig);
			return mport_err_code();
		}
		mport_index_depends_free_vec(depends_orig);
	} else {
		/* already installed, double check we are on the latest */
		mport_index_depends_free_vec(depends_orig);

		if (mport_check_preconditions(mport, packs[0], MPORT_PRECHECK_UPGRADEABLE) == MPORT_OK) {
			if (mport_update(mport, packageName) != MPORT_OK) {
				mport_call_msg_cb(mport, "%s", mport_err_string());
				mport_pkgmeta_vec_free(packs);
				return mport_err_code();
			}
		}

        // TODO: fix crash on nested calls
		//mport_pkgmeta_vec_free(packs);
	}

	return (MPORT_OK);
}