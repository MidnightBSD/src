/*-
 * Copyright (c) 2011 Mikolaj Golub
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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/elf.h>
#include <sys/sysctl.h>
#include <sys/user.h>

#include <vm/vm.h>

#include <err.h>
#include <errno.h>
#include <libprocstat.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "procstat.h"

#define PROC_AUXV_MAX	256

static Elf_Auxinfo auxv[PROC_AUXV_MAX];
static char prefix[256];

#if __ELF_WORD_SIZE == 64
static Elf32_Auxinfo auxv32[PROC_AUXV_MAX];

static const char *elf32_sv_names[] = {
	"Linux ELF32",
	"FreeBSD ELF32",
};

static int
is_elf32(pid_t pid)
{
	int error, name[4];
	size_t len, i;
	static char sv_name[256];

	name[0] = CTL_KERN;
	name[1] = KERN_PROC;
	name[2] = KERN_PROC_SV_NAME;
	name[3] = pid;
	len = sizeof(sv_name);
	error = sysctl(name, 4, sv_name, &len, NULL, 0);
	if (error != 0 || len == 0)
		return (0);
	for (i = 0; i < sizeof(elf32_sv_names) / sizeof(*elf32_sv_names); i++) {
		if (strncmp(sv_name, elf32_sv_names[i], sizeof(sv_name)) == 0)
			return (1);
	}
	return (0);
}

static size_t
retrieve_auxv32(pid_t pid)
{
	int name[4];
	size_t len, i;
	void *ptr;

	name[0] = CTL_KERN;
	name[1] = KERN_PROC;
	name[2] = KERN_PROC_AUXV;
	name[3] = pid;
	len = sizeof(auxv32);
	if (sysctl(name, 4, auxv32, &len, NULL, 0) == -1) {
		if (errno != ESRCH && errno != EPERM)
			warn("sysctl: kern.proc.auxv: %d: %d", pid, errno);
		return (0);
	}
	for (i = 0; i < len; i++) {
		/*
		 * XXX: We expect that values for a_type on a 32-bit platform
		 * are directly mapped to those on 64-bit one, which is not
		 * necessarily true.
		 */
		auxv[i].a_type = auxv32[i].a_type;
		ptr = &auxv32[i].a_un;
		auxv[i].a_un.a_val = *((uint32_t *)ptr);
	}
	return (len);
}
#endif /* __ELF_WORD_SIZE == 64 */

#define	PRINT(name, spec, val)		\
	printf("%s %-16s " #spec "\n", prefix, #name, (val))
#define	PRINT_UNKNOWN(type, val)	\
	printf("%s %16ld %#lx\n", prefix, (long)type, (u_long)(val))

static size_t
retrieve_auxv(pid_t pid)
{
	int name[4];
	size_t len;

#if __ELF_WORD_SIZE == 64
	if (is_elf32(pid))
		return (retrieve_auxv32(pid));
#endif
	name[0] = CTL_KERN;
	name[1] = KERN_PROC;
	name[2] = KERN_PROC_AUXV;
	name[3] = pid;
	len = sizeof(auxv);
	if (sysctl(name, 4, auxv, &len, NULL, 0) == -1) {
		if (errno != ESRCH && errno != EPERM)
			warn("sysctl: kern.proc.auxv: %d: %d", pid, errno);
		return (0);
	}
	return (len);
}

void
procstat_auxv(struct kinfo_proc *kipp)
{
	size_t len, i;

	if (!hflag)
		printf("%5s %-16s %-16s %-16s\n", "PID", "COMM", "AUXV", "VALUE");
	len = retrieve_auxv(kipp->ki_pid);
	if (len == 0)
		return;
	snprintf(prefix, sizeof(prefix), "%5d %-16s", kipp->ki_pid,
	    kipp->ki_comm);
	for (i = 0; i < len; i++) {
		switch(auxv[i].a_type) {
		case AT_NULL:
			return;
		case AT_IGNORE:
			break;
		case AT_EXECFD:
			PRINT(AT_EXECFD, %ld, (long)auxv[i].a_un.a_val);
			break;
		case AT_PHDR:
			PRINT(AT_PHDR, %p, auxv[i].a_un.a_ptr);
			break;
		case AT_PHENT:
			PRINT(AT_PHENT, %ld, (long)auxv[i].a_un.a_val);
			break;
		case AT_PHNUM:
			PRINT(AT_PHNUM, %ld, (long)auxv[i].a_un.a_val);
			break;
		case AT_PAGESZ:
			PRINT(AT_PAGESZ, %ld, (long)auxv[i].a_un.a_val);
			break;
		case AT_BASE:
			PRINT(AT_BASE, %p, auxv[i].a_un.a_ptr);
			break;
		case AT_FLAGS:
			PRINT(AT_FLAGS, %#lx, (u_long)auxv[i].a_un.a_val);
			break;
		case AT_ENTRY:
			PRINT(AT_ENTRY, %p, auxv[i].a_un.a_ptr);
			break;
#ifdef AT_NOTELF
		case AT_NOTELF:
			PRINT(AT_NOTELF, %ld, (long)auxv[i].a_un.a_val);
			break;
#endif
#ifdef AT_UID
		case AT_UID:
			PRINT(AT_UID, %ld, (long)auxv[i].a_un.a_val);
			break;
#endif
#ifdef AT_EUID
		case AT_EUID:
			PRINT(AT_EUID, %ld, (long)auxv[i].a_un.a_val);
			break;
#endif
#ifdef AT_GID
		case AT_GID:
			PRINT(AT_GID, %ld, (long)auxv[i].a_un.a_val);
			break;
#endif
#ifdef AT_EGID
		case AT_EGID:
			PRINT(AT_EGID, %ld, (long)auxv[i].a_un.a_val);
			break;
#endif
		case AT_EXECPATH:
			PRINT(AT_EXECPATH, %p, auxv[i].a_un.a_ptr);
			break;
		case AT_CANARY:
			PRINT(AT_CANARY, %p, auxv[i].a_un.a_ptr);
			break;
		case AT_CANARYLEN:
			PRINT(AT_CANARYLEN, %ld, (long)auxv[i].a_un.a_val);
			break;
		case AT_OSRELDATE:
			PRINT(AT_OSRELDATE, %ld, (long)auxv[i].a_un.a_val);
			break;
		case AT_NCPUS:
			PRINT(AT_NCPUS, %ld, (long)auxv[i].a_un.a_val);
			break;
		case AT_PAGESIZES:
			PRINT(AT_PAGESIZES, %p, auxv[i].a_un.a_ptr);
			break;
		case AT_PAGESIZESLEN:
			PRINT(AT_PAGESIZESLEN, %ld, (long)auxv[i].a_un.a_val);
			break;
		case AT_STACKPROT:
			if ((auxv[i].a_un.a_val & VM_PROT_EXECUTE) != 0)
				PRINT(AT_STACKPROT, %s, "NONEXECUTABLE");
			else
				PRINT(AT_STACKPROT, %s, "EXECUTABLE");
			break;
		case AT_COUNT:
			PRINT(AT_COUNT, %ld, (long)auxv[i].a_un.a_val);
			break;
		default:
			PRINT_UNKNOWN(auxv[i].a_type, auxv[i].a_un.a_val);
			break;
		}
	}
	printf("\n");
}

