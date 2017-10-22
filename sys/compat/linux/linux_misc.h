/*-
 * Copyright (c) 2006 Roman Divacky
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _LINUX_MISC_H_
#define	_LINUX_MISC_H_

/* defines for prctl */
#define	LINUX_PR_SET_PDEATHSIG  1	/* Second arg is a signal. */
#define	LINUX_PR_GET_PDEATHSIG  2	/*
					 * Second arg is a ptr to return the
					 * signal.
					 */
#define	LINUX_PR_GET_KEEPCAPS	7	/* Get drop capabilities on setuid */
#define	LINUX_PR_SET_KEEPCAPS	8	/* Set drop capabilities on setuid */
#define	LINUX_PR_SET_NAME	15	/* Set process name. */
#define	LINUX_PR_GET_NAME	16	/* Get process name. */

#define	LINUX_MAX_COMM_LEN	16	/* Maximum length of the process name. */

#define	LINUX_MREMAP_MAYMOVE	1
#define	LINUX_MREMAP_FIXED	2

extern const char *linux_platform;

/*
 * Non-standard aux entry types used in Linux ELF binaries.
 */

#define	LINUX_AT_PLATFORM	15	/* String identifying CPU */
#define	LINUX_AT_HWCAP		16	/* CPU capabilities */
#define	LINUX_AT_CLKTCK		17	/* frequency at which times() increments */
#define	LINUX_AT_SECURE		23	/* secure mode boolean */
#define	LINUX_AT_BASE_PLATFORM	24	/* string identifying real platform, may
					 * differ from AT_PLATFORM.
					 */
#define	LINUX_AT_EXECFN		31	/* filename of program */

/* Linux sets the i387 to extended precision. */
#if defined(__i386__) || defined(__amd64__)
#define	__LINUX_NPXCW__		0x37f
#endif

extern int stclohz;

#define __WCLONE 0x80000000

int linux_common_wait(struct thread *td, int pid, int *status,
			int options, struct rusage *ru);

#endif	/* _LINUX_MISC_H_ */
