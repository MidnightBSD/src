/*-
 * Copyright (c) 2021 Lucas Holt
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

#include "mport.h"
#include "mport_private.h"
#include <string.h>
#include <stdlib.h>

MPORT_PUBLIC_API int
mport_update(mportInstance *mport, const char *packageName) {
	char *path;
	mportDependsEntry **depends;
	mportIndexEntry **indexEntry;

	if (packageName == NULL) {
		return (1);
	}

	int result = mport_download(mport, packageName, &path);
	if (result != 0)
		return result;

	/* in the event the package is not found in the index, it could be user generated and we still want to update it if
	   present */
	if (mport_index_lookup_pkgname(mport, packageName, &indexEntry) != MPORT_OK ||
			indexEntry == NULL || *indexEntry == NULL) {
		mport_call_msg_cb(mport, "Package %s not found in the index", packageName);
	} else {
		/* get the dependency list and start updating/installing missing entries */
		mport_index_depends_list(mport, packageName, (*indexEntry)->version, &depends);

		while (*depends != NULL) {
			mport_install_depends(mport, (*depends)->d_pkgname, (*depends)->d_version);
			depends++;
		}
	}

	if (mport_update_primative(mport, path) != MPORT_OK) {
		mport_call_msg_cb(mport, "%s\n", mport_err_string());
		free(path);
		return mport_err_code();
	}

	free(path);

	return (0);
}