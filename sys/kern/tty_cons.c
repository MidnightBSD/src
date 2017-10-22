/*-
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 *	from: @(#)cons.c	7.2 (Berkeley) 5/9/91
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/7.0.0/sys/kern/tty_cons.c 170152 2007-05-31 11:51:53Z kib $");

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/fcntl.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/msgbuf.h>
#include <sys/namei.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/reboot.h>
#include <sys/sysctl.h>
#include <sys/tty.h>
#include <sys/uio.h>
#include <sys/vnode.h>

#include <ddb/ddb.h>

#include <machine/cpu.h>

static	d_open_t	cnopen;
static	d_close_t	cnclose;
static	d_read_t	cnread;
static	d_write_t	cnwrite;
static	d_ioctl_t	cnioctl;
static	d_poll_t	cnpoll;
static	d_kqfilter_t	cnkqfilter;

static struct cdevsw cn_cdevsw = {
	.d_version =	D_VERSION,
	.d_open =	cnopen,
	.d_close =	cnclose,
	.d_read =	cnread,
	.d_write =	cnwrite,
	.d_ioctl =	cnioctl,
	.d_poll =	cnpoll,
	.d_name =	"console",
	.d_flags =	D_TTY | D_NEEDGIANT,
	.d_kqfilter =	cnkqfilter,
};

struct cn_device {
	STAILQ_ENTRY(cn_device) cnd_next;
	struct		vnode *cnd_vp;
	struct		consdev *cnd_cn;
};

#define CNDEVPATHMAX	32
#define CNDEVTAB_SIZE	4
static struct cn_device cn_devtab[CNDEVTAB_SIZE];
static STAILQ_HEAD(, cn_device) cn_devlist =
    STAILQ_HEAD_INITIALIZER(cn_devlist);

#define CND_INVALID(cnd, td) 						\
	(cnd == NULL || cnd->cnd_vp == NULL ||				\
	    (cnd->cnd_vp->v_type == VBAD && !cn_devopen(cnd, td, 1)))

static dev_t	cn_udev_t;
SYSCTL_OPAQUE(_machdep, OID_AUTO, consdev, CTLFLAG_RD,
	&cn_udev_t, sizeof cn_udev_t, "T,struct cdev *", "");

int	cons_avail_mask = 0;	/* Bit mask. Each registered low level console
				 * which is currently unavailable for inpit
				 * (i.e., if it is in graphics mode) will have
				 * this bit cleared.
				 */
static int cn_mute;
static int openflag;			/* how /dev/console was opened */
static int cn_is_open;
static char *consbuf;			/* buffer used by `consmsgbuf' */
static struct callout conscallout;	/* callout for outputting to constty */
struct msgbuf consmsgbuf;		/* message buffer for console tty */
static u_char console_pausing;		/* pause after each line during probe */
static char *console_pausestr=
"<pause; press any key to proceed to next line or '.' to end pause mode>";
struct tty *constty;			/* pointer to console "window" tty */
static struct mtx cnputs_mtx;		/* Mutex for cnputs(). */
static int use_cnputs_mtx = 0;		/* != 0 if cnputs_mtx locking reqd. */

static void constty_timeout(void *arg);

static struct consdev cons_consdev;
DATA_SET(cons_set, cons_consdev);
SET_DECLARE(cons_set, struct consdev);

void
cninit(void)
{
	struct consdev *best_cn, *cn, **list;

	/*
	 * Check if we should mute the console (for security reasons perhaps)
	 * It can be changes dynamically using sysctl kern.consmute
	 * once we are up and going.
	 * 
	 */
        cn_mute = ((boothowto & (RB_MUTE
			|RB_SINGLE
			|RB_VERBOSE
			|RB_ASKNAME)) == RB_MUTE);

	/*
	 * Find the first console with the highest priority.
	 */
	best_cn = NULL;
	SET_FOREACH(list, cons_set) {
		cn = *list;
		cnremove(cn);
		if (cn->cn_probe == NULL)
			continue;
		cn->cn_probe(cn);
		if (cn->cn_pri == CN_DEAD)
			continue;
		if (best_cn == NULL || cn->cn_pri > best_cn->cn_pri)
			best_cn = cn;
		if (boothowto & RB_MULTIPLE) {
			/*
			 * Initialize console, and attach to it.
			 */
			cn->cn_init(cn);
			cnadd(cn);
		}
	}
	if (best_cn == NULL)
		return;
	if ((boothowto & RB_MULTIPLE) == 0) {
		best_cn->cn_init(best_cn);
		cnadd(best_cn);
	}
	if (boothowto & RB_PAUSE)
		console_pausing = 1;
	/*
	 * Make the best console the preferred console.
	 */
	cnselect(best_cn);
}

void
cninit_finish()
{
	console_pausing = 0;
} 

/* add a new physical console to back the virtual console */
int
cnadd(struct consdev *cn)
{
	struct cn_device *cnd;
	int i;

	STAILQ_FOREACH(cnd, &cn_devlist, cnd_next)
		if (cnd->cnd_cn == cn)
			return (0);
	for (i = 0; i < CNDEVTAB_SIZE; i++) {
		cnd = &cn_devtab[i];
		if (cnd->cnd_cn == NULL)
			break;
	}
	if (cnd->cnd_cn != NULL)
		return (ENOMEM);
	cnd->cnd_cn = cn;
	if (cn->cn_name[0] == '\0') {
		/* XXX: it is unclear if/where this print might output */
		printf("WARNING: console at %p has no name\n", cn);
	}
	STAILQ_INSERT_TAIL(&cn_devlist, cnd, cnd_next);

	/* Add device to the active mask. */
	cnavailable(cn, (cn->cn_flags & CN_FLAG_NOAVAIL) == 0);

	return (0);
}

void
cnremove(struct consdev *cn)
{
	struct cn_device *cnd;
	int i;

	STAILQ_FOREACH(cnd, &cn_devlist, cnd_next) {
		if (cnd->cnd_cn != cn)
			continue;
		STAILQ_REMOVE(&cn_devlist, cnd, cn_device, cnd_next);
		if (cnd->cnd_vp != NULL)
			vn_close(cnd->cnd_vp, openflag, NOCRED, NULL);
		cnd->cnd_vp = NULL;
		cnd->cnd_cn = NULL;

		/* Remove this device from available mask. */
		for (i = 0; i < CNDEVTAB_SIZE; i++) 
			if (cnd == &cn_devtab[i]) {
				cons_avail_mask &= ~(1 << i);
				break;
			}
#if 0
		/*
		 * XXX
		 * syscons gets really confused if console resources are
		 * freed after the system has initialized.
		 */
		if (cn->cn_term != NULL)
			cn->cn_term(cn);
#endif
		return;
	}
}

void
cnselect(struct consdev *cn)
{
	struct cn_device *cnd;

	STAILQ_FOREACH(cnd, &cn_devlist, cnd_next) {
		if (cnd->cnd_cn != cn)
			continue;
		if (cnd == STAILQ_FIRST(&cn_devlist))
			return;
		STAILQ_REMOVE(&cn_devlist, cnd, cn_device, cnd_next);
		STAILQ_INSERT_HEAD(&cn_devlist, cnd, cnd_next);
		return;
	}
}

void
cnavailable(struct consdev *cn, int available)
{
	int i;

	for (i = 0; i < CNDEVTAB_SIZE; i++) {
		if (cn_devtab[i].cnd_cn == cn)
			break;
	}
	if (available) {
		if (i < CNDEVTAB_SIZE)
			cons_avail_mask |= (1 << i); 
		cn->cn_flags &= ~CN_FLAG_NOAVAIL;
	} else {
		if (i < CNDEVTAB_SIZE)
			cons_avail_mask &= ~(1 << i);
		cn->cn_flags |= CN_FLAG_NOAVAIL;
	}
}

int
cnunavailable(void)
{

	return (cons_avail_mask == 0);
}

/*
 * XXX: rewrite to use sbufs instead
 */

static int
sysctl_kern_console(SYSCTL_HANDLER_ARGS)
{
	struct cn_device *cnd;
	struct consdev *cp, **list;
	char *name, *p;
	int delete, len, error;

	len = 2;
	SET_FOREACH(list, cons_set) {
		cp = *list;
		if (cp->cn_name[0] != '\0')
			len += strlen(cp->cn_name) + 1;
	}
	STAILQ_FOREACH(cnd, &cn_devlist, cnd_next)
		len += strlen(cnd->cnd_cn->cn_name) + 1;
	len = len > CNDEVPATHMAX ? len : CNDEVPATHMAX;
	MALLOC(name, char *, len, M_TEMP, M_WAITOK | M_ZERO);
	p = name;
	STAILQ_FOREACH(cnd, &cn_devlist, cnd_next)
		p += sprintf(p, "%s,", cnd->cnd_cn->cn_name);
	*p++ = '/';
	SET_FOREACH(list, cons_set) {
		cp = *list;
		if (cp->cn_name[0] != '\0')
			p += sprintf(p, "%s,", cp->cn_name);
	}
	error = sysctl_handle_string(oidp, name, len, req);
	if (error == 0 && req->newptr != NULL) {
		p = name;
		error = ENXIO;
		delete = 0;
		if (*p == '-') {
			delete = 1;
			p++;
		}
		SET_FOREACH(list, cons_set) {
			cp = *list;
			if (strcmp(p, cp->cn_name) != 0)
				continue;
			if (delete) {
				cnremove(cp);
				error = 0;
			} else {
				error = cnadd(cp);
				if (error == 0)
					cnselect(cp);
			}
			break;
		}
	}
	FREE(name, M_TEMP);
	return (error);
}

SYSCTL_PROC(_kern, OID_AUTO, console, CTLTYPE_STRING|CTLFLAG_RW,
	0, 0, sysctl_kern_console, "A", "Console device control");

/*
 * User has changed the state of the console muting.
 * This may require us to open or close the device in question.
 */
static int
sysctl_kern_consmute(SYSCTL_HANDLER_ARGS)
{
	int error;
	int ocn_mute;

	ocn_mute = cn_mute;
	error = sysctl_handle_int(oidp, &cn_mute, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (ocn_mute && !cn_mute && cn_is_open)
		error = cnopen(NULL, openflag, 0, curthread);
	else if (!ocn_mute && cn_mute && cn_is_open) {
		error = cnclose(NULL, openflag, 0, curthread);
		cn_is_open = 1;		/* XXX hack */
	}
	return (error);
}

SYSCTL_PROC(_kern, OID_AUTO, consmute, CTLTYPE_INT|CTLFLAG_RW,
	0, sizeof(cn_mute), sysctl_kern_consmute, "I", "");

static int
cn_devopen(struct cn_device *cnd, struct thread *td, int forceopen)
{
	char path[CNDEVPATHMAX];
	struct nameidata nd;
	struct vnode *vp;
	struct cdev *dev;
	struct cdevsw *csw;
	int error;

	if ((vp = cnd->cnd_vp) != NULL) {
		if (!forceopen && vp->v_type != VBAD) {
			dev = vp->v_rdev;
			csw = dev_refthread(dev);
			if (csw == NULL)
				return (ENXIO);
			error = (*csw->d_open)(dev, openflag, 0, td);
			dev_relthread(dev);
			return (error);
		}
		cnd->cnd_vp = NULL;
		vn_close(vp, openflag, td->td_ucred, td);
	}
	snprintf(path, sizeof(path), "/dev/%s", cnd->cnd_cn->cn_name);
	NDINIT(&nd, LOOKUP, FOLLOW, UIO_SYSSPACE, path, td);
	error = vn_open(&nd, &openflag, 0, NULL);
	if (error == 0) {
		NDFREE(&nd, NDF_ONLY_PNBUF);
		VOP_UNLOCK(nd.ni_vp, 0, td);
		if (nd.ni_vp->v_type == VCHR)
			cnd->cnd_vp = nd.ni_vp;
		else
			vn_close(nd.ni_vp, openflag, td->td_ucred, td);
	}
	return (cnd->cnd_vp != NULL);
}

static int
cnopen(struct cdev *dev, int flag, int mode, struct thread *td)
{
	struct cn_device *cnd;

	openflag = flag | FWRITE;	/* XXX */
	cn_is_open = 1;			/* console is logically open */
	if (cn_mute)
		return (0);
	STAILQ_FOREACH(cnd, &cn_devlist, cnd_next)
		cn_devopen(cnd, td, 0);
	return (0);
}

static int
cnclose(struct cdev *dev, int flag, int mode, struct thread *td)
{
	struct cn_device *cnd;
	struct vnode *vp;

	STAILQ_FOREACH(cnd, &cn_devlist, cnd_next) {
		if ((vp = cnd->cnd_vp) == NULL)
			continue; 
		cnd->cnd_vp = NULL;
		vn_close(vp, openflag, td->td_ucred, td);
	}
	cn_is_open = 0;
	return (0);
}

static int
cnread(struct cdev *dev, struct uio *uio, int flag)
{
	struct cn_device *cnd;
	struct cdevsw *csw;
	int error;

	cnd = STAILQ_FIRST(&cn_devlist);
	if (cn_mute || CND_INVALID(cnd, curthread))
		return (0);
	dev = cnd->cnd_vp->v_rdev;
	csw = dev_refthread(dev);
	if (csw == NULL)
		return (ENXIO);
	error = (csw->d_read)(dev, uio, flag);
	dev_relthread(dev);
	return (error);
}

static int
cnwrite(struct cdev *dev, struct uio *uio, int flag)
{
	struct cn_device *cnd;
	struct cdevsw *csw;
	int error;

	cnd = STAILQ_FIRST(&cn_devlist);
	if (cn_mute || CND_INVALID(cnd, curthread))
		goto done;
	if (constty)
		dev = constty->t_dev;
	else
		dev = cnd->cnd_vp->v_rdev;
	if (dev != NULL) {
		log_console(uio);
		csw = dev_refthread(dev);
		if (csw == NULL)
			return (ENXIO);
		error = (csw->d_write)(dev, uio, flag);
		dev_relthread(dev);
		return (error);
	}
done:
	uio->uio_resid = 0; /* dump the data */
	return (0);
}

static int
cnioctl(struct cdev *dev, u_long cmd, caddr_t data, int flag, struct thread *td)
{
	struct cn_device *cnd;
	struct cdevsw *csw;
	int error;

	cnd = STAILQ_FIRST(&cn_devlist);
	if (cn_mute || CND_INVALID(cnd, td))
		return (0);
	/*
	 * Superuser can always use this to wrest control of console
	 * output from the "virtual" console.
	 */
	if (cmd == TIOCCONS && constty) {
		error = priv_check(td, PRIV_TTY_CONSOLE);
		if (error)
			return (error);
		constty = NULL;
		return (0);
	}
	dev = cnd->cnd_vp->v_rdev;
	if (dev == NULL)
		return (0);	/* XXX : ENOTTY ? */
	csw = dev_refthread(dev);
	if (csw == NULL)
		return (ENXIO);
	error = (csw->d_ioctl)(dev, cmd, data, flag, td);
	dev_relthread(dev);
	return (error);
}

/*
 * XXX
 * poll/kqfilter do not appear to be correct
 */
static int
cnpoll(struct cdev *dev, int events, struct thread *td)
{
	struct cn_device *cnd;
	struct cdevsw *csw;
	int error;

	cnd = STAILQ_FIRST(&cn_devlist);
	if (cn_mute || CND_INVALID(cnd, td))
		return (0);
	dev = cnd->cnd_vp->v_rdev;
	if (dev == NULL)
		return (0);
	csw = dev_refthread(dev);
	if (csw == NULL)
		return (ENXIO);
	error = (csw->d_poll)(dev, events, td);
	dev_relthread(dev);
	return (error);
}

static int
cnkqfilter(struct cdev *dev, struct knote *kn)
{
	struct cn_device *cnd;
	struct cdevsw *csw;
	int error;

	cnd = STAILQ_FIRST(&cn_devlist);
	if (cn_mute || CND_INVALID(cnd, curthread))
		return (EINVAL);
	dev = cnd->cnd_vp->v_rdev;
	if (dev == NULL)
		return (ENXIO);
	csw = dev_refthread(dev);
	if (csw == NULL)
		return (ENXIO);
	error = (csw->d_kqfilter)(dev, kn);
	dev_relthread(dev);
	return (error);
}

/*
 * Low level console routines.
 */
int
cngetc(void)
{
	int c;

	if (cn_mute)
		return (-1);
	while ((c = cncheckc()) == -1)
		;
	if (c == '\r')
		c = '\n';		/* console input is always ICRNL */
	return (c);
}

int
cncheckc(void)
{
	struct cn_device *cnd;
	struct consdev *cn;
	int c;

	if (cn_mute)
		return (-1);
	STAILQ_FOREACH(cnd, &cn_devlist, cnd_next) {
		cn = cnd->cnd_cn;
		if (!kdb_active || !(cn->cn_flags & CN_FLAG_NODEBUG)) {
			if (cn->cn_checkc != NULL)
				c = cn->cn_checkc(cn);
			else
				c = cn->cn_getc(cn);
			if (c != -1) {
				return (c);
			}
		}
	}
	return (-1);
}

void
cnputc(int c)
{
	struct cn_device *cnd;
	struct consdev *cn;
	char *cp;

	if (cn_mute || c == '\0')
		return;
	STAILQ_FOREACH(cnd, &cn_devlist, cnd_next) {
		cn = cnd->cnd_cn;
		if (!kdb_active || !(cn->cn_flags & CN_FLAG_NODEBUG)) {
			if (c == '\n')
				cn->cn_putc(cn, '\r');
			cn->cn_putc(cn, c);
		}
	}
	if (console_pausing && c == '\n' && !kdb_active) {
		for (cp = console_pausestr; *cp != '\0'; cp++)
			cnputc(*cp);
		if (cngetc() == '.')
			console_pausing = 0;
		cnputc('\r');
		for (cp = console_pausestr; *cp != '\0'; cp++)
			cnputc(' ');
		cnputc('\r');
	}
}

void
cnputs(char *p)
{
	int c;
	int unlock_reqd = 0;

	if (use_cnputs_mtx) {
		mtx_lock_spin(&cnputs_mtx);
		unlock_reqd = 1;
	}

	while ((c = *p++) != '\0')
		cnputc(c);

	if (unlock_reqd)
		mtx_unlock_spin(&cnputs_mtx);
}

static int consmsgbuf_size = 8192;
SYSCTL_INT(_kern, OID_AUTO, consmsgbuf_size, CTLFLAG_RW, &consmsgbuf_size, 0,
    "");

/*
 * Redirect console output to a tty.
 */
void
constty_set(struct tty *tp)
{
	int size;

	KASSERT(tp != NULL, ("constty_set: NULL tp"));
	if (consbuf == NULL) {
		size = consmsgbuf_size;
		consbuf = malloc(size, M_TTYS, M_WAITOK);
		msgbuf_init(&consmsgbuf, consbuf, size);
		callout_init(&conscallout, 0);
	}
	constty = tp;
	constty_timeout(NULL);
}

/*
 * Disable console redirection to a tty.
 */
void
constty_clear(void)
{
	int c;

	constty = NULL;
	if (consbuf == NULL)
		return;
	callout_stop(&conscallout);
	while ((c = msgbuf_getchar(&consmsgbuf)) != -1)
		cnputc(c);
	free(consbuf, M_TTYS);
	consbuf = NULL;
}

/* Times per second to check for pending console tty messages. */
static int constty_wakeups_per_second = 5;
SYSCTL_INT(_kern, OID_AUTO, constty_wakeups_per_second, CTLFLAG_RW,
    &constty_wakeups_per_second, 0, "");

static void
constty_timeout(void *arg)
{
	int c;

	while (constty != NULL && (c = msgbuf_getchar(&consmsgbuf)) != -1) {
		if (tputchar(c, constty) < 0)
			constty = NULL;
	}
	if (constty != NULL) {
		callout_reset(&conscallout, hz / constty_wakeups_per_second,
		    constty_timeout, NULL);
	} else {
		/* Deallocate the constty buffer memory. */
		constty_clear();
	}
}

static void
cn_drvinit(void *unused)
{

	make_dev(&cn_cdevsw, 0, UID_ROOT, GID_WHEEL, 0600, "console");

	mtx_init(&cnputs_mtx, "cnputs_mtx", NULL, MTX_SPIN | MTX_NOWITNESS);
	use_cnputs_mtx = 1;
}

SYSINIT(cndev, SI_SUB_DRIVERS, SI_ORDER_MIDDLE, cn_drvinit, NULL)
