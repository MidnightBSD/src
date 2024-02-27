/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2010, 2011, 2013, 2014, 2024 Lucas Holt
 * Copyright (c) 2008 Chris Reinhardt
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
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <err.h>
#include <unistd.h>

MPORT_PUBLIC_API int
mport_list_print(mportInstance *mport, mportListPrint *print) 
{

	mportPackageMeta **packs;
	mportIndexEntry **indexEntries;
	mportIndexEntry **iestart;
	mportIndexMovedEntry **movedEntries;
	char *comment;
	char *os_release;
	char name_version[30];

	if (mport_pkgmeta_list(mport, &packs) != MPORT_OK) {
		mport_pkgmeta_vec_free(packs);
		RETURN_CURRENT_ERROR;
	}

	if (packs == NULL) {
		RETURN_ERROR(MPORT_ERR_WARN, "No packages installed matching.");
	}

	os_release = mport_get_osrelease(mport);

	while (*packs != NULL) {
		if (print->update) {
			if (mport_index_lookup_pkgname(mport, (*packs)->name, &indexEntries) != MPORT_OK) {
				RETURN_ERRORX(MPORT_ERR_FATAL, "Error looking up package name %s: %d %s", (*packs)->name, mport_err_code(), mport_err_string());
			}

			if (indexEntries == NULL || *indexEntries == NULL) {
				if (mport_moved_lookup(mport, (*packs)->name, &movedEntries) != MPORT_OK) {
					mport_call_msg_cb(mport,"%-25s %8s is not part of the package repository.", (*packs)->name, (*packs)->version);
					packs++;
					continue;
				}

				if (movedEntries == NULL || *movedEntries == NULL) {
					mport_call_msg_cb(mport,"%-15s %8s is not part of the package repository.", (*packs)->name, (*packs)->version);
					packs++;
					continue;
				}

				if ((*movedEntries)->moved_to[0]!= '\0') {
					mport_call_msg_cb(mport,"%-25s %8s was moved to %s", (*packs)->name, (*packs)->version, (*movedEntries)->moved_to);
					free(movedEntries);
					movedEntries = NULL;
					packs++;
					continue;
				}

				if ((*movedEntries)->date[0]!= '\0') {
					mport_call_msg_cb(mport,"%-25s %8s expired on %s", (*packs)->name, (*packs)->version, (*movedEntries)->date);
					free(movedEntries);
					movedEntries = NULL;
					packs++;
					continue;
				}

				free(movedEntries);
				movedEntries = NULL;
			}
	
			iestart = indexEntries;		
			while (indexEntries != NULL && *indexEntries != NULL) {
				if (((*indexEntries)->version != NULL && mport_version_cmp((*packs)->version, (*indexEntries)->version) < 0) 
					|| ((*packs)->version != NULL && mport_version_cmp((*packs)->os_release, os_release) < 0)) {

					if (mport->verbosity == MPORT_VVERBOSE) {
						mport_call_msg_cb(mport,"%-25s %8s (%s)  <  %-s", (*packs)->name, (*packs)->version, (*packs)->os_release, (*indexEntries)->version);
					} else {
						mport_call_msg_cb(mport,"%-25s %8s  <  %-8s", (*packs)->name, (*packs)->version, (*indexEntries)->version);
					}
				}
				indexEntries++;
			}
				
			mport_index_entry_free_vec(iestart);
			indexEntries = NULL;
		} else if (mport->verbosity == MPORT_VVERBOSE) {
			comment = mport_str_remove((*packs)->comment, '\\');
			snprintf(name_version, 30, "%s-%s", (*packs)->name, (*packs)->version);
			
			mport_call_msg_cb(mport,"%-30s\t%6s\t%s", name_version, (*packs)->os_release, comment);
			free(comment);
		} else if (print->prime && (*packs)->automatic == 0) {
			mport_call_msg_cb(mport,"%s", (*packs)->name);
		} else if (mport->verbosity == MPORT_VQUIET && !print->origin) {
			mport_call_msg_cb(mport,"%s", (*packs)->name);
		} else if (mport->verbosity == MPORT_VQUIET && print->origin) {
			mport_call_msg_cb(mport,"%s", (*packs)->origin);
		} else if (print->origin) {
			mport_call_msg_cb(mport,"Information for %s-%s:\n\nOrigin:\n%s\n",
						  (*packs)->name, (*packs)->version, (*packs)->origin);
		} else if (print->locks) {
			if ((*packs)->locked == 1)
				mport_call_msg_cb(mport,"%s-%s", (*packs)->name, (*packs)->version);

		} else {
			mport_call_msg_cb(mport, "%s-%s", (*packs)->name, (*packs)->version);
		}
		packs++;
	}

	(mport->progress_free_cb)();

	return (MPORT_OK);
}
