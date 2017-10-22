/*-
 * Copyright (c) 2003, Jeffrey Roberson <jeff@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/7.0.0/sys/kern/kern_thr.c 175495 2008-01-19 18:15:07Z kib $");

#include "opt_compat.h"
#include "opt_posix.h"
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/posix4.h>
#include <sys/resourcevar.h>
#include <sys/sched.h>
#include <sys/sysctl.h>
#include <sys/smp.h>
#include <sys/syscallsubr.h>
#include <sys/sysent.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/signalvar.h>
#include <sys/ucontext.h>
#include <sys/thr.h>
#include <sys/rtprio.h>
#include <sys/umtx.h>
#include <sys/limits.h>

#include <machine/frame.h>

#include <security/audit/audit.h>

#ifdef COMPAT_IA32

extern struct sysentvec ia32_freebsd_sysvec;

static inline int
suword_lwpid(void *addr, lwpid_t lwpid)
{
	int error;

	if (curproc->p_sysent != &ia32_freebsd_sysvec)
		error = suword(addr, lwpid);
	else
		error = suword32(addr, lwpid);
	return (error);
}

#else
#define suword_lwpid	suword
#endif

extern int max_threads_per_proc;

static int create_thread(struct thread *td, mcontext_t *ctx,
			 void (*start_func)(void *), void *arg,
			 char *stack_base, size_t stack_size,
			 char *tls_base,
			 long *child_tid, long *parent_tid,
			 int flags, struct rtprio *rtp);

/*
 * System call interface.
 */
int
thr_create(struct thread *td, struct thr_create_args *uap)
    /* ucontext_t *ctx, long *id, int flags */
{
	ucontext_t ctx;
	int error;

	if ((error = copyin(uap->ctx, &ctx, sizeof(ctx))))
		return (error);

	error = create_thread(td, &ctx.uc_mcontext, NULL, NULL,
		NULL, 0, NULL, uap->id, NULL, uap->flags, NULL);
	return (error);
}

int
thr_new(struct thread *td, struct thr_new_args *uap)
    /* struct thr_param * */
{
	struct thr_param param;
	int error;

	if (uap->param_size < 0 || uap->param_size > sizeof(param))
		return (EINVAL);
	bzero(&param, sizeof(param));
	if ((error = copyin(uap->param, &param, uap->param_size)))
		return (error);
	return (kern_thr_new(td, &param));
}

int
kern_thr_new(struct thread *td, struct thr_param *param)
{
	struct rtprio rtp, *rtpp;
	int error;

	rtpp = NULL;
	if (param->rtp != 0) {
		error = copyin(param->rtp, &rtp, sizeof(struct rtprio));
		rtpp = &rtp;
	}
	error = create_thread(td, NULL, param->start_func, param->arg,
		param->stack_base, param->stack_size, param->tls_base,
		param->child_tid, param->parent_tid, param->flags,
		rtpp);
	return (error);
}

static int
create_thread(struct thread *td, mcontext_t *ctx,
	    void (*start_func)(void *), void *arg,
	    char *stack_base, size_t stack_size,
	    char *tls_base,
	    long *child_tid, long *parent_tid,
	    int flags, struct rtprio *rtp)
{
	stack_t stack;
	struct thread *newtd;
	struct proc *p;
	int error;

	error = 0;
	p = td->td_proc;

	/* Have race condition but it is cheap. */
	if (p->p_numthreads >= max_threads_per_proc)
		return (EPROCLIM);

	if (rtp != NULL) {
		switch(rtp->type) {
		case RTP_PRIO_REALTIME:
		case RTP_PRIO_FIFO:
			/* Only root can set scheduler policy */
			if (priv_check(td, PRIV_SCHED_SETPOLICY) != 0)
				return (EPERM);
			if (rtp->prio > RTP_PRIO_MAX)
				return (EINVAL);
			break;
		case RTP_PRIO_NORMAL:
			rtp->prio = 0;
			break;
		default:
			return (EINVAL);
		}
	}

	/* Initialize our td */
	newtd = thread_alloc();
	if (newtd == NULL)
		return (ENOMEM);

	/*
	 * Try the copyout as soon as we allocate the td so we don't
	 * have to tear things down in a failure case below.
	 * Here we copy out tid to two places, one for child and one
	 * for parent, because pthread can create a detached thread,
	 * if parent wants to safely access child tid, it has to provide 
	 * its storage, because child thread may exit quickly and
	 * memory is freed before parent thread can access it.
	 */
	if ((child_tid != NULL &&
	    suword_lwpid(child_tid, newtd->td_tid)) ||
	    (parent_tid != NULL &&
	    suword_lwpid(parent_tid, newtd->td_tid))) {
		thread_free(newtd);
		return (EFAULT);
	}

	bzero(&newtd->td_startzero,
	    __rangeof(struct thread, td_startzero, td_endzero));
	bcopy(&td->td_startcopy, &newtd->td_startcopy,
	    __rangeof(struct thread, td_startcopy, td_endcopy));
	newtd->td_proc = td->td_proc;
	newtd->td_ucred = crhold(td->td_ucred);

	cpu_set_upcall(newtd, td);

	if (ctx != NULL) { /* old way to set user context */
		error = set_mcontext(newtd, ctx);
		if (error != 0) {
			thread_free(newtd);
			crfree(td->td_ucred);
			return (error);
		}
	} else {
		/* Set up our machine context. */
		stack.ss_sp = stack_base;
		stack.ss_size = stack_size;
		/* Set upcall address to user thread entry function. */
		cpu_set_upcall_kse(newtd, start_func, arg, &stack);
		/* Setup user TLS address and TLS pointer register. */
		error = cpu_set_user_tls(newtd, tls_base);
		if (error != 0) {
			thread_free(newtd);
			crfree(td->td_ucred);
			return (error);
		}
	}

	PROC_LOCK(td->td_proc);
	td->td_proc->p_flag |= P_HADTHREADS;
	newtd->td_sigmask = td->td_sigmask;
	PROC_SLOCK(p);
	thread_link(newtd, p); 
	thread_lock(td);
	/* let the scheduler know about these things. */
	sched_fork_thread(td, newtd);
	thread_unlock(td);
	PROC_SUNLOCK(p);
	PROC_UNLOCK(p);
	thread_lock(newtd);
	if (rtp != NULL) {
		if (!(td->td_pri_class == PRI_TIMESHARE &&
		      rtp->type == RTP_PRIO_NORMAL)) {
			rtp_to_pri(rtp, newtd);
			sched_prio(newtd, newtd->td_user_pri);
		} /* ignore timesharing class */
	}
	TD_SET_CAN_RUN(newtd);
	/* if ((flags & THR_SUSPENDED) == 0) */
		sched_add(newtd, SRQ_BORING);
	thread_unlock(newtd);

	return (error);
}

int
thr_self(struct thread *td, struct thr_self_args *uap)
    /* long *id */
{
	int error;

	error = suword_lwpid(uap->id, (unsigned)td->td_tid);
	if (error == -1)
		return (EFAULT);
	return (0);
}

int
thr_exit(struct thread *td, struct thr_exit_args *uap)
    /* long *state */
{
	struct proc *p;

	p = td->td_proc;

	/* Signal userland that it can free the stack. */
	if ((void *)uap->state != NULL) {
		suword_lwpid(uap->state, 1);
		kern_umtx_wake(td, uap->state, INT_MAX);
	}

	PROC_LOCK(p);
	sigqueue_flush(&td->td_sigqueue);
	PROC_SLOCK(p);

	/*
	 * Shutting down last thread in the proc.  This will actually
	 * call exit() in the trampoline when it returns.
	 */
	if (p->p_numthreads != 1) {
		thread_stopped(p);
		thread_exit();
		/* NOTREACHED */
	}
	PROC_SUNLOCK(p);
	PROC_UNLOCK(p);
	return (0);
}

int
thr_kill(struct thread *td, struct thr_kill_args *uap)
    /* long id, int sig */
{
	struct thread *ttd;
	struct proc *p;
	int error;

	p = td->td_proc;
	error = 0;
	PROC_LOCK(p);
	if (uap->id == -1) {
		if (uap->sig != 0 && !_SIG_VALID(uap->sig)) {
			error = EINVAL;
		} else {
			error = ESRCH;
			FOREACH_THREAD_IN_PROC(p, ttd) {
				if (ttd != td) {
					error = 0;
					if (uap->sig == 0)
						break;
					tdsignal(p, ttd, uap->sig, NULL);
				}
			}
		}
	} else {
		if (uap->id != td->td_tid)
			ttd = thread_find(p, uap->id);
		else
			ttd = td;
		if (ttd == NULL)
			error = ESRCH;
		else if (uap->sig == 0)
			;
		else if (!_SIG_VALID(uap->sig))
			error = EINVAL;
		else
			tdsignal(p, ttd, uap->sig, NULL);
	}
	PROC_UNLOCK(p);
	return (error);
}

int
thr_kill2(struct thread *td, struct thr_kill2_args *uap)
    /* pid_t pid, long id, int sig */
{
	struct thread *ttd;
	struct proc *p;
	int error;

	AUDIT_ARG(signum, uap->sig);

	if (uap->pid == td->td_proc->p_pid) {
		p = td->td_proc;
		PROC_LOCK(p);
	} else if ((p = pfind(uap->pid)) == NULL) {
		return (ESRCH);
	}
	AUDIT_ARG(process, p);

	error = p_cansignal(td, p, uap->sig);
	if (error == 0) {
		if (uap->id == -1) {
			if (uap->sig != 0 && !_SIG_VALID(uap->sig)) {
				error = EINVAL;
			} else {
				error = ESRCH;
				FOREACH_THREAD_IN_PROC(p, ttd) {
					if (ttd != td) {
						error = 0;
						if (uap->sig == 0)
							break;
						tdsignal(p, ttd, uap->sig, NULL);
					}
				}
			}
		} else {
			if (uap->id != td->td_tid)
				ttd = thread_find(p, uap->id);
			else
				ttd = td;
			if (ttd == NULL)
				error = ESRCH;
			else if (uap->sig == 0)
				;
			else if (!_SIG_VALID(uap->sig))
				error = EINVAL;
			else
				tdsignal(p, ttd, uap->sig, NULL);
		}
	}
	PROC_UNLOCK(p);
	return (error);
}

int
thr_suspend(struct thread *td, struct thr_suspend_args *uap)
	/* const struct timespec *timeout */
{
	struct timespec ts, *tsp;
	int error;

	error = 0;
	tsp = NULL;
	if (uap->timeout != NULL) {
		error = copyin((const void *)uap->timeout, (void *)&ts,
		    sizeof(struct timespec));
		if (error != 0)
			return (error);
		tsp = &ts;
	}

	return (kern_thr_suspend(td, tsp));
}

int
kern_thr_suspend(struct thread *td, struct timespec *tsp)
{
	struct timeval tv;
	int error = 0, hz = 0;

	if (tsp != NULL) {
		if (tsp->tv_nsec < 0 || tsp->tv_nsec > 1000000000)
			return (EINVAL);
		if (tsp->tv_sec == 0 && tsp->tv_nsec == 0)
			return (ETIMEDOUT);
		TIMESPEC_TO_TIMEVAL(&tv, tsp);
		hz = tvtohz(&tv);
	}

	if (td->td_pflags & TDP_WAKEUP) {
		td->td_pflags &= ~TDP_WAKEUP;
		return (0);
	}

	PROC_LOCK(td->td_proc);
	if ((td->td_flags & TDF_THRWAKEUP) == 0)
		error = msleep((void *)td, &td->td_proc->p_mtx, PCATCH, "lthr",
		    hz);
	if (td->td_flags & TDF_THRWAKEUP) {
		thread_lock(td);
		td->td_flags &= ~TDF_THRWAKEUP;
		thread_unlock(td);
		PROC_UNLOCK(td->td_proc);
		return (0);
	}
	PROC_UNLOCK(td->td_proc);
	if (error == EWOULDBLOCK)
		error = ETIMEDOUT;
	else if (error == ERESTART) {
		if (hz != 0)
			error = EINTR;
	}
	return (error);
}

int
thr_wake(struct thread *td, struct thr_wake_args *uap)
	/* long id */
{
	struct proc *p;
	struct thread *ttd;

	if (uap->id == td->td_tid) {
		td->td_pflags |= TDP_WAKEUP;
		return (0);
	} 

	p = td->td_proc;
	PROC_LOCK(p);
	ttd = thread_find(p, uap->id);
	if (ttd == NULL) {
		PROC_UNLOCK(p);
		return (ESRCH);
	}
	thread_lock(ttd);
	ttd->td_flags |= TDF_THRWAKEUP;
	thread_unlock(ttd);
	wakeup((void *)ttd);
	PROC_UNLOCK(p);
	return (0);
}

int
thr_set_name(struct thread *td, struct thr_set_name_args *uap)
{
	struct proc *p = td->td_proc;
	char name[MAXCOMLEN + 1];
	struct thread *ttd;
	int error;

	error = 0;
	name[0] = '\0';
	if (uap->name != NULL) {
		error = copyinstr(uap->name, name, sizeof(name),
			NULL);
		if (error)
			return (error);
	}
	PROC_LOCK(p);
	if (uap->id == td->td_tid)
		ttd = td;
	else
		ttd = thread_find(p, uap->id);
	if (ttd != NULL)
		strcpy(ttd->td_name, name);
	else 
		error = ESRCH;
	PROC_UNLOCK(p);
	return (error);
}
