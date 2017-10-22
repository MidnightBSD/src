/*-
 * Copyright (c) 2003 Jake Burkholder <jake@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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

#ifndef	_ATOMIC_OPS_H_
#define	_ATOMIC_OPS_H_

#include <machine/atomic.h>

/*
 * Atomic swap:
 *   Atomic (tmp = *dst, *dst = val), then *res = tmp
 *
 * void atomic_swap_long(long *dst, long val, long *res);
 */
static __inline void
atomic_swap_long(volatile long *dst, long val, long *res)
{
	long tmp;
	long r;

	tmp = *dst;
	for (;;) {
		r = atomic_cas_64(dst, tmp, val);
		if (r == tmp)
			break;
		tmp = r;
	}
	*res = tmp;
}

static __inline void
atomic_swap_int(volatile int *dst, int val, int *res)
{
	int tmp;
	int r;

	tmp = *dst;
	for (;;) {
		r = atomic_cas_32(dst, tmp, val);
		if (r == tmp)
			break;
		tmp = r;
	}
	*res = tmp;
}

#define	atomic_swap_ptr(dst, val, res) \
	atomic_swap_long((volatile long *)dst, (long)val, (long *)res)

#endif
