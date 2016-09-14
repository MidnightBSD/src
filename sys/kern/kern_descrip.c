/*-
 * Copyright (c) 1982, 1986, 1989, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)kern_descrip.c	8.6 (Berkeley) 4/19/94
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_capsicum.h"
#include "opt_compat.h"
#include "opt_ddb.h"
#include "opt_ktrace.h"
#include "opt_procdesc.h"

#include <sys/param.h>
#include <sys/systm.h>

#include <sys/capability.h>
#include <sys/conf.h>
#include <sys/domain.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/filio.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/mqueue.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/selinfo.h>
#include <sys/pipe.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/procdesc.h>
#include <sys/protosw.h>
#include <sys/racct.h>
#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <sys/socketvar.h>
#include <sys/stat.h>
#include <sys/sx.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/sysproto.h>
#include <sys/tty.h>
#include <sys/unistd.h>
#include <sys/un.h>
#include <sys/unpcb.h>
#include <sys/user.h>
#include <sys/vnode.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif

#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_pcb.h>

#include <security/audit/audit.h>

#include <vm/uma.h>
#include <vm/vm.h>

#include <ddb/ddb.h>

static MALLOC_DEFINE(M_FILEDESC, "filedesc", "Open file descriptor table");
static MALLOC_DEFINE(M_FILEDESC_TO_LEADER, "filedesc_to_leader",
		     "file desc to leader structures");
static MALLOC_DEFINE(M_SIGIO, "sigio", "sigio structures");

MALLOC_DECLARE(M_FADVISE);

static uma_zone_t file_zone;


/* Flags for do_dup() */
#define DUP_FIXED	0x1	/* Force fixed allocation */
#define DUP_FCNTL	0x2	/* fcntl()-style errors */
#define	DUP_CLOEXEC	0x4	/* Atomically set FD_CLOEXEC. */

static int do_dup(struct thread *td, int flags, int old, int new,
    register_t *retval);
static int	fd_first_free(struct filedesc *, int, int);
static int	fd_last_used(struct filedesc *, int, int);
static void	fdgrowtable(struct filedesc *, int);
static void	fdunused(struct filedesc *fdp, int fd);
static void	fdused(struct filedesc *fdp, int fd);
static int	fill_vnode_info(struct vnode *vp, struct kinfo_file *kif);
static int	fill_socket_info(struct socket *so, struct kinfo_file *kif);
static int	fill_pts_info(struct tty *tp, struct kinfo_file *kif);
static int	fill_pipe_info(struct pipe *pi, struct kinfo_file *kif);
static int	fill_procdesc_info(struct procdesc *pdp,
    struct kinfo_file *kif);
static int	fill_shm_info(struct file *fp, struct kinfo_file *kif);

/*
 * A process is initially started out with NDFILE descriptors stored within
 * this structure, selected to be enough for typical applications based on
 * the historical limit of 20 open files (and the usage of descriptors by
 * shells).  If these descriptors are exhausted, a larger descriptor table
 * may be allocated, up to a process' resource limit; the internal arrays
 * are then unused.
 */
#define NDFILE		20
#define NDSLOTSIZE	sizeof(NDSLOTTYPE)
#define	NDENTRIES	(NDSLOTSIZE * __CHAR_BIT)
#define NDSLOT(x)	((x) / NDENTRIES)
#define NDBIT(x)	((NDSLOTTYPE)1 << ((x) % NDENTRIES))
#define	NDSLOTS(x)	(((x) + NDENTRIES - 1) / NDENTRIES)

/*
 * Storage required per open file descriptor.
 */
#define OFILESIZE (sizeof(struct file *) + sizeof(char))

/*
 * Storage to hold unused ofiles that need to be reclaimed.
 */
struct freetable {
	struct file	**ft_table;
	SLIST_ENTRY(freetable) ft_next;
};

/*
 * Basic allocation of descriptors:
 * one of the above, plus arrays for NDFILE descriptors.
 */
struct filedesc0 {
	struct	filedesc fd_fd;
	/*
	 * ofiles which need to be reclaimed on free.
	 */
	SLIST_HEAD(,freetable) fd_free;
	/*
	 * These arrays are used when the number of open files is
	 * <= NDFILE, and are then pointed to by the pointers above.
	 */
	struct	file *fd_dfiles[NDFILE];
	char	fd_dfileflags[NDFILE];
	NDSLOTTYPE fd_dmap[NDSLOTS(NDFILE)];
};

/*
 * Descriptor management.
 */
volatile int openfiles;			/* actual number of open files */
struct mtx sigio_lock;		/* mtx to protect pointers to sigio */
void	(*mq_fdclose)(struct thread *td, int fd, struct file *fp);

/* A mutex to protect the association between a proc and filedesc. */
static struct mtx	fdesc_mtx;

/*
 * Find the first zero bit in the given bitmap, starting at low and not
 * exceeding size - 1.
 */
static int
fd_first_free(struct filedesc *fdp, int low, int size)
{
	NDSLOTTYPE *map = fdp->fd_map;
	NDSLOTTYPE mask;
	int off, maxoff;

	if (low >= size)
		return (low);

	off = NDSLOT(low);
	if (low % NDENTRIES) {
		mask = ~(~(NDSLOTTYPE)0 >> (NDENTRIES - (low % NDENTRIES)));
		if ((mask &= ~map[off]) != 0UL)
			return (off * NDENTRIES + ffsl(mask) - 1);
		++off;
	}
	for (maxoff = NDSLOTS(size); off < maxoff; ++off)
		if (map[off] != ~0UL)
			return (off * NDENTRIES + ffsl(~map[off]) - 1);
	return (size);
}

/*
 * Find the highest non-zero bit in the given bitmap, starting at low and
 * not exceeding size - 1.
 */
static int
fd_last_used(struct filedesc *fdp, int low, int size)
{
	NDSLOTTYPE *map = fdp->fd_map;
	NDSLOTTYPE mask;
	int off, minoff;

	if (low >= size)
		return (-1);

	off = NDSLOT(size);
	if (size % NDENTRIES) {
		mask = ~(~(NDSLOTTYPE)0 << (size % NDENTRIES));
		if ((mask &= map[off]) != 0)
			return (off * NDENTRIES + flsl(mask) - 1);
		--off;
	}
	for (minoff = NDSLOT(low); off >= minoff; --off)
		if (map[off] != 0)
			return (off * NDENTRIES + flsl(map[off]) - 1);
	return (low - 1);
}

static int
fdisused(struct filedesc *fdp, int fd)
{
        KASSERT(fd >= 0 && fd < fdp->fd_nfiles,
            ("file descriptor %d out of range (0, %d)", fd, fdp->fd_nfiles));
	return ((fdp->fd_map[NDSLOT(fd)] & NDBIT(fd)) != 0);
}

/*
 * Mark a file descriptor as used.
 */
static void
fdused(struct filedesc *fdp, int fd)
{

	FILEDESC_XLOCK_ASSERT(fdp);
	KASSERT(!fdisused(fdp, fd),
	    ("fd already used"));

	fdp->fd_map[NDSLOT(fd)] |= NDBIT(fd);
	if (fd > fdp->fd_lastfile)
		fdp->fd_lastfile = fd;
	if (fd == fdp->fd_freefile)
		fdp->fd_freefile = fd_first_free(fdp, fd, fdp->fd_nfiles);
}

/*
 * Mark a file descriptor as unused.
 */
static void
fdunused(struct filedesc *fdp, int fd)
{

	FILEDESC_XLOCK_ASSERT(fdp);
	KASSERT(fdisused(fdp, fd),
	    ("fd is already unused"));
	KASSERT(fdp->fd_ofiles[fd] == NULL,
	    ("fd is still in use"));

	fdp->fd_map[NDSLOT(fd)] &= ~NDBIT(fd);
	if (fd < fdp->fd_freefile)
		fdp->fd_freefile = fd;
	if (fd == fdp->fd_lastfile)
		fdp->fd_lastfile = fd_last_used(fdp, 0, fd);
}

/*
 * System calls on descriptors.
 */
#ifndef _SYS_SYSPROTO_H_
struct getdtablesize_args {
	int	dummy;
};
#endif
/* ARGSUSED */
int
sys_getdtablesize(struct thread *td, struct getdtablesize_args *uap)
{
	struct proc *p = td->td_proc;
	uint64_t lim;

	PROC_LOCK(p);
	td->td_retval[0] =
	    min((int)lim_cur(p, RLIMIT_NOFILE), maxfilesperproc);
	lim = racct_get_limit(td->td_proc, RACCT_NOFILE);
	PROC_UNLOCK(p);
	if (lim < td->td_retval[0])
		td->td_retval[0] = lim;
	return (0);
}

/*
 * Duplicate a file descriptor to a particular value.
 *
 * Note: keep in mind that a potential race condition exists when closing
 * descriptors from a shared descriptor table (via rfork).
 */
#ifndef _SYS_SYSPROTO_H_
struct dup2_args {
	u_int	from;
	u_int	to;
};
#endif
/* ARGSUSED */
int
sys_dup2(struct thread *td, struct dup2_args *uap)
{

	return (do_dup(td, DUP_FIXED, (int)uap->from, (int)uap->to,
		    td->td_retval));
}

/*
 * Duplicate a file descriptor.
 */
#ifndef _SYS_SYSPROTO_H_
struct dup_args {
	u_int	fd;
};
#endif
/* ARGSUSED */
int
sys_dup(struct thread *td, struct dup_args *uap)
{

	return (do_dup(td, 0, (int)uap->fd, 0, td->td_retval));
}

/*
 * The file control system call.
 */
#ifndef _SYS_SYSPROTO_H_
struct fcntl_args {
	int	fd;
	int	cmd;
	long	arg;
};
#endif
/* ARGSUSED */
int
sys_fcntl(struct thread *td, struct fcntl_args *uap)
{
	struct flock fl;
	struct oflock ofl;
	intptr_t arg;
	int error;
	int cmd;

	error = 0;
	cmd = uap->cmd;
	switch (uap->cmd) {
	case F_OGETLK:
	case F_OSETLK:
	case F_OSETLKW:
		/*
		 * Convert old flock structure to new.
		 */
		error = copyin((void *)(intptr_t)uap->arg, &ofl, sizeof(ofl));
		fl.l_start = ofl.l_start;
		fl.l_len = ofl.l_len;
		fl.l_pid = ofl.l_pid;
		fl.l_type = ofl.l_type;
		fl.l_whence = ofl.l_whence;
		fl.l_sysid = 0;

		switch (uap->cmd) {
		case F_OGETLK:
		    cmd = F_GETLK;
		    break;
		case F_OSETLK:
		    cmd = F_SETLK;
		    break;
		case F_OSETLKW:
		    cmd = F_SETLKW;
		    break;
		}
		arg = (intptr_t)&fl;
		break;
        case F_GETLK:
        case F_SETLK:
        case F_SETLKW:
	case F_SETLK_REMOTE:
                error = copyin((void *)(intptr_t)uap->arg, &fl, sizeof(fl));
                arg = (intptr_t)&fl;
                break;
	default:
		arg = uap->arg;
		break;
	}
	if (error)
		return (error);
	error = kern_fcntl(td, uap->fd, cmd, arg);
	if (error)
		return (error);
	if (uap->cmd == F_OGETLK) {
		ofl.l_start = fl.l_start;
		ofl.l_len = fl.l_len;
		ofl.l_pid = fl.l_pid;
		ofl.l_type = fl.l_type;
		ofl.l_whence = fl.l_whence;
		error = copyout(&ofl, (void *)(intptr_t)uap->arg, sizeof(ofl));
	} else if (uap->cmd == F_GETLK) {
		error = copyout(&fl, (void *)(intptr_t)uap->arg, sizeof(fl));
	}
	return (error);
}

static inline struct file *
fdtofp(int fd, struct filedesc *fdp)
{
	struct file *fp;

	FILEDESC_LOCK_ASSERT(fdp);
	if ((unsigned)fd >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[fd]) == NULL)
		return (NULL);
	return (fp);
}

static inline int
fdunwrap(int fd, cap_rights_t rights, struct filedesc *fdp, struct file **fpp)
{

	*fpp = fdtofp(fd, fdp);
	if (*fpp == NULL)
		return (EBADF);

#ifdef CAPABILITIES
	if ((*fpp)->f_type == DTYPE_CAPABILITY) {
		int err = cap_funwrap(*fpp, rights, fpp);
		if (err != 0) {
			*fpp = NULL;
			return (err);
		}
	}
#endif /* CAPABILITIES */
	return (0);
}

int
kern_fcntl(struct thread *td, int fd, int cmd, intptr_t arg)
{
	struct filedesc *fdp;
	struct flock *flp;
	struct file *fp;
	struct proc *p;
	char *pop;
	struct vnode *vp;
	int error, flg, tmp;
	int vfslocked;
	u_int old, new;
	uint64_t bsize;
	off_t foffset;

	vfslocked = 0;
	error = 0;
	flg = F_POSIX;
	p = td->td_proc;
	fdp = p->p_fd;

	switch (cmd) {
	case F_DUPFD:
		tmp = arg;
		error = do_dup(td, DUP_FCNTL, fd, tmp, td->td_retval);
		break;

	case F_DUPFD_CLOEXEC:
		tmp = arg;
		error = do_dup(td, DUP_FCNTL | DUP_CLOEXEC, fd, tmp,
		    td->td_retval);
		break;

	case F_DUP2FD:
		tmp = arg;
		error = do_dup(td, DUP_FIXED, fd, tmp, td->td_retval);
		break;

	case F_GETFD:
		FILEDESC_SLOCK(fdp);
		if ((fp = fdtofp(fd, fdp)) == NULL) {
			FILEDESC_SUNLOCK(fdp);
			error = EBADF;
			break;
		}
		pop = &fdp->fd_ofileflags[fd];
		td->td_retval[0] = (*pop & UF_EXCLOSE) ? FD_CLOEXEC : 0;
		FILEDESC_SUNLOCK(fdp);
		break;

	case F_SETFD:
		FILEDESC_XLOCK(fdp);
		if ((fp = fdtofp(fd, fdp)) == NULL) {
			FILEDESC_XUNLOCK(fdp);
			error = EBADF;
			break;
		}
		pop = &fdp->fd_ofileflags[fd];
		*pop = (*pop &~ UF_EXCLOSE) |
		    (arg & FD_CLOEXEC ? UF_EXCLOSE : 0);
		FILEDESC_XUNLOCK(fdp);
		break;

	case F_GETFL:
		FILEDESC_SLOCK(fdp);
		error = fdunwrap(fd, CAP_FCNTL, fdp, &fp);
		if (error != 0) {
			FILEDESC_SUNLOCK(fdp);
			break;
		}
		td->td_retval[0] = OFLAGS(fp->f_flag);
		FILEDESC_SUNLOCK(fdp);
		break;

	case F_SETFL:
		FILEDESC_SLOCK(fdp);
		error = fdunwrap(fd, CAP_FCNTL, fdp, &fp);
		if (error != 0) {
			FILEDESC_SUNLOCK(fdp);
			break;
		}
		fhold(fp);
		FILEDESC_SUNLOCK(fdp);
		do {
			tmp = flg = fp->f_flag;
			tmp &= ~FCNTLFLAGS;
			tmp |= FFLAGS(arg & ~O_ACCMODE) & FCNTLFLAGS;
		} while(atomic_cmpset_int(&fp->f_flag, flg, tmp) == 0);
		tmp = fp->f_flag & FNONBLOCK;
		error = fo_ioctl(fp, FIONBIO, &tmp, td->td_ucred, td);
		if (error) {
			fdrop(fp, td);
			break;
		}
		tmp = fp->f_flag & FASYNC;
		error = fo_ioctl(fp, FIOASYNC, &tmp, td->td_ucred, td);
		if (error == 0) {
			fdrop(fp, td);
			break;
		}
		atomic_clear_int(&fp->f_flag, FNONBLOCK);
		tmp = 0;
		(void)fo_ioctl(fp, FIONBIO, &tmp, td->td_ucred, td);
		fdrop(fp, td);
		break;

	case F_GETOWN:
		FILEDESC_SLOCK(fdp);
		error = fdunwrap(fd, CAP_FCNTL, fdp, &fp);
		if (error != 0) {
			FILEDESC_SUNLOCK(fdp);
			break;
		}
		fhold(fp);
		FILEDESC_SUNLOCK(fdp);
		error = fo_ioctl(fp, FIOGETOWN, &tmp, td->td_ucred, td);
		if (error == 0)
			td->td_retval[0] = tmp;
		fdrop(fp, td);
		break;

	case F_SETOWN:
		FILEDESC_SLOCK(fdp);
		error = fdunwrap(fd, CAP_FCNTL, fdp, &fp);
		if (error != 0) {
			FILEDESC_SUNLOCK(fdp);
			break;
		}
		fhold(fp);
		FILEDESC_SUNLOCK(fdp);
		tmp = arg;
		error = fo_ioctl(fp, FIOSETOWN, &tmp, td->td_ucred, td);
		fdrop(fp, td);
		break;

	case F_SETLK_REMOTE:
		error = priv_check(td, PRIV_NFS_LOCKD);
		if (error)
			return (error);
		flg = F_REMOTE;
		goto do_setlk;

	case F_SETLKW:
		flg |= F_WAIT;
		/* FALLTHROUGH F_SETLK */

	case F_SETLK:
	do_setlk:
		FILEDESC_SLOCK(fdp);
		error = fdunwrap(fd, CAP_FLOCK, fdp, &fp);
		if (error != 0) {
			FILEDESC_SUNLOCK(fdp);
			break;
		}
		if (fp->f_type != DTYPE_VNODE) {
			FILEDESC_SUNLOCK(fdp);
			error = EBADF;
			break;
		}
		flp = (struct flock *)arg;
		if (flp->l_whence == SEEK_CUR) {
			foffset = foffset_get(fp);
			if (foffset < 0 ||
			    (flp->l_start > 0 &&
			     foffset > OFF_MAX - flp->l_start)) {
				FILEDESC_SUNLOCK(fdp);
				error = EOVERFLOW;
				break;
			}
			flp->l_start += foffset;
		}

		/*
		 * VOP_ADVLOCK() may block.
		 */
		fhold(fp);
		FILEDESC_SUNLOCK(fdp);
		vp = fp->f_vnode;
		vfslocked = VFS_LOCK_GIANT(vp->v_mount);
		switch (flp->l_type) {
		case F_RDLCK:
			if ((fp->f_flag & FREAD) == 0) {
				error = EBADF;
				break;
			}
			PROC_LOCK(p->p_leader);
			p->p_leader->p_flag |= P_ADVLOCK;
			PROC_UNLOCK(p->p_leader);
			error = VOP_ADVLOCK(vp, (caddr_t)p->p_leader, F_SETLK,
			    flp, flg);
			break;
		case F_WRLCK:
			if ((fp->f_flag & FWRITE) == 0) {
				error = EBADF;
				break;
			}
			PROC_LOCK(p->p_leader);
			p->p_leader->p_flag |= P_ADVLOCK;
			PROC_UNLOCK(p->p_leader);
			error = VOP_ADVLOCK(vp, (caddr_t)p->p_leader, F_SETLK,
			    flp, flg);
			break;
		case F_UNLCK:
			error = VOP_ADVLOCK(vp, (caddr_t)p->p_leader, F_UNLCK,
			    flp, flg);
			break;
		case F_UNLCKSYS:
			/*
			 * Temporary api for testing remote lock
			 * infrastructure.
			 */
			if (flg != F_REMOTE) {
				error = EINVAL;
				break;
			}
			error = VOP_ADVLOCK(vp, (caddr_t)p->p_leader,
			    F_UNLCKSYS, flp, flg);
			break;
		default:
			error = EINVAL;
			break;
		}
		VFS_UNLOCK_GIANT(vfslocked);
		vfslocked = 0;
		/* Check for race with close */
		FILEDESC_SLOCK(fdp);
		if ((unsigned) fd >= fdp->fd_nfiles ||
		    fp != fdp->fd_ofiles[fd]) {
			FILEDESC_SUNLOCK(fdp);
			flp->l_whence = SEEK_SET;
			flp->l_start = 0;
			flp->l_len = 0;
			flp->l_type = F_UNLCK;
			vfslocked = VFS_LOCK_GIANT(vp->v_mount);
			(void) VOP_ADVLOCK(vp, (caddr_t)p->p_leader,
					   F_UNLCK, flp, F_POSIX);
			VFS_UNLOCK_GIANT(vfslocked);
			vfslocked = 0;
		} else
			FILEDESC_SUNLOCK(fdp);
		fdrop(fp, td);
		break;

	case F_GETLK:
		FILEDESC_SLOCK(fdp);
		error = fdunwrap(fd, CAP_FLOCK, fdp, &fp);
		if (error != 0) {
			FILEDESC_SUNLOCK(fdp);
			break;
		}
		if (fp->f_type != DTYPE_VNODE) {
			FILEDESC_SUNLOCK(fdp);
			error = EBADF;
			break;
		}
		flp = (struct flock *)arg;
		if (flp->l_type != F_RDLCK && flp->l_type != F_WRLCK &&
		    flp->l_type != F_UNLCK) {
			FILEDESC_SUNLOCK(fdp);
			error = EINVAL;
			break;
		}
		if (flp->l_whence == SEEK_CUR) {
			foffset = foffset_get(fp);
			if ((flp->l_start > 0 &&
			    foffset > OFF_MAX - flp->l_start) ||
			    (flp->l_start < 0 &&
			     foffset < OFF_MIN - flp->l_start)) {
				FILEDESC_SUNLOCK(fdp);
				error = EOVERFLOW;
				break;
			}
			flp->l_start += foffset;
		}
		/*
		 * VOP_ADVLOCK() may block.
		 */
		fhold(fp);
		FILEDESC_SUNLOCK(fdp);
		vp = fp->f_vnode;
		vfslocked = VFS_LOCK_GIANT(vp->v_mount);
		error = VOP_ADVLOCK(vp, (caddr_t)p->p_leader, F_GETLK, flp,
		    F_POSIX);
		VFS_UNLOCK_GIANT(vfslocked);
		vfslocked = 0;
		fdrop(fp, td);
		break;

	case F_RDAHEAD:
		arg = arg ? 128 * 1024: 0;
		/* FALLTHROUGH */
	case F_READAHEAD:
		FILEDESC_SLOCK(fdp);
		if ((fp = fdtofp(fd, fdp)) == NULL) {
			FILEDESC_SUNLOCK(fdp);
			error = EBADF;
			break;
		}
		if (fp->f_type != DTYPE_VNODE) {
			FILEDESC_SUNLOCK(fdp);
			error = EBADF;
			break;
		}
		fhold(fp);
		FILEDESC_SUNLOCK(fdp);
		if (arg != 0) {
			vp = fp->f_vnode;
			vfslocked = VFS_LOCK_GIANT(vp->v_mount);
			error = vn_lock(vp, LK_SHARED);
			if (error != 0)
				goto readahead_vnlock_fail;
			bsize = fp->f_vnode->v_mount->mnt_stat.f_iosize;
			VOP_UNLOCK(vp, 0);
			fp->f_seqcount = (arg + bsize - 1) / bsize;
			do {
				new = old = fp->f_flag;
				new |= FRDAHEAD;
			} while (!atomic_cmpset_rel_int(&fp->f_flag, old, new));
readahead_vnlock_fail:
			VFS_UNLOCK_GIANT(vfslocked);
			vfslocked = 0;
		} else {
			do {
				new = old = fp->f_flag;
				new &= ~FRDAHEAD;
			} while (!atomic_cmpset_rel_int(&fp->f_flag, old, new));
		}
		fdrop(fp, td);
		break;

	default:
		error = EINVAL;
		break;
	}
	VFS_UNLOCK_GIANT(vfslocked);
	return (error);
}

/*
 * Common code for dup, dup2, fcntl(F_DUPFD) and fcntl(F_DUP2FD).
 */
static int
do_dup(struct thread *td, int flags, int old, int new,
    register_t *retval)
{
	struct filedesc *fdp;
	struct proc *p;
	struct file *fp;
	struct file *delfp;
	int error, holdleaders, maxfd;

	p = td->td_proc;
	fdp = p->p_fd;

	/*
	 * Verify we have a valid descriptor to dup from and possibly to
	 * dup to. Unlike dup() and dup2(), fcntl()'s F_DUPFD should
	 * return EINVAL when the new descriptor is out of bounds.
	 */
	if (old < 0)
		return (EBADF);
	if (new < 0)
		return (flags & DUP_FCNTL ? EINVAL : EBADF);
	PROC_LOCK(p);
	maxfd = min((int)lim_cur(p, RLIMIT_NOFILE), maxfilesperproc);
	PROC_UNLOCK(p);
	if (new >= maxfd)
		return (flags & DUP_FCNTL ? EINVAL : EBADF);

	FILEDESC_XLOCK(fdp);
	if (old >= fdp->fd_nfiles || fdp->fd_ofiles[old] == NULL) {
		FILEDESC_XUNLOCK(fdp);
		return (EBADF);
	}
	if (flags & DUP_FIXED && old == new) {
		*retval = new;
		FILEDESC_XUNLOCK(fdp);
		return (0);
	}
	fp = fdp->fd_ofiles[old];
	fhold(fp);

	/*
	 * If the caller specified a file descriptor, make sure the file
	 * table is large enough to hold it, and grab it.  Otherwise, just
	 * allocate a new descriptor the usual way.  Since the filedesc
	 * lock may be temporarily dropped in the process, we have to look
	 * out for a race.
	 */
	if (flags & DUP_FIXED) {
		if (new >= fdp->fd_nfiles) {
			/*
			 * The resource limits are here instead of e.g.
			 * fdalloc(), because the file descriptor table may be
			 * shared between processes, so we can't really use
			 * racct_add()/racct_sub().  Instead of counting the
			 * number of actually allocated descriptors, just put
			 * the limit on the size of the file descriptor table.
			 */
#ifdef RACCT
			PROC_LOCK(p);
			error = racct_set(p, RACCT_NOFILE, new + 1);
			PROC_UNLOCK(p);
			if (error != 0) {
				FILEDESC_XUNLOCK(fdp);
				fdrop(fp, td);
				return (EMFILE);
			}
#endif
			fdgrowtable(fdp, new + 1);
		}
		if (fdp->fd_ofiles[new] == NULL)
			fdused(fdp, new);
	} else {
		if ((error = fdalloc(td, new, &new)) != 0) {
			FILEDESC_XUNLOCK(fdp);
			fdrop(fp, td);
			return (error);
		}
	}

	/*
	 * If the old file changed out from under us then treat it as a
	 * bad file descriptor.  Userland should do its own locking to
	 * avoid this case.
	 */
	if (fdp->fd_ofiles[old] != fp) {
		/* we've allocated a descriptor which we won't use */
		if (fdp->fd_ofiles[new] == NULL)
			fdunused(fdp, new);
		FILEDESC_XUNLOCK(fdp);
		fdrop(fp, td);
		return (EBADF);
	}
	KASSERT(old != new,
	    ("new fd is same as old"));

	/*
	 * Save info on the descriptor being overwritten.  We cannot close
	 * it without introducing an ownership race for the slot, since we
	 * need to drop the filedesc lock to call closef().
	 *
	 * XXX this duplicates parts of close().
	 */
	delfp = fdp->fd_ofiles[new];
	holdleaders = 0;
	if (delfp != NULL) {
		if (td->td_proc->p_fdtol != NULL) {
			/*
			 * Ask fdfree() to sleep to ensure that all relevant
			 * process leaders can be traversed in closef().
			 */
			fdp->fd_holdleaderscount++;
			holdleaders = 1;
		}
	}

	/*
	 * Duplicate the source descriptor
	 */
	fdp->fd_ofiles[new] = fp;
	if ((flags & DUP_CLOEXEC) != 0)
		fdp->fd_ofileflags[new] = fdp->fd_ofileflags[old] | UF_EXCLOSE;
	else
		fdp->fd_ofileflags[new] = fdp->fd_ofileflags[old] & ~UF_EXCLOSE;
	if (new > fdp->fd_lastfile)
		fdp->fd_lastfile = new;
	*retval = new;

	/*
	 * If we dup'd over a valid file, we now own the reference to it
	 * and must dispose of it using closef() semantics (as if a
	 * close() were performed on it).
	 *
	 * XXX this duplicates parts of close().
	 */
	if (delfp != NULL) {
		knote_fdclose(td, new);
		if (delfp->f_type == DTYPE_MQUEUE)
			mq_fdclose(td, new, delfp);
		FILEDESC_XUNLOCK(fdp);
		(void) closef(delfp, td);
		if (holdleaders) {
			FILEDESC_XLOCK(fdp);
			fdp->fd_holdleaderscount--;
			if (fdp->fd_holdleaderscount == 0 &&
			    fdp->fd_holdleaderswakeup != 0) {
				fdp->fd_holdleaderswakeup = 0;
				wakeup(&fdp->fd_holdleaderscount);
			}
			FILEDESC_XUNLOCK(fdp);
		}
	} else {
		FILEDESC_XUNLOCK(fdp);
	}
	return (0);
}

/*
 * If sigio is on the list associated with a process or process group,
 * disable signalling from the device, remove sigio from the list and
 * free sigio.
 */
void
funsetown(struct sigio **sigiop)
{
	struct sigio *sigio;

	SIGIO_LOCK();
	sigio = *sigiop;
	if (sigio == NULL) {
		SIGIO_UNLOCK();
		return;
	}
	*(sigio->sio_myref) = NULL;
	if ((sigio)->sio_pgid < 0) {
		struct pgrp *pg = (sigio)->sio_pgrp;
		PGRP_LOCK(pg);
		SLIST_REMOVE(&sigio->sio_pgrp->pg_sigiolst, sigio,
			     sigio, sio_pgsigio);
		PGRP_UNLOCK(pg);
	} else {
		struct proc *p = (sigio)->sio_proc;
		PROC_LOCK(p);
		SLIST_REMOVE(&sigio->sio_proc->p_sigiolst, sigio,
			     sigio, sio_pgsigio);
		PROC_UNLOCK(p);
	}
	SIGIO_UNLOCK();
	crfree(sigio->sio_ucred);
	free(sigio, M_SIGIO);
}

/*
 * Free a list of sigio structures.
 * We only need to lock the SIGIO_LOCK because we have made ourselves
 * inaccessible to callers of fsetown and therefore do not need to lock
 * the proc or pgrp struct for the list manipulation.
 */
void
funsetownlst(struct sigiolst *sigiolst)
{
	struct proc *p;
	struct pgrp *pg;
	struct sigio *sigio;

	sigio = SLIST_FIRST(sigiolst);
	if (sigio == NULL)
		return;
	p = NULL;
	pg = NULL;

	/*
	 * Every entry of the list should belong
	 * to a single proc or pgrp.
	 */
	if (sigio->sio_pgid < 0) {
		pg = sigio->sio_pgrp;
		PGRP_LOCK_ASSERT(pg, MA_NOTOWNED);
	} else /* if (sigio->sio_pgid > 0) */ {
		p = sigio->sio_proc;
		PROC_LOCK_ASSERT(p, MA_NOTOWNED);
	}

	SIGIO_LOCK();
	while ((sigio = SLIST_FIRST(sigiolst)) != NULL) {
		*(sigio->sio_myref) = NULL;
		if (pg != NULL) {
			KASSERT(sigio->sio_pgid < 0,
			    ("Proc sigio in pgrp sigio list"));
			KASSERT(sigio->sio_pgrp == pg,
			    ("Bogus pgrp in sigio list"));
			PGRP_LOCK(pg);
			SLIST_REMOVE(&pg->pg_sigiolst, sigio, sigio,
			    sio_pgsigio);
			PGRP_UNLOCK(pg);
		} else /* if (p != NULL) */ {
			KASSERT(sigio->sio_pgid > 0,
			    ("Pgrp sigio in proc sigio list"));
			KASSERT(sigio->sio_proc == p,
			    ("Bogus proc in sigio list"));
			PROC_LOCK(p);
			SLIST_REMOVE(&p->p_sigiolst, sigio, sigio,
			    sio_pgsigio);
			PROC_UNLOCK(p);
		}
		SIGIO_UNLOCK();
		crfree(sigio->sio_ucred);
		free(sigio, M_SIGIO);
		SIGIO_LOCK();
	}
	SIGIO_UNLOCK();
}

/*
 * This is common code for FIOSETOWN ioctl called by fcntl(fd, F_SETOWN, arg).
 *
 * After permission checking, add a sigio structure to the sigio list for
 * the process or process group.
 */
int
fsetown(pid_t pgid, struct sigio **sigiop)
{
	struct proc *proc;
	struct pgrp *pgrp;
	struct sigio *sigio;
	int ret;

	if (pgid == 0) {
		funsetown(sigiop);
		return (0);
	}

	ret = 0;

	/* Allocate and fill in the new sigio out of locks. */
	sigio = malloc(sizeof(struct sigio), M_SIGIO, M_WAITOK);
	sigio->sio_pgid = pgid;
	sigio->sio_ucred = crhold(curthread->td_ucred);
	sigio->sio_myref = sigiop;

	sx_slock(&proctree_lock);
	if (pgid > 0) {
		proc = pfind(pgid);
		if (proc == NULL) {
			ret = ESRCH;
			goto fail;
		}

		/*
		 * Policy - Don't allow a process to FSETOWN a process
		 * in another session.
		 *
		 * Remove this test to allow maximum flexibility or
		 * restrict FSETOWN to the current process or process
		 * group for maximum safety.
		 */
		PROC_UNLOCK(proc);
		if (proc->p_session != curthread->td_proc->p_session) {
			ret = EPERM;
			goto fail;
		}

		pgrp = NULL;
	} else /* if (pgid < 0) */ {
		pgrp = pgfind(-pgid);
		if (pgrp == NULL) {
			ret = ESRCH;
			goto fail;
		}
		PGRP_UNLOCK(pgrp);

		/*
		 * Policy - Don't allow a process to FSETOWN a process
		 * in another session.
		 *
		 * Remove this test to allow maximum flexibility or
		 * restrict FSETOWN to the current process or process
		 * group for maximum safety.
		 */
		if (pgrp->pg_session != curthread->td_proc->p_session) {
			ret = EPERM;
			goto fail;
		}

		proc = NULL;
	}
	funsetown(sigiop);
	if (pgid > 0) {
		PROC_LOCK(proc);
		/*
		 * Since funsetownlst() is called without the proctree
		 * locked, we need to check for P_WEXIT.
		 * XXX: is ESRCH correct?
		 */
		if ((proc->p_flag & P_WEXIT) != 0) {
			PROC_UNLOCK(proc);
			ret = ESRCH;
			goto fail;
		}
		SLIST_INSERT_HEAD(&proc->p_sigiolst, sigio, sio_pgsigio);
		sigio->sio_proc = proc;
		PROC_UNLOCK(proc);
	} else {
		PGRP_LOCK(pgrp);
		SLIST_INSERT_HEAD(&pgrp->pg_sigiolst, sigio, sio_pgsigio);
		sigio->sio_pgrp = pgrp;
		PGRP_UNLOCK(pgrp);
	}
	sx_sunlock(&proctree_lock);
	SIGIO_LOCK();
	*sigiop = sigio;
	SIGIO_UNLOCK();
	return (0);

fail:
	sx_sunlock(&proctree_lock);
	crfree(sigio->sio_ucred);
	free(sigio, M_SIGIO);
	return (ret);
}

/*
 * This is common code for FIOGETOWN ioctl called by fcntl(fd, F_GETOWN, arg).
 */
pid_t
fgetown(sigiop)
	struct sigio **sigiop;
{
	pid_t pgid;

	SIGIO_LOCK();
	pgid = (*sigiop != NULL) ? (*sigiop)->sio_pgid : 0;
	SIGIO_UNLOCK();
	return (pgid);
}

/*
 * Close a file descriptor.
 */
#ifndef _SYS_SYSPROTO_H_
struct close_args {
	int     fd;
};
#endif
/* ARGSUSED */
int
sys_close(td, uap)
	struct thread *td;
	struct close_args *uap;
{

	return (kern_close(td, uap->fd));
}

int
kern_close(td, fd)
	struct thread *td;
	int fd;
{
	struct filedesc *fdp;
	struct file *fp, *fp_object;
	int error;
	int holdleaders;

	error = 0;
	holdleaders = 0;
	fdp = td->td_proc->p_fd;

	AUDIT_SYSCLOSE(td, fd);

	FILEDESC_XLOCK(fdp);
	if ((unsigned)fd >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[fd]) == NULL) {
		FILEDESC_XUNLOCK(fdp);
		return (EBADF);
	}
	fdp->fd_ofiles[fd] = NULL;
	fdp->fd_ofileflags[fd] = 0;
	fdunused(fdp, fd);
	if (td->td_proc->p_fdtol != NULL) {
		/*
		 * Ask fdfree() to sleep to ensure that all relevant
		 * process leaders can be traversed in closef().
		 */
		fdp->fd_holdleaderscount++;
		holdleaders = 1;
	}

	/*
	 * We now hold the fp reference that used to be owned by the
	 * descriptor array.  We have to unlock the FILEDESC *AFTER*
	 * knote_fdclose to prevent a race of the fd getting opened, a knote
	 * added, and deleteing a knote for the new fd.
	 */
	knote_fdclose(td, fd);

	/*
	 * When we're closing an fd with a capability, we need to notify
	 * mqueue if the underlying object is of type mqueue.
	 */
	(void)cap_funwrap(fp, 0, &fp_object);
	if (fp_object->f_type == DTYPE_MQUEUE)
		mq_fdclose(td, fd, fp_object);
	FILEDESC_XUNLOCK(fdp);

	error = closef(fp, td);
	if (holdleaders) {
		FILEDESC_XLOCK(fdp);
		fdp->fd_holdleaderscount--;
		if (fdp->fd_holdleaderscount == 0 &&
		    fdp->fd_holdleaderswakeup != 0) {
			fdp->fd_holdleaderswakeup = 0;
			wakeup(&fdp->fd_holdleaderscount);
		}
		FILEDESC_XUNLOCK(fdp);
	}
	return (error);
}

/*
 * Close open file descriptors.
 */
#ifndef _SYS_SYSPROTO_H_
struct closefrom_args {
	int	lowfd;
};
#endif
/* ARGSUSED */
int
sys_closefrom(struct thread *td, struct closefrom_args *uap)
{
	struct filedesc *fdp;
	int fd;

	fdp = td->td_proc->p_fd;
	AUDIT_ARG_FD(uap->lowfd);

	/*
	 * Treat negative starting file descriptor values identical to
	 * closefrom(0) which closes all files.
	 */
	if (uap->lowfd < 0)
		uap->lowfd = 0;
	FILEDESC_SLOCK(fdp);
	for (fd = uap->lowfd; fd < fdp->fd_nfiles; fd++) {
		if (fdp->fd_ofiles[fd] != NULL) {
			FILEDESC_SUNLOCK(fdp);
			(void)kern_close(td, fd);
			FILEDESC_SLOCK(fdp);
		}
	}
	FILEDESC_SUNLOCK(fdp);
	return (0);
}

#if defined(COMPAT_43)
/*
 * Return status information about a file descriptor.
 */
#ifndef _SYS_SYSPROTO_H_
struct ofstat_args {
	int	fd;
	struct	ostat *sb;
};
#endif
/* ARGSUSED */
int
ofstat(struct thread *td, struct ofstat_args *uap)
{
	struct ostat oub;
	struct stat ub;
	int error;

	error = kern_fstat(td, uap->fd, &ub);
	if (error == 0) {
		cvtstat(&ub, &oub);
		error = copyout(&oub, uap->sb, sizeof(oub));
	}
	return (error);
}
#endif /* COMPAT_43 */

/*
 * Return status information about a file descriptor.
 */
#ifndef _SYS_SYSPROTO_H_
struct fstat_args {
	int	fd;
	struct	stat *sb;
};
#endif
/* ARGSUSED */
int
sys_fstat(struct thread *td, struct fstat_args *uap)
{
	struct stat ub;
	int error;

	error = kern_fstat(td, uap->fd, &ub);
	if (error == 0)
		error = copyout(&ub, uap->sb, sizeof(ub));
	return (error);
}

int
kern_fstat(struct thread *td, int fd, struct stat *sbp)
{
	struct file *fp;
	int error;

	AUDIT_ARG_FD(fd);

	if ((error = fget(td, fd, CAP_FSTAT, &fp)) != 0)
		return (error);

	AUDIT_ARG_FILE(td->td_proc, fp);

	error = fo_stat(fp, sbp, td->td_ucred, td);
	fdrop(fp, td);
#ifdef KTRACE
	if (error == 0 && KTRPOINT(td, KTR_STRUCT))
		ktrstat(sbp);
#endif
	return (error);
}

/*
 * Return status information about a file descriptor.
 */
#ifndef _SYS_SYSPROTO_H_
struct nfstat_args {
	int	fd;
	struct	nstat *sb;
};
#endif
/* ARGSUSED */
int
sys_nfstat(struct thread *td, struct nfstat_args *uap)
{
	struct nstat nub;
	struct stat ub;
	int error;

	error = kern_fstat(td, uap->fd, &ub);
	if (error == 0) {
		cvtnstat(&ub, &nub);
		error = copyout(&nub, uap->sb, sizeof(nub));
	}
	return (error);
}

/*
 * Return pathconf information about a file descriptor.
 */
#ifndef _SYS_SYSPROTO_H_
struct fpathconf_args {
	int	fd;
	int	name;
};
#endif
/* ARGSUSED */
int
sys_fpathconf(struct thread *td, struct fpathconf_args *uap)
{
	struct file *fp;
	struct vnode *vp;
	int error;

	if ((error = fget(td, uap->fd, CAP_FPATHCONF, &fp)) != 0)
		return (error);

	/* If asynchronous I/O is available, it works for all descriptors. */
	if (uap->name == _PC_ASYNC_IO) {
		td->td_retval[0] = async_io_version;
		goto out;
	}
	vp = fp->f_vnode;
	if (vp != NULL) {
		int vfslocked;
		vfslocked = VFS_LOCK_GIANT(vp->v_mount);
		vn_lock(vp, LK_SHARED | LK_RETRY);
		error = VOP_PATHCONF(vp, uap->name, td->td_retval);
		VOP_UNLOCK(vp, 0);
		VFS_UNLOCK_GIANT(vfslocked);
	} else if (fp->f_type == DTYPE_PIPE || fp->f_type == DTYPE_SOCKET) {
		if (uap->name != _PC_PIPE_BUF) {
			error = EINVAL;
		} else {
			td->td_retval[0] = PIPE_BUF;
		error = 0;
		}
	} else {
		error = EOPNOTSUPP;
	}
out:
	fdrop(fp, td);
	return (error);
}

/*
 * Grow the file table to accomodate (at least) nfd descriptors.  This may
 * block and drop the filedesc lock, but it will reacquire it before
 * returning.
 */
static void
fdgrowtable(struct filedesc *fdp, int nfd)
{
	struct filedesc0 *fdp0;
	struct freetable *fo;
	struct file **ntable;
	struct file **otable;
	char *nfileflags;
	int nnfiles, onfiles;
	NDSLOTTYPE *nmap;

	FILEDESC_XLOCK_ASSERT(fdp);

	KASSERT(fdp->fd_nfiles > 0,
	    ("zero-length file table"));

	/* compute the size of the new table */
	onfiles = fdp->fd_nfiles;
	nnfiles = NDSLOTS(nfd) * NDENTRIES; /* round up */
	if (nnfiles <= onfiles)
		/* the table is already large enough */
		return;

	/* allocate a new table and (if required) new bitmaps */
	FILEDESC_XUNLOCK(fdp);
	ntable = malloc((nnfiles * OFILESIZE) + sizeof(struct freetable),
	    M_FILEDESC, M_ZERO | M_WAITOK);
	nfileflags = (char *)&ntable[nnfiles];
	if (NDSLOTS(nnfiles) > NDSLOTS(onfiles))
		nmap = malloc(NDSLOTS(nnfiles) * NDSLOTSIZE,
		    M_FILEDESC, M_ZERO | M_WAITOK);
	else
		nmap = NULL;
	FILEDESC_XLOCK(fdp);

	/*
	 * We now have new tables ready to go.  Since we dropped the
	 * filedesc lock to call malloc(), watch out for a race.
	 */
	onfiles = fdp->fd_nfiles;
	if (onfiles >= nnfiles) {
		/* we lost the race, but that's OK */
		free(ntable, M_FILEDESC);
		if (nmap != NULL)
			free(nmap, M_FILEDESC);
		return;
	}
	bcopy(fdp->fd_ofiles, ntable, onfiles * sizeof(*ntable));
	bcopy(fdp->fd_ofileflags, nfileflags, onfiles);
	otable = fdp->fd_ofiles;
	fdp->fd_ofileflags = nfileflags;
	fdp->fd_ofiles = ntable;
	/*
	 * We must preserve ofiles until the process exits because we can't
	 * be certain that no threads have references to the old table via
	 * _fget().
	 */
	if (onfiles > NDFILE) {
		fo = (struct freetable *)&otable[onfiles];
		fdp0 = (struct filedesc0 *)fdp;
		fo->ft_table = otable;
		SLIST_INSERT_HEAD(&fdp0->fd_free, fo, ft_next);
	}
	if (NDSLOTS(nnfiles) > NDSLOTS(onfiles)) {
		bcopy(fdp->fd_map, nmap, NDSLOTS(onfiles) * sizeof(*nmap));
		if (NDSLOTS(onfiles) > NDSLOTS(NDFILE))
			free(fdp->fd_map, M_FILEDESC);
		fdp->fd_map = nmap;
	}
	fdp->fd_nfiles = nnfiles;
}

/*
 * Allocate a file descriptor for the process.
 */
int
fdalloc(struct thread *td, int minfd, int *result)
{
	struct proc *p = td->td_proc;
	struct filedesc *fdp = p->p_fd;
	int fd = -1, maxfd;
#ifdef RACCT
	int error;
#endif

	FILEDESC_XLOCK_ASSERT(fdp);

	if (fdp->fd_freefile > minfd)
		minfd = fdp->fd_freefile;

	PROC_LOCK(p);
	maxfd = min((int)lim_cur(p, RLIMIT_NOFILE), maxfilesperproc);
	PROC_UNLOCK(p);

	/*
	 * Search the bitmap for a free descriptor.  If none is found, try
	 * to grow the file table.  Keep at it until we either get a file
	 * descriptor or run into process or system limits; fdgrowtable()
	 * may drop the filedesc lock, so we're in a race.
	 */
	for (;;) {
		fd = fd_first_free(fdp, minfd, fdp->fd_nfiles);
		if (fd >= maxfd)
			return (EMFILE);
		if (fd < fdp->fd_nfiles)
			break;
#ifdef RACCT
		PROC_LOCK(p);
		error = racct_set(p, RACCT_NOFILE, min(fdp->fd_nfiles * 2, maxfd));
		PROC_UNLOCK(p);
		if (error != 0)
			return (EMFILE);
#endif
		fdgrowtable(fdp, min(fdp->fd_nfiles * 2, maxfd));
	}

	/*
	 * Perform some sanity checks, then mark the file descriptor as
	 * used and return it to the caller.
	 */
	KASSERT(!fdisused(fdp, fd),
	    ("fd_first_free() returned non-free descriptor"));
	KASSERT(fdp->fd_ofiles[fd] == NULL,
	    ("free descriptor isn't"));
	fdp->fd_ofileflags[fd] = 0; /* XXX needed? */
	fdused(fdp, fd);
	*result = fd;
	return (0);
}

/*
 * Check to see whether n user file descriptors are available to the process
 * p.
 */
int
fdavail(struct thread *td, int n)
{
	struct proc *p = td->td_proc;
	struct filedesc *fdp = td->td_proc->p_fd;
	struct file **fpp;
	int i, lim, last;

	FILEDESC_LOCK_ASSERT(fdp);

	/*
	 * XXX: This is only called from uipc_usrreq.c:unp_externalize();
	 *      call racct_add() from there instead of dealing with containers
	 *      here.
	 */
	PROC_LOCK(p);
	lim = min((int)lim_cur(p, RLIMIT_NOFILE), maxfilesperproc);
	PROC_UNLOCK(p);
	if ((i = lim - fdp->fd_nfiles) > 0 && (n -= i) <= 0)
		return (1);
	last = min(fdp->fd_nfiles, lim);
	fpp = &fdp->fd_ofiles[fdp->fd_freefile];
	for (i = last - fdp->fd_freefile; --i >= 0; fpp++) {
		if (*fpp == NULL && --n <= 0)
			return (1);
	}
	return (0);
}

/*
 * Create a new open file structure and allocate a file decriptor for the
 * process that refers to it.  We add one reference to the file for the
 * descriptor table and one reference for resultfp. This is to prevent us
 * being preempted and the entry in the descriptor table closed after we
 * release the FILEDESC lock.
 */
int
falloc(struct thread *td, struct file **resultfp, int *resultfd, int flags)
{
	struct file *fp;
	int error, fd;

	error = falloc_noinstall(td, &fp);
	if (error)
		return (error);		/* no reference held on error */

	error = finstall(td, fp, &fd, flags);
	if (error) {
		fdrop(fp, td);		/* one reference (fp only) */
		return (error);
	}

	if (resultfp != NULL)
		*resultfp = fp;		/* copy out result */
	else
		fdrop(fp, td);		/* release local reference */

	if (resultfd != NULL)
		*resultfd = fd;

	return (0);
}

/*
 * Create a new open file structure without allocating a file descriptor.
 */
int
falloc_noinstall(struct thread *td, struct file **resultfp)
{
	struct file *fp;
	int maxuserfiles = maxfiles - (maxfiles / 20);
	static struct timeval lastfail;
	static int curfail;

	KASSERT(resultfp != NULL, ("%s: resultfp == NULL", __func__));

	if ((openfiles >= maxuserfiles &&
	    priv_check(td, PRIV_MAXFILES) != 0) ||
	    openfiles >= maxfiles) {
		if (ppsratecheck(&lastfail, &curfail, 1)) {
			printf("kern.maxfiles limit exceeded by uid %i, "
			    "please see tuning(7).\n", td->td_ucred->cr_ruid);
		}
		return (ENFILE);
	}
	atomic_add_int(&openfiles, 1);
	fp = uma_zalloc(file_zone, M_WAITOK | M_ZERO);
	refcount_init(&fp->f_count, 1);
	fp->f_cred = crhold(td->td_ucred);
	fp->f_ops = &badfileops;
	fp->f_data = NULL;
	fp->f_vnode = NULL;
	*resultfp = fp;
	return (0);
}

/*
 * Install a file in a file descriptor table.
 */
int
finstall(struct thread *td, struct file *fp, int *fd, int flags)
{
	struct filedesc *fdp = td->td_proc->p_fd;
	int error;

	KASSERT(fd != NULL, ("%s: fd == NULL", __func__));
	KASSERT(fp != NULL, ("%s: fp == NULL", __func__));

	FILEDESC_XLOCK(fdp);
	if ((error = fdalloc(td, 0, fd))) {
		FILEDESC_XUNLOCK(fdp);
		return (error);
	}
	fhold(fp);
	fdp->fd_ofiles[*fd] = fp;
	if ((flags & O_CLOEXEC) != 0)
		fdp->fd_ofileflags[*fd] |= UF_EXCLOSE;
	FILEDESC_XUNLOCK(fdp);
	return (0);
}

/*
 * Build a new filedesc structure from another.
 * Copy the current, root, and jail root vnode references.
 */
struct filedesc *
fdinit(struct filedesc *fdp)
{
	struct filedesc0 *newfdp;

	newfdp = malloc(sizeof *newfdp, M_FILEDESC, M_WAITOK | M_ZERO);
	FILEDESC_LOCK_INIT(&newfdp->fd_fd);
	if (fdp != NULL) {
		FILEDESC_XLOCK(fdp);
		newfdp->fd_fd.fd_cdir = fdp->fd_cdir;
		if (newfdp->fd_fd.fd_cdir)
			VREF(newfdp->fd_fd.fd_cdir);
		newfdp->fd_fd.fd_rdir = fdp->fd_rdir;
		if (newfdp->fd_fd.fd_rdir)
			VREF(newfdp->fd_fd.fd_rdir);
		newfdp->fd_fd.fd_jdir = fdp->fd_jdir;
		if (newfdp->fd_fd.fd_jdir)
			VREF(newfdp->fd_fd.fd_jdir);
		FILEDESC_XUNLOCK(fdp);
	}

	/* Create the file descriptor table. */
	newfdp->fd_fd.fd_refcnt = 1;
	newfdp->fd_fd.fd_holdcnt = 1;
	newfdp->fd_fd.fd_cmask = CMASK;
	newfdp->fd_fd.fd_ofiles = newfdp->fd_dfiles;
	newfdp->fd_fd.fd_ofileflags = newfdp->fd_dfileflags;
	newfdp->fd_fd.fd_nfiles = NDFILE;
	newfdp->fd_fd.fd_map = newfdp->fd_dmap;
	newfdp->fd_fd.fd_lastfile = -1;
	return (&newfdp->fd_fd);
}

static struct filedesc *
fdhold(struct proc *p)
{
	struct filedesc *fdp;

	mtx_lock(&fdesc_mtx);
	fdp = p->p_fd;
	if (fdp != NULL)
		fdp->fd_holdcnt++;
	mtx_unlock(&fdesc_mtx);
	return (fdp);
}

static void
fddrop(struct filedesc *fdp)
{
	struct filedesc0 *fdp0;
	struct freetable *ft;
	int i;

	mtx_lock(&fdesc_mtx);
	i = --fdp->fd_holdcnt;
	mtx_unlock(&fdesc_mtx);
	if (i > 0)
		return;

	FILEDESC_LOCK_DESTROY(fdp);
	fdp0 = (struct filedesc0 *)fdp;
	while ((ft = SLIST_FIRST(&fdp0->fd_free)) != NULL) {
		SLIST_REMOVE_HEAD(&fdp0->fd_free, ft_next);
		free(ft->ft_table, M_FILEDESC);
	}
	free(fdp, M_FILEDESC);
}

/*
 * Share a filedesc structure.
 */
struct filedesc *
fdshare(struct filedesc *fdp)
{

	FILEDESC_XLOCK(fdp);
	fdp->fd_refcnt++;
	FILEDESC_XUNLOCK(fdp);
	return (fdp);
}

/*
 * Unshare a filedesc structure, if necessary by making a copy
 */
void
fdunshare(struct proc *p, struct thread *td)
{

	FILEDESC_XLOCK(p->p_fd);
	if (p->p_fd->fd_refcnt > 1) {
		struct filedesc *tmp;

		FILEDESC_XUNLOCK(p->p_fd);
		tmp = fdcopy(p->p_fd);
		fdfree(td);
		p->p_fd = tmp;
	} else
		FILEDESC_XUNLOCK(p->p_fd);
}

/*
 * Copy a filedesc structure.  A NULL pointer in returns a NULL reference,
 * this is to ease callers, not catch errors.
 */
struct filedesc *
fdcopy(struct filedesc *fdp)
{
	struct filedesc *newfdp;
	int i;

	/* Certain daemons might not have file descriptors. */
	if (fdp == NULL)
		return (NULL);

	newfdp = fdinit(fdp);
	FILEDESC_SLOCK(fdp);
	while (fdp->fd_lastfile >= newfdp->fd_nfiles) {
		FILEDESC_SUNLOCK(fdp);
		FILEDESC_XLOCK(newfdp);
		fdgrowtable(newfdp, fdp->fd_lastfile + 1);
		FILEDESC_XUNLOCK(newfdp);
		FILEDESC_SLOCK(fdp);
	}
	/* copy all passable descriptors (i.e. not kqueue) */
	newfdp->fd_freefile = -1;
	for (i = 0; i <= fdp->fd_lastfile; ++i) {
		if (fdisused(fdp, i) &&
		    (fdp->fd_ofiles[i]->f_ops->fo_flags & DFLAG_PASSABLE) &&
		    fdp->fd_ofiles[i]->f_ops != &badfileops) {
			newfdp->fd_ofiles[i] = fdp->fd_ofiles[i];
			newfdp->fd_ofileflags[i] = fdp->fd_ofileflags[i];
			fhold(newfdp->fd_ofiles[i]);
			newfdp->fd_lastfile = i;
		} else {
			if (newfdp->fd_freefile == -1)
				newfdp->fd_freefile = i;
		}
	}
	newfdp->fd_cmask = fdp->fd_cmask;
	FILEDESC_SUNLOCK(fdp);
	FILEDESC_XLOCK(newfdp);
	for (i = 0; i <= newfdp->fd_lastfile; ++i)
		if (newfdp->fd_ofiles[i] != NULL)
			fdused(newfdp, i);
	if (newfdp->fd_freefile == -1)
		newfdp->fd_freefile = i;
	FILEDESC_XUNLOCK(newfdp);
	return (newfdp);
}

/*
 * Release a filedesc structure.
 */
void
fdfree(struct thread *td)
{
	struct filedesc *fdp;
	struct file **fpp;
	int i, locked;
	struct filedesc_to_leader *fdtol;
	struct file *fp;
	struct vnode *cdir, *jdir, *rdir, *vp;
	struct flock lf;

	/* Certain daemons might not have file descriptors. */
	fdp = td->td_proc->p_fd;
	if (fdp == NULL)
		return;

#ifdef RACCT
	PROC_LOCK(td->td_proc);
	racct_set(td->td_proc, RACCT_NOFILE, 0);
	PROC_UNLOCK(td->td_proc);
#endif

	/* Check for special need to clear POSIX style locks */
	fdtol = td->td_proc->p_fdtol;
	if (fdtol != NULL) {
		FILEDESC_XLOCK(fdp);
		KASSERT(fdtol->fdl_refcount > 0,
			("filedesc_to_refcount botch: fdl_refcount=%d",
			 fdtol->fdl_refcount));
		if (fdtol->fdl_refcount == 1 &&
		    (td->td_proc->p_leader->p_flag & P_ADVLOCK) != 0) {
			for (i = 0, fpp = fdp->fd_ofiles;
			     i <= fdp->fd_lastfile;
			     i++, fpp++) {
				if (*fpp == NULL ||
				    (*fpp)->f_type != DTYPE_VNODE)
					continue;
				fp = *fpp;
				fhold(fp);
				FILEDESC_XUNLOCK(fdp);
				lf.l_whence = SEEK_SET;
				lf.l_start = 0;
				lf.l_len = 0;
				lf.l_type = F_UNLCK;
				vp = fp->f_vnode;
				locked = VFS_LOCK_GIANT(vp->v_mount);
				(void) VOP_ADVLOCK(vp,
						   (caddr_t)td->td_proc->
						   p_leader,
						   F_UNLCK,
						   &lf,
						   F_POSIX);
				VFS_UNLOCK_GIANT(locked);
				FILEDESC_XLOCK(fdp);
				fdrop(fp, td);
				fpp = fdp->fd_ofiles + i;
			}
		}
	retry:
		if (fdtol->fdl_refcount == 1) {
			if (fdp->fd_holdleaderscount > 0 &&
			    (td->td_proc->p_leader->p_flag & P_ADVLOCK) != 0) {
				/*
				 * close() or do_dup() has cleared a reference
				 * in a shared file descriptor table.
				 */
				fdp->fd_holdleaderswakeup = 1;
				sx_sleep(&fdp->fd_holdleaderscount,
				    FILEDESC_LOCK(fdp), PLOCK, "fdlhold", 0);
				goto retry;
			}
			if (fdtol->fdl_holdcount > 0) {
				/*
				 * Ensure that fdtol->fdl_leader remains
				 * valid in closef().
				 */
				fdtol->fdl_wakeup = 1;
				sx_sleep(fdtol, FILEDESC_LOCK(fdp), PLOCK,
				    "fdlhold", 0);
				goto retry;
			}
		}
		fdtol->fdl_refcount--;
		if (fdtol->fdl_refcount == 0 &&
		    fdtol->fdl_holdcount == 0) {
			fdtol->fdl_next->fdl_prev = fdtol->fdl_prev;
			fdtol->fdl_prev->fdl_next = fdtol->fdl_next;
		} else
			fdtol = NULL;
		td->td_proc->p_fdtol = NULL;
		FILEDESC_XUNLOCK(fdp);
		if (fdtol != NULL)
			free(fdtol, M_FILEDESC_TO_LEADER);
	}
	FILEDESC_XLOCK(fdp);
	i = --fdp->fd_refcnt;
	FILEDESC_XUNLOCK(fdp);
	if (i > 0)
		return;

	fpp = fdp->fd_ofiles;
	for (i = fdp->fd_lastfile; i-- >= 0; fpp++) {
		if (*fpp) {
			FILEDESC_XLOCK(fdp);
			fp = *fpp;
			*fpp = NULL;
			FILEDESC_XUNLOCK(fdp);
			(void) closef(fp, td);
		}
	}
	FILEDESC_XLOCK(fdp);

	/* XXX This should happen earlier. */
	mtx_lock(&fdesc_mtx);
	td->td_proc->p_fd = NULL;
	mtx_unlock(&fdesc_mtx);

	if (fdp->fd_nfiles > NDFILE)
		free(fdp->fd_ofiles, M_FILEDESC);
	if (NDSLOTS(fdp->fd_nfiles) > NDSLOTS(NDFILE))
		free(fdp->fd_map, M_FILEDESC);

	fdp->fd_nfiles = 0;

	cdir = fdp->fd_cdir;
	fdp->fd_cdir = NULL;
	rdir = fdp->fd_rdir;
	fdp->fd_rdir = NULL;
	jdir = fdp->fd_jdir;
	fdp->fd_jdir = NULL;
	FILEDESC_XUNLOCK(fdp);

	if (cdir) {
		locked = VFS_LOCK_GIANT(cdir->v_mount);
		vrele(cdir);
		VFS_UNLOCK_GIANT(locked);
	}
	if (rdir) {
		locked = VFS_LOCK_GIANT(rdir->v_mount);
		vrele(rdir);
		VFS_UNLOCK_GIANT(locked);
	}
	if (jdir) {
		locked = VFS_LOCK_GIANT(jdir->v_mount);
		vrele(jdir);
		VFS_UNLOCK_GIANT(locked);
	}

	fddrop(fdp);
}

/*
 * For setugid programs, we don't want to people to use that setugidness
 * to generate error messages which write to a file which otherwise would
 * otherwise be off-limits to the process.  We check for filesystems where
 * the vnode can change out from under us after execve (like [lin]procfs).
 *
 * Since setugidsafety calls this only for fd 0, 1 and 2, this check is
 * sufficient.  We also don't check for setugidness since we know we are.
 */
static int
is_unsafe(struct file *fp)
{
	if (fp->f_type == DTYPE_VNODE) {
		struct vnode *vp = fp->f_vnode;

		if ((vp->v_vflag & VV_PROCDEP) != 0)
			return (1);
	}
	return (0);
}

/*
 * Make this setguid thing safe, if at all possible.
 */
void
setugidsafety(struct thread *td)
{
	struct filedesc *fdp;
	int i;

	/* Certain daemons might not have file descriptors. */
	fdp = td->td_proc->p_fd;
	if (fdp == NULL)
		return;

	/*
	 * Note: fdp->fd_ofiles may be reallocated out from under us while
	 * we are blocked in a close.  Be careful!
	 */
	FILEDESC_XLOCK(fdp);
	for (i = 0; i <= fdp->fd_lastfile; i++) {
		if (i > 2)
			break;
		if (fdp->fd_ofiles[i] && is_unsafe(fdp->fd_ofiles[i])) {
			struct file *fp;

			knote_fdclose(td, i);
			/*
			 * NULL-out descriptor prior to close to avoid
			 * a race while close blocks.
			 */
			fp = fdp->fd_ofiles[i];
			fdp->fd_ofiles[i] = NULL;
			fdp->fd_ofileflags[i] = 0;
			fdunused(fdp, i);
			FILEDESC_XUNLOCK(fdp);
			(void) closef(fp, td);
			FILEDESC_XLOCK(fdp);
		}
	}
	FILEDESC_XUNLOCK(fdp);
}

/*
 * If a specific file object occupies a specific file descriptor, close the
 * file descriptor entry and drop a reference on the file object.  This is a
 * convenience function to handle a subsequent error in a function that calls
 * falloc() that handles the race that another thread might have closed the
 * file descriptor out from under the thread creating the file object.
 */
void
fdclose(struct filedesc *fdp, struct file *fp, int idx, struct thread *td)
{

	FILEDESC_XLOCK(fdp);
	if (fdp->fd_ofiles[idx] == fp) {
		fdp->fd_ofiles[idx] = NULL;
		fdunused(fdp, idx);
		FILEDESC_XUNLOCK(fdp);
		fdrop(fp, td);
	} else
		FILEDESC_XUNLOCK(fdp);
}

/*
 * Close any files on exec?
 */
void
fdcloseexec(struct thread *td)
{
	struct filedesc *fdp;
	int i;

	/* Certain daemons might not have file descriptors. */
	fdp = td->td_proc->p_fd;
	if (fdp == NULL)
		return;

	FILEDESC_XLOCK(fdp);

	/*
	 * We cannot cache fd_ofiles or fd_ofileflags since operations
	 * may block and rip them out from under us.
	 */
	for (i = 0; i <= fdp->fd_lastfile; i++) {
		if (fdp->fd_ofiles[i] != NULL &&
		    (fdp->fd_ofiles[i]->f_type == DTYPE_MQUEUE ||
		    (fdp->fd_ofileflags[i] & UF_EXCLOSE))) {
			struct file *fp;

			knote_fdclose(td, i);
			/*
			 * NULL-out descriptor prior to close to avoid
			 * a race while close blocks.
			 */
			fp = fdp->fd_ofiles[i];
			fdp->fd_ofiles[i] = NULL;
			fdp->fd_ofileflags[i] = 0;
			fdunused(fdp, i);
			if (fp->f_type == DTYPE_MQUEUE)
				mq_fdclose(td, i, fp);
			FILEDESC_XUNLOCK(fdp);
			(void) closef(fp, td);
			FILEDESC_XLOCK(fdp);
		}
	}
	FILEDESC_XUNLOCK(fdp);
}

/*
 * It is unsafe for set[ug]id processes to be started with file
 * descriptors 0..2 closed, as these descriptors are given implicit
 * significance in the Standard C library.  fdcheckstd() will create a
 * descriptor referencing /dev/null for each of stdin, stdout, and
 * stderr that is not already open.
 */
int
fdcheckstd(struct thread *td)
{
	struct filedesc *fdp;
	register_t retval, save;
	int i, error, devnull;

	fdp = td->td_proc->p_fd;
	if (fdp == NULL)
		return (0);
	KASSERT(fdp->fd_refcnt == 1, ("the fdtable should not be shared"));
	devnull = -1;
	error = 0;
	for (i = 0; i < 3; i++) {
		if (fdp->fd_ofiles[i] != NULL)
			continue;
		if (devnull < 0) {
			save = td->td_retval[0];
			error = kern_open(td, "/dev/null", UIO_SYSSPACE,
			    O_RDWR, 0);
			devnull = td->td_retval[0];
			td->td_retval[0] = save;
			if (error)
				break;
			KASSERT(devnull == i, ("oof, we didn't get our fd"));
		} else {
			error = do_dup(td, DUP_FIXED, devnull, i, &retval);
			if (error != 0)
				break;
		}
	}
	return (error);
}

/*
 * Internal form of close.  Decrement reference count on file structure.
 * Note: td may be NULL when closing a file that was being passed in a
 * message.
 *
 * XXXRW: Giant is not required for the caller, but often will be held; this
 * makes it moderately likely the Giant will be recursed in the VFS case.
 */
int
closef(struct file *fp, struct thread *td)
{
	struct vnode *vp;
	struct flock lf;
	struct filedesc_to_leader *fdtol;
	struct filedesc *fdp;
	struct file *fp_object;

	/*
	 * POSIX record locking dictates that any close releases ALL
	 * locks owned by this process.  This is handled by setting
	 * a flag in the unlock to free ONLY locks obeying POSIX
	 * semantics, and not to free BSD-style file locks.
	 * If the descriptor was in a message, POSIX-style locks
	 * aren't passed with the descriptor, and the thread pointer
	 * will be NULL.  Callers should be careful only to pass a
	 * NULL thread pointer when there really is no owning
	 * context that might have locks, or the locks will be
	 * leaked.
	 *
	 * If this is a capability, we do lock processing under the underlying
	 * node, not the capability itself.
	 */
	(void)cap_funwrap(fp, 0, &fp_object);
	if ((fp_object->f_type == DTYPE_VNODE) && (td != NULL)) {
		int vfslocked;

		vp = fp_object->f_vnode;
		vfslocked = VFS_LOCK_GIANT(vp->v_mount);
		if ((td->td_proc->p_leader->p_flag & P_ADVLOCK) != 0) {
			lf.l_whence = SEEK_SET;
			lf.l_start = 0;
			lf.l_len = 0;
			lf.l_type = F_UNLCK;
			(void) VOP_ADVLOCK(vp, (caddr_t)td->td_proc->p_leader,
					   F_UNLCK, &lf, F_POSIX);
		}
		fdtol = td->td_proc->p_fdtol;
		if (fdtol != NULL) {
			/*
			 * Handle special case where file descriptor table is
			 * shared between multiple process leaders.
			 */
			fdp = td->td_proc->p_fd;
			FILEDESC_XLOCK(fdp);
			for (fdtol = fdtol->fdl_next;
			     fdtol != td->td_proc->p_fdtol;
			     fdtol = fdtol->fdl_next) {
				if ((fdtol->fdl_leader->p_flag &
				     P_ADVLOCK) == 0)
					continue;
				fdtol->fdl_holdcount++;
				FILEDESC_XUNLOCK(fdp);
				lf.l_whence = SEEK_SET;
				lf.l_start = 0;
				lf.l_len = 0;
				lf.l_type = F_UNLCK;
				vp = fp_object->f_vnode;
				(void) VOP_ADVLOCK(vp,
						   (caddr_t)fdtol->fdl_leader,
						   F_UNLCK, &lf, F_POSIX);
				FILEDESC_XLOCK(fdp);
				fdtol->fdl_holdcount--;
				if (fdtol->fdl_holdcount == 0 &&
				    fdtol->fdl_wakeup != 0) {
					fdtol->fdl_wakeup = 0;
					wakeup(fdtol);
				}
			}
			FILEDESC_XUNLOCK(fdp);
		}
		VFS_UNLOCK_GIANT(vfslocked);
	}
	return (fdrop(fp, td));
}

/*
 * Initialize the file pointer with the specified properties.
 *
 * The ops are set with release semantics to be certain that the flags, type,
 * and data are visible when ops is.  This is to prevent ops methods from being
 * called with bad data.
 */
void
finit(struct file *fp, u_int flag, short type, void *data, struct fileops *ops)
{
	fp->f_data = data;
	fp->f_flag = flag;
	fp->f_type = type;
	atomic_store_rel_ptr((volatile uintptr_t *)&fp->f_ops, (uintptr_t)ops);
}

struct file *
fget_unlocked(struct filedesc *fdp, int fd)
{
	struct file *fp;
	u_int count;

	if (fd < 0 || fd >= fdp->fd_nfiles)
		return (NULL);
	/*
	 * Fetch the descriptor locklessly.  We avoid fdrop() races by
	 * never raising a refcount above 0.  To accomplish this we have
	 * to use a cmpset loop rather than an atomic_add.  The descriptor
	 * must be re-verified once we acquire a reference to be certain
	 * that the identity is still correct and we did not lose a race
	 * due to preemption.
	 */
	for (;;) {
		fp = fdp->fd_ofiles[fd];
		if (fp == NULL)
			break;
		count = fp->f_count;
		if (count == 0)
			continue;
		/*
		 * Use an acquire barrier to prevent caching of fd_ofiles
		 * so it is refreshed for verification.
		 */
		if (atomic_cmpset_acq_int(&fp->f_count, count, count + 1) != 1)
			continue;
		if (fp == fdp->fd_ofiles[fd])
			break;
		fdrop(fp, curthread);
	}

	return (fp);
}

/*
 * Extract the file pointer associated with the specified descriptor for the
 * current user process.
 *
 * If the descriptor doesn't exist or doesn't match 'flags', EBADF is
 * returned.
 *
 * If the FGET_GETCAP flag is set, the capability itself will be returned.
 * Calling _fget() with FGET_GETCAP on a non-capability will return EINVAL.
 * Otherwise, if the file is a capability, its rights will be checked against
 * the capability rights mask, and if successful, the object will be unwrapped.
 *
 * If an error occured the non-zero error is returned and *fpp is set to
 * NULL.  Otherwise *fpp is held and set and zero is returned.  Caller is
 * responsible for fdrop().
 */
#define	FGET_GETCAP	0x00000001
static __inline int
_fget(struct thread *td, int fd, struct file **fpp, int flags,
    cap_rights_t needrights, cap_rights_t *haverightsp, u_char *maxprotp,
    int fget_flags)
{
	struct filedesc *fdp;
	struct file *fp;
#ifdef CAPABILITIES
	struct file *fp_fromcap;
#endif
	int error;

	*fpp = NULL;
	if (td == NULL || (fdp = td->td_proc->p_fd) == NULL)
		return (EBADF);
	if ((fp = fget_unlocked(fdp, fd)) == NULL)
		return (EBADF);
	if (fp->f_ops == &badfileops) {
		fdrop(fp, td);
		return (EBADF);
	}

#ifdef CAPABILITIES
	/*
	 * If this is a capability, what rights does it have?
	 */
	if (haverightsp != NULL) {
		if (fp->f_type == DTYPE_CAPABILITY)
			*haverightsp = cap_rights(fp);
		else
			*haverightsp = CAP_MASK_VALID;
	}

	/*
	 * If a capability has been requested, return the capability directly.
	 * Otherwise, check capability rights, extract the underlying object,
	 * and check its access flags.
	 */
	if (fget_flags & FGET_GETCAP) {
		if (fp->f_type != DTYPE_CAPABILITY) {
			fdrop(fp, td);
			return (EINVAL);
		}
	} else {
		if (maxprotp == NULL)
			error = cap_funwrap(fp, needrights, &fp_fromcap);
		else
			error = cap_funwrap_mmap(fp, needrights, maxprotp,
			    &fp_fromcap);
		if (error != 0) {
			fdrop(fp, td);
			return (error);
		}

		/*
		 * If we've unwrapped a file, drop the original capability
		 * and hold the new descriptor.  fp after this point refers to
		 * the actual (unwrapped) object, not the capability.
		 */
		if (fp != fp_fromcap) {
			fhold(fp_fromcap);
			fdrop(fp, td);
			fp = fp_fromcap;
		}
	}
#else /* !CAPABILITIES */
	KASSERT(fp->f_type != DTYPE_CAPABILITY,
	    ("%s: saw capability", __func__));
	if (maxprotp != NULL)
		*maxprotp = VM_PROT_ALL;
#endif /* CAPABILITIES */

	/*
	 * FREAD and FWRITE failure return EBADF as per POSIX.
	 */
	error = 0;
	switch (flags) {
	case FREAD:
	case FWRITE:
		if ((fp->f_flag & flags) == 0)
			error = EBADF;
		break;
	case FEXEC:
	    	if ((fp->f_flag & (FREAD | FEXEC)) == 0 ||
		    ((fp->f_flag & FWRITE) != 0))
			error = EBADF;
		break;
	case 0:
		break;
	default:
		KASSERT(0, ("wrong flags"));
	}

	if (error != 0) {
		fdrop(fp, td);
		return (error);
	}

	*fpp = fp;
	return (0);
}

int
fget(struct thread *td, int fd, cap_rights_t rights, struct file **fpp)
{

	return(_fget(td, fd, fpp, 0, rights, NULL, NULL, 0));
}

int
fget_mmap(struct thread *td, int fd, cap_rights_t rights, u_char *maxprotp,
    struct file **fpp)
{

	return (_fget(td, fd, fpp, 0, rights, NULL, maxprotp, 0));
}

int
fget_read(struct thread *td, int fd, cap_rights_t rights, struct file **fpp)
{

	return(_fget(td, fd, fpp, FREAD, rights, NULL, NULL, 0));
}

int
fget_write(struct thread *td, int fd, cap_rights_t rights, struct file **fpp)
{

	return (_fget(td, fd, fpp, FWRITE, rights, NULL, NULL, 0));
}

/*
 * Unlike the other fget() calls, which accept and check capability rights
 * but never return capabilities, fgetcap() returns the capability but doesn't
 * check capability rights.
 */
int
fgetcap(struct thread *td, int fd, struct file **fpp)
{

	return (_fget(td, fd, fpp, 0, 0, NULL, NULL, FGET_GETCAP));
}


/*
 * Like fget() but loads the underlying vnode, or returns an error if the
 * descriptor does not represent a vnode.  Note that pipes use vnodes but
 * never have VM objects.  The returned vnode will be vref()'d.
 *
 * XXX: what about the unused flags ?
 */
static __inline int
_fgetvp(struct thread *td, int fd, int flags, cap_rights_t needrights,
    cap_rights_t *haverightsp, struct vnode **vpp)
{
	struct file *fp;
	int error;

	*vpp = NULL;
	if ((error = _fget(td, fd, &fp, flags, needrights, haverightsp,
	    NULL, 0)) != 0)
		return (error);
	if (fp->f_vnode == NULL) {
		error = EINVAL;
	} else {
		*vpp = fp->f_vnode;
		vref(*vpp);
	}
	fdrop(fp, td);

	return (error);
}

int
fgetvp(struct thread *td, int fd, cap_rights_t rights, struct vnode **vpp)
{

	return (_fgetvp(td, fd, 0, rights, NULL, vpp));
}

int
fgetvp_rights(struct thread *td, int fd, cap_rights_t need, cap_rights_t *have,
    struct vnode **vpp)
{
	return (_fgetvp(td, fd, 0, need, have, vpp));
}

int
fgetvp_read(struct thread *td, int fd, cap_rights_t rights, struct vnode **vpp)
{

	return (_fgetvp(td, fd, FREAD, rights, NULL, vpp));
}

int
fgetvp_exec(struct thread *td, int fd, cap_rights_t rights, struct vnode **vpp)
{

	return (_fgetvp(td, fd, FEXEC, rights, NULL, vpp));
}

#ifdef notyet
int
fgetvp_write(struct thread *td, int fd, cap_rights_t rights,
    struct vnode **vpp)
{

	return (_fgetvp(td, fd, FWRITE, rights, NULL, vpp));
}
#endif

/*
 * Like fget() but loads the underlying socket, or returns an error if the
 * descriptor does not represent a socket.
 *
 * We bump the ref count on the returned socket.  XXX Also obtain the SX lock
 * in the future.
 *
 * Note: fgetsock() and fputsock() are deprecated, as consumers should rely
 * on their file descriptor reference to prevent the socket from being free'd
 * during use.
 */
int
fgetsock(struct thread *td, int fd, cap_rights_t rights, struct socket **spp,
    u_int *fflagp)
{
	struct file *fp;
	int error;

	*spp = NULL;
	if (fflagp != NULL)
		*fflagp = 0;
	if ((error = _fget(td, fd, &fp, 0, rights, NULL, NULL, 0)) != 0)
		return (error);
	if (fp->f_type != DTYPE_SOCKET) {
		error = ENOTSOCK;
	} else {
		*spp = fp->f_data;
		if (fflagp)
			*fflagp = fp->f_flag;
		SOCK_LOCK(*spp);
		soref(*spp);
		SOCK_UNLOCK(*spp);
	}
	fdrop(fp, td);

	return (error);
}

/*
 * Drop the reference count on the socket and XXX release the SX lock in the
 * future.  The last reference closes the socket.
 *
 * Note: fputsock() is deprecated, see comment for fgetsock().
 */
void
fputsock(struct socket *so)
{

	ACCEPT_LOCK();
	SOCK_LOCK(so);
	CURVNET_SET(so->so_vnet);
	sorele(so);
	CURVNET_RESTORE();
}

/*
 * Handle the last reference to a file being closed.
 *
 * No special capability handling here, as the capability's fo_close will run
 * instead of the object here, and perform any necessary drop on the object.
 */
int
_fdrop(struct file *fp, struct thread *td)
{
	int error;

	error = 0;
	if (fp->f_count != 0)
		panic("fdrop: count %d", fp->f_count);
	if (fp->f_ops != &badfileops)
		error = fo_close(fp, td);
	atomic_subtract_int(&openfiles, 1);
	crfree(fp->f_cred);
	free(fp->f_advice, M_FADVISE);
	uma_zfree(file_zone, fp);

	return (error);
}

/*
 * Apply an advisory lock on a file descriptor.
 *
 * Just attempt to get a record lock of the requested type on the entire file
 * (l_whence = SEEK_SET, l_start = 0, l_len = 0).
 */
#ifndef _SYS_SYSPROTO_H_
struct flock_args {
	int	fd;
	int	how;
};
#endif
/* ARGSUSED */
int
sys_flock(struct thread *td, struct flock_args *uap)
{
	struct file *fp;
	struct vnode *vp;
	struct flock lf;
	int vfslocked;
	int error;

	if ((error = fget(td, uap->fd, CAP_FLOCK, &fp)) != 0)
		return (error);
	if (fp->f_type != DTYPE_VNODE) {
		fdrop(fp, td);
		return (EOPNOTSUPP);
	}

	vp = fp->f_vnode;
	vfslocked = VFS_LOCK_GIANT(vp->v_mount);
	lf.l_whence = SEEK_SET;
	lf.l_start = 0;
	lf.l_len = 0;
	if (uap->how & LOCK_UN) {
		lf.l_type = F_UNLCK;
		atomic_clear_int(&fp->f_flag, FHASLOCK);
		error = VOP_ADVLOCK(vp, (caddr_t)fp, F_UNLCK, &lf, F_FLOCK);
		goto done2;
	}
	if (uap->how & LOCK_EX)
		lf.l_type = F_WRLCK;
	else if (uap->how & LOCK_SH)
		lf.l_type = F_RDLCK;
	else {
		error = EBADF;
		goto done2;
	}
	atomic_set_int(&fp->f_flag, FHASLOCK);
	error = VOP_ADVLOCK(vp, (caddr_t)fp, F_SETLK, &lf,
	    (uap->how & LOCK_NB) ? F_FLOCK : F_FLOCK | F_WAIT);
done2:
	fdrop(fp, td);
	VFS_UNLOCK_GIANT(vfslocked);
	return (error);
}
/*
 * Duplicate the specified descriptor to a free descriptor.
 */
int
dupfdopen(struct thread *td, struct filedesc *fdp, int indx, int dfd, int mode, int error)
{
	struct file *wfp;
	struct file *fp;

	/*
	 * If the to-be-dup'd fd number is greater than the allowed number
	 * of file descriptors, or the fd to be dup'd has already been
	 * closed, then reject.
	 */
	FILEDESC_XLOCK(fdp);
	if (dfd < 0 || dfd >= fdp->fd_nfiles ||
	    (wfp = fdp->fd_ofiles[dfd]) == NULL) {
		FILEDESC_XUNLOCK(fdp);
		return (EBADF);
	}

	/*
	 * There are two cases of interest here.
	 *
	 * For ENODEV simply dup (dfd) to file descriptor (indx) and return.
	 *
	 * For ENXIO steal away the file structure from (dfd) and store it in
	 * (indx).  (dfd) is effectively closed by this operation.
	 *
	 * Any other error code is just returned.
	 */
	switch (error) {
	case ENODEV:
		/*
		 * Check that the mode the file is being opened for is a
		 * subset of the mode of the existing descriptor.
		 */
		if (((mode & (FREAD|FWRITE)) | wfp->f_flag) != wfp->f_flag) {
			FILEDESC_XUNLOCK(fdp);
			return (EACCES);
		}
		fp = fdp->fd_ofiles[indx];
		fdp->fd_ofiles[indx] = wfp;
		fdp->fd_ofileflags[indx] = fdp->fd_ofileflags[dfd];
		if (fp == NULL)
			fdused(fdp, indx);
		fhold(wfp);
		FILEDESC_XUNLOCK(fdp);
		if (fp != NULL)
			/*
			 * We now own the reference to fp that the ofiles[]
			 * array used to own.  Release it.
			 */
			fdrop(fp, td);
		return (0);

	case ENXIO:
		/*
		 * Steal away the file pointer from dfd and stuff it into indx.
		 */
		fp = fdp->fd_ofiles[indx];
		fdp->fd_ofiles[indx] = fdp->fd_ofiles[dfd];
		fdp->fd_ofiles[dfd] = NULL;
		fdp->fd_ofileflags[indx] = fdp->fd_ofileflags[dfd];
		fdp->fd_ofileflags[dfd] = 0;
		fdunused(fdp, dfd);
		if (fp == NULL)
			fdused(fdp, indx);
		FILEDESC_XUNLOCK(fdp);

		/*
		 * We now own the reference to fp that the ofiles[] array
		 * used to own.  Release it.
		 */
		if (fp != NULL)
			fdrop(fp, td);
		return (0);

	default:
		FILEDESC_XUNLOCK(fdp);
		return (error);
	}
	/* NOTREACHED */
}

/*
 * Scan all active processes and prisons to see if any of them have a current
 * or root directory of `olddp'. If so, replace them with the new mount point.
 */
void
mountcheckdirs(struct vnode *olddp, struct vnode *newdp)
{
	struct filedesc *fdp;
	struct prison *pr;
	struct proc *p;
	int nrele;

	if (vrefcnt(olddp) == 1)
		return;
	nrele = 0;
	sx_slock(&allproc_lock);
	FOREACH_PROC_IN_SYSTEM(p) {
		fdp = fdhold(p);
		if (fdp == NULL)
			continue;
		FILEDESC_XLOCK(fdp);
		if (fdp->fd_cdir == olddp) {
			vref(newdp);
			fdp->fd_cdir = newdp;
			nrele++;
		}
		if (fdp->fd_rdir == olddp) {
			vref(newdp);
			fdp->fd_rdir = newdp;
			nrele++;
		}
		if (fdp->fd_jdir == olddp) {
			vref(newdp);
			fdp->fd_jdir = newdp;
			nrele++;
		}
		FILEDESC_XUNLOCK(fdp);
		fddrop(fdp);
	}
	sx_sunlock(&allproc_lock);
	if (rootvnode == olddp) {
		vref(newdp);
		rootvnode = newdp;
		nrele++;
	}
	mtx_lock(&prison0.pr_mtx);
	if (prison0.pr_root == olddp) {
		vref(newdp);
		prison0.pr_root = newdp;
		nrele++;
	}
	mtx_unlock(&prison0.pr_mtx);
	sx_slock(&allprison_lock);
	TAILQ_FOREACH(pr, &allprison, pr_list) {
		mtx_lock(&pr->pr_mtx);
		if (pr->pr_root == olddp) {
			vref(newdp);
			pr->pr_root = newdp;
			nrele++;
		}
		mtx_unlock(&pr->pr_mtx);
	}
	sx_sunlock(&allprison_lock);
	while (nrele--)
		vrele(olddp);
}

struct filedesc_to_leader *
filedesc_to_leader_alloc(struct filedesc_to_leader *old, struct filedesc *fdp, struct proc *leader)
{
	struct filedesc_to_leader *fdtol;

	fdtol = malloc(sizeof(struct filedesc_to_leader),
	       M_FILEDESC_TO_LEADER,
	       M_WAITOK);
	fdtol->fdl_refcount = 1;
	fdtol->fdl_holdcount = 0;
	fdtol->fdl_wakeup = 0;
	fdtol->fdl_leader = leader;
	if (old != NULL) {
		FILEDESC_XLOCK(fdp);
		fdtol->fdl_next = old->fdl_next;
		fdtol->fdl_prev = old;
		old->fdl_next = fdtol;
		fdtol->fdl_next->fdl_prev = fdtol;
		FILEDESC_XUNLOCK(fdp);
	} else {
		fdtol->fdl_next = fdtol;
		fdtol->fdl_prev = fdtol;
	}
	return (fdtol);
}

/*
 * Get file structures globally.
 */
static int
sysctl_kern_file(SYSCTL_HANDLER_ARGS)
{
	struct xfile xf;
	struct filedesc *fdp;
	struct file *fp;
	struct proc *p;
	int error, n;

	error = sysctl_wire_old_buffer(req, 0);
	if (error != 0)
		return (error);
	if (req->oldptr == NULL) {
		n = 0;
		sx_slock(&allproc_lock);
		FOREACH_PROC_IN_SYSTEM(p) {
			if (p->p_state == PRS_NEW)
				continue;
			fdp = fdhold(p);
			if (fdp == NULL)
				continue;
			/* overestimates sparse tables. */
			if (fdp->fd_lastfile > 0)
				n += fdp->fd_lastfile;
			fddrop(fdp);
		}
		sx_sunlock(&allproc_lock);
		return (SYSCTL_OUT(req, 0, n * sizeof(xf)));
	}
	error = 0;
	bzero(&xf, sizeof(xf));
	xf.xf_size = sizeof(xf);
	sx_slock(&allproc_lock);
	FOREACH_PROC_IN_SYSTEM(p) {
		PROC_LOCK(p);
		if (p->p_state == PRS_NEW) {
			PROC_UNLOCK(p);
			continue;
		}
		if (p_cansee(req->td, p) != 0) {
			PROC_UNLOCK(p);
			continue;
		}
		xf.xf_pid = p->p_pid;
		xf.xf_uid = p->p_ucred->cr_uid;
		PROC_UNLOCK(p);
		fdp = fdhold(p);
		if (fdp == NULL)
			continue;
		FILEDESC_SLOCK(fdp);
		for (n = 0; fdp->fd_refcnt > 0 && n < fdp->fd_nfiles; ++n) {
			if ((fp = fdp->fd_ofiles[n]) == NULL)
				continue;
			xf.xf_fd = n;
			xf.xf_file = fp;
			xf.xf_data = fp->f_data;
			xf.xf_vnode = fp->f_vnode;
			xf.xf_type = fp->f_type;
			xf.xf_count = fp->f_count;
			xf.xf_msgcount = 0;
			xf.xf_offset = foffset_get(fp);
			xf.xf_flag = fp->f_flag;
			error = SYSCTL_OUT(req, &xf, sizeof(xf));
			if (error)
				break;
		}
		FILEDESC_SUNLOCK(fdp);
		fddrop(fdp);
		if (error)
			break;
	}
	sx_sunlock(&allproc_lock);
	return (error);
}

SYSCTL_PROC(_kern, KERN_FILE, file, CTLTYPE_OPAQUE|CTLFLAG_RD,
    0, 0, sysctl_kern_file, "S,xfile", "Entire file table");

#ifdef KINFO_OFILE_SIZE
CTASSERT(sizeof(struct kinfo_ofile) == KINFO_OFILE_SIZE);
#endif

#ifdef COMPAT_FREEBSD7
static int
export_vnode_for_osysctl(struct vnode *vp, int type,
    struct kinfo_ofile *kif, struct filedesc *fdp, struct sysctl_req *req)
{
	int error;
	char *fullpath, *freepath;
	int vfslocked;

	bzero(kif, sizeof(*kif));
	kif->kf_structsize = sizeof(*kif);

	vref(vp);
	kif->kf_fd = type;
	kif->kf_type = KF_TYPE_VNODE;
	/* This function only handles directories. */
	if (vp->v_type != VDIR) {
		vrele(vp);
		return (ENOTDIR);
	}
	kif->kf_vnode_type = KF_VTYPE_VDIR;

	/*
	 * This is not a true file descriptor, so we set a bogus refcount
	 * and offset to indicate these fields should be ignored.
	 */
	kif->kf_ref_count = -1;
	kif->kf_offset = -1;

	freepath = NULL;
	fullpath = "-";
	FILEDESC_SUNLOCK(fdp);
	vn_fullpath(curthread, vp, &fullpath, &freepath);
	vfslocked = VFS_LOCK_GIANT(vp->v_mount);
	vrele(vp);
	VFS_UNLOCK_GIANT(vfslocked);
	strlcpy(kif->kf_path, fullpath, sizeof(kif->kf_path));
	if (freepath != NULL)
		free(freepath, M_TEMP);
	error = SYSCTL_OUT(req, kif, sizeof(*kif));
	FILEDESC_SLOCK(fdp);
	return (error);
}

/*
 * Get per-process file descriptors for use by procstat(1), et al.
 */
static int
sysctl_kern_proc_ofiledesc(SYSCTL_HANDLER_ARGS)
{
	char *fullpath, *freepath;
	struct kinfo_ofile *kif;
	struct filedesc *fdp;
	int error, i, *name;
	struct shmfd *shmfd;
	struct socket *so;
	struct vnode *vp;
	struct file *fp;
	struct proc *p;
	struct tty *tp;
	int vfslocked;

	name = (int *)arg1;
	if ((p = pfind((pid_t)name[0])) == NULL)
		return (ESRCH);
	if ((error = p_candebug(curthread, p))) {
		PROC_UNLOCK(p);
		return (error);
	}
	fdp = fdhold(p);
	PROC_UNLOCK(p);
	if (fdp == NULL)
		return (ENOENT);
	kif = malloc(sizeof(*kif), M_TEMP, M_WAITOK);
	FILEDESC_SLOCK(fdp);
	if (fdp->fd_cdir != NULL)
		export_vnode_for_osysctl(fdp->fd_cdir, KF_FD_TYPE_CWD, kif,
				fdp, req);
	if (fdp->fd_rdir != NULL)
		export_vnode_for_osysctl(fdp->fd_rdir, KF_FD_TYPE_ROOT, kif,
				fdp, req);
	if (fdp->fd_jdir != NULL)
		export_vnode_for_osysctl(fdp->fd_jdir, KF_FD_TYPE_JAIL, kif,
				fdp, req);
	for (i = 0; i < fdp->fd_nfiles; i++) {
		if ((fp = fdp->fd_ofiles[i]) == NULL)
			continue;
		bzero(kif, sizeof(*kif));
		kif->kf_structsize = sizeof(*kif);
		vp = NULL;
		so = NULL;
		tp = NULL;
		shmfd = NULL;
		kif->kf_fd = i;

#ifdef CAPABILITIES
		/*
		 * When reporting a capability, most fields will be from the
		 * underlying object, but do mark as a capability. With
		 * ofiledesc, we don't have a field to export the cap_rights_t,
		 * but we do with the new filedesc.
		 */
		if (fp->f_type == DTYPE_CAPABILITY) {
			kif->kf_flags |= KF_FLAG_CAPABILITY;
			(void)cap_funwrap(fp, 0, &fp);
		}
#else
		KASSERT(fp->f_type != DTYPE_CAPABILITY,
		    ("sysctl_kern_proc_ofiledesc: saw capability"));
#endif
		switch (fp->f_type) {
		case DTYPE_VNODE:
			kif->kf_type = KF_TYPE_VNODE;
			vp = fp->f_vnode;
			break;

		case DTYPE_SOCKET:
			kif->kf_type = KF_TYPE_SOCKET;
			so = fp->f_data;
			break;

		case DTYPE_PIPE:
			kif->kf_type = KF_TYPE_PIPE;
			break;

		case DTYPE_FIFO:
			kif->kf_type = KF_TYPE_FIFO;
			vp = fp->f_vnode;
			break;

		case DTYPE_KQUEUE:
			kif->kf_type = KF_TYPE_KQUEUE;
			break;

		case DTYPE_CRYPTO:
			kif->kf_type = KF_TYPE_CRYPTO;
			break;

		case DTYPE_MQUEUE:
			kif->kf_type = KF_TYPE_MQUEUE;
			break;

		case DTYPE_SHM:
			kif->kf_type = KF_TYPE_SHM;
			shmfd = fp->f_data;
			break;

		case DTYPE_SEM:
			kif->kf_type = KF_TYPE_SEM;
			break;

		case DTYPE_PTS:
			kif->kf_type = KF_TYPE_PTS;
			tp = fp->f_data;
			break;

#ifdef PROCDESC
		case DTYPE_PROCDESC:
			kif->kf_type = KF_TYPE_PROCDESC;
			break;
#endif

		default:
			kif->kf_type = KF_TYPE_UNKNOWN;
			break;
		}
		kif->kf_ref_count = fp->f_count;
		if (fp->f_flag & FREAD)
			kif->kf_flags |= KF_FLAG_READ;
		if (fp->f_flag & FWRITE)
			kif->kf_flags |= KF_FLAG_WRITE;
		if (fp->f_flag & FAPPEND)
			kif->kf_flags |= KF_FLAG_APPEND;
		if (fp->f_flag & FASYNC)
			kif->kf_flags |= KF_FLAG_ASYNC;
		if (fp->f_flag & FFSYNC)
			kif->kf_flags |= KF_FLAG_FSYNC;
		if (fp->f_flag & FNONBLOCK)
			kif->kf_flags |= KF_FLAG_NONBLOCK;
		if (fp->f_flag & O_DIRECT)
			kif->kf_flags |= KF_FLAG_DIRECT;
		if (fp->f_flag & FHASLOCK)
			kif->kf_flags |= KF_FLAG_HASLOCK;
		kif->kf_offset = foffset_get(fp);
		if (vp != NULL) {
			vref(vp);
			switch (vp->v_type) {
			case VNON:
				kif->kf_vnode_type = KF_VTYPE_VNON;
				break;
			case VREG:
				kif->kf_vnode_type = KF_VTYPE_VREG;
				break;
			case VDIR:
				kif->kf_vnode_type = KF_VTYPE_VDIR;
				break;
			case VBLK:
				kif->kf_vnode_type = KF_VTYPE_VBLK;
				break;
			case VCHR:
				kif->kf_vnode_type = KF_VTYPE_VCHR;
				break;
			case VLNK:
				kif->kf_vnode_type = KF_VTYPE_VLNK;
				break;
			case VSOCK:
				kif->kf_vnode_type = KF_VTYPE_VSOCK;
				break;
			case VFIFO:
				kif->kf_vnode_type = KF_VTYPE_VFIFO;
				break;
			case VBAD:
				kif->kf_vnode_type = KF_VTYPE_VBAD;
				break;
			default:
				kif->kf_vnode_type = KF_VTYPE_UNKNOWN;
				break;
			}
			/*
			 * It is OK to drop the filedesc lock here as we will
			 * re-validate and re-evaluate its properties when
			 * the loop continues.
			 */
			freepath = NULL;
			fullpath = "-";
			FILEDESC_SUNLOCK(fdp);
			vn_fullpath(curthread, vp, &fullpath, &freepath);
			vfslocked = VFS_LOCK_GIANT(vp->v_mount);
			vrele(vp);
			VFS_UNLOCK_GIANT(vfslocked);
			strlcpy(kif->kf_path, fullpath,
			    sizeof(kif->kf_path));
			if (freepath != NULL)
				free(freepath, M_TEMP);
			FILEDESC_SLOCK(fdp);
		}
		if (so != NULL) {
			struct sockaddr *sa;

			if (so->so_proto->pr_usrreqs->pru_sockaddr(so, &sa)
			    == 0 && sa->sa_len <= sizeof(kif->kf_sa_local)) {
				bcopy(sa, &kif->kf_sa_local, sa->sa_len);
				free(sa, M_SONAME);
			}
			if (so->so_proto->pr_usrreqs->pru_peeraddr(so, &sa)
			    == 0 && sa->sa_len <= sizeof(kif->kf_sa_peer)) {
				bcopy(sa, &kif->kf_sa_peer, sa->sa_len);
				free(sa, M_SONAME);
			}
			kif->kf_sock_domain =
			    so->so_proto->pr_domain->dom_family;
			kif->kf_sock_type = so->so_type;
			kif->kf_sock_protocol = so->so_proto->pr_protocol;
		}
		if (tp != NULL) {
			strlcpy(kif->kf_path, tty_devname(tp),
			    sizeof(kif->kf_path));
		}
		if (shmfd != NULL)
			shm_path(shmfd, kif->kf_path, sizeof(kif->kf_path));
		error = SYSCTL_OUT(req, kif, sizeof(*kif));
		if (error)
			break;
	}
	FILEDESC_SUNLOCK(fdp);
	fddrop(fdp);
	free(kif, M_TEMP);
	return (0);
}

static SYSCTL_NODE(_kern_proc, KERN_PROC_OFILEDESC, ofiledesc, CTLFLAG_RD,
    sysctl_kern_proc_ofiledesc, "Process ofiledesc entries");
#endif	/* COMPAT_FREEBSD7 */

#ifdef KINFO_FILE_SIZE
CTASSERT(sizeof(struct kinfo_file) == KINFO_FILE_SIZE);
#endif

static int
export_fd_for_sysctl(void *data, int type, int fd, int fflags, int refcnt,
    int64_t offset, int fd_is_cap, cap_rights_t fd_cap_rights,
    struct kinfo_file *kif, struct sysctl_req *req)
{
	struct {
		int	fflag;
		int	kf_fflag;
	} fflags_table[] = {
		{ FAPPEND, KF_FLAG_APPEND },
		{ FASYNC, KF_FLAG_ASYNC },
		{ FFSYNC, KF_FLAG_FSYNC },
		{ FHASLOCK, KF_FLAG_HASLOCK },
		{ FNONBLOCK, KF_FLAG_NONBLOCK },
		{ FREAD, KF_FLAG_READ },
		{ FWRITE, KF_FLAG_WRITE },
		{ O_CREAT, KF_FLAG_CREAT },
		{ O_DIRECT, KF_FLAG_DIRECT },
		{ O_EXCL, KF_FLAG_EXCL },
		{ O_EXEC, KF_FLAG_EXEC },
		{ O_EXLOCK, KF_FLAG_EXLOCK },
		{ O_NOFOLLOW, KF_FLAG_NOFOLLOW },
		{ O_SHLOCK, KF_FLAG_SHLOCK },
		{ O_TRUNC, KF_FLAG_TRUNC }
	};
#define	NFFLAGS	(sizeof(fflags_table) / sizeof(*fflags_table))
	struct vnode *vp;
	int error, vfslocked;
	unsigned int i;

	bzero(kif, sizeof(*kif));
	switch (type) {
	case KF_TYPE_FIFO:
	case KF_TYPE_VNODE:
		vp = (struct vnode *)data;
		error = fill_vnode_info(vp, kif);
		vfslocked = VFS_LOCK_GIANT(vp->v_mount);
		vrele(vp);
		VFS_UNLOCK_GIANT(vfslocked);
		break;
	case KF_TYPE_SOCKET:
		error = fill_socket_info((struct socket *)data, kif);
		break;
	case KF_TYPE_PIPE:
		error = fill_pipe_info((struct pipe *)data, kif);
		break;
	case KF_TYPE_PTS:
		error = fill_pts_info((struct tty *)data, kif);
		break;
	case KF_TYPE_PROCDESC:
		error = fill_procdesc_info((struct procdesc *)data, kif);
		break;
	case KF_TYPE_SHM:
		error = fill_shm_info((struct file *)data, kif);
		break;
	default:
		error = 0;
	}
	if (error == 0)
		kif->kf_status |= KF_ATTR_VALID;

	/*
	 * Translate file access flags.
	 */
	for (i = 0; i < NFFLAGS; i++)
		if (fflags & fflags_table[i].fflag)
			kif->kf_flags |=  fflags_table[i].kf_fflag;
	if (fd_is_cap)
		kif->kf_flags |= KF_FLAG_CAPABILITY;
	if (fd_is_cap)
		kif->kf_cap_rights = fd_cap_rights;
	kif->kf_fd = fd;
	kif->kf_type = type;
	kif->kf_ref_count = refcnt;
	kif->kf_offset = offset;
	/* Pack record size down */
	kif->kf_structsize = offsetof(struct kinfo_file, kf_path) +
	    strlen(kif->kf_path) + 1;
	kif->kf_structsize = roundup(kif->kf_structsize, sizeof(uint64_t));
	error = SYSCTL_OUT(req, kif, kif->kf_structsize);
	return (error);
}

/*
 * Get per-process file descriptors for use by procstat(1), et al.
 */
static int
sysctl_kern_proc_filedesc(SYSCTL_HANDLER_ARGS)
{
	struct file *fp;
	struct filedesc *fdp;
	struct kinfo_file *kif;
	struct proc *p;
	struct vnode *cttyvp, *textvp, *tracevp;
	size_t oldidx;
	int64_t offset;
	void *data;
	int error, i, *name;
	int fd_is_cap, type, refcnt, fflags;
	cap_rights_t fd_cap_rights;

	name = (int *)arg1;
	if ((p = pfind((pid_t)name[0])) == NULL)
		return (ESRCH);
	if ((error = p_candebug(curthread, p))) {
		PROC_UNLOCK(p);
		return (error);
	}
	/* ktrace vnode */
	tracevp = p->p_tracevp;
	if (tracevp != NULL)
		vref(tracevp);
	/* text vnode */
	textvp = p->p_textvp;
	if (textvp != NULL)
		vref(textvp);
	/* Controlling tty. */
	cttyvp = NULL;
	if (p->p_pgrp != NULL && p->p_pgrp->pg_session != NULL) {
		cttyvp = p->p_pgrp->pg_session->s_ttyvp;
		if (cttyvp != NULL)
			vref(cttyvp);
	}
	fdp = fdhold(p);
	PROC_UNLOCK(p);
	kif = malloc(sizeof(*kif), M_TEMP, M_WAITOK);
	if (tracevp != NULL)
		export_fd_for_sysctl(tracevp, KF_TYPE_VNODE, KF_FD_TYPE_TRACE,
		    FREAD | FWRITE, -1, -1, 0, 0, kif, req);
	if (textvp != NULL)
		export_fd_for_sysctl(textvp, KF_TYPE_VNODE, KF_FD_TYPE_TEXT,
		    FREAD, -1, -1, 0, 0, kif, req);
	if (cttyvp != NULL)
		export_fd_for_sysctl(cttyvp, KF_TYPE_VNODE, KF_FD_TYPE_CTTY,
		    FREAD | FWRITE, -1, -1, 0, 0, kif, req);
	if (fdp == NULL)
		goto fail;
	FILEDESC_SLOCK(fdp);
	/* working directory */
	if (fdp->fd_cdir != NULL) {
		vref(fdp->fd_cdir);
		data = fdp->fd_cdir;
		FILEDESC_SUNLOCK(fdp);
		export_fd_for_sysctl(data, KF_TYPE_VNODE, KF_FD_TYPE_CWD,
		    FREAD, -1, -1, 0, 0, kif, req);
		FILEDESC_SLOCK(fdp);
	}
	/* root directory */
	if (fdp->fd_rdir != NULL) {
		vref(fdp->fd_rdir);
		data = fdp->fd_rdir;
		FILEDESC_SUNLOCK(fdp);
		export_fd_for_sysctl(data, KF_TYPE_VNODE, KF_FD_TYPE_ROOT,
		    FREAD, -1, -1, 0, 0, kif, req);
		FILEDESC_SLOCK(fdp);
	}
	/* jail directory */
	if (fdp->fd_jdir != NULL) {
		vref(fdp->fd_jdir);
		data = fdp->fd_jdir;
		FILEDESC_SUNLOCK(fdp);
		export_fd_for_sysctl(data, KF_TYPE_VNODE, KF_FD_TYPE_JAIL,
		    FREAD, -1, -1, 0, 0, kif, req);
		FILEDESC_SLOCK(fdp);
	}
	for (i = 0; i < fdp->fd_nfiles; i++) {
		if ((fp = fdp->fd_ofiles[i]) == NULL)
			continue;
		data = NULL;
		fd_is_cap = 0;
		fd_cap_rights = 0;

#ifdef CAPABILITIES
		/*
		 * When reporting a capability, most fields will be from the
		 * underlying object, but do mark as a capability and export
		 * the capability rights mask.
		 */
		if (fp->f_type == DTYPE_CAPABILITY) {
			fd_is_cap = 1;
			fd_cap_rights = cap_rights(fp);
			(void)cap_funwrap(fp, 0, &fp);
		}
#else /* !CAPABILITIES */
		KASSERT(fp->f_type != DTYPE_CAPABILITY,
		    ("sysctl_kern_proc_filedesc: saw capability"));
#endif
		switch (fp->f_type) {
		case DTYPE_VNODE:
			type = KF_TYPE_VNODE;
			vref(fp->f_vnode);
			data = fp->f_vnode;
			break;

		case DTYPE_SOCKET:
			type = KF_TYPE_SOCKET;
			data = fp->f_data;
			break;

		case DTYPE_PIPE:
			type = KF_TYPE_PIPE;
			data = fp->f_data;
			break;

		case DTYPE_FIFO:
			type = KF_TYPE_FIFO;
			vref(fp->f_vnode);
			data = fp->f_vnode;
			break;

		case DTYPE_KQUEUE:
			type = KF_TYPE_KQUEUE;
			break;

		case DTYPE_CRYPTO:
			type = KF_TYPE_CRYPTO;
			break;

		case DTYPE_MQUEUE:
			type = KF_TYPE_MQUEUE;
			break;

		case DTYPE_SHM:
			type = KF_TYPE_SHM;
			data = fp;
			break;

		case DTYPE_SEM:
			type = KF_TYPE_SEM;
			break;

		case DTYPE_PTS:
			type = KF_TYPE_PTS;
			data = fp->f_data;
			break;

#ifdef PROCDESC
		case DTYPE_PROCDESC:
			type = KF_TYPE_PROCDESC;
			data = fp->f_data;
			break;
#endif

		default:
			type = KF_TYPE_UNKNOWN;
			break;
		}
		refcnt = fp->f_count;
		fflags = fp->f_flag;
		offset = foffset_get(fp);

		/*
		 * Create sysctl entry.
		 * It is OK to drop the filedesc lock here as we will
		 * re-validate and re-evaluate its properties when
		 * the loop continues.
		 */
		oldidx = req->oldidx;
		if (type == KF_TYPE_VNODE || type == KF_TYPE_FIFO)
			FILEDESC_SUNLOCK(fdp);
		error = export_fd_for_sysctl(data, type, i, fflags, refcnt,
		    offset, fd_is_cap, fd_cap_rights, kif, req);
		if (type == KF_TYPE_VNODE || type == KF_TYPE_FIFO)
			FILEDESC_SLOCK(fdp);
		if (error) {
			if (error == ENOMEM) {
				/*
				 * The hack to keep the ABI of sysctl
				 * kern.proc.filedesc intact, but not
				 * to account a partially copied
				 * kinfo_file into the oldidx.
				 */
				req->oldidx = oldidx;
				error = 0;
			}
			break;
		}
	}
	FILEDESC_SUNLOCK(fdp);
fail:
	if (fdp != NULL)
		fddrop(fdp);
	free(kif, M_TEMP);
	return (error);
}

int
vntype_to_kinfo(int vtype)
{
	struct {
		int	vtype;
		int	kf_vtype;
	} vtypes_table[] = {
		{ VBAD, KF_VTYPE_VBAD },
		{ VBLK, KF_VTYPE_VBLK },
		{ VCHR, KF_VTYPE_VCHR },
		{ VDIR, KF_VTYPE_VDIR },
		{ VFIFO, KF_VTYPE_VFIFO },
		{ VLNK, KF_VTYPE_VLNK },
		{ VNON, KF_VTYPE_VNON },
		{ VREG, KF_VTYPE_VREG },
		{ VSOCK, KF_VTYPE_VSOCK }
	};
#define	NVTYPES	(sizeof(vtypes_table) / sizeof(*vtypes_table))
	unsigned int i;

	/*
	 * Perform vtype translation.
	 */
	for (i = 0; i < NVTYPES; i++)
		if (vtypes_table[i].vtype == vtype)
			break;
	if (i < NVTYPES)
		return (vtypes_table[i].kf_vtype);

	return (KF_VTYPE_UNKNOWN);
}

static int
fill_vnode_info(struct vnode *vp, struct kinfo_file *kif)
{
	struct vattr va;
	char *fullpath, *freepath;
	int error, vfslocked;

	if (vp == NULL)
		return (1);
	kif->kf_vnode_type = vntype_to_kinfo(vp->v_type);
	freepath = NULL;
	fullpath = "-";
	error = vn_fullpath(curthread, vp, &fullpath, &freepath);
	if (error == 0) {
		strlcpy(kif->kf_path, fullpath, sizeof(kif->kf_path));
	}
	if (freepath != NULL)
		free(freepath, M_TEMP);

	/*
	 * Retrieve vnode attributes.
	 */
	va.va_fsid = VNOVAL;
	va.va_rdev = NODEV;
	vfslocked = VFS_LOCK_GIANT(vp->v_mount);
	vn_lock(vp, LK_SHARED | LK_RETRY);
	error = VOP_GETATTR(vp, &va, curthread->td_ucred);
	VOP_UNLOCK(vp, 0);
	VFS_UNLOCK_GIANT(vfslocked);
	if (error != 0)
		return (error);
	if (va.va_fsid != VNOVAL)
		kif->kf_un.kf_file.kf_file_fsid = va.va_fsid;
	else
		kif->kf_un.kf_file.kf_file_fsid =
		    vp->v_mount->mnt_stat.f_fsid.val[0];
	kif->kf_un.kf_file.kf_file_fileid = va.va_fileid;
	kif->kf_un.kf_file.kf_file_mode = MAKEIMODE(va.va_type, va.va_mode);
	kif->kf_un.kf_file.kf_file_size = va.va_size;
	kif->kf_un.kf_file.kf_file_rdev = va.va_rdev;
	return (0);
}

static int
fill_socket_info(struct socket *so, struct kinfo_file *kif)
{
	struct sockaddr *sa;
	struct inpcb *inpcb;
	struct unpcb *unpcb;
	int error;

	if (so == NULL)
		return (1);
	kif->kf_sock_domain = so->so_proto->pr_domain->dom_family;
	kif->kf_sock_type = so->so_type;
	kif->kf_sock_protocol = so->so_proto->pr_protocol;
	kif->kf_un.kf_sock.kf_sock_pcb = (uintptr_t)so->so_pcb;
	switch(kif->kf_sock_domain) {
	case AF_INET:
	case AF_INET6:
		if (kif->kf_sock_protocol == IPPROTO_TCP) {
			if (so->so_pcb != NULL) {
				inpcb = (struct inpcb *)(so->so_pcb);
				kif->kf_un.kf_sock.kf_sock_inpcb =
				    (uintptr_t)inpcb->inp_ppcb;
			}
		}
		break;
	case AF_UNIX:
		if (so->so_pcb != NULL) {
			unpcb = (struct unpcb *)(so->so_pcb);
			if (unpcb->unp_conn) {
				kif->kf_un.kf_sock.kf_sock_unpconn =
				    (uintptr_t)unpcb->unp_conn;
				kif->kf_un.kf_sock.kf_sock_rcv_sb_state =
				    so->so_rcv.sb_state;
				kif->kf_un.kf_sock.kf_sock_snd_sb_state =
				    so->so_snd.sb_state;
			}
		}
		break;
	}
	error = so->so_proto->pr_usrreqs->pru_sockaddr(so, &sa);
	if (error == 0 && sa->sa_len <= sizeof(kif->kf_sa_local)) {
		bcopy(sa, &kif->kf_sa_local, sa->sa_len);
		free(sa, M_SONAME);
	}
	error = so->so_proto->pr_usrreqs->pru_peeraddr(so, &sa);
	if (error == 0 && sa->sa_len <= sizeof(kif->kf_sa_peer)) {
		bcopy(sa, &kif->kf_sa_peer, sa->sa_len);
		free(sa, M_SONAME);
	}
	strncpy(kif->kf_path, so->so_proto->pr_domain->dom_name,
	    sizeof(kif->kf_path));
	return (0);
}

static int
fill_pts_info(struct tty *tp, struct kinfo_file *kif)
{

	if (tp == NULL)
		return (1);
	kif->kf_un.kf_pts.kf_pts_dev = tty_udev(tp);
	strlcpy(kif->kf_path, tty_devname(tp), sizeof(kif->kf_path));
	return (0);
}

static int
fill_pipe_info(struct pipe *pi, struct kinfo_file *kif)
{

	if (pi == NULL)
		return (1);
	kif->kf_un.kf_pipe.kf_pipe_addr = (uintptr_t)pi;
	kif->kf_un.kf_pipe.kf_pipe_peer = (uintptr_t)pi->pipe_peer;
	kif->kf_un.kf_pipe.kf_pipe_buffer_cnt = pi->pipe_buffer.cnt;
	return (0);
}

static int
fill_procdesc_info(struct procdesc *pdp, struct kinfo_file *kif)
{

	if (pdp == NULL)
		return (1);
	kif->kf_un.kf_proc.kf_pid = pdp->pd_pid;
	return (0);
}

static int
fill_shm_info(struct file *fp, struct kinfo_file *kif)
{
	struct thread *td;
	struct stat sb;

	td = curthread;
	if (fp->f_data == NULL)
		return (1);
	if (fo_stat(fp, &sb, td->td_ucred, td) != 0)
		return (1);
	shm_path(fp->f_data, kif->kf_path, sizeof(kif->kf_path));
	kif->kf_un.kf_file.kf_file_mode = sb.st_mode;
	kif->kf_un.kf_file.kf_file_size = sb.st_size;
	return (0);
}

static SYSCTL_NODE(_kern_proc, KERN_PROC_FILEDESC, filedesc, CTLFLAG_RD,
    sysctl_kern_proc_filedesc, "Process filedesc entries");

#ifdef DDB
/*
 * For the purposes of debugging, generate a human-readable string for the
 * file type.
 */
static const char *
file_type_to_name(short type)
{

	switch (type) {
	case 0:
		return ("zero");
	case DTYPE_VNODE:
		return ("vnod");
	case DTYPE_SOCKET:
		return ("sock");
	case DTYPE_PIPE:
		return ("pipe");
	case DTYPE_FIFO:
		return ("fifo");
	case DTYPE_KQUEUE:
		return ("kque");
	case DTYPE_CRYPTO:
		return ("crpt");
	case DTYPE_MQUEUE:
		return ("mque");
	case DTYPE_SHM:
		return ("shm");
	case DTYPE_SEM:
		return ("ksem");
	default:
		return ("unkn");
	}
}

/*
 * For the purposes of debugging, identify a process (if any, perhaps one of
 * many) that references the passed file in its file descriptor array. Return
 * NULL if none.
 */
static struct proc *
file_to_first_proc(struct file *fp)
{
	struct filedesc *fdp;
	struct proc *p;
	int n;

	FOREACH_PROC_IN_SYSTEM(p) {
		if (p->p_state == PRS_NEW)
			continue;
		fdp = p->p_fd;
		if (fdp == NULL)
			continue;
		for (n = 0; n < fdp->fd_nfiles; n++) {
			if (fp == fdp->fd_ofiles[n])
				return (p);
		}
	}
	return (NULL);
}

static void
db_print_file(struct file *fp, int header)
{
	struct proc *p;

	if (header)
		db_printf("%8s %4s %8s %8s %4s %5s %6s %8s %5s %12s\n",
		    "File", "Type", "Data", "Flag", "GCFl", "Count",
		    "MCount", "Vnode", "FPID", "FCmd");
	p = file_to_first_proc(fp);
	db_printf("%8p %4s %8p %08x %04x %5d %6d %8p %5d %12s\n", fp,
	    file_type_to_name(fp->f_type), fp->f_data, fp->f_flag,
	    0, fp->f_count, 0, fp->f_vnode,
	    p != NULL ? p->p_pid : -1, p != NULL ? p->p_comm : "-");
}

DB_SHOW_COMMAND(file, db_show_file)
{
	struct file *fp;

	if (!have_addr) {
		db_printf("usage: show file <addr>\n");
		return;
	}
	fp = (struct file *)addr;
	db_print_file(fp, 1);
}

DB_SHOW_COMMAND(files, db_show_files)
{
	struct filedesc *fdp;
	struct file *fp;
	struct proc *p;
	int header;
	int n;

	header = 1;
	FOREACH_PROC_IN_SYSTEM(p) {
		if (p->p_state == PRS_NEW)
			continue;
		if ((fdp = p->p_fd) == NULL)
			continue;
		for (n = 0; n < fdp->fd_nfiles; ++n) {
			if ((fp = fdp->fd_ofiles[n]) == NULL)
				continue;
			db_print_file(fp, header);
			header = 0;
		}
	}
}
#endif

SYSCTL_INT(_kern, KERN_MAXFILESPERPROC, maxfilesperproc, CTLFLAG_RW,
    &maxfilesperproc, 0, "Maximum files allowed open per process");

SYSCTL_INT(_kern, KERN_MAXFILES, maxfiles, CTLFLAG_RW,
    &maxfiles, 0, "Maximum number of files");

SYSCTL_INT(_kern, OID_AUTO, openfiles, CTLFLAG_RD,
    __DEVOLATILE(int *, &openfiles), 0, "System-wide number of open files");

/* ARGSUSED*/
static void
filelistinit(void *dummy)
{

	file_zone = uma_zcreate("Files", sizeof(struct file), NULL, NULL,
	    NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_NOFREE);
	mtx_init(&sigio_lock, "sigio lock", NULL, MTX_DEF);
	mtx_init(&fdesc_mtx, "fdesc", NULL, MTX_DEF);
}
SYSINIT(select, SI_SUB_LOCK, SI_ORDER_FIRST, filelistinit, NULL);

/*-------------------------------------------------------------------*/

static int
badfo_readwrite(struct file *fp, struct uio *uio, struct ucred *active_cred,
    int flags, struct thread *td)
{

	return (EBADF);
}

static int
badfo_truncate(struct file *fp, off_t length, struct ucred *active_cred,
    struct thread *td)
{

	return (EINVAL);
}

static int
badfo_ioctl(struct file *fp, u_long com, void *data, struct ucred *active_cred,
    struct thread *td)
{

	return (EBADF);
}

static int
badfo_poll(struct file *fp, int events, struct ucred *active_cred,
    struct thread *td)
{

	return (0);
}

static int
badfo_kqfilter(struct file *fp, struct knote *kn)
{

	return (EBADF);
}

static int
badfo_stat(struct file *fp, struct stat *sb, struct ucred *active_cred,
    struct thread *td)
{

	return (EBADF);
}

static int
badfo_close(struct file *fp, struct thread *td)
{

	return (EBADF);
}

static int
badfo_chmod(struct file *fp, mode_t mode, struct ucred *active_cred,
    struct thread *td)
{

	return (EBADF);
}

static int
badfo_chown(struct file *fp, uid_t uid, gid_t gid, struct ucred *active_cred,
    struct thread *td)
{

	return (EBADF);
}

struct fileops badfileops = {
	.fo_read = badfo_readwrite,
	.fo_write = badfo_readwrite,
	.fo_truncate = badfo_truncate,
	.fo_ioctl = badfo_ioctl,
	.fo_poll = badfo_poll,
	.fo_kqfilter = badfo_kqfilter,
	.fo_stat = badfo_stat,
	.fo_close = badfo_close,
	.fo_chmod = badfo_chmod,
	.fo_chown = badfo_chown,
};

int
invfo_chmod(struct file *fp, mode_t mode, struct ucred *active_cred,
    struct thread *td)
{

	return (EINVAL);
}

int
invfo_chown(struct file *fp, uid_t uid, gid_t gid, struct ucred *active_cred,
    struct thread *td)
{

	return (EINVAL);
}

/*-------------------------------------------------------------------*/

/*
 * File Descriptor pseudo-device driver (/dev/fd/).
 *
 * Opening minor device N dup()s the file (if any) connected to file
 * descriptor N belonging to the calling process.  Note that this driver
 * consists of only the ``open()'' routine, because all subsequent
 * references to this file will be direct to the other driver.
 *
 * XXX: we could give this one a cloning event handler if necessary.
 */

/* ARGSUSED */
static int
fdopen(struct cdev *dev, int mode, int type, struct thread *td)
{

	/*
	 * XXX Kludge: set curthread->td_dupfd to contain the value of the
	 * the file descriptor being sought for duplication. The error
	 * return ensures that the vnode for this device will be released
	 * by vn_open. Open will detect this special error and take the
	 * actions in dupfdopen below. Other callers of vn_open or VOP_OPEN
	 * will simply report the error.
	 */
	td->td_dupfd = dev2unit(dev);
	return (ENODEV);
}

static struct cdevsw fildesc_cdevsw = {
	.d_version =	D_VERSION,
	.d_open =	fdopen,
	.d_name =	"FD",
};

static void
fildesc_drvinit(void *unused)
{
	struct cdev *dev;

	dev = make_dev_credf(MAKEDEV_ETERNAL, &fildesc_cdevsw, 0, NULL,
	    UID_ROOT, GID_WHEEL, 0666, "fd/0");
	make_dev_alias(dev, "stdin");
	dev = make_dev_credf(MAKEDEV_ETERNAL, &fildesc_cdevsw, 1, NULL,
	    UID_ROOT, GID_WHEEL, 0666, "fd/1");
	make_dev_alias(dev, "stdout");
	dev = make_dev_credf(MAKEDEV_ETERNAL, &fildesc_cdevsw, 2, NULL,
	    UID_ROOT, GID_WHEEL, 0666, "fd/2");
	make_dev_alias(dev, "stderr");
}

SYSINIT(fildescdev, SI_SUB_DRIVERS, SI_ORDER_MIDDLE, fildesc_drvinit, NULL);
