/*-
 * Copyright (c) 1982, 1986, 1990, 1991, 1993
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
 *	@(#)kern_synch.c	8.9 (Berkeley) 5/19/95
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_kdtrace.h"
#include "opt_ktrace.h"
#include "opt_sched.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/condvar.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/sched.h>
#include <sys/sdt.h>
#include <sys/signalvar.h>
#include <sys/sleepqueue.h>
#include <sys/smp.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/sysproto.h>
#include <sys/vmmeter.h>
#ifdef KTRACE
#include <sys/uio.h>
#include <sys/ktrace.h>
#endif

#include <machine/cpu.h>

#ifdef XEN
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#endif

#define	KTDSTATE(td)							\
	(((td)->td_inhibitors & TDI_SLEEPING) != 0 ? "sleep"  :		\
	((td)->td_inhibitors & TDI_SUSPENDED) != 0 ? "suspended" :	\
	((td)->td_inhibitors & TDI_SWAPPED) != 0 ? "swapped" :		\
	((td)->td_inhibitors & TDI_LOCK) != 0 ? "blocked" :		\
	((td)->td_inhibitors & TDI_IWAIT) != 0 ? "iwait" : "yielding")

static void synch_setup(void *dummy);
SYSINIT(synch_setup, SI_SUB_KICK_SCHEDULER, SI_ORDER_FIRST, synch_setup,
    NULL);

int	hogticks;
static int pause_wchan;

static struct callout loadav_callout;

struct loadavg averunnable =
	{ {0, 0, 0}, FSCALE };	/* load average, of runnable procs */
/*
 * Constants for averages over 1, 5, and 15 minutes
 * when sampling at 5 second intervals.
 */
static fixpt_t cexp[3] = {
	0.9200444146293232 * FSCALE,	/* exp(-1/12) */
	0.9834714538216174 * FSCALE,	/* exp(-1/60) */
	0.9944598480048967 * FSCALE,	/* exp(-1/180) */
};

/* kernel uses `FSCALE', userland (SHOULD) use kern.fscale */
static int      fscale __unused = FSCALE;
SYSCTL_INT(_kern, OID_AUTO, fscale, CTLFLAG_RD, 0, FSCALE, "");

static void	loadav(void *arg);

SDT_PROVIDER_DECLARE(sched);
SDT_PROBE_DEFINE(sched, , , preempt, preempt);

/*
 * These probes reference Solaris features that are not implemented in FreeBSD.
 * Create the probes anyway for compatibility with existing D scripts; they'll
 * just never fire.
 */
SDT_PROBE_DEFINE(sched, , , cpucaps_sleep, cpucaps-sleep);
SDT_PROBE_DEFINE(sched, , , cpucaps_wakeup, cpucaps-wakeup);
SDT_PROBE_DEFINE(sched, , , schedctl_nopreempt, schedctl-nopreempt);
SDT_PROBE_DEFINE(sched, , , schedctl_preempt, schedctl-preempt);
SDT_PROBE_DEFINE(sched, , , schedctl_yield, schedctl-yield);

void
sleepinit(void)
{

	hogticks = (hz / 10) * 2;	/* Default only. */
	init_sleepqueues();
}

/*
 * General sleep call.  Suspends the current thread until a wakeup is
 * performed on the specified identifier.  The thread will then be made
 * runnable with the specified priority.  Sleeps at most timo/hz seconds
 * (0 means no timeout).  If pri includes PCATCH flag, signals are checked
 * before and after sleeping, else signals are not checked.  Returns 0 if
 * awakened, EWOULDBLOCK if the timeout expires.  If PCATCH is set and a
 * signal needs to be delivered, ERESTART is returned if the current system
 * call should be restarted if possible, and EINTR is returned if the system
 * call should be interrupted by the signal (return EINTR).
 *
 * The lock argument is unlocked before the caller is suspended, and
 * re-locked before _sleep() returns.  If priority includes the PDROP
 * flag the lock is not re-locked before returning.
 */
int
_sleep(void *ident, struct lock_object *lock, int priority,
    const char *wmesg, int timo)
{
	struct thread *td;
	struct proc *p;
	struct lock_class *class;
	int catch, flags, lock_state, pri, rval;
	WITNESS_SAVE_DECL(lock_witness);

	td = curthread;
	p = td->td_proc;
#ifdef KTRACE
	if (KTRPOINT(td, KTR_CSW))
		ktrcsw(1, 0, wmesg);
#endif
	WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, lock,
	    "Sleeping on \"%s\"", wmesg);
	KASSERT(timo != 0 || mtx_owned(&Giant) || lock != NULL,
	    ("sleeping without a lock"));
	KASSERT(p != NULL, ("msleep1"));
	KASSERT(ident != NULL && TD_IS_RUNNING(td), ("msleep"));
	if (priority & PDROP)
		KASSERT(lock != NULL && lock != &Giant.lock_object,
		    ("PDROP requires a non-Giant lock"));
	if (lock != NULL)
		class = LOCK_CLASS(lock);
	else
		class = NULL;

	if (cold || SCHEDULER_STOPPED()) {
		/*
		 * During autoconfiguration, just return;
		 * don't run any other threads or panic below,
		 * in case this is the idle thread and already asleep.
		 * XXX: this used to do "s = splhigh(); splx(safepri);
		 * splx(s);" to give interrupts a chance, but there is
		 * no way to give interrupts a chance now.
		 */
		if (lock != NULL && priority & PDROP)
			class->lc_unlock(lock);
		return (0);
	}
	catch = priority & PCATCH;
	pri = priority & PRIMASK;

	/*
	 * If we are already on a sleep queue, then remove us from that
	 * sleep queue first.  We have to do this to handle recursive
	 * sleeps.
	 */
	if (TD_ON_SLEEPQ(td))
		sleepq_remove(td, td->td_wchan);

	if (ident == &pause_wchan)
		flags = SLEEPQ_PAUSE;
	else
		flags = SLEEPQ_SLEEP;
	if (catch)
		flags |= SLEEPQ_INTERRUPTIBLE;
	if (priority & PBDRY)
		flags |= SLEEPQ_STOP_ON_BDRY;

	sleepq_lock(ident);
	CTR5(KTR_PROC, "sleep: thread %ld (pid %ld, %s) on %s (%p)",
	    td->td_tid, p->p_pid, td->td_name, wmesg, ident);

	if (lock == &Giant.lock_object)
		mtx_assert(&Giant, MA_OWNED);
	DROP_GIANT();
	if (lock != NULL && lock != &Giant.lock_object &&
	    !(class->lc_flags & LC_SLEEPABLE)) {
		WITNESS_SAVE(lock, lock_witness);
		lock_state = class->lc_unlock(lock);
	} else
		/* GCC needs to follow the Yellow Brick Road */
		lock_state = -1;

	/*
	 * We put ourselves on the sleep queue and start our timeout
	 * before calling thread_suspend_check, as we could stop there,
	 * and a wakeup or a SIGCONT (or both) could occur while we were
	 * stopped without resuming us.  Thus, we must be ready for sleep
	 * when cursig() is called.  If the wakeup happens while we're
	 * stopped, then td will no longer be on a sleep queue upon
	 * return from cursig().
	 */
	sleepq_add(ident, lock, wmesg, flags, 0);
	if (timo)
		sleepq_set_timeout(ident, timo);
	if (lock != NULL && class->lc_flags & LC_SLEEPABLE) {
		sleepq_release(ident);
		WITNESS_SAVE(lock, lock_witness);
		lock_state = class->lc_unlock(lock);
		sleepq_lock(ident);
	}
	if (timo && catch)
		rval = sleepq_timedwait_sig(ident, pri);
	else if (timo)
		rval = sleepq_timedwait(ident, pri);
	else if (catch)
		rval = sleepq_wait_sig(ident, pri);
	else {
		sleepq_wait(ident, pri);
		rval = 0;
	}
#ifdef KTRACE
	if (KTRPOINT(td, KTR_CSW))
		ktrcsw(0, 0, wmesg);
#endif
	PICKUP_GIANT();
	if (lock != NULL && lock != &Giant.lock_object && !(priority & PDROP)) {
		class->lc_lock(lock, lock_state);
		WITNESS_RESTORE(lock, lock_witness);
	}
	return (rval);
}

int
msleep_spin(void *ident, struct mtx *mtx, const char *wmesg, int timo)
{
	struct thread *td;
	struct proc *p;
	int rval;
	WITNESS_SAVE_DECL(mtx);

	td = curthread;
	p = td->td_proc;
	KASSERT(mtx != NULL, ("sleeping without a mutex"));
	KASSERT(p != NULL, ("msleep1"));
	KASSERT(ident != NULL && TD_IS_RUNNING(td), ("msleep"));

	if (cold || SCHEDULER_STOPPED()) {
		/*
		 * During autoconfiguration, just return;
		 * don't run any other threads or panic below,
		 * in case this is the idle thread and already asleep.
		 * XXX: this used to do "s = splhigh(); splx(safepri);
		 * splx(s);" to give interrupts a chance, but there is
		 * no way to give interrupts a chance now.
		 */
		return (0);
	}

	sleepq_lock(ident);
	CTR5(KTR_PROC, "msleep_spin: thread %ld (pid %ld, %s) on %s (%p)",
	    td->td_tid, p->p_pid, td->td_name, wmesg, ident);

	DROP_GIANT();
	mtx_assert(mtx, MA_OWNED | MA_NOTRECURSED);
	WITNESS_SAVE(&mtx->lock_object, mtx);
	mtx_unlock_spin(mtx);

	/*
	 * We put ourselves on the sleep queue and start our timeout.
	 */
	sleepq_add(ident, &mtx->lock_object, wmesg, SLEEPQ_SLEEP, 0);
	if (timo)
		sleepq_set_timeout(ident, timo);

	/*
	 * Can't call ktrace with any spin locks held so it can lock the
	 * ktrace_mtx lock, and WITNESS_WARN considers it an error to hold
	 * any spin lock.  Thus, we have to drop the sleepq spin lock while
	 * we handle those requests.  This is safe since we have placed our
	 * thread on the sleep queue already.
	 */
#ifdef KTRACE
	if (KTRPOINT(td, KTR_CSW)) {
		sleepq_release(ident);
		ktrcsw(1, 0, wmesg);
		sleepq_lock(ident);
	}
#endif
#ifdef WITNESS
	sleepq_release(ident);
	WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL, "Sleeping on \"%s\"",
	    wmesg);
	sleepq_lock(ident);
#endif
	if (timo)
		rval = sleepq_timedwait(ident, 0);
	else {
		sleepq_wait(ident, 0);
		rval = 0;
	}
#ifdef KTRACE
	if (KTRPOINT(td, KTR_CSW))
		ktrcsw(0, 0, wmesg);
#endif
	PICKUP_GIANT();
	mtx_lock_spin(mtx);
	WITNESS_RESTORE(&mtx->lock_object, mtx);
	return (rval);
}

/*
 * pause() delays the calling thread by the given number of system ticks.
 * During cold bootup, pause() uses the DELAY() function instead of
 * the tsleep() function to do the waiting. The "timo" argument must be
 * greater than or equal to zero. A "timo" value of zero is equivalent
 * to a "timo" value of one.
 */
int
pause(const char *wmesg, int timo)
{
	KASSERT(timo >= 0, ("pause: timo must be >= 0"));

	/* silently convert invalid timeouts */
	if (timo < 1)
		timo = 1;

	if (cold) {
		/*
		 * We delay one HZ at a time to avoid overflowing the
		 * system specific DELAY() function(s):
		 */
		while (timo >= hz) {
			DELAY(1000000);
			timo -= hz;
		}
		if (timo > 0)
			DELAY(timo * tick);
		return (0);
	}
	return (tsleep(&pause_wchan, 0, wmesg, timo));
}

/*
 * Make all threads sleeping on the specified identifier runnable.
 */
void
wakeup(void *ident)
{
	int wakeup_swapper;

	sleepq_lock(ident);
	wakeup_swapper = sleepq_broadcast(ident, SLEEPQ_SLEEP, 0, 0);
	sleepq_release(ident);
	if (wakeup_swapper) {
		KASSERT(ident != &proc0,
		    ("wakeup and wakeup_swapper and proc0"));
		kick_proc0();
	}
}

/*
 * Make a thread sleeping on the specified identifier runnable.
 * May wake more than one thread if a target thread is currently
 * swapped out.
 */
void
wakeup_one(void *ident)
{
	int wakeup_swapper;

	sleepq_lock(ident);
	wakeup_swapper = sleepq_signal(ident, SLEEPQ_SLEEP, 0, 0);
	sleepq_release(ident);
	if (wakeup_swapper)
		kick_proc0();
}

static void
kdb_switch(void)
{
	thread_unlock(curthread);
	kdb_backtrace();
	kdb_reenter();
	panic("%s: did not reenter debugger", __func__);
}

/*
 * The machine independent parts of context switching.
 */
void
mi_switch(int flags, struct thread *newtd)
{
	uint64_t runtime, new_switchtime;
	struct thread *td;
	struct proc *p;

	td = curthread;			/* XXX */
	THREAD_LOCK_ASSERT(td, MA_OWNED | MA_NOTRECURSED);
	p = td->td_proc;		/* XXX */
	KASSERT(!TD_ON_RUNQ(td), ("mi_switch: called by old code"));
#ifdef INVARIANTS
	if (!TD_ON_LOCK(td) && !TD_IS_RUNNING(td))
		mtx_assert(&Giant, MA_NOTOWNED);
#endif
	KASSERT(td->td_critnest == 1 || panicstr,
	    ("mi_switch: switch in a critical section"));
	KASSERT((flags & (SW_INVOL | SW_VOL)) != 0,
	    ("mi_switch: switch must be voluntary or involuntary"));
	KASSERT(newtd != curthread, ("mi_switch: preempting back to ourself"));

	/*
	 * Don't perform context switches from the debugger.
	 */
	if (kdb_active)
		kdb_switch();
	if (SCHEDULER_STOPPED())
		return;
	if (flags & SW_VOL) {
		td->td_ru.ru_nvcsw++;
		td->td_swvoltick = ticks;
	} else
		td->td_ru.ru_nivcsw++;
#ifdef SCHED_STATS
	SCHED_STAT_INC(sched_switch_stats[flags & SW_TYPE_MASK]);
#endif
	/*
	 * Compute the amount of time during which the current
	 * thread was running, and add that to its total so far.
	 */
	new_switchtime = cpu_ticks();
	runtime = new_switchtime - PCPU_GET(switchtime);
	td->td_runtime += runtime;
	td->td_incruntime += runtime;
	PCPU_SET(switchtime, new_switchtime);
	td->td_generation++;	/* bump preempt-detect counter */
	PCPU_INC(cnt.v_swtch);
	PCPU_SET(switchticks, ticks);
	CTR4(KTR_PROC, "mi_switch: old thread %ld (td_sched %p, pid %ld, %s)",
	    td->td_tid, td->td_sched, p->p_pid, td->td_name);
#if (KTR_COMPILE & KTR_SCHED) != 0
	if (TD_IS_IDLETHREAD(td))
		KTR_STATE1(KTR_SCHED, "thread", sched_tdname(td), "idle",
		    "prio:%d", td->td_priority);
	else
		KTR_STATE3(KTR_SCHED, "thread", sched_tdname(td), KTDSTATE(td),
		    "prio:%d", td->td_priority, "wmesg:\"%s\"", td->td_wmesg,
		    "lockname:\"%s\"", td->td_lockname);
#endif
	SDT_PROBE0(sched, , , preempt);
#ifdef XEN
	PT_UPDATES_FLUSH();
#endif
	sched_switch(td, newtd, flags);
	KTR_STATE1(KTR_SCHED, "thread", sched_tdname(td), "running",
	    "prio:%d", td->td_priority);

	CTR4(KTR_PROC, "mi_switch: new thread %ld (td_sched %p, pid %ld, %s)",
	    td->td_tid, td->td_sched, p->p_pid, td->td_name);

	/* 
	 * If the last thread was exiting, finish cleaning it up.
	 */
	if ((td = PCPU_GET(deadthread))) {
		PCPU_SET(deadthread, NULL);
		thread_stash(td);
	}
}

/*
 * Change thread state to be runnable, placing it on the run queue if
 * it is in memory.  If it is swapped out, return true so our caller
 * will know to awaken the swapper.
 */
int
setrunnable(struct thread *td)
{

	THREAD_LOCK_ASSERT(td, MA_OWNED);
	KASSERT(td->td_proc->p_state != PRS_ZOMBIE,
	    ("setrunnable: pid %d is a zombie", td->td_proc->p_pid));
	switch (td->td_state) {
	case TDS_RUNNING:
	case TDS_RUNQ:
		return (0);
	case TDS_INHIBITED:
		/*
		 * If we are only inhibited because we are swapped out
		 * then arange to swap in this process. Otherwise just return.
		 */
		if (td->td_inhibitors != TDI_SWAPPED)
			return (0);
		/* FALLTHROUGH */
	case TDS_CAN_RUN:
		break;
	default:
		printf("state is 0x%x", td->td_state);
		panic("setrunnable(2)");
	}
	if ((td->td_flags & TDF_INMEM) == 0) {
		if ((td->td_flags & TDF_SWAPINREQ) == 0) {
			td->td_flags |= TDF_SWAPINREQ;
			return (1);
		}
	} else
		sched_wakeup(td);
	return (0);
}

/*
 * Compute a tenex style load average of a quantity on
 * 1, 5 and 15 minute intervals.
 */
static void
loadav(void *arg)
{
	int i, nrun;
	struct loadavg *avg;

	nrun = sched_load();
	avg = &averunnable;

	for (i = 0; i < 3; i++)
		avg->ldavg[i] = (cexp[i] * avg->ldavg[i] +
		    nrun * FSCALE * (FSCALE - cexp[i])) >> FSHIFT;

	/*
	 * Schedule the next update to occur after 5 seconds, but add a
	 * random variation to avoid synchronisation with processes that
	 * run at regular intervals.
	 */
	callout_reset(&loadav_callout, hz * 4 + (int)(random() % (hz * 2 + 1)),
	    loadav, NULL);
}

/* ARGSUSED */
static void
synch_setup(void *dummy)
{
	callout_init(&loadav_callout, CALLOUT_MPSAFE);

	/* Kick off timeout driven events by calling first time. */
	loadav(NULL);
}

int
should_yield(void)
{

	return (ticks - curthread->td_swvoltick >= hogticks);
}

void
maybe_yield(void)
{

	if (should_yield())
		kern_yield(PRI_USER);
}

void
kern_yield(int prio)
{
	struct thread *td;

	td = curthread;
	DROP_GIANT();
	thread_lock(td);
	if (prio == PRI_USER)
		prio = td->td_user_pri;
	if (prio >= 0)
		sched_prio(td, prio);
	mi_switch(SW_VOL | SWT_RELINQUISH, NULL);
	thread_unlock(td);
	PICKUP_GIANT();
}

/*
 * General purpose yield system call.
 */
int
sys_yield(struct thread *td, struct yield_args *uap)
{

	thread_lock(td);
	if (PRI_BASE(td->td_pri_class) == PRI_TIMESHARE)
		sched_prio(td, PRI_MAX_TIMESHARE);
	mi_switch(SW_VOL | SWT_RELINQUISH, NULL);
	thread_unlock(td);
	td->td_retval[0] = 0;
	return (0);
}
