/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)param.h	5.8 (Berkeley) 6/28/91
 * $FreeBSD: release/7.0.0/sys/sun4v/include/param.h 163022 2006-10-05 06:14:28Z kmacy $
 */

/*
 * Machine dependent constants for sparc64.
 */

/*
 * Round p (pointer or byte index) up to a correctly-aligned value
 * for all data types (int, long, ...).   The result is unsigned int
 * and must be cast to any desired pointer type.
 */
#ifndef _ALIGNBYTES
#define _ALIGNBYTES	0xf
#endif
#ifndef _ALIGN
#define _ALIGN(p)	(((u_long)(p) + _ALIGNBYTES) & ~_ALIGNBYTES)
#endif

#ifndef _NO_NAMESPACE_POLLUTION

#ifndef _MACHINE_PARAM_H_
#define	_MACHINE_PARAM_H_

#ifndef MACHINE
#define MACHINE		"sun4v"
#endif
#ifndef MACHINE_ARCH
#define	MACHINE_ARCH	"sparc64"
#endif
#define MID_MACHINE	MID_SPARC64

#ifdef SMP
#define MAXCPU		32
#else
#define MAXCPU		1
#endif /* SMP */

#define	INT_SHIFT	2
#define	PTR_SHIFT	3

#define ALIGNBYTES	_ALIGNBYTES
#define ALIGN(p)	_ALIGN(p)

#define	PAGE_SHIFT_8K	13
#define	PAGE_SIZE_8K	(1L<<PAGE_SHIFT_8K)
#define	PAGE_MASK_8K	(PAGE_SIZE_8K-1)

#define	PAGE_SHIFT_64K	16
#define	PAGE_SIZE_64K	(1L<<PAGE_SHIFT_64K)
#define	PAGE_MASK_64K	(PAGE_SIZE_64K-1)

#define	PAGE_SHIFT_512K	19
#define	PAGE_SIZE_512K	(1L<<PAGE_SHIFT_512K)
#define	PAGE_MASK_512K	(PAGE_SIZE_512K-1)

#define	PAGE_SHIFT_4M	22
#define	PAGE_SIZE_4M	(1L<<PAGE_SHIFT_4M)
#define	PAGE_MASK_4M	(PAGE_SIZE_4M-1)

#define	PAGE_SHIFT_256M	28
#define	PAGE_SIZE_256M	(1L<<PAGE_SHIFT_256M)
#define	PAGE_MASK_256M	(PAGE_SIZE_256M-1)


#define PAGE_SHIFT_MIN	PAGE_SHIFT_8K
#define PAGE_SIZE_MIN	PAGE_SIZE_8K
#define PAGE_MASK_MIN	PAGE_MASK_8K
#define PAGE_SHIFT	PAGE_SHIFT_8K	/* LOG2(PAGE_SIZE) */
#define PAGE_SIZE	PAGE_SIZE_8K	/* bytes/page */
#define PAGE_MASK	PAGE_MASK_8K
#define PAGE_SHIFT_MAX	PAGE_SHIFT_4M
#define PAGE_SIZE_MAX	PAGE_SIZE_4M
#define PAGE_MASK_MAX	PAGE_MASK_4M

#ifndef KSTACK_PAGES
#define KSTACK_PAGES		4	/* pages of kernel stack (with pcb) */
#endif
#define KSTACK_GUARD_PAGES	1	/* pages of kstack guard; 0 disables */
#define PCPU_PAGES		1

/*
 * Ceiling on size of buffer cache (really only effects write queueing,
 * the VM page cache is not effected), can be changed via
 * the kern.maxbcache /boot/loader.conf variable.
 */
#ifndef VM_BCACHE_SIZE_MAX
#define VM_BCACHE_SIZE_MAX      (400 * 1024 * 1024)
#endif

/*
 * Mach derived conversion macros
 */
#ifndef LOCORE
#define round_page(x)		(((unsigned long)(x) + PAGE_MASK) & ~PAGE_MASK)
#define trunc_page(x)		((unsigned long)(x) & ~PAGE_MASK)

#define atop(x)			((unsigned long)(x) >> PAGE_SHIFT)
#define ptoa(x)			((unsigned long)(x) << PAGE_SHIFT)

#define sparc64_btop(x)		((unsigned long)(x) >> PAGE_SHIFT)
#define sparc64_ptob(x)		((unsigned long)(x) << PAGE_SHIFT)

#define	pgtok(x)		((unsigned long)(x) * (PAGE_SIZE / 1024))
#endif /* LOCORE */



#endif /* !_MACHINE_PARAM_H_ */
#endif /* !_NO_NAMESPACE_POLLUTION */
