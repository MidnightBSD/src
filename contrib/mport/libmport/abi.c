/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Lucas Holt
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

/*
 * The package ABI, in the two spellings FreeBSD pkg prints for
 * "pkg config ABI" and "pkg config ALTABI":
 *
 *   ABI     MidnightBSD:4.0:amd64
 *   ALTABI  midnightbsd:4.0:x86:64
 *
 * Both are derived, never stored: the release comes from
 * mport_get_osrelease() (ABI_FILE, the target_os setting, then the running
 * system) and the architecture from ABI_FILE's machine type, else the build
 * host's.
 */

#include <ctype.h>
#include <elf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mport.h"
#include "mport_private.h"

#define MPORT_ABI_OS "MidnightBSD"

/*
 * pkg's legacy "ELF type" spellings for the architectures mport names.  An
 * architecture without a row is copied through unchanged, as pkg does.
 */
static const struct {
	const char *arch;
	const char *legacy;
} altabi_arch[] = {
	{ "amd64", "x86:64" },
	{ "i386", "x86:32" },
	{ "aarch64", "aarch64:64" },
};

/*
 * The package architecture name for an ELF e_machine value, or NULL for a
 * machine type no MidnightBSD port is built for.  Static storage.
 */
/*@null@*/ /*@observer@*/ const char *
mport_arch_from_elf_machine(unsigned int machine)
{
	switch (machine) {
	case EM_386:
		return "i386";
	case EM_X86_64:
		return "amd64";
	case EM_AARCH64:
		return "aarch64";
	default:
		return NULL;
	}
}

/*
 * The target package architecture: the machine type of ABI_FILE when set,
 * otherwise the build host's.  An ABI_FILE that cannot be read or is built
 * for a machine without a port is an error, as a guess would report the wrong
 * target.  The caller frees the result.
 */
MPORT_PUBLIC_API /*@null@*/ /*@only@*/ char *
mport_get_arch(void)
{
	const char *abi_file = getenv("ABI_FILE");
	/*@observer@*/ const char *arch = NULL;

	if (abi_file != NULL && abi_file[0] != '\0') {
		if (mport_abi_file_read(abi_file, NULL, NULL, NULL, &arch) != MPORT_OK)
			return NULL;
		if (arch == NULL) {
			SET_ERRORX(
			    MPORT_ERR_FATAL, "ABI_FILE %s: unsupported machine type", abi_file);
			return NULL;
		}
	} else {
		arch = MPORT_ARCH;
	}

	return strdup(arch);
}

/*
 * "MidnightBSD:<release>:<arch>", e.g. MidnightBSD:4.0:amd64.  The release
 * keeps its minor version because each minor release has its own package
 * repository.  The caller frees the result; NULL when either part is unknown.
 */
MPORT_PUBLIC_API /*@null@*/ /*@only@*/ char *
mport_get_abi(/*@null@*/ mportInstance *mport)
{
	char *osrel;
	char *arch;
	char *abi = NULL;

	/* arch first: a bad ABI_FILE is reported by name, not as a missing release */
	arch = mport_get_arch();
	if (arch == NULL)
		return NULL;

	osrel = mport_get_osrelease(mport);
	if (osrel == NULL) {
		free(arch);
		SET_ERROR(MPORT_ERR_FATAL, "Unable to determine the target release");
		return NULL;
	}

	if (asprintf(&abi, "%s:%s:%s", MPORT_ABI_OS, osrel, arch) == -1) {
		SET_ERROR(MPORT_ERR_FATAL, "Out of memory");
		abi = NULL;
	}

	free(osrel);
	free(arch);

	return abi;
}

/*
 * The ABI in pkg's legacy spelling: the OS lower-cased and the architecture
 * replaced by its ELF type, e.g. midnightbsd:4.0:x86:64.  The caller frees
 * the result.
 */
MPORT_PUBLIC_API /*@null@*/ /*@only@*/ char *
mport_get_altabi(/*@null@*/ mportInstance *mport)
{
	char *osrel;
	char *arch;
	char *altabi = NULL;
	/*@observer@*/ const char *legacy = NULL;
	char os[sizeof(MPORT_ABI_OS)];
	size_t i;

	/* arch first: a bad ABI_FILE is reported by name, not as a missing release */
	arch = mport_get_arch();
	if (arch == NULL)
		return NULL;

	osrel = mport_get_osrelease(mport);
	if (osrel == NULL) {
		free(arch);
		SET_ERROR(MPORT_ERR_FATAL, "Unable to determine the target release");
		return NULL;
	}

	for (i = 0; i < sizeof(os); i++)
		os[i] = (char)tolower((unsigned char)MPORT_ABI_OS[i]);

	for (i = 0; i < sizeof(altabi_arch) / sizeof(altabi_arch[0]); i++) {
		if (strcmp(arch, altabi_arch[i].arch) == 0) {
			legacy = altabi_arch[i].legacy;
			break;
		}
	}
	if (legacy == NULL)
		legacy = arch;

	if (asprintf(&altabi, "%s:%s:%s", os, osrel, legacy) == -1) {
		SET_ERROR(MPORT_ERR_FATAL, "Out of memory");
		altabi = NULL;
	}

	free(osrel);
	free(arch);

	return altabi;
}
