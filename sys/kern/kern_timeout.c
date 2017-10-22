/*-
 * Copyright (c) 1982, 1986, 1991, 1993
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
 *	From: @(#)kern_clock.c	8.5 (Berkeley) 1/21/94
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/7.0.0/sys/kern/kern_timeout.c 172184 2007-09-15 12:33:24Z rwatson $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/condvar.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sleepqueue.h>
#include <sys/sysctl.h>

static int avg_depth;
SYSCTL_INT(_debug, OID_AUTO, to_avg_depth, CTLFLAG_RD, &avg_depth, 0,
    "Average number of items examined per softclock call. Units = 1/1000");
static int avg_gcalls;
SYSCTL_INT(_debug, OID_AUTO, to_avg_gcalls, CTLFLAG_RD, &avg_gcalls, 0,
    "Average number of Giant callouts made per softclock call. Units = 1/1000");
static int avg_mtxcalls;
SYSCTL_INT(_debug, OID_AUTO, to_avg_mtxcalls, CTLFLAG_RD, &avg_mtxcalls, 0,
    "Average number of mtx callouts made per softclock call. Units = 1/1000");
static int avg_mpcalls;
SYSCTL_INT(_debug, OID_AUTO, to_avg_mpcalls, CTLFLAG_RD, &avg_mpcalls, 0,
    "Average number of MP callouts made per softclock call. Units = 1/1000");
/*
 * TODO:
 *	allocate more timeout table slots when table overflows.
 */

/* Exported to machdep.c and/or kern_clock.c.  */
struct callout *callout;
struct callout_list callfree;
int callwheelsize, callwheelbits, callwheelmask;
struct callout_tailq *callwheel;
int softticks;			/* Like ticks, but for softclock(). */
struct mtx callout_lock;

static struct callout *nextsoftcheck;	/* Next callout to be checked. */

/**
 * Locked by callout_lock:
 *   curr_callout    - If a callout is in progress, it is curr_callout.
 *                     If curr_callout is non-NULL, threads waiting in
 *                     callout_drain() will be woken up as soon as the 
 *                     relevant callout completes.
 *   curr_cancelled  - Changing to 1 with both callout_lock and c_mtx held
 *                     guarantees that the current callout will not run.
 *                     The softclock() function sets this to 0 before it
 *                     drops callout_lock to acquire c_mtx, and it calls
 *                     the handler only if curr_cancelled is still 0 after
 *                     c_mtx is successfully acquired.
 *   callout_wait    - If a thread is waiting in callout_drain(), then
 *                     callout_wait is nonzero.  Set only when
 *                     curr_callout is non-NULL.
 */
static struct callout *curr_callout;
static int curr_cancelled;
static int callout_wait;

/*
 * kern_timeout_callwheel_alloc() - kernel low level callwheel initialization 
 *
 *	This code is called very early in the kernel initialization sequence,
 *	and may be called more then once.
 */
caddr_t
kern_timeout_callwheel_alloc(caddr_t v)
{
	/*
	 * Calculate callout wheel size
	 */
	for (callwheelsize = 1, callwheelbits = 0;
	     callwheelsize < ncallout;
	     callwheelsize <<= 1, ++callwheelbits)
		;
	callwheelmask = callwheelsize - 1;

	callout = (struct callout *)v;
	v = (caddr_t)(callout + ncallout);
	callwheel = (struct callout_tailq *)v;
	v = (caddr_t)(callwheel + callwheelsize);
	return(v);
}

/*
 * kern_timeout_callwheel_init() - initialize previously reserved callwheel
 *				   space.
 *
 *	This code is called just once, after the space reserved for the
 *	callout wheel has been finalized.
 */
void
kern_timeout_callwheel_init(void)
{
	int i;

	SLIST_INIT(&callfree);
	for (i = 0; i < ncallout; i++) {
		callout_init(&callout[i], 0);
		callout[i].c_flags = CALLOUT_LOCAL_ALLOC;
		SLIST_INSERT_HEAD(&callfree, &callout[i], c_links.sle);
	}
	for (i = 0; i < callwheelsize; i++) {
		TAILQ_INIT(&callwheel[i]);
	}
	mtx_init(&callout_lock, "callout", NULL, MTX_SPIN | MTX_RECURSE);
}

/*
 * The callout mechanism is based on the work of Adam M. Costello and 
 * George Varghese, published in a technical report entitled "Redesigning
 * the BSD Callout and Timer Facilities" and modified slightly for inclusion
 * in FreeBSD by Justin T. Gibbs.  The original work on the data structures
 * used in this implementation was published by G. Varghese and T. Lauck in
 * the paper "Hashed and Hierarchical Timing Wheels: Data Structures for
 * the Efficient Implementation of a Timer Facility" in the Proceedings of
 * the 11th ACM Annual Symposium on Operating Systems Principles,
 * Austin, Texas Nov 1987.
 */

/*
 * Software (low priority) clock interrupt.
 * Run periodic events from timeout queue.
 */
void
softclock(void *dummy)
{
	struct callout *c;
	struct callout_tailq *bucket;
	int curticks;
	int steps;	/* #steps since we last allowed interrupts */
	int depth;
	int mpcalls;
	int mtxcalls;
	int gcalls;
#ifdef DIAGNOSTIC
	struct bintime bt1, bt2;
	struct timespec ts2;
	static uint64_t maxdt = 36893488147419102LL;	/* 2 msec */
	static timeout_t *lastfunc;
#endif

#ifndef MAX_SOFTCLOCK_STEPS
#define MAX_SOFTCLOCK_STEPS 100 /* Maximum allowed value of steps. */
#endif /* MAX_SOFTCLOCK_STEPS */

	mpcalls = 0;
	mtxcalls = 0;
	gcalls = 0;
	depth = 0;
	steps = 0;
	mtx_lock_spin(&callout_lock);
	while (softticks != ticks) {
		softticks++;
		/*
		 * softticks may be modified by hard clock, so cache
		 * it while we work on a given bucket.
		 */
		curticks = softticks;
		bucket = &callwheel[curticks & callwheelmask];
		c = TAILQ_FIRST(bucket);
		while (c) {
			depth++;
			if (c->c_time != curticks) {
				c = TAILQ_NEXT(c, c_links.tqe);
				++steps;
				if (steps >= MAX_SOFTCLOCK_STEPS) {
					nextsoftcheck = c;
					/* Give interrupts a chance. */
					mtx_unlock_spin(&callout_lock);
					;	/* nothing */
					mtx_lock_spin(&callout_lock);
					c = nextsoftcheck;
					steps = 0;
				}
			} else {
				void (*c_func)(void *);
				void *c_arg;
				struct mtx *c_mtx;
				int c_flags;

				nextsoftcheck = TAILQ_NEXT(c, c_links.tqe);
				TAILQ_REMOVE(bucket, c, c_links.tqe);
				c_func = c->c_func;
				c_arg = c->c_arg;
				c_mtx = c->c_mtx;
				c_flags = c->c_flags;
				if (c->c_flags & CALLOUT_LOCAL_ALLOC) {
					c->c_func = NULL;
					c->c_flags = CALLOUT_LOCAL_ALLOC;
					SLIST_INSERT_HEAD(&callfree, c,
							  c_links.sle);
					curr_callout = NULL;
				} else {
					c->c_flags =
					    (c->c_flags & ~CALLOUT_PENDING);
					curr_callout = c;
				}
				curr_cancelled = 0;
				mtx_unlock_spin(&callout_lock);
				if (c_mtx != NULL) {
					mtx_lock(c_mtx);
					/*
					 * The callout may have been cancelled
					 * while we switched locks.
					 */
					if (curr_cancelled) {
						mtx_unlock(c_mtx);
						goto skip;
					}
					/* The callout cannot be stopped now. */
					curr_cancelled = 1;

					if (c_mtx == &Giant) {
						gcalls++;
						CTR3(KTR_CALLOUT,
						    "callout %p func %p arg %p",
						    c, c_func, c_arg);
					} else {
						mtxcalls++;
						CTR3(KTR_CALLOUT, "callout mtx"
						    " %p func %p arg %p",
						    c, c_func, c_arg);
					}
				} else {
					mpcalls++;
					CTR3(KTR_CALLOUT,
					    "callout mpsafe %p func %p arg %p",
					    c, c_func, c_arg);
				}
#ifdef DIAGNOSTIC
				binuptime(&bt1);
#endif
				THREAD_NO_SLEEPING();
				c_func(c_arg);
				THREAD_SLEEPING_OK();
#ifdef DIAGNOSTIC
				binuptime(&bt2);
				bintime_sub(&bt2, &bt1);
				if (bt2.frac > maxdt) {
					if (lastfunc != c_func ||
					    bt2.frac > maxdt * 2) {
						bintime2timespec(&bt2, &ts2);
						printf(
			"Expensive timeout(9) function: %p(%p) %jd.%09ld s\n",
						    c_func, c_arg,
						    (intmax_t)ts2.tv_sec,
						    ts2.tv_nsec);
					}
					maxdt = bt2.frac;
					lastfunc = c_func;
				}
#endif
				if ((c_flags & CALLOUT_RETURNUNLOCKED) == 0)
					mtx_unlock(c_mtx);
			skip:
				mtx_lock_spin(&callout_lock);
				curr_callout = NULL;
				if (callout_wait) {
					/*
					 * There is someone waiting
					 * for the callout to complete.
					 */
					callout_wait = 0;
					mtx_unlock_spin(&callout_lock);
					wakeup(&callout_wait);
					mtx_lock_spin(&callout_lock);
				}
				steps = 0;
				c = nextsoftcheck;
			}
		}
	}
	avg_depth += (depth * 1000 - avg_depth) >> 8;
	avg_mpcalls += (mpcalls * 1000 - avg_mpcalls) >> 8;
	avg_mtxcalls += (mtxcalls * 1000 - avg_mtxcalls) >> 8;
	avg_gcalls += (gcalls * 1000 - avg_gcalls) >> 8;
	nextsoftcheck = NULL;
	mtx_unlock_spin(&callout_lock);
}

/*
 * timeout --
 *	Execute a function after a specified length of time.
 *
 * untimeout --
 *	Cancel previous timeout function call.
 *
 * callout_handle_init --
 *	Initialize a handle so that using it with untimeout is benign.
 *
 *	See AT&T BCI Driver Reference Manual for specification.  This
 *	implementation differs from that one in that although an 
 *	identification value is returned from timeout, the original
 *	arguments to timeout as well as the identifier are used to
 *	identify entries for untimeout.
 */
struct callout_handle
timeout(ftn, arg, to_ticks)
	timeout_t *ftn;
	void *arg;
	int to_ticks;
{
	struct callout *new;
	struct callout_handle handle;

	mtx_lock_spin(&callout_lock);

	/* Fill in the next free callout structure. */
	new = SLIST_FIRST(&callfree);
	if (new == NULL)
		/* XXX Attempt to malloc first */
		panic("timeout table full");
	SLIST_REMOVE_HEAD(&callfree, c_links.sle);
	
	callout_reset(new, to_ticks, ftn, arg);

	handle.callout = new;
	mtx_unlock_spin(&callout_lock);
	return (handle);
}

void
untimeout(ftn, arg, handle)
	timeout_t *ftn;
	void *arg;
	struct callout_handle handle;
{

	/*
	 * Check for a handle that was initialized
	 * by callout_handle_init, but never used
	 * for a real timeout.
	 */
	if (handle.callout == NULL)
		return;

	mtx_lock_spin(&callout_lock);
	if (handle.callout->c_func == ftn && handle.callout->c_arg == arg)
		callout_stop(handle.callout);
	mtx_unlock_spin(&callout_lock);
}

void
callout_handle_init(struct callout_handle *handle)
{
	handle->callout = NULL;
}

/*
 * New interface; clients allocate their own callout structures.
 *
 * callout_reset() - establish or change a timeout
 * callout_stop() - disestablish a timeout
 * callout_init() - initialize a callout structure so that it can
 *	safely be passed to callout_reset() and callout_stop()
 *
 * <sys/callout.h> defines three convenience macros:
 *
 * callout_active() - returns truth if callout has not been stopped,
 *	drained, or deactivated since the last time the callout was
 *	reset.
 * callout_pending() - returns truth if callout is still waiting for timeout
 * callout_deactivate() - marks the callout as having been serviced
 */
int
callout_reset(c, to_ticks, ftn, arg)
	struct	callout *c;
	int	to_ticks;
	void	(*ftn)(void *);
	void	*arg;
{
	int cancelled = 0;

#ifdef notyet /* Some callers of timeout() do not hold Giant. */
	if (c->c_mtx != NULL)
		mtx_assert(c->c_mtx, MA_OWNED);
#endif

	mtx_lock_spin(&callout_lock);
	if (c == curr_callout) {
		/*
		 * We're being asked to reschedule a callout which is
		 * currently in progress.  If there is a mutex then we
		 * can cancel the callout if it has not really started.
		 */
		if (c->c_mtx != NULL && !curr_cancelled)
			cancelled = curr_cancelled = 1;
		if (callout_wait) {
			/*
			 * Someone has called callout_drain to kill this
			 * callout.  Don't reschedule.
			 */
			CTR4(KTR_CALLOUT, "%s %p func %p arg %p",
			    cancelled ? "cancelled" : "failed to cancel",
			    c, c->c_func, c->c_arg);
			mtx_unlock_spin(&callout_lock);
			return (cancelled);
		}
	}
	if (c->c_flags & CALLOUT_PENDING) {
		if (nextsoftcheck == c) {
			nextsoftcheck = TAILQ_NEXT(c, c_links.tqe);
		}
		TAILQ_REMOVE(&callwheel[c->c_time & callwheelmask], c,
		    c_links.tqe);

		cancelled = 1;

		/*
		 * Part of the normal "stop a pending callout" process
		 * is to clear the CALLOUT_ACTIVE and CALLOUT_PENDING
		 * flags.  We're not going to bother doing that here,
		 * because we're going to be setting those flags ten lines
		 * after this point, and we're holding callout_lock
		 * between now and then.
		 */
	}

	/*
	 * We could unlock callout_lock here and lock it again before the
	 * TAILQ_INSERT_TAIL, but there's no point since doing this setup
	 * doesn't take much time.
	 */
	if (to_ticks <= 0)
		to_ticks = 1;

	c->c_arg = arg;
	c->c_flags |= (CALLOUT_ACTIVE | CALLOUT_PENDING);
	c->c_func = ftn;
	c->c_time = ticks + to_ticks;
	TAILQ_INSERT_TAIL(&callwheel[c->c_time & callwheelmask], 
			  c, c_links.tqe);
	CTR5(KTR_CALLOUT, "%sscheduled %p func %p arg %p in %d",
	    cancelled ? "re" : "", c, c->c_func, c->c_arg, to_ticks);
	mtx_unlock_spin(&callout_lock);

	return (cancelled);
}

int
_callout_stop_safe(c, safe)
	struct	callout *c;
	int	safe;
{
	int use_mtx, sq_locked;

	if (!safe && c->c_mtx != NULL) {
#ifdef notyet /* Some callers do not hold Giant for Giant-locked callouts. */
		mtx_assert(c->c_mtx, MA_OWNED);
		use_mtx = 1;
#else
		use_mtx = mtx_owned(c->c_mtx);
#endif
	} else {
		use_mtx = 0;
	}

	sq_locked = 0;
again:
	mtx_lock_spin(&callout_lock);
	/*
	 * If the callout isn't pending, it's not on the queue, so
	 * don't attempt to remove it from the queue.  We can try to
	 * stop it by other means however.
	 */
	if (!(c->c_flags & CALLOUT_PENDING)) {
		c->c_flags &= ~CALLOUT_ACTIVE;

		/*
		 * If it wasn't on the queue and it isn't the current
		 * callout, then we can't stop it, so just bail.
		 */
		if (c != curr_callout) {
			CTR3(KTR_CALLOUT, "failed to stop %p func %p arg %p",
			    c, c->c_func, c->c_arg);
			mtx_unlock_spin(&callout_lock);
			if (sq_locked)
				sleepq_release(&callout_wait);
			return (0);
		}

		if (safe) {
			/*
			 * The current callout is running (or just
			 * about to run) and blocking is allowed, so
			 * just wait for the current invocation to
			 * finish.
			 */
			while (c == curr_callout) {

				/*
				 * Use direct calls to sleepqueue interface
				 * instead of cv/msleep in order to avoid
				 * a LOR between callout_lock and sleepqueue
				 * chain spinlocks.  This piece of code
				 * emulates a msleep_spin() call actually.
				 *
				 * If we already have the sleepqueue chain
				 * locked, then we can safely block.  If we
				 * don't already have it locked, however,
				 * we have to drop the callout_lock to lock
				 * it.  This opens several races, so we
				 * restart at the beginning once we have
				 * both locks.  If nothing has changed, then
				 * we will end up back here with sq_locked
				 * set.
				 */
				if (!sq_locked) {
					mtx_unlock_spin(&callout_lock);
					sleepq_lock(&callout_wait);
					sq_locked = 1;
					goto again;
				}

				callout_wait = 1;
				DROP_GIANT();
				mtx_unlock_spin(&callout_lock);
				sleepq_add(&callout_wait,
				    &callout_lock.lock_object, "codrain",
				    SLEEPQ_SLEEP, 0);
				sleepq_wait(&callout_wait);
				sq_locked = 0;

				/* Reacquire locks previously released. */
				PICKUP_GIANT();
				mtx_lock_spin(&callout_lock);
			}
		} else if (use_mtx && !curr_cancelled) {
			/*
			 * The current callout is waiting for it's
			 * mutex which we hold.  Cancel the callout
			 * and return.  After our caller drops the
			 * mutex, the callout will be skipped in
			 * softclock().
			 */
			curr_cancelled = 1;
			CTR3(KTR_CALLOUT, "cancelled %p func %p arg %p",
			    c, c->c_func, c->c_arg);
			mtx_unlock_spin(&callout_lock);
			KASSERT(!sq_locked, ("sleepqueue chain locked"));
			return (1);
		}
		CTR3(KTR_CALLOUT, "failed to stop %p func %p arg %p",
		    c, c->c_func, c->c_arg);
		mtx_unlock_spin(&callout_lock);
		KASSERT(!sq_locked, ("sleepqueue chain still locked"));
		return (0);
	}
	if (sq_locked)
		sleepq_release(&callout_wait);

	c->c_flags &= ~(CALLOUT_ACTIVE | CALLOUT_PENDING);

	if (nextsoftcheck == c) {
		nextsoftcheck = TAILQ_NEXT(c, c_links.tqe);
	}
	TAILQ_REMOVE(&callwheel[c->c_time & callwheelmask], c, c_links.tqe);

	CTR3(KTR_CALLOUT, "cancelled %p func %p arg %p",
	    c, c->c_func, c->c_arg);

	if (c->c_flags & CALLOUT_LOCAL_ALLOC) {
		c->c_func = NULL;
		SLIST_INSERT_HEAD(&callfree, c, c_links.sle);
	}
	mtx_unlock_spin(&callout_lock);
	return (1);
}

void
callout_init(c, mpsafe)
	struct	callout *c;
	int mpsafe;
{
	bzero(c, sizeof *c);
	if (mpsafe) {
		c->c_mtx = NULL;
		c->c_flags = CALLOUT_RETURNUNLOCKED;
	} else {
		c->c_mtx = &Giant;
		c->c_flags = 0;
	}
}

void
callout_init_mtx(c, mtx, flags)
	struct	callout *c;
	struct	mtx *mtx;
	int flags;
{
	bzero(c, sizeof *c);
	c->c_mtx = mtx;
	KASSERT((flags & ~(CALLOUT_RETURNUNLOCKED)) == 0,
	    ("callout_init_mtx: bad flags %d", flags));
	/* CALLOUT_RETURNUNLOCKED makes no sense without a mutex. */
	KASSERT(mtx != NULL || (flags & CALLOUT_RETURNUNLOCKED) == 0,
	    ("callout_init_mtx: CALLOUT_RETURNUNLOCKED with no mutex"));
	c->c_flags = flags & (CALLOUT_RETURNUNLOCKED);
}

#ifdef APM_FIXUP_CALLTODO
/* 
 * Adjust the kernel calltodo timeout list.  This routine is used after 
 * an APM resume to recalculate the calltodo timer list values with the 
 * number of hz's we have been sleeping.  The next hardclock() will detect 
 * that there are fired timers and run softclock() to execute them.
 *
 * Please note, I have not done an exhaustive analysis of what code this
 * might break.  I am motivated to have my select()'s and alarm()'s that
 * have expired during suspend firing upon resume so that the applications
 * which set the timer can do the maintanence the timer was for as close
 * as possible to the originally intended time.  Testing this code for a 
 * week showed that resuming from a suspend resulted in 22 to 25 timers 
 * firing, which seemed independant on whether the suspend was 2 hours or
 * 2 days.  Your milage may vary.   - Ken Key <key@cs.utk.edu>
 */
void
adjust_timeout_calltodo(time_change)
    struct timeval *time_change;
{
	register struct callout *p;
	unsigned long delta_ticks;

	/* 
	 * How many ticks were we asleep?
	 * (stolen from tvtohz()).
	 */

	/* Don't do anything */
	if (time_change->tv_sec < 0)
		return;
	else if (time_change->tv_sec <= LONG_MAX / 1000000)
		delta_ticks = (time_change->tv_sec * 1000000 +
			       time_change->tv_usec + (tick - 1)) / tick + 1;
	else if (time_change->tv_sec <= LONG_MAX / hz)
		delta_ticks = time_change->tv_sec * hz +
			      (time_change->tv_usec + (tick - 1)) / tick + 1;
	else
		delta_ticks = LONG_MAX;

	if (delta_ticks > INT_MAX)
		delta_ticks = INT_MAX;

	/* 
	 * Now rip through the timer calltodo list looking for timers
	 * to expire.
	 */

	/* don't collide with softclock() */
	mtx_lock_spin(&callout_lock);
	for (p = calltodo.c_next; p != NULL; p = p->c_next) {
		p->c_time -= delta_ticks;

		/* Break if the timer had more time on it than delta_ticks */
		if (p->c_time > 0)
			break;

		/* take back the ticks the timer didn't use (p->c_time <= 0) */
		delta_ticks = -p->c_time;
	}
	mtx_unlock_spin(&callout_lock);

	return;
}
#endif /* APM_FIXUP_CALLTODO */
