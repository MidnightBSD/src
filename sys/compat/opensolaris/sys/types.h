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
 * $FreeBSD: release/7.0.0/sys/compat/opensolaris/sys/types.h 168962 2007-04-23 00:52:07Z pjd $
 */

#ifndef _OPENSOLARIS_SYS_TYPES_H_
#define	_OPENSOLARIS_SYS_TYPES_H_

/*
 * This is a bag of dirty hacks to keep things compiling.
 */

#include <sys/stdint.h>
#include_next <sys/types.h>

#define	MAXNAMELEN	256

typedef	struct timespec	timestruc_t;

#ifdef _KERNEL

#define	B_FALSE	0
#define	B_TRUE	1

typedef	short		index_t;
typedef	off_t		offset_t;
typedef	long		ptrdiff_t;	/* pointer difference */
typedef	void		pathname_t;
typedef	int64_t		rlim64_t;

#else

typedef	longlong_t	offset_t;
typedef	u_longlong_t	u_offset_t;
typedef	uint64_t	upad64_t;
typedef	struct timespec	timespec_t;
typedef	short		pri_t;
typedef	int32_t		daddr32_t;
typedef	int32_t		time32_t;
typedef	u_longlong_t	diskaddr_t;
typedef	ushort_t	o_mode_t;	/* old file attribute type */

#endif	/* !_KERNEL */

#endif	/* !_OPENSOLARIS_SYS_TYPES_H_ */
