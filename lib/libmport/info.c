/*-
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

#include <sys/cdefs.h>
__MBSDID("$MidnightBSD$");

#include "mport.h"
#include "mport_private.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <err.h>
#include <unistd.h>


MPORT_PUBLIC_API char *
mport_info(mportInstance *mport, const char *packageName)
{
	mportIndexEntry **indexEntry;
	mportPackageMeta **packs;
	char *status, *origin, *flavor, *deprecated;
	char *os_release;
	char *cpe;
	int locked = 0;
	int no_shlib_provided = 0;
	char *info_text = NULL;
	time_t expirationDate;

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
		flavor = strdup("");
		deprecated = strdup("N/A");
		expirationDate = 0;
	} else {
		status = (*packs)->version;
		origin = (*packs)->origin;
		os_release = (*packs)->os_release;
		cpe = (*packs)->cpe;
		locked = (*packs)->locked;
		no_shlib_provided = (*packs)->no_provide_shlib;
		flavor = (*packs)->flavor;
		deprecated  = (*packs)->deprecated;
		if (deprecated[0] == '\0') {
			deprecated = strdup("N/A");
		}
		expirationDate = (*packs)->expiration_date;
	}

	asprintf(&info_text,
		 "%s\nlatest: %s\ninstalled: %s\nlicense: %s\norigin: %s\nflavor: %s\nos: %s\n\n%s\ncpe: %s\nlocked: %s\nno_shlib_provided: %s\ndeprecated:%s\nexpirationDate:%s\n",
		 (*indexEntry)->pkgname,
		 (*indexEntry)->version,
		 status,
		 (*indexEntry)->license,
		 origin,
		 flavor,
		 os_release,
		 (*indexEntry)->comment,
		 cpe,
		 locked ? "yes" : "no"),
		no_shlib_provided ? "yes" : "no",
		deprecated,
		expirationDate == 0 ? "N/A" : ctime(&expirationDate);

	if (packs == NULL) {
		free(status);
		free(origin);
		free(os_release);
		free(cpe);
		free(flavor);
		free(deprecated);
	} else
		mport_pkgmeta_vec_free(packs);

	mport_index_entry_free_vec(indexEntry);
	return info_text;
}