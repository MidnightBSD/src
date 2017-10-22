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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/7.0.0/sys/kern/sched_4bsd.c 174808 2007-12-20 07:15:40Z davidxu $");

#include "opt_hwpmc_hooks.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/kthread.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/sx.h>
#include <sys/turnstile.h>
#include <sys/umtx.h>
#include <machine/pcb.h>
#include <machine/smp.h>

#ifdef HWPMC_HOOKS
#include <sys/pmckern.h>
#endif

/*
 * INVERSE_ESTCPU_WEIGHT is only suitable for statclock() frequencies in
 * the range 100-256 Hz (approximately).
 */
#define	ESTCPULIM(e) \
    min((e), INVERSE_ESTCPU_WEIGHT * (NICE_WEIGHT * (PRIO_MAX - PRIO_MIN) - \
    RQ_PPQ) + INVERSE_ESTCPU_WEIGHT - 1)
#ifdef SMP
#define	INVERSE_ESTCPU_WEIGHT	(8 * smp_cpus)
#else
#define	INVERSE_ESTCPU_WEIGHT	8	/* 1 / (priorities per estcpu level). */
#endif
#define	NICE_WEIGHT		1	/* Priorities per nice level. */

/*
 * The schedulable entity that runs a context.
 * This is  an extension to the thread structure and is tailored to
 * the requirements of this scheduler
 */
struct td_sched {
	TAILQ_ENTRY(td_sched) ts_procq;	/* (j/z) Run queue. */
	struct thread	*ts_thread;	/* (*) Active associated thread. */
	fixpt_t		ts_pctcpu;	/* (j) %cpu during p_swtime. */
	u_char		ts_rqindex;	/* (j) Run queue index. */
	int		ts_cpticks;	/* (j) Ticks of cpu time. */
	int		ts_slptime;	/* (j) Seconds !RUNNING. */
	struct runq	*ts_runq;	/* runq the thread is currently on */
};

/* flags kept in td_flags */
#define TDF_DIDRUN	TDF_SCHED0	/* thread actually ran. */
#define TDF_EXIT	TDF_SCHED1	/* thread is being killed. */
#define TDF_BOUND	TDF_SCHED2

#define ts_flags	ts_thread->td_flags
#define TSF_DIDRUN	TDF_DIDRUN /* thread actually ran. */
#define TSF_EXIT	TDF_EXIT /* thread is being killed. */
#define TSF_BOUND	TDF_BOUND /* stuck to one CPU */

#define SKE_RUNQ_PCPU(ts)						\
    ((ts)->ts_runq != 0 && (ts)->ts_runq != &runq)

static struct td_sched td_sched0;
struct mtx sched_lock;

static int	sched_tdcnt;	/* Total runnable threads in the system. */
static int	sched_quantum;	/* Roundrobin scheduling quantum in ticks. */
#define	SCHED_QUANTUM	(hz / 10)	/* Default sched quantum */

static struct callout roundrobin_callout;

static void	setup_runqs(void);
static void	roundrobin(void *arg);
static void	schedcpu(void);
static void	schedcpu_thread(void);
static void	sched_priority(struct thread *td, u_char prio);
static void	sched_setup(void *dummy);
static void	maybe_resched(struct thread *td);
static void	updatepri(struct thread *td);
static void	resetpriority(struct thread *td);
static void	resetpriority_thread(struct thread *td);
#ifdef SMP
static int	forward_wakeup(int  cpunum);
#endif

static struct kproc_desc sched_kp = {
        "schedcpu",
        schedcpu_thread,
        NULL
};
SYSINIT(schedcpu, SI_SUB_RUN_SCHEDULER, SI_ORDER_FIRST, kproc_start, &sched_kp)
SYSINIT(sched_setup, SI_SUB_RUN_QUEUE, SI_ORDER_FIRST, sched_setup, NULL)

/*
 * Global run queue.
 */
static struct runq runq;

#ifdef SMP
/*
 * Per-CPU run queues
 */
static struct runq runq_pcpu[MAXCPU];
#endif

static void
setup_runqs(void)
{
#ifdef SMP
	int i;

	for (i = 0; i < MAXCPU; ++i)
		runq_init(&runq_pcpu[i]);
#endif

	runq_init(&runq);
}

static int
sysctl_kern_quantum(SYSCTL_HANDLER_ARGS)
{
	int error, new_val;

	new_val = sched_quantum * tick;
	error = sysctl_handle_int(oidp, &new_val, 0, req);
        if (error != 0 || req->newptr == NULL)
		return (error);
	if (new_val < tick)
		return (EINVAL);
	sched_quantum = new_val / tick;
	hogticks = 2 * sched_quantum;
	return (0);
}

SYSCTL_NODE(_kern, OID_AUTO, sched, CTLFLAG_RD, 0, "Scheduler");

SYSCTL_STRING(_kern_sched, OID_AUTO, name, CTLFLAG_RD, "4BSD", 0,
    "Scheduler name");

SYSCTL_PROC(_kern_sched, OID_AUTO, quantum, CTLTYPE_INT | CTLFLAG_RW,
    0, sizeof sched_quantum, sysctl_kern_quantum, "I",
    "Roundrobin scheduling quantum in microseconds");

#ifdef SMP
/* Enable forwarding of wakeups to all other cpus */
SYSCTL_NODE(_kern_sched, OID_AUTO, ipiwakeup, CTLFLAG_RD, NULL, "Kernel SMP");

static int forward_wakeup_enabled = 1;
SYSCTL_INT(_kern_sched_ipiwakeup, OID_AUTO, enabled, CTLFLAG_RW,
	   &forward_wakeup_enabled, 0,
	   "Forwarding of wakeup to idle CPUs");

static int forward_wakeups_requested = 0;
SYSCTL_INT(_kern_sched_ipiwakeup, OID_AUTO, requested, CTLFLAG_RD,
	   &forward_wakeups_requested, 0,
	   "Requests for Forwarding of wakeup to idle CPUs");

static int forward_wakeups_delivered = 0;
SYSCTL_INT(_kern_sched_ipiwakeup, OID_AUTO, delivered, CTLFLAG_RD,
	   &forward_wakeups_delivered, 0,
	   "Completed Forwarding of wakeup to idle CPUs");

static int forward_wakeup_use_mask = 1;
SYSCTL_INT(_kern_sched_ipiwakeup, OID_AUTO, usemask, CTLFLAG_RW,
	   &forward_wakeup_use_mask, 0,
	   "Use the mask of idle cpus");

static int forward_wakeup_use_loop = 0;
SYSCTL_INT(_kern_sched_ipiwakeup, OID_AUTO, useloop, CTLFLAG_RW,
	   &forward_wakeup_use_loop, 0,
	   "Use a loop to find idle cpus");

static int forward_wakeup_use_single = 0;
SYSCTL_INT(_kern_sched_ipiwakeup, OID_AUTO, onecpu, CTLFLAG_RW,
	   &forward_wakeup_use_single, 0,
	   "Only signal one idle cpu");

static int forward_wakeup_use_htt = 0;
SYSCTL_INT(_kern_sched_ipiwakeup, OID_AUTO, htt2, CTLFLAG_RW,
	   &forward_wakeup_use_htt, 0,
	   "account for htt");

#endif
#if 0
static int sched_followon = 0;
SYSCTL_INT(_kern_sched, OID_AUTO, followon, CTLFLAG_RW,
	   &sched_followon, 0,
	   "allow threads to share a quantum");
#endif

static __inline void
sched_load_add(void)
{
	sched_tdcnt++;
	CTR1(KTR_SCHED, "global load: %d", sched_tdcnt);
}

static __inline void
sched_load_rem(void)
{
	sched_tdcnt--;
	CTR1(KTR_SCHED, "global load: %d", sched_tdcnt);
}
/*
 * Arrange to reschedule if necessary, taking the priorities and
 * schedulers into account.
 */
static void
maybe_resched(struct thread *td)
{

	THREAD_LOCK_ASSERT(td, MA_OWNED);
	if (td->td_priority < curthread->td_priority)
		curthread->td_flags |= TDF_NEEDRESCHED;
}

/*
 * Force switch among equal priority processes every 100ms.
 * We don't actually need to force a context switch of the current process.
 * The act of firing the event triggers a context switch to softclock() and
 * then switching back out again which is equivalent to a preemption, thus
 * no further work is needed on the local CPU.
 */
/* ARGSUSED */
static void
roundrobin(void *arg)
{

#ifdef SMP
	mtx_lock_spin(&sched_lock);
	forward_roundrobin();
	mtx_unlock_spin(&sched_lock);
#endif

	callout_reset(&roundrobin_callout, sched_quantum, roundrobin, NULL);
}

/*
 * Constants for digital decay and forget:
 *	90% of (td_estcpu) usage in 5 * loadav time
 *	95% of (ts_pctcpu) usage in 60 seconds (load insensitive)
 *          Note that, as ps(1) mentions, this can let percentages
 *          total over 100% (I've seen 137.9% for 3 processes).
 *
 * Note that schedclock() updates td_estcpu and p_cpticks asynchronously.
 *
 * We wish to decay away 90% of td_estcpu in (5 * loadavg) seconds.
 * That is, the system wants to compute a value of decay such
 * that the following for loop:
 * 	for (i = 0; i < (5 * loadavg); i++)
 * 		td_estcpu *= decay;
 * will compute
 * 	td_estcpu *= 0.1;
 * for all values of loadavg:
 *
 * Mathematically this loop can be expressed by saying:
 * 	decay ** (5 * loadavg) ~= .1
 *
 * The system computes decay as:
 * 	decay = (2 * loadavg) / (2 * loadavg + 1)
 *
 * We wish to prove that the system's computation of decay
 * will always fulfill the equation:
 * 	decay ** (5 * loadavg) ~= .1
 *
 * If we compute b as:
 * 	b = 2 * loadavg
 * then
 * 	decay = b / (b + 1)
 *
 * We now need to prove two things:
 *	1) Given factor ** (5 * loadavg) ~= .1, prove factor == b/(b+1)
 *	2) Given b/(b+1) ** power ~= .1, prove power == (5 * loadavg)
 *
 * Facts:
 *         For x close to zero, exp(x) =~ 1 + x, since
 *              exp(x) = 0! + x**1/1! + x**2/2! + ... .
 *              therefore exp(-1/b) =~ 1 - (1/b) = (b-1)/b.
 *         For x close to zero, ln(1+x) =~ x, since
 *              ln(1+x) = x - x**2/2 + x**3/3 - ...     -1 < x < 1
 *              therefore ln(b/(b+1)) = ln(1 - 1/(b+1)) =~ -1/(b+1).
 *         ln(.1) =~ -2.30
 *
 * Proof of (1):
 *    Solve (factor)**(power) =~ .1 given power (5*loadav):
 *	solving for factor,
 *      ln(factor) =~ (-2.30/5*loadav), or
 *      factor =~ exp(-1/((5/2.30)*loadav)) =~ exp(-1/(2*loadav)) =
 *          exp(-1/b) =~ (b-1)/b =~ b/(b+1).                    QED
 *
 * Proof of (2):
 *    Solve (factor)**(power) =~ .1 given factor == (b/(b+1)):
 *	solving for power,
 *      power*ln(b/(b+1)) =~ -2.30, or
 *      power =~ 2.3 * (b + 1) = 4.6*loadav + 2.3 =~ 5*loadav.  QED
 *
 * Actual power values for the implemented algorithm are as follows:
 *      loadav: 1       2       3       4
 *      power:  5.68    10.32   14.94   19.55
 */

/* calculations for digital decay to forget 90% of usage in 5*loadav sec */
#define	loadfactor(loadav)	(2 * (loadav))
#define	decay_cpu(loadfac, cpu)	(((loadfac) * (cpu)) / ((loadfac) + FSCALE))

/* decay 95% of `ts_pctcpu' in 60 seconds; see CCPU_SHIFT before changing */
static fixpt_t	ccpu = 0.95122942450071400909 * FSCALE;	/* exp(-1/20) */
SYSCTL_INT(_kern, OID_AUTO, ccpu, CTLFLAG_RD, &ccpu, 0, "");

/*
 * If `ccpu' is not equal to `exp(-1/20)' and you still want to use the
 * faster/more-accurate formula, you'll have to estimate CCPU_SHIFT below
 * and possibly adjust FSHIFT in "param.h" so that (FSHIFT >= CCPU_SHIFT).
 *
 * To estimate CCPU_SHIFT for exp(-1/20), the following formula was used:
 *	1 - exp(-1/20) ~= 0.0487 ~= 0.0488 == 1 (fixed pt, *11* bits).
 *
 * If you don't want to bother with the faster/more-accurate formula, you
 * can set CCPU_SHIFT to (FSHIFT + 1) which will use a slower/less-accurate
 * (more general) method of calculating the %age of CPU used by a process.
 */
#define	CCPU_SHIFT	11

/*
 * Recompute process priorities, every hz ticks.
 * MP-safe, called without the Giant mutex.
 */
/* ARGSUSED */
static void
schedcpu(void)
{
	register fixpt_t loadfac = loadfactor(averunnable.ldavg[0]);
	struct thread *td;
	struct proc *p;
	struct td_sched *ts;
	int awake, realstathz;

	realstathz = stathz ? stathz : hz;
	sx_slock(&allproc_lock);
	FOREACH_PROC_IN_SYSTEM(p) {
		PROC_SLOCK(p);
		FOREACH_THREAD_IN_PROC(p, td) { 
			awake = 0;
			thread_lock(td);
			ts = td->td_sched;
			/*
			 * Increment sleep time (if sleeping).  We
			 * ignore overflow, as above.
			 */
			/*
			 * The td_sched slptimes are not touched in wakeup
			 * because the thread may not HAVE everything in
			 * memory? XXX I think this is out of date.
			 */
			if (TD_ON_RUNQ(td)) {
				awake = 1;
				ts->ts_flags &= ~TSF_DIDRUN;
			} else if (TD_IS_RUNNING(td)) {
				awake = 1;
				/* Do not clear TSF_DIDRUN */
			} else if (ts->ts_flags & TSF_DIDRUN) {
				awake = 1;
				ts->ts_flags &= ~TSF_DIDRUN;
			}

			/*
			 * ts_pctcpu is only for ps and ttyinfo().
			 * Do it per td_sched, and add them up at the end?
			 * XXXKSE
			 */
			ts->ts_pctcpu = (ts->ts_pctcpu * ccpu) >> FSHIFT;
			/*
			 * If the td_sched has been idle the entire second,
			 * stop recalculating its priority until
			 * it wakes up.
			 */
			if (ts->ts_cpticks != 0) {
#if	(FSHIFT >= CCPU_SHIFT)
				ts->ts_pctcpu += (realstathz == 100)
				    ? ((fixpt_t) ts->ts_cpticks) <<
				    (FSHIFT - CCPU_SHIFT) :
				    100 * (((fixpt_t) ts->ts_cpticks)
				    << (FSHIFT - CCPU_SHIFT)) / realstathz;
#else
				ts->ts_pctcpu += ((FSCALE - ccpu) *
				    (ts->ts_cpticks *
				    FSCALE / realstathz)) >> FSHIFT;
#endif
				ts->ts_cpticks = 0;
			}
			/* 
			 * If there are ANY running threads in this process,
			 * then don't count it as sleeping.
XXX  this is broken

			 */
			if (awake) {
				if (ts->ts_slptime > 1) {
					/*
					 * In an ideal world, this should not
					 * happen, because whoever woke us
					 * up from the long sleep should have
					 * unwound the slptime and reset our
					 * priority before we run at the stale
					 * priority.  Should KASSERT at some
					 * point when all the cases are fixed.
					 */
					updatepri(td);
				}
				ts->ts_slptime = 0;
			} else
				ts->ts_slptime++;
			if (ts->ts_slptime > 1) {
				thread_unlock(td);
				continue;
			}
			td->td_estcpu = decay_cpu(loadfac, td->td_estcpu);
		      	resetpriority(td);
			resetpriority_thread(td);
			thread_unlock(td);
		} /* end of thread loop */
		PROC_SUNLOCK(p);
	} /* end of process loop */
	sx_sunlock(&allproc_lock);
}

/*
 * Main loop for a kthread that executes schedcpu once a second.
 */
static void
schedcpu_thread(void)
{

	for (;;) {
		schedcpu();
		pause("-", hz);
	}
}

/*
 * Recalculate the priority of a process after it has slept for a while.
 * For all load averages >= 1 and max td_estcpu of 255, sleeping for at
 * least six times the loadfactor will decay td_estcpu to zero.
 */
static void
updatepri(struct thread *td)
{
	struct td_sched *ts;
	fixpt_t loadfac;
	unsigned int newcpu;

	ts = td->td_sched;
	loadfac = loadfactor(averunnable.ldavg[0]);
	if (ts->ts_slptime > 5 * loadfac)
		td->td_estcpu = 0;
	else {
		newcpu = td->td_estcpu;
		ts->ts_slptime--;	/* was incremented in schedcpu() */
		while (newcpu && --ts->ts_slptime)
			newcpu = decay_cpu(loadfac, newcpu);
		td->td_estcpu = newcpu;
	}
}

/*
 * Compute the priority of a process when running in user mode.
 * Arrange to reschedule if the resulting priority is better
 * than that of the current process.
 */
static void
resetpriority(struct thread *td)
{
	register unsigned int newpriority;

	if (td->td_pri_class == PRI_TIMESHARE) {
		newpriority = PUSER + td->td_estcpu / INVERSE_ESTCPU_WEIGHT +
		    NICE_WEIGHT * (td->td_proc->p_nice - PRIO_MIN);
		newpriority = min(max(newpriority, PRI_MIN_TIMESHARE),
		    PRI_MAX_TIMESHARE);
		sched_user_prio(td, newpriority);
	}
}

/*
 * Update the thread's priority when the associated process's user
 * priority changes.
 */
static void
resetpriority_thread(struct thread *td)
{

	/* Only change threads with a time sharing user priority. */
	if (td->td_priority < PRI_MIN_TIMESHARE ||
	    td->td_priority > PRI_MAX_TIMESHARE)
		return;

	/* XXX the whole needresched thing is broken, but not silly. */
	maybe_resched(td);

	sched_prio(td, td->td_user_pri);
}

/* ARGSUSED */
static void
sched_setup(void *dummy)
{
	setup_runqs();

	if (sched_quantum == 0)
		sched_quantum = SCHED_QUANTUM;
	hogticks = 2 * sched_quantum;

	callout_init(&roundrobin_callout, CALLOUT_MPSAFE);

	/* Kick off timeout driven events by calling first time. */
	roundrobin(NULL);

	/* Account for thread0. */
	sched_load_add();
}

/* External interfaces start here */
/*
 * Very early in the boot some setup of scheduler-specific
 * parts of proc0 and of some scheduler resources needs to be done.
 * Called from:
 *  proc0_init()
 */
void
schedinit(void)
{
	/*
	 * Set up the scheduler specific parts of proc0.
	 */
	proc0.p_sched = NULL; /* XXX */
	thread0.td_sched = &td_sched0;
	thread0.td_lock = &sched_lock;
	td_sched0.ts_thread = &thread0;
	mtx_init(&sched_lock, "sched lock", NULL, MTX_SPIN | MTX_RECURSE);
}

int
sched_runnable(void)
{
#ifdef SMP
	return runq_check(&runq) + runq_check(&runq_pcpu[PCPU_GET(cpuid)]);
#else
	return runq_check(&runq);
#endif
}

int 
sched_rr_interval(void)
{
	if (sched_quantum == 0)
		sched_quantum = SCHED_QUANTUM;
	return (sched_quantum);
}

/*
 * We adjust the priority of the current process.  The priority of
 * a process gets worse as it accumulates CPU time.  The cpu usage
 * estimator (td_estcpu) is increased here.  resetpriority() will
 * compute a different priority each time td_estcpu increases by
 * INVERSE_ESTCPU_WEIGHT
 * (until MAXPRI is reached).  The cpu usage estimator ramps up
 * quite quickly when the process is running (linearly), and decays
 * away exponentially, at a rate which is proportionally slower when
 * the system is busy.  The basic principle is that the system will
 * 90% forget that the process used a lot of CPU time in 5 * loadav
 * seconds.  This causes the system to favor processes which haven't
 * run much recently, and to round-robin among other processes.
 */
void
sched_clock(struct thread *td)
{
	struct td_sched *ts;

	THREAD_LOCK_ASSERT(td, MA_OWNED);
	ts = td->td_sched;

	ts->ts_cpticks++;
	td->td_estcpu = ESTCPULIM(td->td_estcpu + 1);
	if ((td->td_estcpu % INVERSE_ESTCPU_WEIGHT) == 0) {
		resetpriority(td);
		resetpriority_thread(td);
	}
}

/*
 * charge childs scheduling cpu usage to parent.
 */
void
sched_exit(struct proc *p, struct thread *td)
{

	CTR3(KTR_SCHED, "sched_exit: %p(%s) prio %d",
	    td, td->td_proc->p_comm, td->td_priority);
	PROC_SLOCK_ASSERT(p, MA_OWNED);
	sched_exit_thread(FIRST_THREAD_IN_PROC(p), td);
}

void
sched_exit_thread(struct thread *td, struct thread *child)
{

	CTR3(KTR_SCHED, "sched_exit_thread: %p(%s) prio %d",
	    child, child->td_proc->p_comm, child->td_priority);
	thread_lock(td);
	td->td_estcpu = ESTCPULIM(td->td_estcpu + child->td_estcpu);
	thread_unlock(td);
	mtx_lock_spin(&sched_lock);
	if ((child->td_proc->p_flag & P_NOLOAD) == 0)
		sched_load_rem();
	mtx_unlock_spin(&sched_lock);
}

void
sched_fork(struct thread *td, struct thread *childtd)
{
	sched_fork_thread(td, childtd);
}

void
sched_fork_thread(struct thread *td, struct thread *childtd)
{
	childtd->td_estcpu = td->td_estcpu;
	childtd->td_lock = &sched_lock;
	sched_newthread(childtd);
}

void
sched_nice(struct proc *p, int nice)
{
	struct thread *td;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	PROC_SLOCK_ASSERT(p, MA_OWNED);
	p->p_nice = nice;
	FOREACH_THREAD_IN_PROC(p, td) {
		thread_lock(td);
		resetpriority(td);
		resetpriority_thread(td);
		thread_unlock(td);
	}
}

void
sched_class(struct thread *td, int class)
{
	THREAD_LOCK_ASSERT(td, MA_OWNED);
	td->td_pri_class = class;
}

/*
 * Adjust the priority of a thread.
 */
static void
sched_priority(struct thread *td, u_char prio)
{
	CTR6(KTR_SCHED, "sched_prio: %p(%s) prio %d newprio %d by %p(%s)",
	    td, td->td_proc->p_comm, td->td_priority, prio, curthread, 
	    curthread->td_proc->p_comm);

	THREAD_LOCK_ASSERT(td, MA_OWNED);
	if (td->td_priority == prio)
		return;
	td->td_priority = prio;
	if (TD_ON_RUNQ(td) && 
	    td->td_sched->ts_rqindex != (prio / RQ_PPQ)) {
		sched_rem(td);
		sched_add(td, SRQ_BORING);
	}
}

/*
 * Update a thread's priority when it is lent another thread's
 * priority.
 */
void
sched_lend_prio(struct thread *td, u_char prio)
{

	td->td_flags |= TDF_BORROWING;
	sched_priority(td, prio);
}

/*
 * Restore a thread's priority when priority propagation is
 * over.  The prio argument is the minimum priority the thread
 * needs to have to satisfy other possible priority lending
 * requests.  If the thread's regulary priority is less
 * important than prio the thread will keep a priority boost
 * of prio.
 */
void
sched_unlend_prio(struct thread *td, u_char prio)
{
	u_char base_pri;

	if (td->td_base_pri >= PRI_MIN_TIMESHARE &&
	    td->td_base_pri <= PRI_MAX_TIMESHARE)
		base_pri = td->td_user_pri;
	else
		base_pri = td->td_base_pri;
	if (prio >= base_pri) {
		td->td_flags &= ~TDF_BORROWING;
		sched_prio(td, base_pri);
	} else
		sched_lend_prio(td, prio);
}

void
sched_prio(struct thread *td, u_char prio)
{
	u_char oldprio;

	/* First, update the base priority. */
	td->td_base_pri = prio;

	/*
	 * If the thread is borrowing another thread's priority, don't ever
	 * lower the priority.
	 */
	if (td->td_flags & TDF_BORROWING && td->td_priority < prio)
		return;

	/* Change the real priority. */
	oldprio = td->td_priority;
	sched_priority(td, prio);

	/*
	 * If the thread is on a turnstile, then let the turnstile update
	 * its state.
	 */
	if (TD_ON_LOCK(td) && oldprio != prio)
		turnstile_adjust(td, oldprio);
}

void
sched_user_prio(struct thread *td, u_char prio)
{
	u_char oldprio;

	THREAD_LOCK_ASSERT(td, MA_OWNED);
	td->td_base_user_pri = prio;
	if (td->td_flags & TDF_UBORROWING && td->td_user_pri <= prio)
		return;
	oldprio = td->td_user_pri;
	td->td_user_pri = prio;
}

void
sched_lend_user_prio(struct thread *td, u_char prio)
{
	u_char oldprio;

	THREAD_LOCK_ASSERT(td, MA_OWNED);
	td->td_flags |= TDF_UBORROWING;

	oldprio = td->td_user_pri;
	td->td_user_pri = prio;
}

void
sched_unlend_user_prio(struct thread *td, u_char prio)
{
	u_char base_pri;

	THREAD_LOCK_ASSERT(td, MA_OWNED);
	base_pri = td->td_base_user_pri;
	if (prio >= base_pri) {
		td->td_flags &= ~TDF_UBORROWING;
		sched_user_prio(td, base_pri);
	} else {
		sched_lend_user_prio(td, prio);
	}
}

void
sched_sleep(struct thread *td)
{

	THREAD_LOCK_ASSERT(td, MA_OWNED);
	td->td_slptick = ticks;
	td->td_sched->ts_slptime = 0;
}

void
sched_switch(struct thread *td, struct thread *newtd, int flags)
{
	struct td_sched *ts;
	struct proc *p;

	ts = td->td_sched;
	p = td->td_proc;

	THREAD_LOCK_ASSERT(td, MA_OWNED);
	/*  
	 * Switch to the sched lock to fix things up and pick
	 * a new thread.
	 */
	if (td->td_lock != &sched_lock) {
		mtx_lock_spin(&sched_lock);
		thread_unlock(td);
	}

	if ((p->p_flag & P_NOLOAD) == 0)
		sched_load_rem();

	if (newtd) 
		newtd->td_flags |= (td->td_flags & TDF_NEEDRESCHED);

	td->td_lastcpu = td->td_oncpu;
	td->td_flags &= ~TDF_NEEDRESCHED;
	td->td_owepreempt = 0;
	td->td_oncpu = NOCPU;
	/*
	 * At the last moment, if this thread is still marked RUNNING,
	 * then put it back on the run queue as it has not been suspended
	 * or stopped or any thing else similar.  We never put the idle
	 * threads on the run queue, however.
	 */
	if (td->td_flags & TDF_IDLETD) {
		TD_SET_CAN_RUN(td);
#ifdef SMP
		idle_cpus_mask &= ~PCPU_GET(cpumask);
#endif
	} else {
		if (TD_IS_RUNNING(td)) {
			/* Put us back on the run queue. */
			sched_add(td, (flags & SW_PREEMPT) ?
			    SRQ_OURSELF|SRQ_YIELDING|SRQ_PREEMPTED :
			    SRQ_OURSELF|SRQ_YIELDING);
		}
	}
	if (newtd) {
		/* 
		 * The thread we are about to run needs to be counted
		 * as if it had been added to the run queue and selected.
		 * It came from:
		 * * A preemption
		 * * An upcall 
		 * * A followon
		 */
		KASSERT((newtd->td_inhibitors == 0),
			("trying to run inhibited thread"));
		newtd->td_sched->ts_flags |= TSF_DIDRUN;
        	TD_SET_RUNNING(newtd);
		if ((newtd->td_proc->p_flag & P_NOLOAD) == 0)
			sched_load_add();
	} else {
		newtd = choosethread();
	}
	MPASS(newtd->td_lock == &sched_lock);

	if (td != newtd) {
#ifdef	HWPMC_HOOKS
		if (PMC_PROC_IS_USING_PMCS(td->td_proc))
			PMC_SWITCH_CONTEXT(td, PMC_FN_CSW_OUT);
#endif

                /* I feel sleepy */
		cpu_switch(td, newtd, td->td_lock);
		/*
		 * Where am I?  What year is it?
		 * We are in the same thread that went to sleep above,
		 * but any amount of time may have passed. All out context
		 * will still be available as will local variables.
		 * PCPU values however may have changed as we may have
		 * changed CPU so don't trust cached values of them.
		 * New threads will go to fork_exit() instead of here
		 * so if you change things here you may need to change
		 * things there too.
		 * If the thread above was exiting it will never wake
		 * up again here, so either it has saved everything it
		 * needed to, or the thread_wait() or wait() will
		 * need to reap it.
		 */
#ifdef	HWPMC_HOOKS
		if (PMC_PROC_IS_USING_PMCS(td->td_proc))
			PMC_SWITCH_CONTEXT(td, PMC_FN_CSW_IN);
#endif
	}

#ifdef SMP
	if (td->td_flags & TDF_IDLETD)
		idle_cpus_mask |= PCPU_GET(cpumask);
#endif
	sched_lock.mtx_lock = (uintptr_t)td;
	td->td_oncpu = PCPU_GET(cpuid);
	MPASS(td->td_lock == &sched_lock);
}

void
sched_wakeup(struct thread *td)
{
	struct td_sched *ts;

	THREAD_LOCK_ASSERT(td, MA_OWNED);
	ts = td->td_sched;
	if (ts->ts_slptime > 1) {
		updatepri(td);
		resetpriority(td);
	}
	td->td_slptick = ticks;
	ts->ts_slptime = 0;
	sched_add(td, SRQ_BORING);
}

#ifdef SMP
/* enable HTT_2 if you have a 2-way HTT cpu.*/
static int
forward_wakeup(int  cpunum)
{
	cpumask_t map, me, dontuse;
	cpumask_t map2;
	struct pcpu *pc;
	cpumask_t id, map3;

	mtx_assert(&sched_lock, MA_OWNED);

	CTR0(KTR_RUNQ, "forward_wakeup()");

	if ((!forward_wakeup_enabled) ||
	     (forward_wakeup_use_mask == 0 && forward_wakeup_use_loop == 0))
		return (0);
	if (!smp_started || cold || panicstr)
		return (0);

	forward_wakeups_requested++;

/*
 * check the idle mask we received against what we calculated before
 * in the old version.
 */
	me = PCPU_GET(cpumask);
	/* 
	 * don't bother if we should be doing it ourself..
	 */
	if ((me & idle_cpus_mask) && (cpunum == NOCPU || me == (1 << cpunum)))
		return (0);

	dontuse = me | stopped_cpus | hlt_cpus_mask;
	map3 = 0;
	if (forward_wakeup_use_loop) {
		SLIST_FOREACH(pc, &cpuhead, pc_allcpu) {
			id = pc->pc_cpumask;
			if ( (id & dontuse) == 0 &&
			    pc->pc_curthread == pc->pc_idlethread) {
				map3 |= id;
			}
		}
	}

	if (forward_wakeup_use_mask) {
		map = 0;
		map = idle_cpus_mask & ~dontuse;

		/* If they are both on, compare and use loop if different */
		if (forward_wakeup_use_loop) {
			if (map != map3) {
				printf("map (%02X) != map3 (%02X)\n",
						map, map3);
				map = map3;
			}
		}
	} else {
		map = map3;
	}
	/* If we only allow a specific CPU, then mask off all the others */
	if (cpunum != NOCPU) {
		KASSERT((cpunum <= mp_maxcpus),("forward_wakeup: bad cpunum."));
		map &= (1 << cpunum);
	} else {
		/* Try choose an idle die. */
		if (forward_wakeup_use_htt) {
			map2 =  (map & (map >> 1)) & 0x5555;
			if (map2) {
				map = map2;
			}
		}

		/* set only one bit */ 
		if (forward_wakeup_use_single) {
			map = map & ((~map) + 1);
		}
	}
	if (map) {
		forward_wakeups_delivered++;
		ipi_selected(map, IPI_AST);
		return (1);
	}
	if (cpunum == NOCPU)
		printf("forward_wakeup: Idle processor not found\n");
	return (0);
}
#endif

#ifdef SMP
static void kick_other_cpu(int pri,int cpuid);

static void
kick_other_cpu(int pri,int cpuid)
{	
	struct pcpu * pcpu = pcpu_find(cpuid);
	int cpri = pcpu->pc_curthread->td_priority;

	if (idle_cpus_mask & pcpu->pc_cpumask) {
		forward_wakeups_delivered++;
		ipi_selected(pcpu->pc_cpumask, IPI_AST);
		return;
	}

	if (pri >= cpri)
		return;

#if defined(IPI_PREEMPTION) && defined(PREEMPTION)
#if !defined(FULL_PREEMPTION)
	if (pri <= PRI_MAX_ITHD)
#endif /* ! FULL_PREEMPTION */
	{
		ipi_selected(pcpu->pc_cpumask, IPI_PREEMPT);
		return;
	}
#endif /* defined(IPI_PREEMPTION) && defined(PREEMPTION) */

	pcpu->pc_curthread->td_flags |= TDF_NEEDRESCHED;
	ipi_selected( pcpu->pc_cpumask , IPI_AST);
	return;
}
#endif /* SMP */

void
sched_add(struct thread *td, int flags)
#ifdef SMP
{
	struct td_sched *ts;
	int forwarded = 0;
	int cpu;
	int single_cpu = 0;

	ts = td->td_sched;
	THREAD_LOCK_ASSERT(td, MA_OWNED);
	KASSERT((td->td_inhibitors == 0),
	    ("sched_add: trying to run inhibited thread"));
	KASSERT((TD_CAN_RUN(td) || TD_IS_RUNNING(td)),
	    ("sched_add: bad thread state"));
	KASSERT(td->td_flags & TDF_INMEM,
	    ("sched_add: thread swapped out"));
	CTR5(KTR_SCHED, "sched_add: %p(%s) prio %d by %p(%s)",
	    td, td->td_proc->p_comm, td->td_priority, curthread,
	    curthread->td_proc->p_comm);
	/*
	 * Now that the thread is moving to the run-queue, set the lock
	 * to the scheduler's lock.
	 */
	if (td->td_lock != &sched_lock) {
		mtx_lock_spin(&sched_lock);
		thread_lock_set(td, &sched_lock);
	}
	TD_SET_RUNQ(td);

	if (td->td_pinned != 0) {
		cpu = td->td_lastcpu;
		ts->ts_runq = &runq_pcpu[cpu];
		single_cpu = 1;
		CTR3(KTR_RUNQ,
		    "sched_add: Put td_sched:%p(td:%p) on cpu%d runq", ts, td, cpu);
	} else if ((ts)->ts_flags & TSF_BOUND) {
		/* Find CPU from bound runq */
		KASSERT(SKE_RUNQ_PCPU(ts),("sched_add: bound td_sched not on cpu runq"));
		cpu = ts->ts_runq - &runq_pcpu[0];
		single_cpu = 1;
		CTR3(KTR_RUNQ,
		    "sched_add: Put td_sched:%p(td:%p) on cpu%d runq", ts, td, cpu);
	} else {	
		CTR2(KTR_RUNQ,
		    "sched_add: adding td_sched:%p (td:%p) to gbl runq", ts, td);
		cpu = NOCPU;
		ts->ts_runq = &runq;
	}
	
	if (single_cpu && (cpu != PCPU_GET(cpuid))) {
	        kick_other_cpu(td->td_priority,cpu);
	} else {
		
		if (!single_cpu) {
			cpumask_t me = PCPU_GET(cpumask);
			int idle = idle_cpus_mask & me;	

			if (!idle && ((flags & SRQ_INTR) == 0) &&
			    (idle_cpus_mask & ~(hlt_cpus_mask | me)))
				forwarded = forward_wakeup(cpu);
		}

		if (!forwarded) {
			if ((flags & SRQ_YIELDING) == 0 && maybe_preempt(td))
				return;
			else
				maybe_resched(td);
		}
	}
	
	if ((td->td_proc->p_flag & P_NOLOAD) == 0)
		sched_load_add();
	runq_add(ts->ts_runq, ts, flags);
}
#else /* SMP */
{
	struct td_sched *ts;
	ts = td->td_sched;
	THREAD_LOCK_ASSERT(td, MA_OWNED);
	KASSERT((td->td_inhibitors == 0),
	    ("sched_add: trying to run inhibited thread"));
	KASSERT((TD_CAN_RUN(td) || TD_IS_RUNNING(td)),
	    ("sched_add: bad thread state"));
	KASSERT(td->td_flags & TDF_INMEM,
	    ("sched_add: thread swapped out"));
	CTR5(KTR_SCHED, "sched_add: %p(%s) prio %d by %p(%s)",
	    td, td->td_proc->p_comm, td->td_priority, curthread,
	    curthread->td_proc->p_comm);
	/*
	 * Now that the thread is moving to the run-queue, set the lock
	 * to the scheduler's lock.
	 */
	if (td->td_lock != &sched_lock) {
		mtx_lock_spin(&sched_lock);
		thread_lock_set(td, &sched_lock);
	}
	TD_SET_RUNQ(td);
	CTR2(KTR_RUNQ, "sched_add: adding td_sched:%p (td:%p) to runq", ts, td);
	ts->ts_runq = &runq;

	/* 
	 * If we are yielding (on the way out anyhow) 
	 * or the thread being saved is US,
	 * then don't try be smart about preemption
	 * or kicking off another CPU
	 * as it won't help and may hinder.
	 * In the YIEDLING case, we are about to run whoever is 
	 * being put in the queue anyhow, and in the 
	 * OURSELF case, we are puting ourself on the run queue
	 * which also only happens when we are about to yield.
	 */
	if((flags & SRQ_YIELDING) == 0) {
		if (maybe_preempt(td))
			return;
	}	
	if ((td->td_proc->p_flag & P_NOLOAD) == 0)
		sched_load_add();
	runq_add(ts->ts_runq, ts, flags);
	maybe_resched(td);
}
#endif /* SMP */

void
sched_rem(struct thread *td)
{
	struct td_sched *ts;

	ts = td->td_sched;
	KASSERT(td->td_flags & TDF_INMEM,
	    ("sched_rem: thread swapped out"));
	KASSERT(TD_ON_RUNQ(td),
	    ("sched_rem: thread not on run queue"));
	mtx_assert(&sched_lock, MA_OWNED);
	CTR5(KTR_SCHED, "sched_rem: %p(%s) prio %d by %p(%s)",
	    td, td->td_proc->p_comm, td->td_priority, curthread,
	    curthread->td_proc->p_comm);

	if ((td->td_proc->p_flag & P_NOLOAD) == 0)
		sched_load_rem();
	runq_remove(ts->ts_runq, ts);
	TD_SET_CAN_RUN(td);
}

/*
 * Select threads to run.
 * Notice that the running threads still consume a slot.
 */
struct thread *
sched_choose(void)
{
	struct td_sched *ts;
	struct runq *rq;

	mtx_assert(&sched_lock,  MA_OWNED);
#ifdef SMP
	struct td_sched *kecpu;

	rq = &runq;
	ts = runq_choose(&runq);
	kecpu = runq_choose(&runq_pcpu[PCPU_GET(cpuid)]);

	if (ts == NULL || 
	    (kecpu != NULL && 
	     kecpu->ts_thread->td_priority < ts->ts_thread->td_priority)) {
		CTR2(KTR_RUNQ, "choosing td_sched %p from pcpu runq %d", kecpu,
		     PCPU_GET(cpuid));
		ts = kecpu;
		rq = &runq_pcpu[PCPU_GET(cpuid)];
	} else { 
		CTR1(KTR_RUNQ, "choosing td_sched %p from main runq", ts);
	}

#else
	rq = &runq;
	ts = runq_choose(&runq);
#endif

	if (ts) {
		runq_remove(rq, ts);
		ts->ts_flags |= TSF_DIDRUN;

		KASSERT(ts->ts_thread->td_flags & TDF_INMEM,
		    ("sched_choose: thread swapped out"));
		return (ts->ts_thread);
	} 
	return (PCPU_GET(idlethread));
}

void
sched_userret(struct thread *td)
{
	/*
	 * XXX we cheat slightly on the locking here to avoid locking in
	 * the usual case.  Setting td_priority here is essentially an
	 * incomplete workaround for not setting it properly elsewhere.
	 * Now that some interrupt handlers are threads, not setting it
	 * properly elsewhere can clobber it in the window between setting
	 * it here and returning to user mode, so don't waste time setting
	 * it perfectly here.
	 */
	KASSERT((td->td_flags & TDF_BORROWING) == 0,
	    ("thread with borrowed priority returning to userland"));
	if (td->td_priority != td->td_user_pri) {
		thread_lock(td);
		td->td_priority = td->td_user_pri;
		td->td_base_pri = td->td_user_pri;
		thread_unlock(td);
	}
}

void
sched_bind(struct thread *td, int cpu)
{
	struct td_sched *ts;

	THREAD_LOCK_ASSERT(td, MA_OWNED);
	KASSERT(TD_IS_RUNNING(td),
	    ("sched_bind: cannot bind non-running thread"));

	ts = td->td_sched;

	ts->ts_flags |= TSF_BOUND;
#ifdef SMP
	ts->ts_runq = &runq_pcpu[cpu];
	if (PCPU_GET(cpuid) == cpu)
		return;

	mi_switch(SW_VOL, NULL);
#endif
}

void
sched_unbind(struct thread* td)
{
	THREAD_LOCK_ASSERT(td, MA_OWNED);
	td->td_sched->ts_flags &= ~TSF_BOUND;
}

int
sched_is_bound(struct thread *td)
{
	THREAD_LOCK_ASSERT(td, MA_OWNED);
	return (td->td_sched->ts_flags & TSF_BOUND);
}

void
sched_relinquish(struct thread *td)
{
	thread_lock(td);
	SCHED_STAT_INC(switch_relinquish);
	mi_switch(SW_VOL, NULL);
	thread_unlock(td);
}

int
sched_load(void)
{
	return (sched_tdcnt);
}

int
sched_sizeof_proc(void)
{
	return (sizeof(struct proc));
}

int
sched_sizeof_thread(void)
{
	return (sizeof(struct thread) + sizeof(struct td_sched));
}

fixpt_t
sched_pctcpu(struct thread *td)
{
	struct td_sched *ts;

	ts = td->td_sched;
	return (ts->ts_pctcpu);
}

void
sched_tick(void)
{
}

/*
 * The actual idle process.
 */
void
sched_idletd(void *dummy)
{
	struct proc *p;
	struct thread *td;

	td = curthread;
	p = td->td_proc;
	for (;;) {
		mtx_assert(&Giant, MA_NOTOWNED);

		while (sched_runnable() == 0)
			cpu_idle();

		mtx_lock_spin(&sched_lock);
		mi_switch(SW_VOL, NULL);
		mtx_unlock_spin(&sched_lock);
	}
}

/*
 * A CPU is entering for the first time or a thread is exiting.
 */
void
sched_throw(struct thread *td)
{
	/*
	 * Correct spinlock nesting.  The idle thread context that we are
	 * borrowing was created so that it would start out with a single
	 * spin lock (sched_lock) held in fork_trampoline().  Since we've
	 * explicitly acquired locks in this function, the nesting count
	 * is now 2 rather than 1.  Since we are nested, calling
	 * spinlock_exit() will simply adjust the counts without allowing
	 * spin lock using code to interrupt us.
	 */
	if (td == NULL) {
		mtx_lock_spin(&sched_lock);
		spinlock_exit();
	} else {
		MPASS(td->td_lock == &sched_lock);
	}
	mtx_assert(&sched_lock, MA_OWNED);
	KASSERT(curthread->td_md.md_spinlock_count == 1, ("invalid count"));
	PCPU_SET(switchtime, cpu_ticks());
	PCPU_SET(switchticks, ticks);
	cpu_throw(td, choosethread());	/* doesn't return */
}

void
sched_fork_exit(struct thread *td)
{

	/*
	 * Finish setting up thread glue so that it begins execution in a
	 * non-nested critical section with sched_lock held but not recursed.
	 */
	td->td_oncpu = PCPU_GET(cpuid);
	sched_lock.mtx_lock = (uintptr_t)td;
	THREAD_LOCK_ASSERT(td, MA_OWNED | MA_NOTRECURSED);
}

#define KERN_SWITCH_INCLUDE 1
#include "kern/kern_switch.c"
