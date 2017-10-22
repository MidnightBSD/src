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
 *	@(#)kern_exit.c	8.7 (Berkeley) 2/12/94
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_compat.h"
#include "opt_kdtrace.h"
#include "opt_ktrace.h"
#include "opt_procdesc.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/capability.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/procdesc.h>
#include <sys/pioctl.h>
#include <sys/jail.h>
#include <sys/tty.h>
#include <sys/wait.h>
#include <sys/vmmeter.h>
#include <sys/vnode.h>
#include <sys/racct.h>
#include <sys/resourcevar.h>
#include <sys/sbuf.h>
#include <sys/signalvar.h>
#include <sys/sched.h>
#include <sys/sx.h>
#include <sys/syscallsubr.h>
#include <sys/syslog.h>
#include <sys/ptrace.h>
#include <sys/acct.h>		/* for acct_process() function prototype */
#include <sys/filedesc.h>
#include <sys/sdt.h>
#include <sys/shm.h>
#include <sys/sem.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif

#include <security/audit/audit.h>
#include <security/mac/mac_framework.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/uma.h>

#ifdef KDTRACE_HOOKS
#include <sys/dtrace_bsd.h>
dtrace_execexit_func_t	dtrace_fasttrap_exit;
#endif

SDT_PROVIDER_DECLARE(proc);
SDT_PROBE_DEFINE(proc, kernel, , exit, exit);
SDT_PROBE_ARGTYPE(proc, kernel, , exit, 0, "int");

/* Hook for NFS teardown procedure. */
void (*nlminfo_release_p)(struct proc *p);

static void
clear_orphan(struct proc *p)
{

	PROC_LOCK_ASSERT(p, MA_OWNED);

	if (p->p_flag & P_ORPHAN) {
		LIST_REMOVE(p, p_orphan);
		p->p_flag &= ~P_ORPHAN;
	}
}

/*
 * exit -- death of process.
 */
void
sys_sys_exit(struct thread *td, struct sys_exit_args *uap)
{

	exit1(td, W_EXITCODE(uap->rval, 0));
	/* NOTREACHED */
}

/*
 * Exit: deallocate address space and other resources, change proc state to
 * zombie, and unlink proc from allproc and parent's lists.  Save exit status
 * and rusage for wait().  Check for child processes and orphan them.
 */
void
exit1(struct thread *td, int rv)
{
	struct proc *p, *nq, *q;
	struct vnode *vtmp;
	struct vnode *ttyvp = NULL;
	struct plimit *plim;
	int locked;

	mtx_assert(&Giant, MA_NOTOWNED);

	p = td->td_proc;
	/*
	 * XXX in case we're rebooting we just let init die in order to
	 * work around an unsolved stack overflow seen very late during
	 * shutdown on sparc64 when the gmirror worker process exists.
	 */ 
	if (p == initproc && rebooting == 0) {
		printf("init died (signal %d, exit %d)\n",
		    WTERMSIG(rv), WEXITSTATUS(rv));
		panic("Going nowhere without my init!");
	}

	/*
	 * MUST abort all other threads before proceeding past here.
	 */
	PROC_LOCK(p);
	while (p->p_flag & P_HADTHREADS) {
		/*
		 * First check if some other thread got here before us..
		 * if so, act apropriatly, (exit or suspend);
		 */
		thread_suspend_check(0);

		/*
		 * Kill off the other threads. This requires
		 * some co-operation from other parts of the kernel
		 * so it may not be instantaneous.  With this state set
		 * any thread entering the kernel from userspace will
		 * thread_exit() in trap().  Any thread attempting to
		 * sleep will return immediately with EINTR or EWOULDBLOCK
		 * which will hopefully force them to back out to userland
		 * freeing resources as they go.  Any thread attempting
		 * to return to userland will thread_exit() from userret().
		 * thread_exit() will unsuspend us when the last of the
		 * other threads exits.
		 * If there is already a thread singler after resumption,
		 * calling thread_single will fail; in that case, we just
		 * re-check all suspension request, the thread should
		 * either be suspended there or exit.
		 */
		if (! thread_single(SINGLE_EXIT))
			break;

		/*
		 * All other activity in this process is now stopped.
		 * Threading support has been turned off.
		 */
	}
	KASSERT(p->p_numthreads == 1,
	    ("exit1: proc %p exiting with %d threads", p, p->p_numthreads));
	racct_sub(p, RACCT_NTHR, 1);
	/*
	 * Wakeup anyone in procfs' PIOCWAIT.  They should have a hold
	 * on our vmspace, so we should block below until they have
	 * released their reference to us.  Note that if they have
	 * requested S_EXIT stops we will block here until they ack
	 * via PIOCCONT.
	 */
	_STOPEVENT(p, S_EXIT, rv);

	/*
	 * Note that we are exiting and do another wakeup of anyone in
	 * PIOCWAIT in case they aren't listening for S_EXIT stops or
	 * decided to wait again after we told them we are exiting.
	 */
	p->p_flag |= P_WEXIT;
	wakeup(&p->p_stype);

	/*
	 * Wait for any processes that have a hold on our vmspace to
	 * release their reference.
	 */
	while (p->p_lock > 0)
		msleep(&p->p_lock, &p->p_mtx, PWAIT, "exithold", 0);

	p->p_xstat = rv;	/* Let event handler change exit status */
	PROC_UNLOCK(p);
	/* Drain the limit callout while we don't have the proc locked */
	callout_drain(&p->p_limco);

#ifdef AUDIT
	/*
	 * The Sun BSM exit token contains two components: an exit status as
	 * passed to exit(), and a return value to indicate what sort of exit
	 * it was.  The exit status is WEXITSTATUS(rv), but it's not clear
	 * what the return value is.
	 */
	AUDIT_ARG_EXIT(WEXITSTATUS(rv), 0);
	AUDIT_SYSCALL_EXIT(0, td);
#endif

	/* Are we a task leader? */
	if (p == p->p_leader) {
		mtx_lock(&ppeers_lock);
		q = p->p_peers;
		while (q != NULL) {
			PROC_LOCK(q);
			kern_psignal(q, SIGKILL);
			PROC_UNLOCK(q);
			q = q->p_peers;
		}
		while (p->p_peers != NULL)
			msleep(p, &ppeers_lock, PWAIT, "exit1", 0);
		mtx_unlock(&ppeers_lock);
	}

	/*
	 * Check if any loadable modules need anything done at process exit.
	 * E.g. SYSV IPC stuff
	 * XXX what if one of these generates an error?
	 */
	EVENTHANDLER_INVOKE(process_exit, p);

	/*
	 * If parent is waiting for us to exit or exec,
	 * P_PPWAIT is set; we will wakeup the parent below.
	 */
	PROC_LOCK(p);
	rv = p->p_xstat;	/* Event handler could change exit status */
	stopprofclock(p);
	p->p_flag &= ~(P_TRACED | P_PPWAIT);

	/*
	 * Stop the real interval timer.  If the handler is currently
	 * executing, prevent it from rearming itself and let it finish.
	 */
	if (timevalisset(&p->p_realtimer.it_value) &&
	    callout_stop(&p->p_itcallout) == 0) {
		timevalclear(&p->p_realtimer.it_interval);
		msleep(&p->p_itcallout, &p->p_mtx, PWAIT, "ritwait", 0);
		KASSERT(!timevalisset(&p->p_realtimer.it_value),
		    ("realtime timer is still armed"));
	}
	PROC_UNLOCK(p);

	/*
	 * Reset any sigio structures pointing to us as a result of
	 * F_SETOWN with our pid.
	 */
	funsetownlst(&p->p_sigiolst);

	/*
	 * If this process has an nlminfo data area (for lockd), release it
	 */
	if (nlminfo_release_p != NULL && p->p_nlminfo != NULL)
		(*nlminfo_release_p)(p);

	/*
	 * Close open files and release open-file table.
	 * This may block!
	 */
	fdfree(td);

	/*
	 * If this thread tickled GEOM, we need to wait for the giggling to
	 * stop before we return to userland
	 */
	if (td->td_pflags & TDP_GEOM)
		g_waitidle();

	/*
	 * Remove ourself from our leader's peer list and wake our leader.
	 */
	mtx_lock(&ppeers_lock);
	if (p->p_leader->p_peers) {
		q = p->p_leader;
		while (q->p_peers != p)
			q = q->p_peers;
		q->p_peers = p->p_peers;
		wakeup(p->p_leader);
	}
	mtx_unlock(&ppeers_lock);

	vmspace_exit(td);

	sx_xlock(&proctree_lock);
	if (SESS_LEADER(p)) {
		struct session *sp = p->p_session;
		struct tty *tp;

		/*
		 * s_ttyp is not zero'd; we use this to indicate that
		 * the session once had a controlling terminal. (for
		 * logging and informational purposes)
		 */
		SESS_LOCK(sp);
		ttyvp = sp->s_ttyvp;
		tp = sp->s_ttyp;
		sp->s_ttyvp = NULL;
		sp->s_ttydp = NULL;
		sp->s_leader = NULL;
		SESS_UNLOCK(sp);

		/*
		 * Signal foreground pgrp and revoke access to
		 * controlling terminal if it has not been revoked
		 * already.
		 *
		 * Because the TTY may have been revoked in the mean
		 * time and could already have a new session associated
		 * with it, make sure we don't send a SIGHUP to a
		 * foreground process group that does not belong to this
		 * session.
		 */

		if (tp != NULL) {
			tty_lock(tp);
			if (tp->t_session == sp)
				tty_signal_pgrp(tp, SIGHUP);
			tty_unlock(tp);
		}

		if (ttyvp != NULL) {
			sx_xunlock(&proctree_lock);
			if (vn_lock(ttyvp, LK_EXCLUSIVE) == 0) {
				VOP_REVOKE(ttyvp, REVOKEALL);
				VOP_UNLOCK(ttyvp, 0);
			}
			sx_xlock(&proctree_lock);
		}
	}
	fixjobc(p, p->p_pgrp, 0);
	sx_xunlock(&proctree_lock);
	(void)acct_process(td);

	/* Release the TTY now we've unlocked everything. */
	if (ttyvp != NULL)
		vrele(ttyvp);
#ifdef KTRACE
	ktrprocexit(td);
#endif
	/*
	 * Release reference to text vnode
	 */
	if ((vtmp = p->p_textvp) != NULL) {
		p->p_textvp = NULL;
		locked = VFS_LOCK_GIANT(vtmp->v_mount);
		vrele(vtmp);
		VFS_UNLOCK_GIANT(locked);
	}

	/*
	 * Release our limits structure.
	 */
	PROC_LOCK(p);
	plim = p->p_limit;
	p->p_limit = NULL;
	PROC_UNLOCK(p);
	lim_free(plim);

	tidhash_remove(td);

	/*
	 * Remove proc from allproc queue and pidhash chain.
	 * Place onto zombproc.  Unlink from parent's child list.
	 */
	sx_xlock(&allproc_lock);
	LIST_REMOVE(p, p_list);
	LIST_INSERT_HEAD(&zombproc, p, p_list);
	LIST_REMOVE(p, p_hash);
	sx_xunlock(&allproc_lock);

	/*
	 * Call machine-dependent code to release any
	 * machine-dependent resources other than the address space.
	 * The address space is released by "vmspace_exitfree(p)" in
	 * vm_waitproc().
	 */
	cpu_exit(td);

	WITNESS_WARN(WARN_PANIC, NULL, "process (pid %d) exiting", p->p_pid);

	/*
	 * Reparent all of our children to init.
	 */
	sx_xlock(&proctree_lock);
	q = LIST_FIRST(&p->p_children);
	if (q != NULL)		/* only need this if any child is S_ZOMB */
		wakeup(initproc);
	for (; q != NULL; q = nq) {
		nq = LIST_NEXT(q, p_sibling);
		PROC_LOCK(q);
		proc_reparent(q, initproc);
		q->p_sigparent = SIGCHLD;
		/*
		 * Traced processes are killed
		 * since their existence means someone is screwing up.
		 */
		if (q->p_flag & P_TRACED) {
			struct thread *temp;

			/*
			 * Since q was found on our children list, the
			 * proc_reparent() call moved q to the orphan
			 * list due to present P_TRACED flag. Clear
			 * orphan link for q now while q is locked.
			 */
			clear_orphan(q);
			q->p_flag &= ~(P_TRACED | P_STOPPED_TRACE);
			FOREACH_THREAD_IN_PROC(q, temp)
				temp->td_dbgflags &= ~TDB_SUSPEND;
			kern_psignal(q, SIGKILL);
		}
		PROC_UNLOCK(q);
	}

	/*
	 * Also get rid of our orphans.
	 */
	while ((q = LIST_FIRST(&p->p_orphans)) != NULL) {
		PROC_LOCK(q);
		clear_orphan(q);
		PROC_UNLOCK(q);
	}

	/* Save exit status. */
	PROC_LOCK(p);
	p->p_xthread = td;

	/* Tell the prison that we are gone. */
	prison_proc_free(p->p_ucred->cr_prison);

#ifdef KDTRACE_HOOKS
	/*
	 * Tell the DTrace fasttrap provider about the exit if it
	 * has declared an interest.
	 */
	if (dtrace_fasttrap_exit)
		dtrace_fasttrap_exit(p);
#endif

	/*
	 * Notify interested parties of our demise.
	 */
	KNOTE_LOCKED(&p->p_klist, NOTE_EXIT);

#ifdef KDTRACE_HOOKS
	int reason = CLD_EXITED;
	if (WCOREDUMP(rv))
		reason = CLD_DUMPED;
	else if (WIFSIGNALED(rv))
		reason = CLD_KILLED;
	SDT_PROBE(proc, kernel, , exit, reason, 0, 0, 0, 0);
#endif

	/*
	 * Just delete all entries in the p_klist. At this point we won't
	 * report any more events, and there are nasty race conditions that
	 * can beat us if we don't.
	 */
	knlist_clear(&p->p_klist, 1);

	/*
	 * If this is a process with a descriptor, we may not need to deliver
	 * a signal to the parent.  proctree_lock is held over
	 * procdesc_exit() to serialize concurrent calls to close() and
	 * exit().
	 */
#ifdef PROCDESC
	if (p->p_procdesc == NULL || procdesc_exit(p)) {
#endif
		/*
		 * Notify parent that we're gone.  If parent has the
		 * PS_NOCLDWAIT flag set, or if the handler is set to SIG_IGN,
		 * notify process 1 instead (and hope it will handle this
		 * situation).
		 */
		PROC_LOCK(p->p_pptr);
		mtx_lock(&p->p_pptr->p_sigacts->ps_mtx);
		if (p->p_pptr->p_sigacts->ps_flag &
		    (PS_NOCLDWAIT | PS_CLDSIGIGN)) {
			struct proc *pp;

			mtx_unlock(&p->p_pptr->p_sigacts->ps_mtx);
			pp = p->p_pptr;
			PROC_UNLOCK(pp);
			proc_reparent(p, initproc);
			p->p_sigparent = SIGCHLD;
			PROC_LOCK(p->p_pptr);

			/*
			 * Notify parent, so in case he was wait(2)ing or
			 * executing waitpid(2) with our pid, he will
			 * continue.
			 */
			wakeup(pp);
		} else
			mtx_unlock(&p->p_pptr->p_sigacts->ps_mtx);

		if (p->p_pptr == initproc)
			kern_psignal(p->p_pptr, SIGCHLD);
		else if (p->p_sigparent != 0) {
			if (p->p_sigparent == SIGCHLD)
				childproc_exited(p);
			else	/* LINUX thread */
				kern_psignal(p->p_pptr, p->p_sigparent);
		}
#ifdef PROCDESC
	} else
		PROC_LOCK(p->p_pptr);
#endif
	sx_xunlock(&proctree_lock);

	/*
	 * The state PRS_ZOMBIE prevents other proesses from sending
	 * signal to the process, to avoid memory leak, we free memory
	 * for signal queue at the time when the state is set.
	 */
	sigqueue_flush(&p->p_sigqueue);
	sigqueue_flush(&td->td_sigqueue);

	/*
	 * We have to wait until after acquiring all locks before
	 * changing p_state.  We need to avoid all possible context
	 * switches (including ones from blocking on a mutex) while
	 * marked as a zombie.  We also have to set the zombie state
	 * before we release the parent process' proc lock to avoid
	 * a lost wakeup.  So, we first call wakeup, then we grab the
	 * sched lock, update the state, and release the parent process'
	 * proc lock.
	 */
	wakeup(p->p_pptr);
	cv_broadcast(&p->p_pwait);
	sched_exit(p->p_pptr, td);
	PROC_SLOCK(p);
	p->p_state = PRS_ZOMBIE;
	PROC_UNLOCK(p->p_pptr);

	/*
	 * Hopefully no one will try to deliver a signal to the process this
	 * late in the game.
	 */
	knlist_destroy(&p->p_klist);

	/*
	 * Save our children's rusage information in our exit rusage.
	 */
	ruadd(&p->p_ru, &p->p_rux, &p->p_stats->p_cru, &p->p_crux);

	/*
	 * Make sure the scheduler takes this thread out of its tables etc.
	 * This will also release this thread's reference to the ucred.
	 * Other thread parts to release include pcb bits and such.
	 */
	thread_exit();
}


#ifndef _SYS_SYSPROTO_H_
struct abort2_args {
	char *why;
	int nargs;
	void **args;
};
#endif

int
sys_abort2(struct thread *td, struct abort2_args *uap)
{
	struct proc *p = td->td_proc;
	struct sbuf *sb;
	void *uargs[16];
	int error, i, sig;

	/*
	 * Do it right now so we can log either proper call of abort2(), or
	 * note, that invalid argument was passed. 512 is big enough to
	 * handle 16 arguments' descriptions with additional comments.
	 */
	sb = sbuf_new(NULL, NULL, 512, SBUF_FIXEDLEN);
	sbuf_clear(sb);
	sbuf_printf(sb, "%s(pid %d uid %d) aborted: ",
	    p->p_comm, p->p_pid, td->td_ucred->cr_uid);
	/* 
	 * Since we can't return from abort2(), send SIGKILL in cases, where
	 * abort2() was called improperly
	 */
	sig = SIGKILL;
	/* Prevent from DoSes from user-space. */
	if (uap->nargs < 0 || uap->nargs > 16)
		goto out;
	if (uap->nargs > 0) {
		if (uap->args == NULL)
			goto out;
		error = copyin(uap->args, uargs, uap->nargs * sizeof(void *));
		if (error != 0)
			goto out;
	}
	/*
	 * Limit size of 'reason' string to 128. Will fit even when
	 * maximal number of arguments was chosen to be logged.
	 */
	if (uap->why != NULL) {
		error = sbuf_copyin(sb, uap->why, 128);
		if (error < 0)
			goto out;
	} else {
		sbuf_printf(sb, "(null)");
	}
	if (uap->nargs > 0) {
		sbuf_printf(sb, "(");
		for (i = 0;i < uap->nargs; i++)
			sbuf_printf(sb, "%s%p", i == 0 ? "" : ", ", uargs[i]);
		sbuf_printf(sb, ")");
	}
	/*
	 * Final stage: arguments were proper, string has been
	 * successfully copied from userspace, and copying pointers
	 * from user-space succeed.
	 */
	sig = SIGABRT;
out:
	if (sig == SIGKILL) {
		sbuf_trim(sb);
		sbuf_printf(sb, " (Reason text inaccessible)");
	}
	sbuf_cat(sb, "\n");
	sbuf_finish(sb);
	log(LOG_INFO, "%s", sbuf_data(sb));
	sbuf_delete(sb);
	exit1(td, W_EXITCODE(0, sig));
	return (0);
}


#ifdef COMPAT_43
/*
 * The dirty work is handled by kern_wait().
 */
int
owait(struct thread *td, struct owait_args *uap __unused)
{
	int error, status;

	error = kern_wait(td, WAIT_ANY, &status, 0, NULL);
	if (error == 0)
		td->td_retval[1] = status;
	return (error);
}
#endif /* COMPAT_43 */

/*
 * The dirty work is handled by kern_wait().
 */
int
sys_wait4(struct thread *td, struct wait_args *uap)
{
	struct rusage ru, *rup;
	int error, status;

	if (uap->rusage != NULL)
		rup = &ru;
	else
		rup = NULL;
	error = kern_wait(td, uap->pid, &status, uap->options, rup);
	if (uap->status != NULL && error == 0)
		error = copyout(&status, uap->status, sizeof(status));
	if (uap->rusage != NULL && error == 0)
		error = copyout(&ru, uap->rusage, sizeof(struct rusage));
	return (error);
}

/*
 * Reap the remains of a zombie process and optionally return status and
 * rusage.  Asserts and will release both the proctree_lock and the process
 * lock as part of its work.
 */
void
proc_reap(struct thread *td, struct proc *p, int *status, int options,
    struct rusage *rusage)
{
	struct proc *q, *t;

	sx_assert(&proctree_lock, SA_XLOCKED);
	PROC_LOCK_ASSERT(p, MA_OWNED);
	PROC_SLOCK_ASSERT(p, MA_OWNED);
	KASSERT(p->p_state == PRS_ZOMBIE, ("proc_reap: !PRS_ZOMBIE"));

	q = td->td_proc;
	if (rusage) {
		*rusage = p->p_ru;
		calcru(p, &rusage->ru_utime, &rusage->ru_stime);
	}
	PROC_SUNLOCK(p);
	td->td_retval[0] = p->p_pid;
	if (status)
		*status = p->p_xstat;	/* convert to int */
	if (options & WNOWAIT) {
		/*
		 *  Only poll, returning the status.  Caller does not wish to
		 * release the proc struct just yet.
		 */
		PROC_UNLOCK(p);
		sx_xunlock(&proctree_lock);
		return;
	}

	PROC_LOCK(q);
	sigqueue_take(p->p_ksi);
	PROC_UNLOCK(q);
	PROC_UNLOCK(p);

	/*
	 * If we got the child via a ptrace 'attach', we need to give it back
	 * to the old parent.
	 */
	if (p->p_oppid && (t = pfind(p->p_oppid)) != NULL) {
		PROC_LOCK(p);
		proc_reparent(p, t);
		p->p_oppid = 0;
		PROC_UNLOCK(p);
		pksignal(t, SIGCHLD, p->p_ksi);
		wakeup(t);
		cv_broadcast(&p->p_pwait);
		PROC_UNLOCK(t);
		sx_xunlock(&proctree_lock);
		return;
	}

	/*
	 * Remove other references to this process to ensure we have an
	 * exclusive reference.
	 */
	sx_xlock(&allproc_lock);
	LIST_REMOVE(p, p_list);	/* off zombproc */
	sx_xunlock(&allproc_lock);
	LIST_REMOVE(p, p_sibling);
	PROC_LOCK(p);
	clear_orphan(p);
	PROC_UNLOCK(p);
	leavepgrp(p);
#ifdef PROCDESC
	if (p->p_procdesc != NULL)
		procdesc_reap(p);
#endif
	sx_xunlock(&proctree_lock);

	/*
	 * As a side effect of this lock, we know that all other writes to
	 * this proc are visible now, so no more locking is needed for p.
	 */
	PROC_LOCK(p);
	p->p_xstat = 0;		/* XXX: why? */
	PROC_UNLOCK(p);
	PROC_LOCK(q);
	ruadd(&q->p_stats->p_cru, &q->p_crux, &p->p_ru, &p->p_rux);
	PROC_UNLOCK(q);

	/*
	 * Decrement the count of procs running with this uid.
	 */
	(void)chgproccnt(p->p_ucred->cr_ruidinfo, -1, 0);

	/*
	 * Destroy resource accounting information associated with the process.
	 */
#ifdef RACCT
	PROC_LOCK(p);
	racct_sub(p, RACCT_NPROC, 1);
	PROC_UNLOCK(p);
#endif
	racct_proc_exit(p);

	/*
	 * Free credentials, arguments, and sigacts.
	 */
	crfree(p->p_ucred);
	p->p_ucred = NULL;
	pargs_drop(p->p_args);
	p->p_args = NULL;
	sigacts_free(p->p_sigacts);
	p->p_sigacts = NULL;

	/*
	 * Do any thread-system specific cleanups.
	 */
	thread_wait(p);

	/*
	 * Give vm and machine-dependent layer a chance to free anything that
	 * cpu_exit couldn't release while still running in process context.
	 */
	vm_waitproc(p);
#ifdef MAC
	mac_proc_destroy(p);
#endif
	KASSERT(FIRST_THREAD_IN_PROC(p),
	    ("proc_reap: no residual thread!"));
	uma_zfree(proc_zone, p);
	sx_xlock(&allproc_lock);
	nprocs--;
	sx_xunlock(&allproc_lock);
}

static int
proc_to_reap(struct thread *td, struct proc *p, pid_t pid, int *status,
    int options, struct rusage *rusage)
{
	struct proc *q;

	sx_assert(&proctree_lock, SA_XLOCKED);

	q = td->td_proc;
	PROC_LOCK(p);
	if (pid != WAIT_ANY && p->p_pid != pid && p->p_pgid != -pid) {
		PROC_UNLOCK(p);
		return (0);
	}
	if (p_canwait(td, p)) {
		PROC_UNLOCK(p);
		return (0);
	}

	/*
	 * This special case handles a kthread spawned by linux_clone
	 * (see linux_misc.c).  The linux_wait4 and linux_waitpid
	 * functions need to be able to distinguish between waiting
	 * on a process and waiting on a thread.  It is a thread if
	 * p_sigparent is not SIGCHLD, and the WLINUXCLONE option
	 * signifies we want to wait for threads and not processes.
	 */
	if ((p->p_sigparent != SIGCHLD) ^
	    ((options & WLINUXCLONE) != 0)) {
		PROC_UNLOCK(p);
		return (0);
	}

	PROC_SLOCK(p);
	if (p->p_state == PRS_ZOMBIE) {
		proc_reap(td, p, status, options, rusage);
		return (-1);
	}
	PROC_SUNLOCK(p);
	PROC_UNLOCK(p);
	return (1);
}

int
kern_wait(struct thread *td, pid_t pid, int *status, int options,
    struct rusage *rusage)
{
	struct proc *p, *q;
	int error, nfound, ret;

	AUDIT_ARG_PID(pid);
	AUDIT_ARG_VALUE(options);

	q = td->td_proc;
	if (pid == 0) {
		PROC_LOCK(q);
		pid = -q->p_pgid;
		PROC_UNLOCK(q);
	}
	/* If we don't know the option, just return. */
	if (options & ~(WUNTRACED|WNOHANG|WCONTINUED|WNOWAIT|WLINUXCLONE))
		return (EINVAL);
loop:
	if (q->p_flag & P_STATCHILD) {
		PROC_LOCK(q);
		q->p_flag &= ~P_STATCHILD;
		PROC_UNLOCK(q);
	}
	nfound = 0;
	sx_xlock(&proctree_lock);
	LIST_FOREACH(p, &q->p_children, p_sibling) {
		ret = proc_to_reap(td, p, pid, status, options, rusage);
		if (ret == 0)
			continue;
		else if (ret == 1)
			nfound++;
		else
			return (0);

		PROC_LOCK(p);
		PROC_SLOCK(p);
		if ((p->p_flag & P_STOPPED_SIG) &&
		    (p->p_suspcount == p->p_numthreads) &&
		    (p->p_flag & P_WAITED) == 0 &&
		    (p->p_flag & P_TRACED || options & WUNTRACED)) {
			PROC_SUNLOCK(p);
			p->p_flag |= P_WAITED;
			sx_xunlock(&proctree_lock);
			td->td_retval[0] = p->p_pid;
			if (status)
				*status = W_STOPCODE(p->p_xstat);

			PROC_LOCK(q);
			sigqueue_take(p->p_ksi);
			PROC_UNLOCK(q);
			PROC_UNLOCK(p);

			return (0);
		}
		PROC_SUNLOCK(p);
		if (options & WCONTINUED && (p->p_flag & P_CONTINUED)) {
			sx_xunlock(&proctree_lock);
			td->td_retval[0] = p->p_pid;
			p->p_flag &= ~P_CONTINUED;

			PROC_LOCK(q);
			sigqueue_take(p->p_ksi);
			PROC_UNLOCK(q);
			PROC_UNLOCK(p);

			if (status)
				*status = SIGCONT;
			return (0);
		}
		PROC_UNLOCK(p);
	}

	/*
	 * Look in the orphans list too, to allow the parent to
	 * collect it's child exit status even if child is being
	 * debugged.
	 *
	 * Debugger detaches from the parent upon successful
	 * switch-over from parent to child.  At this point due to
	 * re-parenting the parent loses the child to debugger and a
	 * wait4(2) call would report that it has no children to wait
	 * for.  By maintaining a list of orphans we allow the parent
	 * to successfully wait until the child becomes a zombie.
	 */
	LIST_FOREACH(p, &q->p_orphans, p_orphan) {
		ret = proc_to_reap(td, p, pid, status, options, rusage);
		if (ret == 0)
			continue;
		else if (ret == 1)
			nfound++;
		else
			return (0);
	}
	if (nfound == 0) {
		sx_xunlock(&proctree_lock);
		return (ECHILD);
	}
	if (options & WNOHANG) {
		sx_xunlock(&proctree_lock);
		td->td_retval[0] = 0;
		return (0);
	}
	PROC_LOCK(q);
	sx_xunlock(&proctree_lock);
	if (q->p_flag & P_STATCHILD) {
		q->p_flag &= ~P_STATCHILD;
		error = 0;
	} else
		error = msleep(q, &q->p_mtx, PWAIT | PCATCH, "wait", 0);
	PROC_UNLOCK(q);
	if (error)
		return (error);	
	goto loop;
}

/*
 * Make process 'parent' the new parent of process 'child'.
 * Must be called with an exclusive hold of proctree lock.
 */
void
proc_reparent(struct proc *child, struct proc *parent)
{

	sx_assert(&proctree_lock, SX_XLOCKED);
	PROC_LOCK_ASSERT(child, MA_OWNED);
	if (child->p_pptr == parent)
		return;

	PROC_LOCK(child->p_pptr);
	sigqueue_take(child->p_ksi);
	PROC_UNLOCK(child->p_pptr);
	LIST_REMOVE(child, p_sibling);
	LIST_INSERT_HEAD(&parent->p_children, child, p_sibling);

	clear_orphan(child);
	if (child->p_flag & P_TRACED) {
		LIST_INSERT_HEAD(&child->p_pptr->p_orphans, child, p_orphan);
		child->p_flag |= P_ORPHAN;
	}

	child->p_pptr = parent;
}
