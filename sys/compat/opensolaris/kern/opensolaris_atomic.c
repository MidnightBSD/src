/*-
 * Copyright (c) 2007 Pawel Jakub Dawidek <pjd@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/7.0.0/sys/compat/opensolaris/kern/opensolaris_atomic.c 170431 2007-06-08 12:35:47Z pjd $");

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/atomic.h>

#ifdef _KERNEL
#include <sys/kernel.h>

struct mtx atomic_mtx;
MTX_SYSINIT(atomic, &atomic_mtx, "atomic", MTX_DEF);
#else
#include <pthread.h>

#define	mtx_lock(lock)		pthread_mutex_lock(lock)
#define	mtx_unlock(lock)	pthread_mutex_unlock(lock)

static pthread_mutex_t atomic_mtx;

static __attribute__((constructor)) void
atomic_init(void)
{
	pthread_mutex_init(&atomic_mtx, NULL);
}
#endif

#ifndef __LP64__
void
atomic_add_64(volatile uint64_t *target, int64_t delta)
{

	mtx_lock(&atomic_mtx);
	*target += delta;
	mtx_unlock(&atomic_mtx);
}
#endif

uint64_t
atomic_add_64_nv(volatile uint64_t *target, int64_t delta)
{
	uint64_t newval;

	mtx_lock(&atomic_mtx);
	newval = (*target += delta);
	mtx_unlock(&atomic_mtx);
	return (newval);
}

#if defined(__sparc64__) || defined(__powerpc__) || defined(__arm__)
void
atomic_or_8(volatile uint8_t *target, uint8_t value)
{
	mtx_lock(&atomic_mtx);
	*target |= value;
	mtx_unlock(&atomic_mtx);
}
#endif

uint8_t
atomic_or_8_nv(volatile uint8_t *target, uint8_t value)
{
	uint8_t newval;

	mtx_lock(&atomic_mtx);
	newval = (*target |= value);
	mtx_unlock(&atomic_mtx);
	return (newval);
}

#ifndef __LP64__
void *
atomic_cas_ptr(volatile void *target, void *cmp,  void *newval)
{
	void *oldval, **trg;

	mtx_lock(&atomic_mtx);
	trg = __DEVOLATILE(void **, target);
	oldval = *trg;
	if (oldval == cmp)
		*trg = newval;
	mtx_unlock(&atomic_mtx);
	return (oldval);
}
#endif

#ifndef __sparc64__
uint64_t
atomic_cas_64(volatile uint64_t *target, uint64_t cmp, uint64_t newval)
{
	uint64_t oldval;

	mtx_lock(&atomic_mtx);
	oldval = *target;
	if (oldval == cmp)
		*target = newval;
	mtx_unlock(&atomic_mtx);
	return (oldval);
}
#endif

void
membar_producer(void)
{
	/* nothing */
}
