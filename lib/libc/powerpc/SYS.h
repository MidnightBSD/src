/*-
 * Copyright (c) 2002 Benno Rice.  All rights reserved.
 * Copyright (c) 2002 David E. O'Brien.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
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
 *	$NetBSD: SYS.h,v 1.8 2002/01/14 00:55:56 thorpej Exp $
 * $FreeBSD: src/lib/libc/powerpc/SYS.h,v 1.6 2003/01/18 23:26:04 obrien Exp $
 */

#include <sys/syscall.h>
#include <machine/asm.h>

#define _SYSCALL(x)						\
	.text;							\
	.align 2;						\
	li	0,(__CONCAT(SYS_,x));				\
	sc

#define	SYSCALL(x)						\
	.text;							\
	.align 2;						\
2:	b	PIC_PLT(CNAME(HIDENAME(cerror)));		\
ENTRY(__CONCAT(__sys_,x));					\
	.weak	CNAME(x);					\
	.set	CNAME(x),CNAME(__CONCAT(__sys_,x));		\
	.weak	CNAME(__CONCAT(_,x));				\
	.set	CNAME(__CONCAT(_,x)),CNAME(__CONCAT(__sys_,x));	\
	_SYSCALL(x);						\
	bso	2b

#define	PSEUDO(x)						\
	.text;							\
	.align 2;						\
ENTRY(__CONCAT(__sys_,x));					\
	.weak	CNAME(__CONCAT(_,x));				\
	.set	CNAME(__CONCAT(_,x)),CNAME(__CONCAT(__sys_,x));	\
	_SYSCALL(x);						\
	bnslr;							\
	b	PIC_PLT(CNAME(HIDENAME(cerror)))

#define	RSYSCALL(x)						\
	.text;							\
	.align 2;						\
2:	b	PIC_PLT(CNAME(HIDENAME(cerror)));		\
ENTRY(__CONCAT(__sys_,x));					\
	.weak	CNAME(x);					\
	.set	CNAME(x),CNAME(__CONCAT(__sys_,x));		\
	.weak	CNAME(__CONCAT(_,x));				\
	.set	CNAME(__CONCAT(_,x)),CNAME(__CONCAT(__sys_,x));	\
	_SYSCALL(x);						\
	bnslr;							\
	b	PIC_PLT(CNAME(HIDENAME(cerror)))
