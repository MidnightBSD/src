/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2015, 2016, 2018 Lucas Holt
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <err.h>
#include <unistd.h>
#include <libutil.h>

/*
 * ctime_r(3) requires a caller-supplied buffer of at least 26 bytes to hold
 * the formatted "Www Mmm dd hh:mm:ss yyyy\n\0" time string.
 */
#define CTIME_R_BUFLEN 26

MPORT_PUBLIC_API char *
mport_info(mportInstance *mport, const char *packageName)
{
	mportIndexEntry **indexEntries = NULL;
	mportIndexEntry *indexEntry = NULL;
	mportPackageMeta **packs = NULL;
	mportIndexMovedEntry **movedEntries = NULL;
	char *status, *origin, *flavor, *deprecated;
	char *os_release = NULL;
	char *cpe = NULL;
	int locked = 0;
	int no_shlib_provided = 0;
	char *info_text = NULL;
	time_t expirationDate, installDate;
	char *options = NULL;
	char *desc = NULL;
	mportAutomatic automatic;
	mportType type;
	char purl[256];
	int64_t flatsize;

	if (mport == NULL) {
		SET_ERROR(MPORT_ERR_FATAL, "mport not initialized");
		return (NULL);
	}

	if (packageName == NULL) {
		SET_ERROR(MPORT_ERR_FATAL, "Package name not found.");
		return (NULL);
	}

	if (mport_index_select_pkgname(mport, packageName, "Multiple packages match your query.",
		&indexEntries, &indexEntry) != MPORT_OK) {
		return (NULL);
	}

	if (mport_pkgmeta_search_master(mport, &packs, "pkg=%Q", packageName) != MPORT_OK) {
		mport_index_entry_free_vec(indexEntries);
		return (NULL);
	}

	/*
	 * Resolution only fails when the package is neither in the index nor
	 * installed locally. A package that is installed but no longer in the
	 * index still has local metadata to display.
	 */
	if (indexEntry == NULL && packs == NULL) {
		SET_ERROR(MPORT_ERR_FATAL, "Could not resolve package.");
		mport_index_entry_free_vec(indexEntries);
		indexEntries = NULL;
		return (NULL);
	}

	if (packs != NULL &&
	    mport_moved_lookup(mport, (*packs)->origin, &movedEntries) != MPORT_OK) {
		SET_ERROR(MPORT_ERR_FATAL, "The moved lookup failed.");
		return (NULL);
	}

	if (packs == NULL) {
		status = strdup("N/A");
		origin = strdup("");
		os_release = strdup("");
		cpe = strdup("");
		flavor = strdup("N/A");
		deprecated = strdup("N/A");
		expirationDate = 0;
		options = strdup("");
		desc = strdup("");

		if (!status || !origin || !os_release || !cpe || !flavor || !deprecated ||
		    !options || !desc) {
			free(status);
			free(origin);
			free(os_release);
			free(cpe);
			free(flavor);
			free(deprecated);
			free(options);
			free(desc);
			mport_index_entry_free_vec(indexEntries);
			free(movedEntries);
			SET_ERROR(MPORT_ERR_FATAL, "Out of memory");
			return (NULL);
		}

		automatic = MPORT_EXPLICIT;
		installDate = 0;
		type = 0;
		purl[0] = '\0';
		flatsize = 0;
	} else {
		status = (*packs)->version;
		origin = (*packs)->origin;
		os_release = (*packs)->os_release;
		cpe = (*packs)->cpe;
		locked = (*packs)->locked;
		no_shlib_provided = (*packs)->no_provide_shlib;
		flavor = (*packs)->flavor;
		if (flavor == NULL) {
			flavor = strdup("");
			if (flavor == NULL) {
				SET_ERROR(MPORT_ERR_FATAL, "Out of memory");
				return (NULL);
			}
		}
		deprecated = (*packs)->deprecated;
		if (deprecated == NULL || deprecated[0] == '\0') {
			if (movedEntries != NULL && *movedEntries != NULL &&
			    (*movedEntries)->date[0] != '\0') {
				deprecated = strdup("yes");
			} else {
				deprecated = strdup("no");
			}
			if (deprecated == NULL) {
				SET_ERROR(MPORT_ERR_FATAL, "Out of memory");
				return (NULL);
			}
		}

		expirationDate = (*packs)->expiration_date;
		if (expirationDate == 0 && movedEntries != NULL && *movedEntries != NULL &&
		    (*movedEntries)->date[0] != '\0') {
			struct tm expDate;
			strptime((*movedEntries)->date, "%Y-%m-%d", &expDate);
			expirationDate = mktime(&expDate);
		}
		options = (*packs)->options;

		if (options == NULL) {
			options = strdup("");
			if (options == NULL) {
				SET_ERROR(MPORT_ERR_FATAL, "Out of memory");
				return (NULL);
			}
		}

		desc = (*packs)->desc;
		if (desc == NULL) {
			desc = strdup("");
			if (desc == NULL) {
				SET_ERROR(MPORT_ERR_FATAL, "Out of memory");
				return (NULL);
			}
		}

		automatic = (*packs)->automatic;
		installDate = (*packs)->install_date;
		type = (*packs)->type;
		flatsize = (*packs)->flatsize;

		if (indexEntry == NULL)
			purl[0] = '\0';
		else if (packs != NULL && indexEntry->pkgname != NULL &&
		    (*packs)->version != NULL) {
			char *tmppurl = mport_purl_uri(*packs);
			if (tmppurl != NULL) {
				snprintf(purl, sizeof(purl), "%s", tmppurl);
				free(tmppurl);
			} else {
				purl[0] = '\0';
			}
		} else
			purl[0] = '\0';
	}

	char flatsize_str[8];
	humanize_number(flatsize_str, sizeof(flatsize_str), flatsize, "B", HN_AUTOSCALE,
	    HN_DECIMAL | HN_IEC_PREFIXES);

	char *annotations_str = NULL;
	if (packs != NULL) {
		char **tags = NULL;
		int tag_count = 0;
		if (mport_annotation_list(mport, (*packs)->name, &tags, &tag_count) == MPORT_OK &&
		    tag_count > 0) {
			size_t len = 0;
			FILE *fp = open_memstream(&annotations_str, &len);
			if (fp != NULL) {
				for (int i = 0; i < tag_count; i++) {
					char *val = NULL;
					if (mport_annotation_get(
						mport, (*packs)->name, tags[i], &val) == MPORT_OK &&
					    val != NULL) {
						if (i == 0)
							fprintf(fp,
							    "Annotations     :\n                       %s:        %s\n",
							    tags[i], val);
						else
							fprintf(fp,
							    "                       %s:        %s\n",
							    tags[i], val);
						free(val);
					}
					free(tags[i]);
				}
				free(tags);
				fclose(fp);
			}
		}
	}

	if (annotations_str == NULL)
		annotations_str = strdup("");

	/*
	 * ctime() returns a pointer to a single static buffer, so calling it
	 * twice within one asprintf() argument list makes both date fields
	 * print the same (last evaluated) value. Format each date into its
	 * own buffer with ctime_r() up front instead.
	 */
	char expdate_buf[CTIME_R_BUFLEN], insdate_buf[CTIME_R_BUFLEN];
	const char *expdate_str = expirationDate == 0 ? "" : ctime_r(&expirationDate, expdate_buf);
	const char *insdate_str = installDate == 0 ? "\n" : ctime_r(&installDate, insdate_buf);
	if (expdate_str == NULL)
		expdate_str = "";
	if (insdate_str == NULL)
		insdate_str = "\n";

	if (packs != NULL && indexEntry == NULL) {
		asprintf(&info_text,
		    "%s-%s\n"
		    "Name            : %s\nVersion         : %s\nLatest          : %s\nLicenses        : %s\nOrigin          : %s\n"
		    "Flavor          : %s\nOS              : %s\n"
		    "CPE             : %s\nPURL            : %s\nLocked          : %s\nPrime           : %s\nShared library  : %s\nDeprecated      : %s\nExpiration Date : %s\nInstall Date    : %s"
		    "Comment         : %s\n%sOptions         : %s\nType            : %s\nFlat Size       : %s\nDescription     :\n%s\n",
		    (*packs)->name, (*packs)->version, (*packs)->name, status, "", "", origin,
		    flavor, os_release, cpe, purl, locked ? "yes" : "no",
		    automatic == MPORT_EXPLICIT ? "yes" : "no", no_shlib_provided ? "yes" : "no",
		    deprecated, expdate_str, insdate_str, "", annotations_str, options,
		    type == MPORT_TYPE_APP ? "Application" : "System", flatsize_str, desc);
	} else if (packs != NULL) {
		asprintf(&info_text,
		    "%s-%s\n"
		    "Name            : %s\nVersion         : %s\nLatest          : %s\nLicenses        : %s\nOrigin          : %s\n"
		    "Flavor          : %s\nOS              : %s\n"
		    "CPE             : %s\nPURL            : %s\nLocked          : %s\nPrime           : %s\nShared library  : %s\nDeprecated      : %s\nExpiration Date : %s\nInstall Date    : %s"
		    "Comment         : %s\n%sOptions         : %s\nType            : %s\nFlat Size       : %s\nDescription     :\n%s\n",
		    (*packs)->name, (*packs)->version, (*packs)->name, status,
		    indexEntry == NULL ? "" : indexEntry->version,
		    indexEntry == NULL ? "" : indexEntry->license, origin, flavor, os_release, cpe,
		    purl, locked ? "yes" : "no", automatic == MPORT_EXPLICIT ? "yes" : "no",
		    no_shlib_provided ? "yes" : "no", deprecated, expdate_str, insdate_str,
		    indexEntry == NULL ? "" : indexEntry->comment, annotations_str, options,
		    type == MPORT_TYPE_APP ? "Application" : "System", flatsize_str, desc);
	} else {
		/* Not installed locally; report index information only. */
		asprintf(&info_text,
		    "%s-%s\n"
		    "Name            : %s\nVersion         : %s\nLatest          : %s\nLicenses        : %s\nOrigin          : %s\n"
		    "Flavor          : %s\nOS              : %s\n"
		    "CPE             : %s\nPURL            : %s\nLocked          : %s\nPrime           : %s\nShared library  : %s\nDeprecated      : %s\nExpiration Date : %s\nInstall Date    : %s"
		    "Comment         : %s\n%sOptions         : %s\nType            : %s\nFlat Size       : %s\nDescription     :\n%s\n",
		    indexEntry->pkgname, indexEntry->version, indexEntry->pkgname, status,
		    indexEntry->version, indexEntry->license == NULL ? "" : indexEntry->license,
		    origin, flavor, os_release, cpe, purl, locked ? "yes" : "no",
		    automatic == MPORT_EXPLICIT ? "yes" : "no", no_shlib_provided ? "yes" : "no",
		    deprecated, "", /* expiration date: not installed, always empty */
		    "\n", /* install date: not installed, always empty */
		    indexEntry->comment == NULL ? "" : indexEntry->comment, annotations_str,
		    options, type == MPORT_TYPE_APP ? "Application" : "System", flatsize_str, desc);
	}

	if (info_text == NULL) {
		SET_ERROR(MPORT_ERR_FATAL, "Out of memory.");
		return (NULL);
	}

	if (packs == NULL) {
		free(status);
		free(origin);
		free(os_release);
		free(cpe);
		free(flavor);
		free(deprecated);
		free(options);
		free(desc);
	} else {
		mport_pkgmeta_vec_free(packs);
		packs = NULL;
	}

	mport_index_entry_free_vec(indexEntries);
	indexEntries = NULL;
	indexEntry = NULL;

	free(movedEntries);
	movedEntries = NULL;

	free(annotations_str);

	return info_text;
}
