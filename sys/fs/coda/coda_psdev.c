/*-
 *             Coda: an Experimental Distributed File System
 *                              Release 3.1
 *
 *           Copyright (c) 1987-1998 Carnegie Mellon University
 *                          All Rights Reserved
 *
 * Permission  to  use, copy, modify and distribute this software and its
 * documentation is hereby granted,  provided  that  both  the  copyright
 * notice  and  this  permission  notice  appear  in  all  copies  of the
 * software, derivative works or  modified  versions,  and  any  portions
 * thereof, and that both notices appear in supporting documentation, and
 * that credit is given to Carnegie Mellon University  in  all  documents
 * and publicity pertaining to direct or indirect use of this code or its
 * derivatives.
 *
 * CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
 * SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
 * FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
 * DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
 * RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
 * ANY DERIVATIVE WORK.
 *
 * Carnegie  Mellon  encourages  users  of  this  software  to return any
 * improvements or extensions that  they  make,  and  to  grant  Carnegie
 * Mellon the rights to redistribute these changes without encumbrance.
 *
 * 	@(#) src/sys/coda/coda_psdev.c,v 1.1.1.1 1998/08/29 21:14:52 rvb Exp $
 */
/*-
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */

/*
 * This code was written for the Coda filesystem at Carnegie Mellon
 * University.  Contributers include David Steere, James Kistler, and
 * M. Satyanarayanan.  */

/*
 * These routines define the psuedo device for communication between Coda's
 * Venus and Minicache in Mach 2.6. They used to be in cfs_subr.c, but I
 * moved them to make it easier to port the Minicache without porting coda.
 * -- DCS 10/12/94
 */

/*
 * These routines are the device entry points for Venus.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/capability.h>
#include <sys/conf.h>
#include <sys/ioccom.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/file.h>		/* must come after sys/malloc.h */
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/poll.h>
#include <sys/proc.h>
#include <sys/filedesc.h>

#include <fs/coda/coda.h>
#include <fs/coda/cnode.h>
#include <fs/coda/coda_io.h>
#include <fs/coda/coda_psdev.h>

/*
 * Variables to determine how Coda sleeps and whether or not it is
 * interruptible when it does sleep waiting for Venus.
 */
/* #define	CTL_C */

#ifdef CTL_C
#include <sys/signalvar.h>
#endif

int coda_psdev_print_entry = 0;
static int outstanding_upcalls = 0;
int coda_call_sleep = PZERO - 1;
#ifdef CTL_C
int coda_pcatch = PCATCH;
#else
#endif

#define	ENTRY do {							\
	if (coda_psdev_print_entry)					\
		myprintf(("Entered %s\n", __func__));			\
} while (0)

struct vmsg {
	TAILQ_ENTRY(vmsg)	vm_chain;
	caddr_t		vm_data;
	u_short		vm_flags;
	u_short		vm_inSize;	/* Size is at most 5000 bytes */
	u_short		vm_outSize;
	u_short		vm_opcode;	/* Copied from data to save ptr deref */
	int		vm_unique;
	caddr_t		vm_sleep;	/* Not used by Mach. */
};

#define	VM_READ		1
#define	VM_WRITE	2
#define	VM_INTR		4		/* Unused. */

int
vc_open(struct cdev *dev, int flag, int mode, struct thread *td)
{
	struct vcomm *vcp;
	struct coda_mntinfo *mnt;

	ENTRY;
	mnt = dev2coda_mntinfo(dev);
	KASSERT(mnt, ("Coda: tried to open uninitialized cfs device"));
	vcp = &mnt->mi_vcomm;
	if (VC_OPEN(vcp))
		return (EBUSY);
	bzero(&(vcp->vc_selproc), sizeof (struct selinfo));
	TAILQ_INIT(&vcp->vc_requests);
	TAILQ_INIT(&vcp->vc_replies);
	MARK_VC_OPEN(vcp);
	mnt->mi_vfsp = NULL;
	mnt->mi_rootvp = NULL;
	return (0);
}

int
vc_close(struct cdev *dev, int flag, int mode, struct thread *td)
{
	struct vcomm *vcp;
	struct vmsg *vmp, *nvmp = NULL;
	struct coda_mntinfo *mi;
	int err;

	ENTRY;
	mi = dev2coda_mntinfo(dev);
	KASSERT(mi, ("Coda: closing unknown cfs device"));
	vcp = &mi->mi_vcomm;
	KASSERT(VC_OPEN(vcp), ("Coda: closing unopened cfs device"));

	/*
	 * Prevent future operations on this vfs from succeeding by
	 * auto-unmounting any vfs mounted via this device.  This frees user
	 * or sysadm from having to remember where all mount points are
	 * located.  Put this before WAKEUPs to avoid queuing new messages
	 * between the WAKEUP and the unmount (which can happen if we're
	 * unlucky).
	 */
	if (mi->mi_rootvp == NULL) {
		/*
		 * Just a simple open/close with no mount.
		 */
		MARK_VC_CLOSED(vcp);
		return (0);
	}

	/*
	 * Let unmount know this is for real.
	 */
	VTOC(mi->mi_rootvp)->c_flags |= C_UNMOUNTING;
	coda_unmounting(mi->mi_vfsp);

	/*
	 * Wakeup clients so they can return.
	 */
	outstanding_upcalls = 0;
	TAILQ_FOREACH_SAFE(vmp, &vcp->vc_requests, vm_chain, nvmp) {
		/*
		 * Free signal request messages and don't wakeup cause no one
		 * is waiting.
		 */
		if (vmp->vm_opcode == CODA_SIGNAL) {
			CODA_FREE((caddr_t)vmp->vm_data,
			    (u_int)VC_IN_NO_DATA);
			CODA_FREE((caddr_t)vmp, (u_int)sizeof(struct vmsg));
			continue;
		}
		outstanding_upcalls++;
		wakeup(&vmp->vm_sleep);
	}
	TAILQ_FOREACH(vmp, &vcp->vc_replies, vm_chain) {
		outstanding_upcalls++;
		wakeup(&vmp->vm_sleep);
	}
	MARK_VC_CLOSED(vcp);
	if (outstanding_upcalls) {
#ifdef CODA_VERBOSE
		printf("presleep: outstanding_upcalls = %d\n",
		    outstanding_upcalls);
#endif
    		(void) tsleep(&outstanding_upcalls, coda_call_sleep,
		    "coda_umount", 0);
#ifdef CODA_VERBOSE
		printf("postsleep: outstanding_upcalls = %d\n",
		    outstanding_upcalls);
#endif
	}
	err = dounmount(mi->mi_vfsp, flag, td);
	if (err)
		myprintf(("Error %d unmounting vfs in vcclose(%s)\n", err,
		    devtoname(dev)));
	return (0);
}

int
vc_read(struct cdev *dev, struct uio *uiop, int flag)
{
	struct vcomm *vcp;
	struct vmsg *vmp;
	int error = 0;

	ENTRY;
	vcp = &dev2coda_mntinfo(dev)->mi_vcomm;

	/*
	 * Get message at head of request queue.
	 */
	vmp = TAILQ_FIRST(&vcp->vc_requests);
	if (vmp == NULL)
		return (0);	/* Nothing to read */

	/*
	 * Move the input args into userspace.
	 *
	 * XXXRW: This is not safe in the presence of >1 reader, as vmp is
	 * still on the head of the list.
	 */
	uiop->uio_rw = UIO_READ;
	error = uiomove(vmp->vm_data, vmp->vm_inSize, uiop);
	if (error) {
		myprintf(("vcread: error (%d) on uiomove\n", error));
		error = EINVAL;
	}
	TAILQ_REMOVE(&vcp->vc_requests, vmp, vm_chain);

	/*
	 * If request was a signal, free up the message and don't enqueue it
	 * in the reply queue.
	 */
	if (vmp->vm_opcode == CODA_SIGNAL) {
		if (codadebug)
			myprintf(("vcread: signal msg (%d, %d)\n",
			    vmp->vm_opcode, vmp->vm_unique));
		CODA_FREE((caddr_t)vmp->vm_data, (u_int)VC_IN_NO_DATA);
		CODA_FREE((caddr_t)vmp, (u_int)sizeof(struct vmsg));
		return (error);
	}
	vmp->vm_flags |= VM_READ;
	TAILQ_INSERT_TAIL(&vcp->vc_replies, vmp, vm_chain);
	return (error);
}

int
vc_write(struct cdev *dev, struct uio *uiop, int flag)
{
	struct vcomm *vcp;
	struct vmsg *vmp;
	struct coda_out_hdr *out;
	u_long seq;
	u_long opcode;
	int buf[2];
	int error = 0;

	ENTRY;
	vcp = &dev2coda_mntinfo(dev)->mi_vcomm;

	/*
	 * Peek at the opcode, unique without transfering the data.
	 */
	uiop->uio_rw = UIO_WRITE;
	error = uiomove((caddr_t)buf, sizeof(int) * 2, uiop);
	if (error) {
		myprintf(("vcwrite: error (%d) on uiomove\n", error));
		return (EINVAL);
	}
	opcode = buf[0];
	seq = buf[1];
	if (codadebug)
		myprintf(("vcwrite got a call for %ld.%ld\n", opcode, seq));
	if (DOWNCALL(opcode)) {
		union outputArgs pbuf;

		/*
		 * Get the rest of the data.
		 */
		uiop->uio_rw = UIO_WRITE;
		error = uiomove((caddr_t)&pbuf.coda_purgeuser.oh.result,
		    sizeof(pbuf) - (sizeof(int)*2), uiop);
		if (error) {
			myprintf(("vcwrite: error (%d) on uiomove (Op %ld "
			    "seq %ld)\n", error, opcode, seq));
			return (EINVAL);
		}
		return (handleDownCall(dev2coda_mntinfo(dev), opcode, &pbuf));
	}

	/*
	 * Look for the message on the (waiting for) reply queue.
	 */
	TAILQ_FOREACH(vmp, &vcp->vc_replies, vm_chain) {
		if (vmp->vm_unique == seq)
			break;
	}
	if (vmp == NULL) {
		if (codadebug)
			myprintf(("vcwrite: msg (%ld, %ld) not found\n",
			    opcode, seq));
		return (ESRCH);
	}

	/*
	 * Remove the message from the reply queue.
	 */
	TAILQ_REMOVE(&vcp->vc_replies, vmp, vm_chain);

	/*
	 * Move data into response buffer.
	 */
	out = (struct coda_out_hdr *)vmp->vm_data;

	/*
	 * Don't need to copy opcode and uniquifier.
	 *
	 * Get the rest of the data.
	 */
	if (vmp->vm_outSize < uiop->uio_resid) {
		myprintf(("vcwrite: more data than asked for (%d < %zd)\n",
		    vmp->vm_outSize, uiop->uio_resid));

		/*
		 * Notify caller of the error.
		 */
		wakeup(&vmp->vm_sleep);
		return (EINVAL);
	}

	/*
	 * Save the value.
	 */
	buf[0] = uiop->uio_resid;
	uiop->uio_rw = UIO_WRITE;
	error = uiomove((caddr_t) &out->result, vmp->vm_outSize -
	    (sizeof(int) * 2), uiop);
	if (error) {
		myprintf(("vcwrite: error (%d) on uiomove (op %ld seq %ld)\n",
		    error, opcode, seq));
		return (EINVAL);
	}

	/*
	 * I don't think these are used, but just in case.
	 *
	 * XXX - aren't these two already correct? -bnoble
	 */
	out->opcode = opcode;
	out->unique = seq;
	vmp->vm_outSize	= buf[0];	/* Amount of data transferred? */
	vmp->vm_flags |= VM_WRITE;
	error = 0;
	if (opcode == CODA_OPEN_BY_FD) {
		struct coda_open_by_fd_out *tmp =
		    (struct coda_open_by_fd_out *)out;
		struct file *fp;
		struct vnode *vp = NULL;

		if (tmp->oh.result == 0) {
			error = getvnode(uiop->uio_td->td_proc->p_fd, CAP_WRITE,
			    tmp->fd, &fp);
			if (!error) {
				/*
				 * XXX: Since the whole driver runs with
				 * Giant, don't actually need to acquire it
				 * explicitly here yet.
				 */
				mtx_lock(&Giant);
				vp = fp->f_vnode;
				VREF(vp);
				fdrop(fp, uiop->uio_td);
				mtx_unlock(&Giant);
			}
		}
		tmp->vp = vp;
	}
	wakeup(&vmp->vm_sleep);
	return (error);
}

int
vc_ioctl(struct cdev *dev, u_long cmd, caddr_t addr, int flag,
    struct thread *t)
{

	ENTRY;
	switch(cmd) {
	case CODARESIZE:
		return (ENODEV);

	case CODASTATS:
		return (ENODEV);

	case CODAPRINT:
		return (ENODEV);

	case CIOC_KERNEL_VERSION:
		switch (*(u_int *)addr) {
		case 0:
			*(u_int *)addr = coda_kernel_version;
			return (0);

		case 1:
		case 2:
			if (coda_kernel_version != *(u_int *)addr)
				return (ENOENT);
			else
			    	return (0);

		default:
			return (ENOENT);
		}

	default:
		return (EINVAL);
	}
}

int
vc_poll(struct cdev *dev, int events, struct thread *td)
{
	struct vcomm *vcp;
	int event_msk = 0;

	ENTRY;
	vcp = &dev2coda_mntinfo(dev)->mi_vcomm;
	event_msk = events & (POLLIN|POLLRDNORM);
	if (!event_msk)
		return (0);
	if (!TAILQ_EMPTY(&vcp->vc_requests))
		return (events & (POLLIN|POLLRDNORM));
	selrecord(td, &(vcp->vc_selproc));
	return (0);
}

/*
 * Statistics.
 */
struct coda_clstat coda_clstat;

/*
 * Key question: whether to sleep interuptably or uninteruptably when waiting
 * for Venus.  The former seems better (cause you can ^C a job), but then
 * GNU-EMACS completion breaks.  Use tsleep with no timeout, and no longjmp
 * happens.  But, when sleeping "uninterruptibly", we don't get told if it
 * returns abnormally (e.g. kill -9).
 */
int
coda_call(struct coda_mntinfo *mntinfo, int inSize, int *outSize,
    caddr_t buffer)
{
	struct vcomm *vcp;
	struct vmsg *vmp;
	int error;
#ifdef CTL_C
	struct thread *td = curthread;
	struct proc *p = td->td_proc;
	sigset_t psig_omask;
	sigset_t tempset;
	int i;
#endif

	/*
	 * Unlikely, but could be a race condition with a dying warden.
	 */
	if (mntinfo == NULL)
		return ENODEV;
	vcp = &(mntinfo->mi_vcomm);
	coda_clstat.ncalls++;
	coda_clstat.reqs[((struct coda_in_hdr *)buffer)->opcode]++;
	if (!VC_OPEN(vcp))
		return (ENODEV);
	CODA_ALLOC(vmp,struct vmsg *,sizeof(struct vmsg));

	/*
	 * Format the request message.
	 */
	vmp->vm_data = buffer;
	vmp->vm_flags = 0;
	vmp->vm_inSize = inSize;
	vmp->vm_outSize
	    = *outSize ? *outSize : inSize; /* |buffer| >= inSize */
	vmp->vm_opcode = ((struct coda_in_hdr *)buffer)->opcode;
	vmp->vm_unique = ++vcp->vc_seq;
	if (codadebug)
		myprintf(("Doing a call for %d.%d\n", vmp->vm_opcode,
		    vmp->vm_unique));

	/*
	 * Fill in the common input args.
	 */
	((struct coda_in_hdr *)buffer)->unique = vmp->vm_unique;

	/*
	 * Append msg to request queue and poke Venus.
	 */
	TAILQ_INSERT_TAIL(&vcp->vc_requests, vmp, vm_chain);
	selwakeuppri(&(vcp->vc_selproc), coda_call_sleep);

	/*
	 * We can be interrupted while we wait for Venus to process our
	 * request.  If the interrupt occurs before Venus has read the
	 * request, we dequeue and return. If it occurs after the read but
	 * before the reply, we dequeue, send a signal message, and return.
	 * If it occurs after the reply we ignore it.  In no case do we want
	 * to restart the syscall.  If it was interrupted by a venus shutdown
	 * (vcclose), return ENODEV.
	 *
	 * Ignore return, we have to check anyway.
	 */
#ifdef CTL_C
	/*
	 * This is work in progress.  Setting coda_pcatch lets tsleep
	 * reawaken on a ^c or ^z.  The problem is that emacs sets certain
	 * interrupts as SA_RESTART.  This means that we should exit sleep
	 * handle the "signal" and then go to sleep again.  Mostly this is
	 * done by letting the syscall complete and be restarted.  We are not
	 * idempotent and can not do this.  A better solution is necessary.
	 */
	i = 0;
	PROC_LOCK(p);
	psig_omask = td->td_sigmask;
	do {
		error = msleep(&vmp->vm_sleep, &p->p_mtx,
		    (coda_call_sleep|coda_pcatch), "coda_call", hz*2);
		if (error == 0)
			break;
		else if (error == EWOULDBLOCK) {
#ifdef CODA_VERBOSE
			printf("coda_call: tsleep TIMEOUT %d sec\n", 2+2*i);
#endif
		}
		else {
			SIGEMPTYSET(tempset);
			SIGADDSET(tempset, SIGIO);
			if (SIGSETEQ(td->td_siglist, tempset)) {
				SIGADDSET(td->td_sigmask, SIGIO);
#ifdef CODA_VERBOSE
				printf("coda_call: tsleep returns %d SIGIO, "
				    "cnt %d\n", error, i);
#endif
			} else {
				SIGDELSET(tempset, SIGIO);
				SIGADDSET(tempset, SIGALRM);
				if (SIGSETEQ(td->td_siglist, tempset)) {
					SIGADDSET(td->td_sigmask, SIGALRM);
#ifdef CODA_VERBOSE
					printf("coda_call: tsleep returns "
					    "%d SIGALRM, cnt %d\n", error, i);
#endif
				} else {
#ifdef CODA_VERBOSE
					printf("coda_call: tsleep returns "
					    "%d, cnt %d\n", error, i);
#endif

#ifdef notyet
					tempset = td->td_siglist;
					SIGSETNAND(tempset, td->td_sigmask);
					printf("coda_call: siglist = %p, "
					    "sigmask = %p, mask %p\n",
					    td->td_siglist, td->td_sigmask,
					    tempset);
					break;
					SIGSETOR(td->td_sigmask, td->td_siglist);
					tempset = td->td_siglist;
					SIGSETNAND(tempset, td->td_sigmask);
					printf("coda_call: new mask, "
					    "siglist = %p, sigmask = %p, "
					    "mask %p\n", td->td_siglist,
					    td->td_sigmask, tempset);
#endif
				}
			}
		}
	} while (error && i++ < 128 && VC_OPEN(vcp));
	td->td_sigmask = psig_omask;
	signotify(td);
	PROC_UNLOCK(p);
#else
	(void)tsleep(&vmp->vm_sleep, coda_call_sleep, "coda_call", 0);
#endif
	if (VC_OPEN(vcp)) {
		/*
		 * Venus is still alive.
		 *
		 * Op went through, interrupt or not...
		 */
		if (vmp->vm_flags & VM_WRITE) {
			error = 0;
			*outSize = vmp->vm_outSize;
		} else if (!(vmp->vm_flags & VM_READ)) {
			/* Interrupted before venus read it. */
#ifdef CODA_VERBOSE
			if (1)
#else
			if (codadebug)
#endif
				myprintf(("interrupted before read: op = "
				    "%d.%d, flags = %x\n", vmp->vm_opcode,
				    vmp->vm_unique, vmp->vm_flags));
			TAILQ_REMOVE(&vcp->vc_requests, vmp, vm_chain);
			error = EINTR;
		} else {
			/*
			 * (!(vmp->vm_flags & VM_WRITE)) means interrupted
			 * after upcall started.
			 *
			 * Interrupted after start of upcall, send venus a
			 * signal.
			 */
			struct coda_in_hdr *dog;
			struct vmsg *svmp;

#ifdef CODA_VERBOSE
			if (1)
#else
			if (codadebug)
#endif
				myprintf(("Sending Venus a signal: op = "
				    "%d.%d, flags = %x\n", vmp->vm_opcode,
				    vmp->vm_unique, vmp->vm_flags));
			TAILQ_REMOVE(&vcp->vc_requests, vmp, vm_chain);
			error = EINTR;
			CODA_ALLOC(svmp, struct vmsg *, sizeof(struct vmsg));
			CODA_ALLOC((svmp->vm_data), char *,
			    sizeof(struct coda_in_hdr));
			dog = (struct coda_in_hdr *)svmp->vm_data;
			svmp->vm_flags = 0;
			dog->opcode = svmp->vm_opcode = CODA_SIGNAL;
			dog->unique = svmp->vm_unique = vmp->vm_unique;
			svmp->vm_inSize = sizeof (struct coda_in_hdr);
/*??? rvb */		svmp->vm_outSize = sizeof (struct coda_in_hdr);
			if (codadebug)
				myprintf(("coda_call: enqueing signal msg "
				    "(%d, %d)\n", svmp->vm_opcode,
				    svmp->vm_unique));

			/*
			 * Insert at head of queue!
			 *
			 * XXXRW: Actually, the tail.
			 */
			TAILQ_INSERT_TAIL(&vcp->vc_requests, svmp, vm_chain);
			selwakeuppri(&(vcp->vc_selproc), coda_call_sleep);
		}
	} else {
		/* If venus died (!VC_OPEN(vcp)) */
		if (codadebug)
			myprintf(("vcclose woke op %d.%d flags %d\n",
			    vmp->vm_opcode, vmp->vm_unique, vmp->vm_flags));
		error = ENODEV;
	}
	CODA_FREE(vmp, sizeof(struct vmsg));
	if (outstanding_upcalls > 0 && (--outstanding_upcalls == 0))
		wakeup(&outstanding_upcalls);
	if (!error)
		error = ((struct coda_out_hdr *)buffer)->result;
	return (error);
}
