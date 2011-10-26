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
 *
 * $FreeBSD: src/sys/cddl/compat/opensolaris/sys/time.h,v 1.3.2.2.2.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef _OPENSOLARIS_SYS_TIME_H_
#define	_OPENSOLARIS_SYS_TIME_H_

#include_next <sys/time.h>

#define SEC		1
#define MILLISEC	1000
#define MICROSEC	1000000
#define NANOSEC		1000000000

typedef longlong_t	hrtime_t;

#define	LBOLT	((gethrtime() * hz) / NANOSEC)

#ifdef _KERNEL
#define	lbolt64	(int64_t)(LBOLT)

static __inline hrtime_t
gethrtime(void) {

	struct timespec ts;
	hrtime_t nsec;

#if 1
	getnanouptime(&ts);
#else
	nanouptime(&ts);
#endif
	nsec = (hrtime_t)ts.tv_sec * NANOSEC + ts.tv_nsec;
	return (nsec);
}

#define	gethrestime_sec()	(time_second)
#define	gethrestime(ts)		getnanotime(ts)

#else

static __inline hrtime_t gethrtime(void) {
	struct timespec ts;
	clock_gettime(CLOCK_UPTIME,&ts);
	return (((u_int64_t) ts.tv_sec) * NANOSEC + ts.tv_nsec);
}


#endif	/* _KERNEL */

#endif	/* !_OPENSOLARIS_SYS_TIME_H_ */
