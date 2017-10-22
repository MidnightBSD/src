/*-
 * Copyright (c) 2001 Doug Rabson
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
 * $FreeBSD: release/7.0.0/sys/compat/freebsd32/freebsd32.h 174785 2007-12-19 20:37:53Z jhb $
 */

#ifndef _COMPAT_FREEBSD32_FREEBSD32_H_
#define _COMPAT_FREEBSD32_FREEBSD32_H_

#define PTRIN(v)	(void *)(uintptr_t) (v)
#define PTROUT(v)	(u_int32_t)(uintptr_t) (v)

#define CP(src,dst,fld) do { (dst).fld = (src).fld; } while (0)
#define PTRIN_CP(src,dst,fld) \
	do { (dst).fld = PTRIN((src).fld); } while (0)
#define PTROUT_CP(src,dst,fld) \
	do { (dst).fld = PTROUT((src).fld); } while (0)

struct timeval32 {
	int32_t	tv_sec;
	int32_t tv_usec;
};
#define TV_CP(src,dst,fld) do {			\
	CP((src).fld,(dst).fld,tv_sec);		\
	CP((src).fld,(dst).fld,tv_usec);	\
} while (0)

struct timespec32 {
	u_int32_t tv_sec;
	u_int32_t tv_nsec;
};
#define TS_CP(src,dst,fld) do {			\
	CP((src).fld,(dst).fld,tv_sec);		\
	CP((src).fld,(dst).fld,tv_nsec);	\
} while (0);

struct rusage32 {
	struct timeval32 ru_utime;
	struct timeval32 ru_stime;
	int32_t	ru_maxrss;
	int32_t	ru_ixrss;
	int32_t	ru_idrss;
	int32_t	ru_isrss;
	int32_t	ru_minflt;
	int32_t	ru_majflt;
	int32_t	ru_nswap;
	int32_t	ru_inblock;
	int32_t	ru_oublock;
	int32_t	ru_msgsnd;
	int32_t	ru_msgrcv;
	int32_t	ru_nsignals;
	int32_t	ru_nvcsw;
	int32_t	ru_nivcsw;
};

struct itimerval32 {
	struct timeval32 it_interval;
	struct timeval32 it_value;
};

#define FREEBSD4_MNAMELEN        (88 - 2 * sizeof(int32_t)) /* size of on/from name bufs */

/* 4.x version */
struct statfs32 {
	int32_t	f_spare2;
	int32_t	f_bsize;
	int32_t	f_iosize;
	int32_t	f_blocks;
	int32_t	f_bfree;
	int32_t	f_bavail;
	int32_t	f_files;
	int32_t	f_ffree;
	fsid_t	f_fsid;
	uid_t	f_owner;
	int32_t	f_type;
	int32_t	f_flags;
	int32_t	f_syncwrites;
	int32_t	f_asyncwrites;
	char	f_fstypename[MFSNAMELEN];
	char	f_mntonname[FREEBSD4_MNAMELEN];
	int32_t	f_syncreads;
	int32_t	f_asyncreads;
	int16_t	f_spares1;
	char	f_mntfromname[FREEBSD4_MNAMELEN];
	int16_t	f_spares2 __packed;
	int32_t f_spare[2];
};

struct kevent32 {
	u_int32_t	ident;		/* identifier for this event */
	short		filter;		/* filter for event */
	u_short		flags;
	u_int		fflags;
	int32_t		data;
	u_int32_t	udata;		/* opaque user data identifier */
};

struct iovec32 {
	u_int32_t iov_base;
	int	iov_len;
};

struct msghdr32 {
	u_int32_t	 msg_name;
	socklen_t	 msg_namelen;
	u_int32_t	 msg_iov;
	int		 msg_iovlen;
	u_int32_t	 msg_control;
	socklen_t	 msg_controllen;
	int		 msg_flags;
};

struct stat32 {
	dev_t	st_dev;
	ino_t	st_ino;
	mode_t	st_mode;
	nlink_t	st_nlink;
	uid_t	st_uid;
	gid_t	st_gid;
	dev_t	st_rdev;
	struct timespec32 st_atimespec;
	struct timespec32 st_mtimespec;
	struct timespec32 st_ctimespec;
	off_t	st_size;
	int64_t	st_blocks;
	u_int32_t st_blksize;
	u_int32_t st_flags;
	u_int32_t st_gen;
	struct timespec32 st_birthtimespec;
	unsigned int :(8 / 2) * (16 - (int)sizeof(struct timespec32));
	unsigned int :(8 / 2) * (16 - (int)sizeof(struct timespec32));
};

struct sigaction32 {
	u_int32_t	sa_u;
	int		sa_flags;
	sigset_t	sa_mask;
};

struct thr_param32 {
	uint32_t start_func;
	uint32_t arg;
	uint32_t stack_base;
	uint32_t stack_size;
	uint32_t tls_base;
	uint32_t tls_size;
	uint32_t child_tid;
	uint32_t parent_tid;
	int32_t	 flags;
	uint32_t rtp;
	uint32_t spare[3];
};

#endif /* !_COMPAT_FREEBSD32_FREEBSD32_H_ */
