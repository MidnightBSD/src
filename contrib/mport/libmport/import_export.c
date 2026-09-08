/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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

#include "mport.h"
#include "mport_private.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stddef.h>
#include <fcntl.h>
#include <unistd.h>

MPORT_PUBLIC_API int
mport_import(mportInstance *mport, char *path)
{
	FILE *file = NULL;
	FILE *input;
	bool console = false;
	char name[1024];

	if (path == NULL)
		console = true;

	if (!console) {
		int fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
		if (fd == -1)
			RETURN_ERRORX(
			    MPORT_ERR_FATAL, "Couldn't open import file %s", path, strerror(errno));
		file = fdopen(fd, "r");
		if (file == NULL) {
			int error = errno;
			close(fd);
			RETURN_ERRORX(
			    MPORT_ERR_FATAL, "Couldn't open import file %s", path, strerror(error));
		}
	}

	input = console ? stdin : file;

	while (fgets(name, sizeof(name), input) != NULL) {
		name[strcspn(name, "\n")] = '\0';
		if (name[0] == '\0')
			continue;

		mport_call_msg_cb(mport, "Installing %s", name);
		mport_install(mport, name, NULL, NULL, MPORT_EXPLICIT);
	}

	if (ferror(input)) {
		if (!console)
			fclose(file);
		RETURN_ERRORX(MPORT_ERR_FATAL, "Couldn't read import source: %s", strerror(errno));
	}

	if (!console) {
		fclose(file);
	}

	return (MPORT_OK);
}

MPORT_PUBLIC_API int
mport_export(mportInstance *mport, char *path)
{
	mportPackageMeta **packs;
	mportPackageMeta **packs_orig;

	FILE *file = NULL;
	bool console = false;

	if (path == NULL)
		console = true;

	if (mport_pkgmeta_list(mport, &packs) != MPORT_OK) {
		RETURN_ERROR(MPORT_ERR_FATAL, "Unable to get package list");
	}

	if (packs == NULL) {
		RETURN_ERROR(MPORT_ERR_FATAL, "No packages installed");
	}

	if (!console) {
		int fd = open(path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
		if (fd == -1) {
			int error = errno;
			mport_pkgmeta_vec_free(packs);
			RETURN_ERRORX(
			    MPORT_ERR_FATAL, "Couldn't open export file %s", path, strerror(error));
		}
		file = fdopen(fd, "w");
		if (file == NULL) {
			int error = errno;
			close(fd);
			mport_pkgmeta_vec_free(packs);
			RETURN_ERRORX(
			    MPORT_ERR_FATAL, "Couldn't open export file %s", path, strerror(error));
		}
	}

	packs_orig = packs;
	while (*packs != NULL) {
		if (console)
			printf("%s\n", (*packs)->name);
		else
			fprintf(file, "%s\n", (*packs)->name);
		packs++;
	}

	if (!console) {
		fclose(file);
	}

	mport_pkgmeta_vec_free(packs_orig);

	return (MPORT_OK);
}
