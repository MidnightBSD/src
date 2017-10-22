/*
 * Copyright (c) 2003 Networks Associates Technology, Inc.
 * Copyright (c) 2006 Robert N. M. Watson
 * Copyright (c) 2006 Olivier Houchard
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project in part by Network
 * Associates Laboratories, the Security Research Division of Network
 * Associates, Inc. under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"),
 * as part of the DARPA CHATS research program.
 *
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)tty_pty.c	8.4 (Berkeley) 2/20/95
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/7.0.0/sys/kern/tty_pts.c 171228 2007-07-05 05:54:47Z peter $");

/*
 * Pseudo-teletype Driver
 * (Actually two drivers, requiring two entries in 'cdevsw')
 */
#include "opt_compat.h"
#include "opt_tty.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sx.h>
#if defined(COMPAT_43TTY)
#include <sys/ioctl_compat.h>
#endif
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/tty.h>
#include <sys/fcntl.h>
#include <sys/poll.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/signalvar.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/sysctl.h>
#include <sys/filio.h>

static MALLOC_DEFINE(M_PTY, "ptys", "pty data structures");

static void ptsstart(struct tty *tp);
static void ptsstop(struct tty *tp, int rw);
static void ptcwakeup(struct tty *tp, int flag);

static d_open_t		ptsopen;
static d_close_t	ptsclose;
static d_read_t		ptsread;
static d_write_t	ptswrite;
static d_ioctl_t	ptsioctl;
static d_ioctl_t	ptcioctl;
static d_open_t		ptcopen;
static d_close_t	ptcclose;
static d_read_t		ptcread;
static d_write_t	ptcwrite;
static d_poll_t		ptcpoll;

static struct cdevsw pts_cdevsw = {
	.d_version = 	D_VERSION,
	.d_open =	ptsopen,
	.d_close =	ptsclose,
	.d_read =	ptsread,
	.d_write =	ptswrite,
	.d_ioctl =	ptsioctl,
	.d_poll =	ttypoll,
	.d_name =	"pts",
	.d_flags =	D_TTY | D_NEEDGIANT,
	.d_kqfilter =	ttykqfilter,
};

static struct cdevsw ptc_cdevsw = {
	.d_version = 	D_VERSION,
	.d_open =	ptcopen,
	.d_close =	ptcclose,
	.d_read =	ptcread,
	.d_write =	ptcwrite,
	.d_ioctl =	ptcioctl,
	.d_poll =	ptcpoll,
	.d_name =	"ptc",
	.d_flags =	D_TTY | D_NEEDGIANT,
	.d_kqfilter =	ttykqfilter,
};

#define BUFSIZ 100		/* Chunk size iomoved to/from user */

#define TSA_PTC_READ(tp)	((void *)&(tp)->t_outq.c_cf)
#define TSA_PTC_WRITE(tp)	((void *)&(tp)->t_rawq.c_cl)
#define TSA_PTS_READ(tp)	((void *)&(tp)->t_canq)

#define NUM_TO_MINOR(c)		((c & 0xff) | ((c & ~0xff) << 16))
/*-
 * Once a tty is allocated, it cannot (currently) be freed.  As such,
 * we keep a global list of ptys that have been used so we can recycle
 * them.  Another list is provided for released pts, which are 
 * not currently allocated, permitting reuse.  pt_flags holds state
 * associated with a particular session, so isn't overloaded for this.
 * When a pty descriptor is unused, its number is set to -1 giving
 * more consistent and traditional allocation orders to pty numbers.
 *
 * Locking: (p) indicates that the field is locked by the global pt_mtx.
 * (c) indicates the value is constant after allocation.   Other fields
 * await tty locking generally, and are protected by Giant.
 */
struct	pt_desc {
	int			 pt_num;	/* (c) pty number */
	LIST_ENTRY(pt_desc)	 pt_list;	/* (p) global pty list */

	int			 pt_flags;
	struct selinfo		 pt_selr, pt_selw;
	u_char			 pt_send;
	u_char			 pt_ucntl;
	struct tty		 *pt_tty;
	struct cdev		 *pt_devs, *pt_devc;
	int			 pt_pts_open, pt_ptc_open;
	struct prison		*pt_prison;
};

static struct mtx		pt_mtx;
static LIST_HEAD(,pt_desc)	pt_list;
static LIST_HEAD(,pt_desc)	pt_free_list;

#define	PF_PKT		0x008		/* packet mode */
#define	PF_STOPPED	0x010		/* user told stopped */
#define	PF_NOSTOP	0x040
#define PF_UCNTL	0x080		/* user control mode */

static unsigned int next_avail_nb;

static int use_pts = 0;

static unsigned int max_pts = 1000;

static unsigned int nb_allocated;

TUNABLE_INT("kern.pts.enable", &use_pts);

SYSCTL_NODE(_kern, OID_AUTO, pts, CTLFLAG_RD, 0, "pts");

SYSCTL_INT(_kern_pts, OID_AUTO, enable, CTLFLAG_RW, &use_pts, 0,
    "enable pts");

SYSCTL_INT(_kern_pts, OID_AUTO, max, CTLFLAG_RW, &max_pts, 0, "max pts");

/*
 * If there's a free pty descriptor in the pty descriptor list, retrieve it.
 * Otherwise, allocate a new one, initialize it, and hook it up.  If there's
 * not a tty number, reject.
 */
static struct pt_desc *
pty_new(void)
{
	struct pt_desc *pt;
	int nb;

	mtx_lock(&pt_mtx);
	if (nb_allocated >= max_pts || nb_allocated == 0xffffff) {
		mtx_unlock(&pt_mtx);
		return (NULL);
	}
	nb_allocated++;
	pt = LIST_FIRST(&pt_free_list);
	if (pt) {
		LIST_REMOVE(pt, pt_list);
		LIST_INSERT_HEAD(&pt_list, pt, pt_list);
		mtx_unlock(&pt_mtx);
	} else {
		nb = next_avail_nb++;
		mtx_unlock(&pt_mtx);
		pt = malloc(sizeof(*pt), M_PTY, M_WAITOK | M_ZERO);
		mtx_lock(&pt_mtx);
		pt->pt_num = nb;
		LIST_INSERT_HEAD(&pt_list, pt, pt_list);
		mtx_unlock(&pt_mtx);
		pt->pt_tty = ttyalloc();
	}
	return (pt);
}

/*
 * Release a pty descriptor back to the pool for reuse.  The pty number
 * remains allocated.
 */
static void
pty_release(void *v)
{
	struct pt_desc *pt = (struct pt_desc *)v;

	mtx_lock(&pt_mtx);
	KASSERT(pt->pt_ptc_open == 0 && pt->pt_pts_open == 0,
	    ("pty_release: pts/%d freed while open\n", pt->pt_num));
	KASSERT(pt->pt_devs == NULL && pt->pt_devc == NULL,
	    ("pty_release: pts/%d freed whith non-null struct cdev\n", pt->pt_num));
	nb_allocated--;
	LIST_REMOVE(pt, pt_list);
	LIST_INSERT_HEAD(&pt_free_list, pt, pt_list);
	mtx_unlock(&pt_mtx);
}

/*
 * Given a pty descriptor, if both endpoints are closed, release all
 * resources and destroy the device nodes to flush file system level
 * state for the tty (owner, avoid races, etc).
 */
static void
pty_maybecleanup(struct pt_desc *pt)
{
	struct cdev *pt_devs, *pt_devc;

	if (pt->pt_ptc_open || pt->pt_pts_open)
		return;

	if (pt->pt_tty->t_refcnt > 1)
		return;

	if (bootverbose)
		printf("destroying pty %d\n", pt->pt_num);

	pt_devs = pt->pt_devs;
	pt_devc = pt->pt_devc;
	pt->pt_devs = pt->pt_devc = NULL;
	pt->pt_tty->t_dev = NULL;
	pt_devc->si_drv1 = NULL;
	ttyrel(pt->pt_tty);
	pt->pt_tty = NULL;
	destroy_dev_sched(pt_devs);
	destroy_dev_sched_cb(pt_devc, pty_release, pt);
}

/*ARGSUSED*/
static int
ptsopen(struct cdev *dev, int flag, int devtype, struct thread *td)
{
	struct tty *tp;
	int error;
	struct pt_desc *pt;

	pt = dev->si_drv1;
	tp = dev->si_tty;
	if ((tp->t_state & TS_ISOPEN) == 0)
		ttyinitmode(tp, 1, 0);
	else if (tp->t_state & TS_XCLUDE && priv_check(td,
	    PRIV_TTY_EXCLUSIVE)) {
		return (EBUSY);
	} else if (pt->pt_prison != td->td_ucred->cr_prison &&
	    priv_check(td, PRIV_TTY_PRISON)) {
		return (EBUSY);
	}
	if (tp->t_oproc)			/* Ctrlr still around. */
		ttyld_modem(tp, 1);
	while ((tp->t_state & TS_CARR_ON) == 0) {
		if (flag & FNONBLOCK)
			break;
		error = ttysleep(tp, TSA_CARR_ON(tp), TTIPRI | PCATCH,
				 "ptsopn", 0);
		if (error)
			return (error);
	}
	error = ttyld_open(tp, dev);
	if (error == 0) {
		ptcwakeup(tp, FREAD|FWRITE);
		pt->pt_pts_open = 1;
	}
	return (error);
}

static int
ptsclose(struct cdev *dev, int flag, int mode, struct thread *td)
{
	struct pt_desc *pt = dev->si_drv1;
	struct tty *tp;
	int err;

	tp = dev->si_tty;
	err = ttyld_close(tp, flag);
	ptsstop(tp, FREAD|FWRITE);
	(void) tty_close(tp);
	pt->pt_pts_open = 0;
	pty_maybecleanup(pt);
	return (err);
}

static int
ptsread(struct cdev *dev, struct uio *uio, int flag)
{
	struct tty *tp = dev->si_tty;
	int error = 0;

	if (tp->t_oproc)
		error = ttyld_read(tp, uio, flag);
	ptcwakeup(tp, FWRITE);
	return (error);
}

/*
 * Write to pseudo-tty.
 * Wakeups of controlling tty will happen
 * indirectly, when tty driver calls ptsstart.
 */
static int
ptswrite(struct cdev *dev, struct uio *uio, int flag)
{
	struct tty *tp;

	tp = dev->si_tty;
	if (tp->t_oproc == 0)
		return (EIO);
	return (ttyld_write(tp, uio, flag));
}

/*
 * Start output on pseudo-tty.
 * Wake up process selecting or sleeping for input from controlling tty.
 */
static void
ptsstart(struct tty *tp)
{
	struct pt_desc *pt = tp->t_dev->si_drv1;

	if (tp->t_state & TS_TTSTOP)
		return;
	if (pt->pt_flags & PF_STOPPED) {
		pt->pt_flags &= ~PF_STOPPED;
		pt->pt_send = TIOCPKT_START;
	}
	ptcwakeup(tp, FREAD);
}

static void
ptcwakeup(struct tty *tp, int flag)
{
	struct pt_desc *pt = tp->t_dev->si_drv1;

	if (flag & FREAD) {
		selwakeup(&pt->pt_selr);
		wakeup(TSA_PTC_READ(tp));
	}
	if (flag & FWRITE) {
		selwakeup(&pt->pt_selw);
		wakeup(TSA_PTC_WRITE(tp));
	}
}

/*
 * ptcopen implementes exclusive access to the master/control device
 * as well as creating the slave device based on the credential of the
 * process opening the master.  By creating the slave here, we avoid
 * a race to access the master in terms of having a process with access
 * to an incorrectly owned slave, but it does create the possibility
 * that a racing process can cause a ptmx user to get EIO if it gets
 * there first.  Consumers of ptmx must look for EIO and retry if it
 * happens.  VFS locking may actually prevent this from occurring due
 * to the lookup into devfs holding the vnode lock through open, but
 * it's better to be careful.
 */
static int
ptcopen(struct cdev *dev, int flag, int devtype, struct thread *td)
{
	struct pt_desc *pt;
	struct tty *tp;
	struct cdev *devs;

	pt = dev->si_drv1;
	if (pt == NULL)
		return (EIO);
	/*
	 * In case we have destroyed the struct tty at the last connect time,
	 * we need to recreate it.
	 */
	if (pt->pt_tty == NULL) {
		pt->pt_tty = ttyalloc();
		dev->si_tty = pt->pt_tty;
	}
	tp = dev->si_tty;
	if (tp->t_oproc)
		return (EIO);

	/*
	 * XXX: Might want to make the ownership/permissions here more
	 * configurable.
	 */
	if (pt->pt_devs)
		devs = pt->pt_devs;
	else
		pt->pt_devs = devs = make_dev_cred(&pts_cdevsw, 
		    NUM_TO_MINOR(pt->pt_num), 
		    td->td_ucred, UID_ROOT, GID_WHEEL, 0666, "pts/%d",
		    pt->pt_num);
	devs->si_drv1 = pt;
	devs->si_tty = pt->pt_tty;
	pt->pt_tty->t_dev = devs;

	tp->t_timeout = -1;
	tp->t_oproc = ptsstart;
	tp->t_stop = ptsstop;
	ttyld_modem(tp, 1);
	tp->t_lflag &= ~EXTPROC;
	pt = dev->si_drv1;
	pt->pt_prison = td->td_ucred->cr_prison;
	pt->pt_flags = 0;
	pt->pt_send = 0;
	pt->pt_ucntl = 0;
	pt->pt_ptc_open = 1;
	return (0);
}

static int
ptcclose(struct cdev *dev, int flags, int fmt, struct thread *td)
{
	struct pt_desc *pt = dev->si_drv1;
	struct tty *tp;

	tp = dev->si_tty;
	ttyld_modem(tp, 0);

	/*
	 * XXX MDMBUF makes no sense for ptys but would inhibit the above
	 * l_modem().  CLOCAL makes sense but isn't supported.   Special
	 * l_modem()s that ignore carrier drop make no sense for ptys but
	 * may be in use because other parts of the line discipline make
	 * sense for ptys.  Recover by doing everything that a normal
	 * ttymodem() would have done except for sending a SIGHUP.
	 */
	if (tp->t_state & TS_ISOPEN) {
		tp->t_state &= ~(TS_CARR_ON | TS_CONNECTED);
		tp->t_state |= TS_ZOMBIE;
		ttyflush(tp, FREAD | FWRITE);
	}

	tp->t_oproc = 0;		/* mark closed */
	pt->pt_ptc_open = 0;
	pty_maybecleanup(pt);
	return (0);
}

static int
ptcread(struct cdev *dev, struct uio *uio, int flag)
{
	struct tty *tp = dev->si_tty;
	struct pt_desc *pt = dev->si_drv1;
	char buf[BUFSIZ];
	int error = 0, cc;

	/*
	 * We want to block until the slave
	 * is open, and there's something to read;
	 * but if we lost the slave or we're NBIO,
	 * then return the appropriate error instead.
	 */
	for (;;) {
		if (tp->t_state&TS_ISOPEN) {
			if (pt->pt_flags&PF_PKT && pt->pt_send) {
				error = ureadc((int)pt->pt_send, uio);
				if (error)
					return (error);
				if (pt->pt_send & TIOCPKT_IOCTL) {
					cc = min(uio->uio_resid,
						sizeof(tp->t_termios));
					uiomove(&tp->t_termios, cc, uio);
				}
				pt->pt_send = 0;
				return (0);
			}
			if (pt->pt_flags&PF_UCNTL && pt->pt_ucntl) {
				error = ureadc((int)pt->pt_ucntl, uio);
				if (error)
					return (error);
				pt->pt_ucntl = 0;
				return (0);
			}
			if (tp->t_outq.c_cc && (tp->t_state&TS_TTSTOP) == 0)
				break;
		}
		if ((tp->t_state & TS_CONNECTED) == 0)
			return (0);	/* EOF */
		if (flag & O_NONBLOCK)
			return (EWOULDBLOCK);
		error = tsleep(TSA_PTC_READ(tp), TTIPRI | PCATCH, "ptcin", 0);
		if (error)
			return (error);
	}
	if (pt->pt_flags & (PF_PKT|PF_UCNTL))
		error = ureadc(0, uio);
	while (uio->uio_resid > 0 && error == 0) {
		cc = q_to_b(&tp->t_outq, buf, min(uio->uio_resid, BUFSIZ));
		if (cc <= 0)
			break;
		error = uiomove(buf, cc, uio);
	}
	ttwwakeup(tp);
	return (error);
}

static void
ptsstop(struct tty *tp, int flush)
{
	struct pt_desc *pt = tp->t_dev->si_drv1;
	int flag;

	/* note: FLUSHREAD and FLUSHWRITE already ok */
	if (flush == 0) {
		flush = TIOCPKT_STOP;
		pt->pt_flags |= PF_STOPPED;
	} else
		pt->pt_flags &= ~PF_STOPPED;
	pt->pt_send |= flush;
	/* change of perspective */
	flag = 0;
	if (flush & FREAD)
		flag |= FWRITE;
	if (flush & FWRITE)
		flag |= FREAD;
	ptcwakeup(tp, flag);
}

static int
ptcpoll(struct cdev *dev, int events, struct thread *td)
{
	struct tty *tp = dev->si_tty;
	struct pt_desc *pt = dev->si_drv1;
	int revents = 0;
	int s;

	if ((tp->t_state & TS_CONNECTED) == 0)
		return (events & 
		   (POLLHUP | POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM));

	/*
	 * Need to block timeouts (ttrstart).
	 */
	s = spltty();

	if (events & (POLLIN | POLLRDNORM))
		if ((tp->t_state & TS_ISOPEN) &&
		    ((tp->t_outq.c_cc && (tp->t_state & TS_TTSTOP) == 0) ||
		     ((pt->pt_flags & PF_PKT) && pt->pt_send) ||
		     ((pt->pt_flags & PF_UCNTL) && pt->pt_ucntl)))
			revents |= events & (POLLIN | POLLRDNORM);

	if (events & (POLLOUT | POLLWRNORM))
		if (tp->t_state & TS_ISOPEN &&
		     (((tp->t_rawq.c_cc + tp->t_canq.c_cc < TTYHOG - 2) ||
		      (tp->t_canq.c_cc == 0 && (tp->t_lflag & ICANON)))))
			revents |= events & (POLLOUT | POLLWRNORM);

	if (events & POLLHUP)
		if ((tp->t_state & TS_CARR_ON) == 0)
			revents |= POLLHUP;

	if (revents == 0) {
		if (events & (POLLIN | POLLRDNORM))
			selrecord(td, &pt->pt_selr);

		if (events & (POLLOUT | POLLWRNORM))
			selrecord(td, &pt->pt_selw);
	}
	splx(s);

	return (revents);
}

static int
ptcwrite(struct cdev *dev, struct uio *uio, int flag)
{
	struct tty *tp = dev->si_tty;
	u_char *cp = 0;
	int cc = 0;
	u_char locbuf[BUFSIZ];
	int cnt = 0;
	int error = 0;

again:
	if ((tp->t_state&TS_ISOPEN) == 0)
		goto block;
	while (uio->uio_resid > 0 || cc > 0) {
		if (cc == 0) {
			cc = min(uio->uio_resid, BUFSIZ);
			cp = locbuf;
			error = uiomove(cp, cc, uio);
			if (error)
				return (error);
			/* check again for safety */
			if ((tp->t_state & TS_ISOPEN) == 0) {
				/* adjust for data copied in but not written */
				uio->uio_resid += cc;
				return (EIO);
			}
		}
		while (cc > 0) {
			if ((tp->t_rawq.c_cc + tp->t_canq.c_cc) >= TTYHOG - 2 &&
			   (tp->t_canq.c_cc > 0 || !(tp->t_lflag&ICANON))) {
				wakeup(TSA_HUP_OR_INPUT(tp));
				goto block;
			}
			ttyld_rint(tp, *cp++);
			cnt++;
			cc--;
		}
		cc = 0;
	}
	return (0);
block:
	/*
	 * Come here to wait for slave to open, for space
	 * in outq, or space in rawq, or an empty canq.
	 */
	if ((tp->t_state & TS_CONNECTED) == 0) {
		/* adjust for data copied in but not written */
		uio->uio_resid += cc;
		return (EIO);
	}
	if (flag & IO_NDELAY) {
		/* adjust for data copied in but not written */
		uio->uio_resid += cc;
		if (cnt == 0)
			return (EWOULDBLOCK);
		return (0);
	}
	error = tsleep(TSA_PTC_WRITE(tp), TTOPRI | PCATCH, "ptcout", 0);
	if (error) {
		/* adjust for data copied in but not written */
		uio->uio_resid += cc;
		return (error);
	}
	goto again;
}

static int
ptcioctl(struct cdev *dev, u_long cmd, caddr_t data, int flag, struct thread *td)
{
	struct tty *tp = dev->si_tty;
	struct pt_desc *pt = dev->si_drv1;
#if defined(COMPAT_FREEBSD6) || defined(COMPAT_FREEBSD5) || \
    defined(COMPAT_FREEBSD4) || defined(COMPAT_43)
	int ival;
#endif

	switch (cmd) {
		
	case TIOCGPGRP:
		/*
		 * We avoid calling ttioctl on the controller since,
		 * in that case, tp must be the controlling terminal.
		 */
		*(int *)data = tp->t_pgrp ? tp->t_pgrp->pg_id : 0;
		return (0);
		
	case TIOCPKT:
		if (*(int *)data) {
			if (pt->pt_flags & PF_UCNTL)
				return (EINVAL);
			pt->pt_flags |= PF_PKT;
		} else
			pt->pt_flags &= ~PF_PKT;
		return (0);
		
	case TIOCUCNTL:
		if (*(int *)data) {
			if (pt->pt_flags & PF_PKT)
				return (EINVAL);
			pt->pt_flags |= PF_UCNTL;
		} else
			pt->pt_flags &= ~PF_UCNTL;
		return (0);
	case TIOCGPTN:
		*(unsigned int *)data = pt->pt_num;
		return (0);
	}
	
	/*
	 * The rest of the ioctls shouldn't be called until
	 * the slave is open.
	 */
	if ((tp->t_state & TS_ISOPEN) == 0) {
		if (cmd == TIOCGETA) {
			/* 
			 * TIOCGETA is used by isatty() to make sure it's
			 * a tty. Linux openpty() calls isatty() very early,
			 * before the slave is opened, so don't actually
			 * fill the struct termios, but just let isatty()
			 * know it's a tty.
			 */
			return (0);
		}
		if (cmd != FIONBIO && cmd != FIOASYNC)
			return (EAGAIN);
	}
	
	switch (cmd) {
#ifdef COMPAT_43TTY
	case TIOCSETP:
	case TIOCSETN:
#endif
	case TIOCSETD:
	case TIOCSETA:
	case TIOCSETAW:
	case TIOCSETAF:
		/*
		 * IF CONTROLLER STTY THEN MUST FLUSH TO PREVENT A HANG.
		 * ttywflush(tp) will hang if there are characters in
		 * the outq.
		 */
		ndflush(&tp->t_outq, tp->t_outq.c_cc);
		break;
		
#if defined(COMPAT_FREEBSD6) || defined(COMPAT_FREEBSD5) || \
    defined(COMPAT_FREEBSD4) || defined(COMPAT_43)
	case _IO('t', 95):
		ival = IOCPARM_IVAL(data);
		data = (caddr_t)&ival;
		/* FALLTHROUGH */
#endif
	case TIOCSIG:
		if (*(unsigned int *)data >= NSIG ||
		    *(unsigned int *)data == 0)
			return(EINVAL);
		if ((tp->t_lflag&NOFLSH) == 0)
			ttyflush(tp, FREAD|FWRITE);
		if (tp->t_pgrp != NULL) {
			PGRP_LOCK(tp->t_pgrp);
			pgsignal(tp->t_pgrp, *(unsigned int *)data, 1);
			PGRP_UNLOCK(tp->t_pgrp);
		}
		if ((*(unsigned int *)data == SIGINFO) &&
		    ((tp->t_lflag&NOKERNINFO) == 0))
			ttyinfo(tp);
		return(0);
	}
	return (ptsioctl(dev, cmd, data, flag, td));
}
/*ARGSUSED*/
static int
ptsioctl(struct cdev *dev, u_long cmd, caddr_t data, int flag, struct thread *td)
{
	struct tty *tp = dev->si_tty;
	struct pt_desc *pt = dev->si_drv1;
	u_char *cc = tp->t_cc;
	int stop, error;

	if (cmd == TIOCEXT) {
		/*
		 * When the EXTPROC bit is being toggled, we need
		 * to send an TIOCPKT_IOCTL if the packet driver
		 * is turned on.
		 */
		if (*(int *)data) {
			if (pt->pt_flags & PF_PKT) {
				pt->pt_send |= TIOCPKT_IOCTL;
				ptcwakeup(tp, FREAD);
			}
			tp->t_lflag |= EXTPROC;
		} else {
			if ((tp->t_lflag & EXTPROC) &&
			    (pt->pt_flags & PF_PKT)) {
				pt->pt_send |= TIOCPKT_IOCTL;
				ptcwakeup(tp, FREAD);
			}
			tp->t_lflag &= ~EXTPROC;
		}
		return(0);
	}
	error = ttioctl(tp, cmd, data, flag);
	if (error == ENOTTY) {
		if (pt->pt_flags & PF_UCNTL &&
		    (cmd & ~0xff) == UIOCCMD(0)) {
			if (cmd & 0xff) {
				pt->pt_ucntl = (u_char)cmd;
				ptcwakeup(tp, FREAD);
			}
			return (0);
		}
		error = ENOTTY;
	}
	/*
	 * If external processing and packet mode send ioctl packet.
	 */
	if ((tp->t_lflag&EXTPROC) && (pt->pt_flags & PF_PKT)) {
		switch(cmd) {
		case TIOCSETA:
		case TIOCSETAW:
		case TIOCSETAF:
#ifdef COMPAT_43TTY
		case TIOCSETP:
		case TIOCSETN:
		case TIOCSETC:
		case TIOCSLTC:
		case TIOCLBIS:
		case TIOCLBIC:
		case TIOCLSET:
#endif
			pt->pt_send |= TIOCPKT_IOCTL;
			ptcwakeup(tp, FREAD);
			break;
		default:
			break;
		}
	}
	stop = (tp->t_iflag & IXON) && CCEQ(cc[VSTOP], CTRL('s'))
		&& CCEQ(cc[VSTART], CTRL('q'));
	if (pt->pt_flags & PF_NOSTOP) {
		if (stop) {
			pt->pt_send &= ~TIOCPKT_NOSTOP;
			pt->pt_send |= TIOCPKT_DOSTOP;
			pt->pt_flags &= ~PF_NOSTOP;
			ptcwakeup(tp, FREAD);
		}
	} else {
		if (!stop) {
			pt->pt_send &= ~TIOCPKT_DOSTOP;
			pt->pt_send |= TIOCPKT_NOSTOP;
			pt->pt_flags |= PF_NOSTOP;
			ptcwakeup(tp, FREAD);
		}
	}
	return (error);
}

/*
 * Match lookups on /dev/ptmx, find the next free pty (if any), set up
 * the pty descriptor, register it, and return a reference to the master.
 *
 * pts == /dev/pts/xxx (oldstyle: ttyp...)
 * ptc == /dev/pty/xxx (oldstyle: ptyp...)
 */
static void
pty_clone(void *arg, struct ucred *cred, char *name, int namelen,
    struct cdev **dev)
{
	struct pt_desc *pt;
	struct cdev *devc;

	if (!use_pts)
		return;

	if (*dev != NULL)
		return;

	if (strcmp(name, "ptmx") != 0)
		return;

	mtx_lock(&Giant);
	pt = pty_new();
	if (pt == NULL) {
		mtx_unlock(&Giant);
		return;
	}

	/*
	 * XXX: Lack of locking here considered worrying.  We expose the
	 * pts/pty device nodes before they are fully initialized, although
	 * Giant likely protects us (unless make_dev blocks...?).
	 *
	 * XXX: If a process performs a lookup on /dev/ptmx but never an
	 * open, we won't GC the device node.  We should have a callout
	 * sometime later that GC's device instances that were never
	 * opened, or some way to tell devfs that "this had better be for
	 * an open() or we won't create a device".
	 */
	pt->pt_devc = devc = make_dev_credf(MAKEDEV_REF, &ptc_cdevsw, 
	    NUM_TO_MINOR(pt->pt_num), cred, UID_ROOT, GID_WHEEL, 0666,
	    "pty/%d", pt->pt_num);

	devc->si_drv1 = pt;
	devc->si_tty = pt->pt_tty;
	*dev = devc;
	mtx_unlock(&Giant);

	if (bootverbose)
		printf("pty_clone: allocated pty %d to uid %d\n", pt->pt_num,
	    cred->cr_ruid);

	return;
}

static void
pty_drvinit(void *unused)
{

	mtx_init(&pt_mtx, "pt_mtx", NULL, MTX_DEF);
	LIST_INIT(&pt_list);
	LIST_INIT(&pt_free_list);
	EVENTHANDLER_REGISTER(dev_clone, pty_clone, 0, 1000);
}

SYSINIT(ptydev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE,pty_drvinit,NULL)
