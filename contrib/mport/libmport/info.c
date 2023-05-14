/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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

MPORT_PUBLIC_API char *
mport_info(mportInstance *mport, const char *packageName) {
	mportIndexEntry **indexEntry;
	mportPackageMeta **packs;
	char *status, *origin, *flavor, *deprecated;
	char *os_release;
	char *cpe;
	int locked = 0;
	int no_shlib_provided = 0;
	char *info_text = NULL;
	time_t expirationDate, installDate;
	char *options;
	char *desc;
	mportAutomatic automatic;
	mportType type;
	char purl[256];

	if (mport == NULL) {
		SET_ERROR(MPORT_ERR_FATAL, "mport not initialized");
		return (NULL);
	}

	if (packageName == NULL) {
		SET_ERROR(MPORT_ERR_FATAL, "Package name not found.");
		return (NULL);
	}

	if (mport_index_lookup_pkgname(mport, packageName, &indexEntry) != MPORT_OK) {
		return (NULL);
	}

	if (indexEntry == NULL || *indexEntry == NULL) {
		SET_ERROR(MPORT_ERR_FATAL, "Could not resolve package.");
		return (NULL);
	}

	if (mport_pkgmeta_search_master(mport, &packs, "pkg=%Q", packageName) != MPORT_OK) {
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
		automatic = MPORT_EXPLICIT;
		installDate = 0;
		type = 0;
		purl[0] = '\0';
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
		}
		deprecated = (*packs)->deprecated;
		if (deprecated == NULL || deprecated[0] == '\0') {
			deprecated = strdup("no");
		}
		expirationDate = (*packs)->expiration_date;
		options = (*packs)->options;
		if (options == NULL) {
			options = strdup("");
		}
		desc = (*packs)->desc;
		if (desc == NULL) {
			desc = strdup("");
		}
		automatic = (*packs)->automatic;
		installDate = (*packs)->install_date;
		type = (*packs)->type;
		snprintf(purl, sizeof(purl), "pkg:mport/midnightbsd/%s@%s?arch=%s&osrel=%s", (*indexEntry)->pkgname, (*packs)->version, MPORT_ARCH, os_release);
	}

	asprintf(&info_text,
	         "%s-%s\n"
	         "Name            : %s\nVersion         : %s\nLatest          : %s\nLicenses        : %s\nOrigin          : %s\n"
	         "Flavor          : %s\nOS              : %s\n"
	         "CPE             : %s\nPURL            : %s\nLocked          : %s\nPrime           : %s\nShared library  : %s\nDeprecated      : %s\nExpiration Date : %s\nInstall Date    : %s"
	         "Comment         : %s\nOptions         : %s\nType            : %s\nDescription     :\n%s\n",
	         (*indexEntry)->pkgname, (*indexEntry)->version,
	         (*indexEntry)->pkgname,
	         status,
	         (*indexEntry)->version,
	         (*indexEntry)->license,
	         origin,
	         flavor,
	         os_release,
	         cpe,
			 purl,
	         locked ? "yes" : "no",
	         automatic == MPORT_EXPLICIT ? "yes" : "no",
	         no_shlib_provided ? "yes" : "no",
	         deprecated,
	         expirationDate == 0 ? "" : ctime(&expirationDate),
	         installDate == 0 ? "\n" : ctime(&installDate),
	         (*indexEntry)->comment,
	         options,
			 type == MPORT_TYPE_APP ? "Application" : "System", 
	         desc);

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

	mport_index_entry_free_vec(indexEntry);
	indexEntry = NULL;

	return info_text;
}
