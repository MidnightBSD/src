/*-
 * Copyright (c) 2002 Ian Dowse.  All rights reserved.
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
 * $FreeBSD: release/7.0.0/sys/sys/syscallsubr.h 174854 2007-12-22 06:32:46Z cvs2svn $
 */

#ifndef _SYS_SYSCALLSUBR_H_
#define _SYS_SYSCALLSUBR_H_

#include <sys/signal.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/mac.h>
#include <sys/mount.h>

struct file;
struct itimerval;
struct image_args;
struct mbuf;
struct msghdr;
struct msqid_ds;
struct rlimit;
struct rusage;
union semun;
struct sockaddr;
struct stat;
struct kevent;
struct kevent_copyops;
struct sendfile_args;
struct thr_param;

int	kern___getcwd(struct thread *td, u_char *buf, enum uio_seg bufseg,
	    u_int buflen);
int	kern_accept(struct thread *td, int s, struct sockaddr **name,
	    socklen_t *namelen, struct file **fp);
int	kern_access(struct thread *td, char *path, enum uio_seg pathseg,
	    int flags);
int	kern_adjtime(struct thread *td, struct timeval *delta,
	    struct timeval *olddelta);
int	kern_alternate_path(struct thread *td, const char *prefix, char *path,
	    enum uio_seg pathseg, char **pathbuf, int create);
int	kern_bind(struct thread *td, int fd, struct sockaddr *sa);
int	kern_chdir(struct thread *td, char *path, enum uio_seg pathseg);
int	kern_chmod(struct thread *td, char *path, enum uio_seg pathseg,
	    int mode);
int	kern_chown(struct thread *td, char *path, enum uio_seg pathseg, int uid,
	    int gid);
int	kern_clock_getres(struct thread *td, clockid_t clock_id,
	    struct timespec *ts);
int	kern_clock_gettime(struct thread *td, clockid_t clock_id,
	    struct timespec *ats);
int	kern_clock_settime(struct thread *td, clockid_t clock_id,
	    struct timespec *ats);
int	kern_close(struct thread *td, int fd);
int	kern_connect(struct thread *td, int fd, struct sockaddr *sa);
int	kern_eaccess(struct thread *td, char *path, enum uio_seg pathseg,
	    int flags);
int	kern_execve(struct thread *td, struct image_args *args,
	    struct mac *mac_p);
int	kern_fcntl(struct thread *td, int fd, int cmd, intptr_t arg);
int	kern_fhstatfs(struct thread *td, fhandle_t fh, struct statfs *buf);
int	kern_fstat(struct thread *td, int fd, struct stat *sbp);
int	kern_fstatfs(struct thread *td, int fd, struct statfs *buf);
int	kern_futimes(struct thread *td, int fd, struct timeval *tptr,
	    enum uio_seg tptrseg);
int	kern_getfsstat(struct thread *td, struct statfs **buf, size_t bufsize,
	    enum uio_seg bufseg, int flags);
int	kern_getgroups(struct thread *td, u_int *ngrp, gid_t *groups);
int	kern_getitimer(struct thread *, u_int, struct itimerval *);
int	kern_getpeername(struct thread *td, int fd, struct sockaddr **sa,
	    socklen_t *alen);
int	kern_getrusage(struct thread *td, int who, struct rusage *rup);
int	kern_getsockname(struct thread *td, int fd, struct sockaddr **sa,
	    socklen_t *alen);
int	kern_getsockopt(struct thread *td, int s, int level, int name,
	    void *optval, enum uio_seg valseg, socklen_t *valsize);
int	kern_ioctl(struct thread *td, int fd, u_long com, caddr_t data);
int	kern_kevent(struct thread *td, int fd, int nchanges, int nevents,
	    struct kevent_copyops *k_ops, const struct timespec *timeout);
int	kern_kldload(struct thread *td, const char *file, int *fileid);
int	kern_kldunload(struct thread *td, int fileid, int flags);
int	kern_lchown(struct thread *td, char *path, enum uio_seg pathseg,
	    int uid, int gid);
int	kern_link(struct thread *td, char *path, char *link,
	    enum uio_seg segflg);
int	kern_lstat(struct thread *td, char *path, enum uio_seg pathseg,
	    struct stat *sbp);
int	kern_lutimes(struct thread *td, char *path, enum uio_seg pathseg,
	    struct timeval *tptr, enum uio_seg tptrseg);
int	kern_mkdir(struct thread *td, char *path, enum uio_seg segflg,
	    int mode);
int	kern_mkfifo(struct thread *td, char *path, enum uio_seg pathseg,
	    int mode);
int	kern_mknod(struct thread *td, char *path, enum uio_seg pathseg,
	    int mode, int dev);
int	kern_msgctl(struct thread *, int, int, struct msqid_ds *);
int	kern_msgsnd(struct thread *, int, const void *, size_t, int, long);
int	kern_msgrcv(struct thread *, int, void *, size_t, long, int, long *);
int     kern_nanosleep(struct thread *td, struct timespec *rqt,
	    struct timespec *rmt);
int	kern_open(struct thread *td, char *path, enum uio_seg pathseg,
	    int flags, int mode);
int	kern_pathconf(struct thread *td, char *path, enum uio_seg pathseg,
	    int name);
int	kern_preadv(struct thread *td, int fd, struct uio *auio, off_t offset);
int	kern_ptrace(struct thread *td, int req, pid_t pid, void *addr,
	    int data);
int	kern_pwritev(struct thread *td, int fd, struct uio *auio, off_t offset);
int	kern_readlink(struct thread *td, char *path, enum uio_seg pathseg,
	    char *buf, enum uio_seg bufseg, int count);
int	kern_readv(struct thread *td, int fd, struct uio *auio);
int	kern_recvit(struct thread *td, int s, struct msghdr *mp,
	    enum uio_seg fromseg, struct mbuf **controlp);
int	kern_rename(struct thread *td, char *from, char *to,
	    enum uio_seg pathseg);
int	kern_rmdir(struct thread *td, char *path, enum uio_seg pathseg);
int	kern_sched_rr_get_interval(struct thread *td, pid_t pid,
	    struct timespec *ts);
int	kern_semctl(struct thread *td, int semid, int semnum, int cmd,
	    union semun *arg, register_t *rval);
int	kern_select(struct thread *td, int nd, fd_set *fd_in, fd_set *fd_ou,
	    fd_set *fd_ex, struct timeval *tvp);
int	kern_sendfile(struct thread *td, struct sendfile_args *uap,
	    struct uio *hdr_uio, struct uio *trl_uio, int compat);
int	kern_sendit(struct thread *td, int s, struct msghdr *mp, int flags,
	    struct mbuf *control, enum uio_seg segflg);
int	kern_setgroups(struct thread *td, u_int ngrp, gid_t *groups);
int	kern_setitimer(struct thread *, u_int, struct itimerval *,
	    struct itimerval *);
int	kern_setrlimit(struct thread *, u_int, struct rlimit *);
int	kern_setsockopt(struct thread *td, int s, int level, int name,
	    void *optval, enum uio_seg valseg, socklen_t valsize);
int	kern_settimeofday(struct thread *td, struct timeval *tv,
	    struct timezone *tzp);
int	kern_shmat(struct thread *td, int shmid, const void *shmaddr,
	    int shmflg);
int	kern_shmctl(struct thread *td, int shmid, int cmd, void *buf,
	    size_t *bufsz);
int	kern_sigaction(struct thread *td, int sig, struct sigaction *act,
	    struct sigaction *oact, int flags);
int	kern_sigaltstack(struct thread *td, stack_t *ss, stack_t *oss);
int	kern_sigprocmask(struct thread *td, int how,
	    sigset_t *set, sigset_t *oset, int old);
int	kern_sigsuspend(struct thread *td, sigset_t mask);
int	kern_stat(struct thread *td, char *path, enum uio_seg pathseg,
	    struct stat *sbp);
int	kern_statfs(struct thread *td, char *path, enum uio_seg pathseg,
	    struct statfs *buf);
int	kern_symlink(struct thread *td, char *path, char *link,
	    enum uio_seg segflg);
int	kern_thr_new(struct thread *td, struct thr_param *param);
int	kern_thr_suspend(struct thread *td, struct timespec *tsp);
int	kern_truncate(struct thread *td, char *path, enum uio_seg pathseg,
	    off_t length);
int	kern_unlink(struct thread *td, char *path, enum uio_seg pathseg);
int	kern_utimes(struct thread *td, char *path, enum uio_seg pathseg,
	    struct timeval *tptr, enum uio_seg tptrseg);
int	kern_wait(struct thread *td, pid_t pid, int *status, int options,
	    struct rusage *rup);
int	kern_writev(struct thread *td, int fd, struct uio *auio);

/* flags for kern_sigaction */
#define	KSA_OSIGSET	0x0001	/* uses osigact_t */
#define	KSA_FREEBSD4	0x0002	/* uses ucontext4 */

#endif /* !_SYS_SYSCALLSUBR_H_ */
