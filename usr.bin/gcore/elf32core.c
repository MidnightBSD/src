/* $MidnightBSD$ */
/* $FreeBSD: stable/10/usr.bin/gcore/elf32core.c 283910 2015-06-02 14:54:53Z jhb $ */
#ifndef __LP64__
#error "this file must be compiled for LP64."
#endif

#define __ELF_WORD_SIZE 32
#define _MACHINE_ELF_WANT_32BIT

#include <sys/procfs.h>

#define	ELFCORE_COMPAT_32	1
#include "elfcore.c"

static void
elf_convert_gregset(elfcore_gregset_t *rd, struct reg *rs)
{
#ifdef __amd64__
	rd->r_gs = rs->r_gs;
	rd->r_fs = rs->r_fs;
	rd->r_es = rs->r_es;
	rd->r_ds = rs->r_ds;
	rd->r_edi = rs->r_rdi;
	rd->r_esi = rs->r_rsi;
	rd->r_ebp = rs->r_rbp;
	rd->r_ebx = rs->r_rbx;
	rd->r_edx = rs->r_rdx;
	rd->r_ecx = rs->r_rcx;
	rd->r_eax = rs->r_rax;
	rd->r_eip = rs->r_rip;
	rd->r_cs = rs->r_cs;
	rd->r_eflags = rs->r_rflags;
	rd->r_esp = rs->r_rsp;
	rd->r_ss = rs->r_ss;
#else
#error Unsupported architecture
#endif
}

static void
elf_convert_fpregset(elfcore_fpregset_t *rd, struct fpreg *rs)
{
#ifdef __amd64__
	/* XXX this is wrong... */
	memcpy(rd, rs, sizeof(*rd));
#else
#error Unsupported architecture
#endif
}
