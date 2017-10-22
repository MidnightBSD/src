/*	$FreeBSD$	*/
/*	$NecBSD: physio_proc.h,v 3.4 1999/07/23 20:47:03 honda Exp $	*/
/*	$NetBSD$	*/

/*-
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _COMPAT_NETBSD_PHYSIO_PROC_H_
#define _COMPAT_NETBSD_PHYSIO_PROC_H_
#include <sys/queue.h>

struct buf;

struct physio_proc {
};

static __inline struct physio_proc *physio_proc_enter(struct buf *);
static __inline void physio_proc_leave(struct physio_proc *);

static __inline struct physio_proc *
physio_proc_enter(bp)
	struct buf *bp;
{
	return NULL;
}

static __inline void
physio_proc_leave(pp)
	struct physio_proc *pp;
{
}
#endif /* _COMPAT_NETBSD_PHYSIO_PROC_H_ */
