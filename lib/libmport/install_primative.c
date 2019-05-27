/*-
 * Copyright (c) 2015 Lucas Holt
 * Copyright (c) 2007-2009 Chris Reinhardt
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

MPORT_PUBLIC_API int
mport_install_primative(mportInstance *mport, const char *filename, const char *prefix)
{
	mportBundleRead *bundle;
	mportPackageMeta **pkgs, *pkg;
	int i;
	bool error = false;

	if ((bundle = mport_bundle_read_new()) == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");

	if (mport_bundle_read_init(bundle, filename) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	if (mport_bundle_read_prep_for_install(mport, bundle) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	if (mport_pkgmeta_read_stub(mport, &pkgs) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	for (i = 0; *(pkgs + i) != NULL; i++) {
		pkg = pkgs[i];

		if (prefix != NULL) {
			/* override the default prefix with the given prefix */
			free(pkg->prefix);
			if ((pkg->prefix = strdup(prefix)) == NULL) /* all hope is lost! bail */
				RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");
		}

		if ((mport_check_preconditions(mport, pkg, MPORT_PRECHECK_INSTALLED | MPORT_PRECHECK_DEPENDS |
		                                           MPORT_PRECHECK_CONFLICTS) != MPORT_OK)
		    ||
		    (mport_bundle_read_install_pkg(mport, bundle, pkg) != MPORT_OK)) {
			mport_call_msg_cb(mport, "Unable to install %s-%s: %s", pkg->name, pkg->version,
			                  mport_err_string());
			/* TODO: WHY WAS THIS HERE mport_set_err(MPORT_OK, NULL); */
			error = true;
			break; /* do not keep going if we have a package failure! */
		}
	}

	if (mport_bundle_read_finish(mport, bundle) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	if (error)
		return MPORT_ERR_FATAL;

	return MPORT_OK;
}
