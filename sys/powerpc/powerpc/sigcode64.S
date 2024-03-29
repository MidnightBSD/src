/* $NetBSD: sigcode.S,v 1.1 1999/11/17 14:56:11 kleink Exp $	*/

/*-
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <machine/asm.h>
#include <sys/syscall.h>
#include "assym.inc"

/*
 * The following code gets copied to the top of the user stack on process
 * execution.  It does signal trampolining on signal delivery.
 *
 * On entry r1 points to a struct sigframe at bottom of current stack.
 * All other registers are unchanged.
 *
 * Entered midway through for v2 ELF binaries that don't need to deal with
 * function descriptors.
 *
 */
	.globl	CNAME(sigcode64),CNAME(szsigcode64)
	.globl	CNAME(sigcode64_elfv2),CNAME(szsigcode64_elfv2)
CNAME(sigcode64):
	mflr	2			/* resolve function descriptor */
	ld	0,0(2)			
	ld	2,8(2)
	mtlr	0

CNAME(sigcode64_elfv2):
	addi	1,1,-112		/* reserved space for callee */
	mflr	12			/* ELFv2 wants the address in r12 */
	blrl

	addi	3,1,112+SF_UC		/* restore sp, and get &frame->sf_uc */
	li	0,SYS_sigreturn
	sc				/* sigreturn(scp) */
	li	0,SYS_exit
	sc				/* exit(errno) */
	nop				/* align to doubleword */
endsigcode64:
	
	.data
CNAME(szsigcode64):
	.long	endsigcode64 - CNAME(sigcode64)
CNAME(szsigcode64_elfv2):
	.long	endsigcode64 - CNAME(sigcode64_elfv2)

