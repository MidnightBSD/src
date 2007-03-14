/*-
 * Copyright (c) 2003-2007 Tim Kientzle
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/lib/libarchive/archive_platform.h,v 1.16.2.2 2007/01/27 06:44:52 kientzle Exp $
 */

/*
 * This header is the first thing included in any of the libarchive
 * source files.  As far as possible, platform-specific issues should
 * be dealt with here and not within individual source files.  I'm
 * actively trying to minimize #if blocks within the main source,
 * since they obfuscate the code.
 */

#ifndef ARCHIVE_PLATFORM_H_INCLUDED
#define	ARCHIVE_PLATFORM_H_INCLUDED

#if defined(HAVE_CONFIG_H)
/* Most POSIX platforms use the 'configure' script to build config.h */
#include "../config.h"
#elif defined(__FreeBSD__)
/* Building as part of FreeBSD system requires a pre-built config.h. */
#include "config_freebsd.h"
#elif defined(_WIN32)
/* Win32 can't run the 'configure' script. */
#include "config_windows.h"
#else
/* Warn if the library hasn't been (automatically or manually) configured. */
#error Oops: No config.h and no pre-built configuration in archive_platform.h.
#endif

/*
 * The config files define a lot of feature macros.  The following
 * uses those macros to select/define replacements and include key
 * headers as required.
 */

/* No non-FreeBSD platform will have __FBSDID, so just define it here. */
#ifdef __FreeBSD__
#include <sys/cdefs.h>  /* For __FBSDID */
#else
#define	__FBSDID(a)     /* null */
#endif

/* Try to get standard C99-style integer type definitions. */
#if HAVE_INTTYPES_H
#include <inttypes.h>
#elif HAVE_STDINT_H
#include <stdint.h>
#endif

/*
 * If this platform has <sys/acl.h>, acl_create(), acl_init(),
 * acl_set_file(), and ACL_USER, we assume it has the rest of the
 * POSIX.1e draft functions used in archive_read_extract.c.
 */
#if HAVE_SYS_ACL_H && HAVE_ACL_CREATE_ENTRY && HAVE_ACL_INIT && HAVE_ACL_SET_FILE && HAVE_ACL_USER
#define	HAVE_POSIX_ACL	1
#endif

/*
 * If we can't restore metadata using a file descriptor, then
 * for compatibility's sake, close files before trying to restore metadata.
 */
#if defined(HAVE_FCHMOD) || defined(HAVE_FUTIMES) || defined(HAVE_ACL_SET_FD) || defined(HAVE_ACL_SET_FD_NP) || defined(HAVE_FCHOWN)
#define	CAN_RESTORE_METADATA_FD
#endif

/* Set up defaults for internal error codes. */
#ifndef ARCHIVE_ERRNO_FILE_FORMAT
#if HAVE_EFTYPE
#define	ARCHIVE_ERRNO_FILE_FORMAT EFTYPE
#else
#if HAVE_EILSEQ
#define	ARCHIVE_ERRNO_FILE_FORMAT EILSEQ
#else
#define	ARCHIVE_ERRNO_FILE_FORMAT EINVAL
#endif
#endif
#endif

#ifndef ARCHIVE_ERRNO_PROGRAMMER
#define	ARCHIVE_ERRNO_PROGRAMMER EINVAL
#endif

#ifndef ARCHIVE_ERRNO_MISC
#define	ARCHIVE_ERRNO_MISC (-1)
#endif

/* Select the best way to set/get hi-res timestamps. */
#if HAVE_STRUCT_STAT_ST_MTIMESPEC_TV_NSEC
/* FreeBSD uses "timespec" members. */
#define	ARCHIVE_STAT_ATIME_NANOS(st)	(st)->st_atimespec.tv_nsec
#define	ARCHIVE_STAT_CTIME_NANOS(st)	(st)->st_ctimespec.tv_nsec
#define	ARCHIVE_STAT_MTIME_NANOS(st)	(st)->st_mtimespec.tv_nsec
#define	ARCHIVE_STAT_SET_ATIME_NANOS(st, n) (st)->st_atimespec.tv_nsec = (n)
#define	ARCHIVE_STAT_SET_CTIME_NANOS(st, n) (st)->st_ctimespec.tv_nsec = (n)
#define	ARCHIVE_STAT_SET_MTIME_NANOS(st, n) (st)->st_mtimespec.tv_nsec = (n)
#else
#if HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC
/* Linux uses "tim" members. */
#define	ARCHIVE_STAT_ATIME_NANOS(pstat)	(pstat)->st_atim.tv_nsec
#define	ARCHIVE_STAT_CTIME_NANOS(pstat)	(pstat)->st_ctim.tv_nsec
#define	ARCHIVE_STAT_MTIME_NANOS(pstat)	(pstat)->st_mtim.tv_nsec
#define	ARCHIVE_STAT_SET_ATIME_NANOS(st, n) (st)->st_atim.tv_nsec = (n)
#define	ARCHIVE_STAT_SET_CTIME_NANOS(st, n) (st)->st_ctim.tv_nsec = (n)
#define	ARCHIVE_STAT_SET_MTIME_NANOS(st, n) (st)->st_mtim.tv_nsec = (n)
#else
/* If we can't find a better way, just use stubs. */
#define	ARCHIVE_STAT_ATIME_NANOS(pstat)	0
#define	ARCHIVE_STAT_CTIME_NANOS(pstat)	0
#define	ARCHIVE_STAT_MTIME_NANOS(pstat)	0
#define	ARCHIVE_STAT_SET_ATIME_NANOS(st, n) ((void)(n))
#define	ARCHIVE_STAT_SET_CTIME_NANOS(st, n) ((void)(n))
#define	ARCHIVE_STAT_SET_MTIME_NANOS(st, n) ((void)(n))
#endif
#endif

#endif /* !ARCHIVE_H_INCLUDED */
