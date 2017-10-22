/*-
 * Copyright (c) 1982, 1986, 1989, 1993
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
 *	@(#)sys_generic.c	8.5 (Berkeley) 1/21/94
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/7.0.0/sys/kern/sys_generic.c 171212 2007-07-04 22:57:21Z peter $");

#include "opt_compat.h"
#include "opt_ktrace.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/filedesc.h>
#include <sys/filio.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/socketvar.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/malloc.h>
#include <sys/poll.h>
#include <sys/resourcevar.h>
#include <sys/selinfo.h>
#include <sys/sleepqueue.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/vnode.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/condvar.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif

static MALLOC_DEFINE(M_IOCTLOPS, "ioctlops", "ioctl data buffer");
static MALLOC_DEFINE(M_SELECT, "select", "select() buffer");
MALLOC_DEFINE(M_IOV, "iov", "large iov's");

static int	pollscan(struct thread *, struct pollfd *, u_int);
static int	selscan(struct thread *, fd_mask **, fd_mask **, int);
static int	dofileread(struct thread *, int, struct file *, struct uio *,
		    off_t, int);
static int	dofilewrite(struct thread *, int, struct file *, struct uio *,
		    off_t, int);
static void	doselwakeup(struct selinfo *, int);

#ifndef _SYS_SYSPROTO_H_
struct read_args {
	int	fd;
	void	*buf;
	size_t	nbyte;
};
#endif
int
read(td, uap)
	struct thread *td;
	struct read_args *uap;
{
	struct uio auio;
	struct iovec aiov;
	int error;

	if (uap->nbyte > INT_MAX)
		return (EINVAL);
	aiov.iov_base = uap->buf;
	aiov.iov_len = uap->nbyte;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_resid = uap->nbyte;
	auio.uio_segflg = UIO_USERSPACE;
	error = kern_readv(td, uap->fd, &auio);
	return(error);
}

/*
 * Positioned read system call
 */
#ifndef _SYS_SYSPROTO_H_
struct pread_args {
	int	fd;
	void	*buf;
	size_t	nbyte;
	int	pad;
	off_t	offset;
};
#endif
int
pread(td, uap)
	struct thread *td;
	struct pread_args *uap;
{
	struct uio auio;
	struct iovec aiov;
	int error;

	if (uap->nbyte > INT_MAX)
		return (EINVAL);
	aiov.iov_base = uap->buf;
	aiov.iov_len = uap->nbyte;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_resid = uap->nbyte;
	auio.uio_segflg = UIO_USERSPACE;
	error = kern_preadv(td, uap->fd, &auio, uap->offset);
	return(error);
}

int
freebsd6_pread(td, uap)
	struct thread *td;
	struct freebsd6_pread_args *uap;
{
	struct pread_args oargs;

	oargs.fd = uap->fd;
	oargs.buf = uap->buf;
	oargs.nbyte = uap->nbyte;
	oargs.offset = uap->offset;
	return (pread(td, &oargs));
}

/*
 * Scatter read system call.
 */
#ifndef _SYS_SYSPROTO_H_
struct readv_args {
	int	fd;
	struct	iovec *iovp;
	u_int	iovcnt;
};
#endif
int
readv(struct thread *td, struct readv_args *uap)
{
	struct uio *auio;
	int error;

	error = copyinuio(uap->iovp, uap->iovcnt, &auio);
	if (error)
		return (error);
	error = kern_readv(td, uap->fd, auio);
	free(auio, M_IOV);
	return (error);
}

int
kern_readv(struct thread *td, int fd, struct uio *auio)
{
	struct file *fp;
	int error;

	error = fget_read(td, fd, &fp);
	if (error)
		return (error);
	error = dofileread(td, fd, fp, auio, (off_t)-1, 0);
	fdrop(fp, td);
	return (error);
}

/*
 * Scatter positioned read system call.
 */
#ifndef _SYS_SYSPROTO_H_
struct preadv_args {
	int	fd;
	struct	iovec *iovp;
	u_int	iovcnt;
	off_t	offset;
};
#endif
int
preadv(struct thread *td, struct preadv_args *uap)
{
	struct uio *auio;
	int error;

	error = copyinuio(uap->iovp, uap->iovcnt, &auio);
	if (error)
		return (error);
	error = kern_preadv(td, uap->fd, auio, uap->offset);
	free(auio, M_IOV);
	return (error);
}

int
kern_preadv(td, fd, auio, offset)
	struct thread *td;
	int fd;
	struct uio *auio;
	off_t offset;
{
	struct file *fp;
	int error;

	error = fget_read(td, fd, &fp);
	if (error)
		return (error);
	if (!(fp->f_ops->fo_flags & DFLAG_SEEKABLE))
		error = ESPIPE;
	else if (offset < 0 && fp->f_vnode->v_type != VCHR)
		error = EINVAL;
	else
		error = dofileread(td, fd, fp, auio, offset, FOF_OFFSET);
	fdrop(fp, td);
	return (error);
}

/*
 * Common code for readv and preadv that reads data in
 * from a file using the passed in uio, offset, and flags.
 */
static int
dofileread(td, fd, fp, auio, offset, flags)
	struct thread *td;
	int fd;
	struct file *fp;
	struct uio *auio;
	off_t offset;
	int flags;
{
	ssize_t cnt;
	int error;
#ifdef KTRACE
	struct uio *ktruio = NULL;
#endif

	/* Finish zero length reads right here */
	if (auio->uio_resid == 0) {
		td->td_retval[0] = 0;
		return(0);
	}
	auio->uio_rw = UIO_READ;
	auio->uio_offset = offset;
	auio->uio_td = td;
#ifdef KTRACE
	if (KTRPOINT(td, KTR_GENIO)) 
		ktruio = cloneuio(auio);
#endif
	cnt = auio->uio_resid;
	if ((error = fo_read(fp, auio, td->td_ucred, flags, td))) {
		if (auio->uio_resid != cnt && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
			error = 0;
	}
	cnt -= auio->uio_resid;
#ifdef KTRACE
	if (ktruio != NULL) {
		ktruio->uio_resid = cnt;
		ktrgenio(fd, UIO_READ, ktruio, error);
	}
#endif
	td->td_retval[0] = cnt;
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct write_args {
	int	fd;
	const void *buf;
	size_t	nbyte;
};
#endif
int
write(td, uap)
	struct thread *td;
	struct write_args *uap;
{
	struct uio auio;
	struct iovec aiov;
	int error;

	if (uap->nbyte > INT_MAX)
		return (EINVAL);
	aiov.iov_base = (void *)(uintptr_t)uap->buf;
	aiov.iov_len = uap->nbyte;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_resid = uap->nbyte;
	auio.uio_segflg = UIO_USERSPACE;
	error = kern_writev(td, uap->fd, &auio);
	return(error);
}

/*
 * Positioned write system call.
 */
#ifndef _SYS_SYSPROTO_H_
struct pwrite_args {
	int	fd;
	const void *buf;
	size_t	nbyte;
	int	pad;
	off_t	offset;
};
#endif
int
pwrite(td, uap)
	struct thread *td;
	struct pwrite_args *uap;
{
	struct uio auio;
	struct iovec aiov;
	int error;

	if (uap->nbyte > INT_MAX)
		return (EINVAL);
	aiov.iov_base = (void *)(uintptr_t)uap->buf;
	aiov.iov_len = uap->nbyte;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_resid = uap->nbyte;
	auio.uio_segflg = UIO_USERSPACE;
	error = kern_pwritev(td, uap->fd, &auio, uap->offset);
	return(error);
}

int
freebsd6_pwrite(td, uap)
	struct thread *td;
	struct freebsd6_pwrite_args *uap;
{
	struct pwrite_args oargs;

	oargs.fd = uap->fd;
	oargs.buf = uap->buf;
	oargs.nbyte = uap->nbyte;
	oargs.offset = uap->offset;
	return (pwrite(td, &oargs));
}

/*
 * Gather write system call.
 */
#ifndef _SYS_SYSPROTO_H_
struct writev_args {
	int	fd;
	struct	iovec *iovp;
	u_int	iovcnt;
};
#endif
int
writev(struct thread *td, struct writev_args *uap)
{
	struct uio *auio;
	int error;

	error = copyinuio(uap->iovp, uap->iovcnt, &auio);
	if (error)
		return (error);
	error = kern_writev(td, uap->fd, auio);
	free(auio, M_IOV);
	return (error);
}

int
kern_writev(struct thread *td, int fd, struct uio *auio)
{
	struct file *fp;
	int error;

	error = fget_write(td, fd, &fp);
	if (error)
		return (error);
	error = dofilewrite(td, fd, fp, auio, (off_t)-1, 0);
	fdrop(fp, td);
	return (error);
}

/*
 * Gather positioned write system call.
 */
#ifndef _SYS_SYSPROTO_H_
struct pwritev_args {
	int	fd;
	struct	iovec *iovp;
	u_int	iovcnt;
	off_t	offset;
};
#endif
int
pwritev(struct thread *td, struct pwritev_args *uap)
{
	struct uio *auio;
	int error;

	error = copyinuio(uap->iovp, uap->iovcnt, &auio);
	if (error)
		return (error);
	error = kern_pwritev(td, uap->fd, auio, uap->offset);
	free(auio, M_IOV);
	return (error);
}

int
kern_pwritev(td, fd, auio, offset)
	struct thread *td;
	struct uio *auio;
	int fd;
	off_t offset;
{
	struct file *fp;
	int error;

	error = fget_write(td, fd, &fp);
	if (error)
		return (error);
	if (!(fp->f_ops->fo_flags & DFLAG_SEEKABLE))
		error = ESPIPE;
	else if (offset < 0 && fp->f_vnode->v_type != VCHR)
		error = EINVAL;
	else
		error = dofilewrite(td, fd, fp, auio, offset, FOF_OFFSET);
	fdrop(fp, td);
	return (error);
}

/*
 * Common code for writev and pwritev that writes data to
 * a file using the passed in uio, offset, and flags.
 */
static int
dofilewrite(td, fd, fp, auio, offset, flags)
	struct thread *td;
	int fd;
	struct file *fp;
	struct uio *auio;
	off_t offset;
	int flags;
{
	ssize_t cnt;
	int error;
#ifdef KTRACE
	struct uio *ktruio = NULL;
#endif

	auio->uio_rw = UIO_WRITE;
	auio->uio_td = td;
	auio->uio_offset = offset;
#ifdef KTRACE
	if (KTRPOINT(td, KTR_GENIO))
		ktruio = cloneuio(auio);
#endif
	cnt = auio->uio_resid;
	if (fp->f_type == DTYPE_VNODE)
		bwillwrite();
	if ((error = fo_write(fp, auio, td->td_ucred, flags, td))) {
		if (auio->uio_resid != cnt && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
			error = 0;
		/* Socket layer is responsible for issuing SIGPIPE. */
		if (fp->f_type != DTYPE_SOCKET && error == EPIPE) {
			PROC_LOCK(td->td_proc);
			psignal(td->td_proc, SIGPIPE);
			PROC_UNLOCK(td->td_proc);
		}
	}
	cnt -= auio->uio_resid;
#ifdef KTRACE
	if (ktruio != NULL) {
		ktruio->uio_resid = cnt;
		ktrgenio(fd, UIO_WRITE, ktruio, error);
	}
#endif
	td->td_retval[0] = cnt;
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct ioctl_args {
	int	fd;
	u_long	com;
	caddr_t	data;
};
#endif
/* ARGSUSED */
int
ioctl(struct thread *td, struct ioctl_args *uap)
{
	u_long com;
	int arg, error;
	u_int size;
	caddr_t data;

	if (uap->com > 0xffffffff) {
		printf(
		    "WARNING pid %d (%s): ioctl sign-extension ioctl %lx\n",
		    td->td_proc->p_pid, td->td_proc->p_comm, uap->com);
		uap->com &= 0xffffffff;
	}
	com = uap->com;

	/*
	 * Interpret high order word to find amount of data to be
	 * copied to/from the user's address space.
	 */
	size = IOCPARM_LEN(com);
	if ((size > IOCPARM_MAX) ||
	    ((com & (IOC_VOID  | IOC_IN | IOC_OUT)) == 0) ||
#if defined(COMPAT_FREEBSD5) || defined(COMPAT_FREEBSD4) || defined(COMPAT_43)
	    ((com & IOC_OUT) && size == 0) ||
#else
	    ((com & (IOC_IN | IOC_OUT)) && size == 0) ||
#endif
	    ((com & IOC_VOID) && size > 0 && size != sizeof(int)))
		return (ENOTTY);

	if (size > 0) {
		if (!(com & IOC_VOID))
			data = malloc((u_long)size, M_IOCTLOPS, M_WAITOK);
		else {
			/* Integer argument. */
			arg = (intptr_t)uap->data;
			data = (void *)&arg;
			size = 0;
		}
	} else
		data = (void *)&uap->data;
	if (com & IOC_IN) {
		error = copyin(uap->data, data, (u_int)size);
		if (error) {
			if (size > 0)
				free(data, M_IOCTLOPS);
			return (error);
		}
	} else if (com & IOC_OUT) {
		/*
		 * Zero the buffer so the user always
		 * gets back something deterministic.
		 */
		bzero(data, size);
	}

	error = kern_ioctl(td, uap->fd, com, data);

	if (error == 0 && (com & IOC_OUT))
		error = copyout(data, uap->data, (u_int)size);

	if (size > 0)
		free(data, M_IOCTLOPS);
	return (error);
}

int
kern_ioctl(struct thread *td, int fd, u_long com, caddr_t data)
{
	struct file *fp;
	struct filedesc *fdp;
	int error;
	int tmp;

	if ((error = fget(td, fd, &fp)) != 0)
		return (error);
	if ((fp->f_flag & (FREAD | FWRITE)) == 0) {
		fdrop(fp, td);
		return (EBADF);
	}
	fdp = td->td_proc->p_fd;
	switch (com) {
	case FIONCLEX:
		FILEDESC_XLOCK(fdp);
		fdp->fd_ofileflags[fd] &= ~UF_EXCLOSE;
		FILEDESC_XUNLOCK(fdp);
		goto out;
	case FIOCLEX:
		FILEDESC_XLOCK(fdp);
		fdp->fd_ofileflags[fd] |= UF_EXCLOSE;
		FILEDESC_XUNLOCK(fdp);
		goto out;
	case FIONBIO:
		FILE_LOCK(fp);
		if ((tmp = *(int *)data))
			fp->f_flag |= FNONBLOCK;
		else
			fp->f_flag &= ~FNONBLOCK;
		FILE_UNLOCK(fp);
		data = (void *)&tmp;
		break;
	case FIOASYNC:
		FILE_LOCK(fp);
		if ((tmp = *(int *)data))
			fp->f_flag |= FASYNC;
		else
			fp->f_flag &= ~FASYNC;
		FILE_UNLOCK(fp);
		data = (void *)&tmp;
		break;
	}

	error = fo_ioctl(fp, com, data, td->td_ucred, td);
out:
	fdrop(fp, td);
	return (error);
}

/*
 * sellock and selwait are initialized in selectinit() via SYSINIT.
 */
struct mtx	sellock;
struct cv	selwait;
u_int		nselcoll;	/* Select collisions since boot */
SYSCTL_UINT(_kern, OID_AUTO, nselcoll, CTLFLAG_RD, &nselcoll, 0, "");

#ifndef _SYS_SYSPROTO_H_
struct select_args {
	int	nd;
	fd_set	*in, *ou, *ex;
	struct	timeval *tv;
};
#endif
int
select(td, uap)
	register struct thread *td;
	register struct select_args *uap;
{
	struct timeval tv, *tvp;
	int error;

	if (uap->tv != NULL) {
		error = copyin(uap->tv, &tv, sizeof(tv));
		if (error)
			return (error);
		tvp = &tv;
	} else
		tvp = NULL;

	return (kern_select(td, uap->nd, uap->in, uap->ou, uap->ex, tvp));
}

int
kern_select(struct thread *td, int nd, fd_set *fd_in, fd_set *fd_ou,
    fd_set *fd_ex, struct timeval *tvp)
{
	struct filedesc *fdp;
	/*
	 * The magic 2048 here is chosen to be just enough for FD_SETSIZE
	 * infds with the new FD_SETSIZE of 1024, and more than enough for
	 * FD_SETSIZE infds, outfds and exceptfds with the old FD_SETSIZE
	 * of 256.
	 */
	fd_mask s_selbits[howmany(2048, NFDBITS)];
	fd_mask *ibits[3], *obits[3], *selbits, *sbp;
	struct timeval atv, rtv, ttv;
	int error, timo;
	u_int ncoll, nbufbytes, ncpbytes, nfdbits;

	if (nd < 0)
		return (EINVAL);
	fdp = td->td_proc->p_fd;
	
	FILEDESC_SLOCK(fdp);
	if (nd > td->td_proc->p_fd->fd_nfiles)
		nd = td->td_proc->p_fd->fd_nfiles;   /* forgiving; slightly wrong */
	FILEDESC_SUNLOCK(fdp);

	/*
	 * Allocate just enough bits for the non-null fd_sets.  Use the
	 * preallocated auto buffer if possible.
	 */
	nfdbits = roundup(nd, NFDBITS);
	ncpbytes = nfdbits / NBBY;
	nbufbytes = 0;
	if (fd_in != NULL)
		nbufbytes += 2 * ncpbytes;
	if (fd_ou != NULL)
		nbufbytes += 2 * ncpbytes;
	if (fd_ex != NULL)
		nbufbytes += 2 * ncpbytes;
	if (nbufbytes <= sizeof s_selbits)
		selbits = &s_selbits[0];
	else
		selbits = malloc(nbufbytes, M_SELECT, M_WAITOK);

	/*
	 * Assign pointers into the bit buffers and fetch the input bits.
	 * Put the output buffers together so that they can be bzeroed
	 * together.
	 */
	sbp = selbits;
#define	getbits(name, x) \
	do {								\
		if (name == NULL)					\
			ibits[x] = NULL;				\
		else {							\
			ibits[x] = sbp + nbufbytes / 2 / sizeof *sbp;	\
			obits[x] = sbp;					\
			sbp += ncpbytes / sizeof *sbp;			\
			error = copyin(name, ibits[x], ncpbytes);	\
			if (error != 0)					\
				goto done_nosellock;			\
		}							\
	} while (0)
	getbits(fd_in, 0);
	getbits(fd_ou, 1);
	getbits(fd_ex, 2);
#undef	getbits
	if (nbufbytes != 0)
		bzero(selbits, nbufbytes / 2);

	if (tvp != NULL) {
		atv = *tvp;
		if (itimerfix(&atv)) {
			error = EINVAL;
			goto done_nosellock;
		}
		getmicrouptime(&rtv);
		timevaladd(&atv, &rtv);
	} else {
		atv.tv_sec = 0;
		atv.tv_usec = 0;
	}
	timo = 0;
	TAILQ_INIT(&td->td_selq);
	mtx_lock(&sellock);
retry:
	ncoll = nselcoll;
	thread_lock(td);
	td->td_flags |= TDF_SELECT;
	thread_unlock(td);
	mtx_unlock(&sellock);

	error = selscan(td, ibits, obits, nd);
	mtx_lock(&sellock);
	if (error || td->td_retval[0])
		goto done;
	if (atv.tv_sec || atv.tv_usec) {
		getmicrouptime(&rtv);
		if (timevalcmp(&rtv, &atv, >=))
			goto done;
		ttv = atv;
		timevalsub(&ttv, &rtv);
		timo = ttv.tv_sec > 24 * 60 * 60 ?
		    24 * 60 * 60 * hz : tvtohz(&ttv);
	}

	/*
	 * An event of interest may occur while we do not hold
	 * sellock, so check TDF_SELECT and the number of
	 * collisions and rescan the file descriptors if
	 * necessary.
	 */
	thread_lock(td);
	if ((td->td_flags & TDF_SELECT) == 0 || nselcoll != ncoll) {
		thread_unlock(td);
		goto retry;
	}
	thread_unlock(td);

	if (timo > 0)
		error = cv_timedwait_sig(&selwait, &sellock, timo);
	else
		error = cv_wait_sig(&selwait, &sellock);
	
	if (error == 0)
		goto retry;

done:
	clear_selinfo_list(td);
	thread_lock(td);
	td->td_flags &= ~TDF_SELECT;
	thread_unlock(td);
	mtx_unlock(&sellock);

done_nosellock:
	/* select is not restarted after signals... */
	if (error == ERESTART)
		error = EINTR;
	if (error == EWOULDBLOCK)
		error = 0;
#define	putbits(name, x) \
	if (name && (error2 = copyout(obits[x], name, ncpbytes))) \
		error = error2;
	if (error == 0) {
		int error2;

		putbits(fd_in, 0);
		putbits(fd_ou, 1);
		putbits(fd_ex, 2);
#undef putbits
	}
	if (selbits != &s_selbits[0])
		free(selbits, M_SELECT);

	return (error);
}

static int
selscan(td, ibits, obits, nfd)
	struct thread *td;
	fd_mask **ibits, **obits;
	int nfd;
{
	int msk, i, fd;
	fd_mask bits;
	struct file *fp;
	int n = 0;
	/* Note: backend also returns POLLHUP/POLLERR if appropriate. */
	static int flag[3] = { POLLRDNORM, POLLWRNORM, POLLRDBAND };
	struct filedesc *fdp = td->td_proc->p_fd;

	FILEDESC_SLOCK(fdp);
	for (msk = 0; msk < 3; msk++) {
		if (ibits[msk] == NULL)
			continue;
		for (i = 0; i < nfd; i += NFDBITS) {
			bits = ibits[msk][i/NFDBITS];
			/* ffs(int mask) not portable, fd_mask is long */
			for (fd = i; bits && fd < nfd; fd++, bits >>= 1) {
				if (!(bits & 1))
					continue;
				if ((fp = fget_locked(fdp, fd)) == NULL) {
					FILEDESC_SUNLOCK(fdp);
					return (EBADF);
				}
				if (fo_poll(fp, flag[msk], td->td_ucred,
				    td)) {
					obits[msk][(fd)/NFDBITS] |=
					    ((fd_mask)1 << ((fd) % NFDBITS));
					n++;
				}
			}
		}
	}
	FILEDESC_SUNLOCK(fdp);
	td->td_retval[0] = n;
	return (0);
}

#ifndef _SYS_SYSPROTO_H_
struct poll_args {
	struct pollfd *fds;
	u_int	nfds;
	int	timeout;
};
#endif
int
poll(td, uap)
	struct thread *td;
	struct poll_args *uap;
{
	struct pollfd *bits;
	struct pollfd smallbits[32];
	struct timeval atv, rtv, ttv;
	int error = 0, timo;
	u_int ncoll, nfds;
	size_t ni;

	nfds = uap->nfds;

	/*
	 * This is kinda bogus.  We have fd limits, but that is not
	 * really related to the size of the pollfd array.  Make sure
	 * we let the process use at least FD_SETSIZE entries and at
	 * least enough for the current limits.  We want to be reasonably
	 * safe, but not overly restrictive.
	 */
	PROC_LOCK(td->td_proc);
	if ((nfds > lim_cur(td->td_proc, RLIMIT_NOFILE)) &&
	    (nfds > FD_SETSIZE)) {
		PROC_UNLOCK(td->td_proc);
		error = EINVAL;
		goto done2;
	}
	PROC_UNLOCK(td->td_proc);
	ni = nfds * sizeof(struct pollfd);
	if (ni > sizeof(smallbits))
		bits = malloc(ni, M_TEMP, M_WAITOK);
	else
		bits = smallbits;
	error = copyin(uap->fds, bits, ni);
	if (error)
		goto done_nosellock;
	if (uap->timeout != INFTIM) {
		atv.tv_sec = uap->timeout / 1000;
		atv.tv_usec = (uap->timeout % 1000) * 1000;
		if (itimerfix(&atv)) {
			error = EINVAL;
			goto done_nosellock;
		}
		getmicrouptime(&rtv);
		timevaladd(&atv, &rtv);
	} else {
		atv.tv_sec = 0;
		atv.tv_usec = 0;
	}
	timo = 0;
	TAILQ_INIT(&td->td_selq);
	mtx_lock(&sellock);
retry:
	ncoll = nselcoll;
	thread_lock(td);
	td->td_flags |= TDF_SELECT;
	thread_unlock(td);
	mtx_unlock(&sellock);

	error = pollscan(td, bits, nfds);
	mtx_lock(&sellock);
	if (error || td->td_retval[0])
		goto done;
	if (atv.tv_sec || atv.tv_usec) {
		getmicrouptime(&rtv);
		if (timevalcmp(&rtv, &atv, >=))
			goto done;
		ttv = atv;
		timevalsub(&ttv, &rtv);
		timo = ttv.tv_sec > 24 * 60 * 60 ?
		    24 * 60 * 60 * hz : tvtohz(&ttv);
	}
	/*
	 * An event of interest may occur while we do not hold
	 * sellock, so check TDF_SELECT and the number of collisions
	 * and rescan the file descriptors if necessary.
	 */
	thread_lock(td);
	if ((td->td_flags & TDF_SELECT) == 0 || nselcoll != ncoll) {
		thread_unlock(td);
		goto retry;
	}
	thread_unlock(td);

	if (timo > 0)
		error = cv_timedwait_sig(&selwait, &sellock, timo);
	else
		error = cv_wait_sig(&selwait, &sellock);

	if (error == 0)
		goto retry;

done:
	clear_selinfo_list(td);
	thread_lock(td);
	td->td_flags &= ~TDF_SELECT;
	thread_unlock(td);
	mtx_unlock(&sellock);

done_nosellock:
	/* poll is not restarted after signals... */
	if (error == ERESTART)
		error = EINTR;
	if (error == EWOULDBLOCK)
		error = 0;
	if (error == 0) {
		error = copyout(bits, uap->fds, ni);
		if (error)
			goto out;
	}
out:
	if (ni > sizeof(smallbits))
		free(bits, M_TEMP);
done2:
	return (error);
}

static int
pollscan(td, fds, nfd)
	struct thread *td;
	struct pollfd *fds;
	u_int nfd;
{
	register struct filedesc *fdp = td->td_proc->p_fd;
	int i;
	struct file *fp;
	int n = 0;

	FILEDESC_SLOCK(fdp);
	for (i = 0; i < nfd; i++, fds++) {
		if (fds->fd >= fdp->fd_nfiles) {
			fds->revents = POLLNVAL;
			n++;
		} else if (fds->fd < 0) {
			fds->revents = 0;
		} else {
			fp = fdp->fd_ofiles[fds->fd];
			if (fp == NULL) {
				fds->revents = POLLNVAL;
				n++;
			} else {
				/*
				 * Note: backend also returns POLLHUP and
				 * POLLERR if appropriate.
				 */
				fds->revents = fo_poll(fp, fds->events,
				    td->td_ucred, td);
				if (fds->revents != 0)
					n++;
			}
		}
	}
	FILEDESC_SUNLOCK(fdp);
	td->td_retval[0] = n;
	return (0);
}

/*
 * OpenBSD poll system call.
 *
 * XXX this isn't quite a true representation..  OpenBSD uses select ops.
 */
#ifndef _SYS_SYSPROTO_H_
struct openbsd_poll_args {
	struct pollfd *fds;
	u_int	nfds;
	int	timeout;
};
#endif
int
openbsd_poll(td, uap)
	register struct thread *td;
	register struct openbsd_poll_args *uap;
{
	return (poll(td, (struct poll_args *)uap));
}

/*
 * Remove the references to the thread from all of the objects we were
 * polling.
 *
 * This code assumes that the underlying owner of the selinfo structure will
 * hold sellock before it changes it, and that it will unlink itself from our
 * list if it goes away.
 */
void
clear_selinfo_list(td)
	struct thread *td;
{
	struct selinfo *si;

	mtx_assert(&sellock, MA_OWNED);
	TAILQ_FOREACH(si, &td->td_selq, si_thrlist)
		si->si_thread = NULL;
	TAILQ_INIT(&td->td_selq);
}

/*
 * Record a select request.
 */
void
selrecord(selector, sip)
	struct thread *selector;
	struct selinfo *sip;
{

	mtx_lock(&sellock);
	/*
	 * If the selinfo's thread pointer is NULL then take ownership of it.
	 *
	 * If the thread pointer is not NULL and it points to another
	 * thread, then we have a collision.
	 *
	 * If the thread pointer is not NULL and points back to us then leave
	 * it alone as we've already added pointed it at us and added it to
	 * our list.
	 */
	if (sip->si_thread == NULL) {
		sip->si_thread = selector;
		TAILQ_INSERT_TAIL(&selector->td_selq, sip, si_thrlist);
	} else if (sip->si_thread != selector) {
		sip->si_flags |= SI_COLL;
	}

	mtx_unlock(&sellock);
}

/* Wake up a selecting thread. */
void
selwakeup(sip)
	struct selinfo *sip;
{
	doselwakeup(sip, -1);
}

/* Wake up a selecting thread, and set its priority. */
void
selwakeuppri(sip, pri)
	struct selinfo *sip;
	int pri;
{
	doselwakeup(sip, pri);
}

/*
 * Do a wakeup when a selectable event occurs.
 */
static void
doselwakeup(sip, pri)
	struct selinfo *sip;
	int pri;
{
	struct thread *td;

	mtx_lock(&sellock);
	td = sip->si_thread;
	if ((sip->si_flags & SI_COLL) != 0) {
		nselcoll++;
		sip->si_flags &= ~SI_COLL;
		cv_broadcastpri(&selwait, pri);
	}
	if (td == NULL) {
		mtx_unlock(&sellock);
		return;
	}
	TAILQ_REMOVE(&td->td_selq, sip, si_thrlist);
	sip->si_thread = NULL;
	thread_lock(td);
	td->td_flags &= ~TDF_SELECT;
	thread_unlock(td);
	sleepq_remove(td, &selwait);
	mtx_unlock(&sellock);
}

static void selectinit(void *);
SYSINIT(select, SI_SUB_LOCK, SI_ORDER_FIRST, selectinit, NULL)

/* ARGSUSED*/
static void
selectinit(dummy)
	void *dummy;
{
	cv_init(&selwait, "select");
	mtx_init(&sellock, "sellck", NULL, MTX_DEF);
}
