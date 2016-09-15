/*-
 * Copyright (c) 2012 Konstantin Belousov <kib@FreeBSD.org>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/elf.h>
#include <sys/time.h>
#include <sys/vdso.h>
#include <errno.h>
#include <time.h>
#include <machine/atomic.h>
#include "libc_private.h"

static u_int
tc_delta(const struct vdso_timehands *th)
{

	return ((__vdso_gettc(th) - th->th_offset_count) &
	    th->th_counter_mask);
}

static int
binuptime(struct bintime *bt, struct vdso_timekeep *tk, int abs)
{
	struct vdso_timehands *th;
	uint32_t curr, gen;

	do {
		if (!tk->tk_enabled)
			return (ENOSYS);

		/*
		 * XXXKIB. The load of tk->tk_current should use
		 * atomic_load_acq_32 to provide load barrier. But
		 * since tk points to r/o mapped page, x86
		 * implementation of atomic_load_acq faults.
		 */
		curr = tk->tk_current;
		rmb();
		th = &tk->tk_th[curr];
		if (th->th_algo != VDSO_TH_ALGO_1)
			return (ENOSYS);
		gen = th->th_gen;
		*bt = th->th_offset;
		bintime_addx(bt, th->th_scale * tc_delta(th));
		if (abs)
			bintime_add(bt, &th->th_boottime);

		/*
		 * Barrier for load of both tk->tk_current and th->th_gen.
		 */
		rmb();
	} while (curr != tk->tk_current || gen == 0 || gen != th->th_gen);
	return (0);
}

static struct vdso_timekeep *tk;

int
__vdso_gettimeofday(struct timeval *tv, struct timezone *tz)
{
	struct bintime bt;
	int error;

	if (tz != NULL)
		return (ENOSYS);
	if (tk == NULL) {
		error = _elf_aux_info(AT_TIMEKEEP, &tk, sizeof(tk));
		if (error != 0 || tk == NULL)
			return (ENOSYS);
	}
	if (tk->tk_ver != VDSO_TK_VER_CURR)
		return (ENOSYS);
	error = binuptime(&bt, tk, 1);
	if (error != 0)
		return (error);
	bintime2timeval(&bt, tv);
	return (0);
}

int
__vdso_clock_gettime(clockid_t clock_id, struct timespec *ts)
{
	struct bintime bt;
	int abs, error;

	if (tk == NULL) {
		error = _elf_aux_info(AT_TIMEKEEP, &tk, sizeof(tk));
		if (error != 0 || tk == NULL)
			return (ENOSYS);
	}
	if (tk->tk_ver != VDSO_TK_VER_CURR)
		return (ENOSYS);
	switch (clock_id) {
	case CLOCK_REALTIME:
	case CLOCK_REALTIME_PRECISE:
	case CLOCK_REALTIME_FAST:
	case CLOCK_SECOND:
		abs = 1;
		break;
	case CLOCK_MONOTONIC:
	case CLOCK_MONOTONIC_PRECISE:
	case CLOCK_MONOTONIC_FAST:
	case CLOCK_UPTIME:
	case CLOCK_UPTIME_PRECISE:
	case CLOCK_UPTIME_FAST:
		abs = 0;
		break;
	default:
		return (ENOSYS);
	}
	error = binuptime(&bt, tk, abs);
	if (error != 0)
		return (error);
	bintime2timespec(&bt, ts);
	if (clock_id == CLOCK_SECOND)
		ts->tv_nsec = 0;
	return (0);
}
