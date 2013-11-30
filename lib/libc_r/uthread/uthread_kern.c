/*
 * Copyright (c) 1995-1998 John Birrell <jb@cimlogic.com.au>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JOHN BIRRELL AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/lib/libc_r/uthread/uthread_kern.c,v 1.46 2007/01/12 07:25:26 imp Exp $
 *
 */
#include <errno.h>
#include <poll.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <setjmp.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/signalvar.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <pthread.h>
#include "pthread_private.h"

/* #define DEBUG_THREAD_KERN */
#ifdef DEBUG_THREAD_KERN
#define DBG_MSG		stdout_debug
#else
#define DBG_MSG(x...)
#endif

/* Static function prototype definitions: */
static void
thread_kern_poll(int wait_reqd);

static void
dequeue_signals(void);

static inline void
thread_run_switch_hook(pthread_t thread_out, pthread_t thread_in);

/* Static variables: */
static int	last_tick = 0;
static int	called_from_handler = 0;

/*
 * This is called when a signal handler finishes and wants to
 * return to a previous frame.
 */
void
_thread_kern_sched_frame(struct pthread_signal_frame *psf)
{
	struct pthread	*curthread = _get_curthread();

	/*
	 * Flag the pthread kernel as executing scheduler code
	 * to avoid a signal from interrupting this execution and
	 * corrupting the (soon-to-be) current frame.
	 */
	_thread_kern_in_sched = 1;

	/* Restore the signal frame: */
	_thread_sigframe_restore(curthread, psf);

	/* The signal mask was restored; check for any pending signals: */
	curthread->check_pending = 1;

	/* Switch to the thread scheduler: */
	___longjmp(_thread_kern_sched_jb, 1);
}


void
_thread_kern_sched(ucontext_t *ucp)
{
	struct pthread	*curthread = _get_curthread();

	/*
	 * Flag the pthread kernel as executing scheduler code
	 * to avoid a scheduler signal from interrupting this
	 * execution and calling the scheduler again.
	 */
	_thread_kern_in_sched = 1;

	/* Check if this function was called from the signal handler: */
	if (ucp != NULL) {
		called_from_handler = 1;
		DBG_MSG("Entering scheduler due to signal\n");
	}

	/* Save the state of the current thread: */
	if (_setjmp(curthread->ctx.jb) != 0) {
		DBG_MSG("Returned from ___longjmp, thread %p\n",
		    curthread);
		/*
		 * This point is reached when a longjmp() is called
		 * to restore the state of a thread.
		 *
		 * This is the normal way out of the scheduler.
		 */
		_thread_kern_in_sched = 0;

		if (curthread->sig_defer_count == 0) {
			if (((curthread->cancelflags &
			    PTHREAD_AT_CANCEL_POINT) == 0) &&
			    ((curthread->cancelflags &
			    PTHREAD_CANCEL_ASYNCHRONOUS) != 0))
				/*
				 * Cancellations override signals.
				 *
				 * Stick a cancellation point at the
				 * start of each async-cancellable
				 * thread's resumption.
				 *
				 * We allow threads woken at cancel
				 * points to do their own checks.
				 */
				pthread_testcancel();
		}

		if (_sched_switch_hook != NULL) {
			/* Run the installed switch hook: */
			thread_run_switch_hook(_last_user_thread, curthread);
		}
		if (ucp == NULL)
			return;
		else {
			/*
			 * Set the process signal mask in the context; it
			 * could have changed by the handler.
			 */
			ucp->uc_sigmask = _process_sigmask;

			/* Resume the interrupted thread: */
			__sys_sigreturn(ucp);
		}
	}
	/* Switch to the thread scheduler: */
	___longjmp(_thread_kern_sched_jb, 1);
}

void
_thread_kern_sched_sig(void)
{
	struct pthread	*curthread = _get_curthread();

	curthread->check_pending = 1;
	_thread_kern_sched(NULL);
}


void
_thread_kern_scheduler(void)
{
	struct timespec	ts;
	struct timeval	tv;
	struct pthread	*curthread = _get_curthread();
	pthread_t	pthread, pthread_h;
	unsigned int	current_tick;
	int		add_to_prioq;

	/* If the currently running thread is a user thread, save it: */
	if ((curthread->flags & PTHREAD_FLAGS_PRIVATE) == 0)
		_last_user_thread = curthread;

	if (called_from_handler != 0) {
		called_from_handler = 0;

		/*
		 * We were called from a signal handler; restore the process
		 * signal mask.
		 */
		if (__sys_sigprocmask(SIG_SETMASK,
		    &_process_sigmask, NULL) != 0)
			PANIC("Unable to restore process mask after signal");
	}

	/*
	 * Enter a scheduling loop that finds the next thread that is
	 * ready to run. This loop completes when there are no more threads
	 * in the global list or when a thread has its state restored by
	 * either a sigreturn (if the state was saved as a sigcontext) or a
	 * longjmp (if the state was saved by a setjmp).
	 */
	while (!(TAILQ_EMPTY(&_thread_list))) {
		/* Get the current time of day: */
		GET_CURRENT_TOD(tv);
		TIMEVAL_TO_TIMESPEC(&tv, &ts);
		current_tick = _sched_ticks;

		/*
		 * Protect the scheduling queues from access by the signal
		 * handler.
		 */
		_queue_signals = 1;
		add_to_prioq = 0;

		if (curthread != &_thread_kern_thread) {
			/*
			 * This thread no longer needs to yield the CPU.
			 */
			curthread->yield_on_sig_undefer = 0;
	
			if (curthread->state != PS_RUNNING) {
				/*
				 * Save the current time as the time that the
				 * thread became inactive:
				 */
				curthread->last_inactive = (long)current_tick;
				if (curthread->last_inactive <
				    curthread->last_active) {
					/* Account for a rollover: */
					curthread->last_inactive =+
					    UINT_MAX + 1;
				}
			}

			/*
			 * Place the currently running thread into the
			 * appropriate queue(s).
			 */
			switch (curthread->state) {
			case PS_DEAD:
			case PS_STATE_MAX: /* to silence -Wall */
			case PS_SUSPENDED:
				/*
				 * Dead and suspended threads are not placed
				 * in any queue:
				 */
				break;

			case PS_RUNNING:
				/*
				 * Runnable threads can't be placed in the
				 * priority queue until after waiting threads
				 * are polled (to preserve round-robin
				 * scheduling).
				 */
				add_to_prioq = 1;
				break;

			/*
			 * States which do not depend on file descriptor I/O
			 * operations or timeouts:
			 */
			case PS_DEADLOCK:
			case PS_FDLR_WAIT:
			case PS_FDLW_WAIT:
			case PS_FILE_WAIT:
			case PS_JOIN:
			case PS_MUTEX_WAIT:
			case PS_SIGSUSPEND:
			case PS_SIGTHREAD:
			case PS_SIGWAIT:
			case PS_WAIT_WAIT:
				/* No timeouts for these states: */
				curthread->wakeup_time.tv_sec = -1;
				curthread->wakeup_time.tv_nsec = -1;

				/* Restart the time slice: */
				curthread->slice_usec = -1;

				/* Insert into the waiting queue: */
				PTHREAD_WAITQ_INSERT(curthread);
				break;

			/* States which can timeout: */
			case PS_COND_WAIT:
			case PS_SLEEP_WAIT:
				/* Restart the time slice: */
				curthread->slice_usec = -1;

				/* Insert into the waiting queue: */
				PTHREAD_WAITQ_INSERT(curthread);
				break;
	
			/* States that require periodic work: */
			case PS_SPINBLOCK:
				/* No timeouts for this state: */
				curthread->wakeup_time.tv_sec = -1;
				curthread->wakeup_time.tv_nsec = -1;

				/* Increment spinblock count: */
				_spinblock_count++;

				/* FALLTHROUGH */
			case PS_FDR_WAIT:
			case PS_FDW_WAIT:
			case PS_POLL_WAIT:
			case PS_SELECT_WAIT:
				/* Restart the time slice: */
				curthread->slice_usec = -1;
	
				/* Insert into the waiting queue: */
				PTHREAD_WAITQ_INSERT(curthread);
	
				/* Insert into the work queue: */
				PTHREAD_WORKQ_INSERT(curthread);
				break;
			}

			/*
			 * Are there pending signals for this thread?
			 *
			 * This check has to be performed after the thread
			 * has been placed in the queue(s) appropriate for
			 * its state.  The process of adding pending signals
			 * can change a threads state, which in turn will
			 * attempt to add or remove the thread from any
			 * scheduling queue to which it belongs.
			 */
			if (curthread->check_pending != 0) {
				curthread->check_pending = 0;
				_thread_sig_check_pending(curthread);
			}
		}

		/*
		 * Avoid polling file descriptors if there are none
		 * waiting:
		 */
		if (TAILQ_EMPTY(&_workq) != 0) {
		}
		/*
		 * Poll file descriptors only if a new scheduling signal
		 * has occurred or if we have no more runnable threads.
		 */
		else if (((current_tick = _sched_ticks) != last_tick) ||
		    ((curthread->state != PS_RUNNING) &&
		    (PTHREAD_PRIOQ_FIRST() == NULL))) {
			/* Unprotect the scheduling queues: */
			_queue_signals = 0;

			/*
			 * Poll file descriptors to update the state of threads
			 * waiting on file I/O where data may be available:
			 */
			thread_kern_poll(0);

			/* Protect the scheduling queues: */
			_queue_signals = 1;
		}
		last_tick = current_tick;

		/*
		 * Wake up threads that have timedout.  This has to be
		 * done after polling in case a thread does a poll or
		 * select with zero time.
		 */
		PTHREAD_WAITQ_SETACTIVE();
		while (((pthread = TAILQ_FIRST(&_waitingq)) != NULL) &&
		    (pthread->wakeup_time.tv_sec != -1) &&
		    (((pthread->wakeup_time.tv_sec == 0) &&
		    (pthread->wakeup_time.tv_nsec == 0)) ||
		    (pthread->wakeup_time.tv_sec < ts.tv_sec) ||
		    ((pthread->wakeup_time.tv_sec == ts.tv_sec) &&
		    (pthread->wakeup_time.tv_nsec <= ts.tv_nsec)))) {
			switch (pthread->state) {
			case PS_POLL_WAIT:
			case PS_SELECT_WAIT:
				/* Return zero file descriptors ready: */
				pthread->data.poll_data->nfds = 0;
				/* FALLTHROUGH */
			default:
				/*
				 * Remove this thread from the waiting queue
				 * (and work queue if necessary) and place it
				 * in the ready queue.
				 */
				PTHREAD_WAITQ_CLEARACTIVE();
				if (pthread->flags & PTHREAD_FLAGS_IN_WORKQ)
					PTHREAD_WORKQ_REMOVE(pthread);
				PTHREAD_NEW_STATE(pthread, PS_RUNNING);
				PTHREAD_WAITQ_SETACTIVE();
				break;
			}
			/*
			 * Flag the timeout in the thread structure:
			 */
			pthread->timeout = 1;
		}
		PTHREAD_WAITQ_CLEARACTIVE();

		/*
		 * Check to see if the current thread needs to be added
		 * to the priority queue:
		 */
		if (add_to_prioq != 0) {
			/*
			 * Save the current time as the time that the
			 * thread became inactive:
			 */
			current_tick = _sched_ticks;
			curthread->last_inactive = (long)current_tick;
			if (curthread->last_inactive <
			    curthread->last_active) {
				/* Account for a rollover: */
				curthread->last_inactive =+ UINT_MAX + 1;
			}

			if ((curthread->slice_usec != -1) &&
		 	   (curthread->attr.sched_policy != SCHED_FIFO)) {
				/*
				 * Accumulate the number of microseconds for
				 * which the current thread has run:
				 */
				curthread->slice_usec +=
				    (curthread->last_inactive -
				    curthread->last_active) *
				    (long)_clock_res_usec;
				/* Check for time quantum exceeded: */
				if (curthread->slice_usec > TIMESLICE_USEC)
					curthread->slice_usec = -1;
			}

			if (curthread->slice_usec == -1) {
				/*
				 * The thread exceeded its time
				 * quantum or it yielded the CPU;
				 * place it at the tail of the
				 * queue for its priority.
				 */
				PTHREAD_PRIOQ_INSERT_TAIL(curthread);
			} else {
				/*
				 * The thread hasn't exceeded its
				 * interval.  Place it at the head
				 * of the queue for its priority.
				 */
				PTHREAD_PRIOQ_INSERT_HEAD(curthread);
			}
		}

		/*
		 * Get the highest priority thread in the ready queue.
		 */
		pthread_h = PTHREAD_PRIOQ_FIRST();

		/* Check if there are no threads ready to run: */
		if (pthread_h == NULL) {
			/*
			 * Lock the pthread kernel by changing the pointer to
			 * the running thread to point to the global kernel
			 * thread structure:
			 */
			_set_curthread(&_thread_kern_thread);
			curthread = &_thread_kern_thread;

			DBG_MSG("No runnable threads, using kernel thread %p\n",
			    curthread);

			/* Unprotect the scheduling queues: */
			_queue_signals = 0;

			/*
			 * There are no threads ready to run, so wait until
			 * something happens that changes this condition:
			 */
			thread_kern_poll(1);

			/*
			 * This process' usage will likely be very small
			 * while waiting in a poll.  Since the scheduling
			 * clock is based on the profiling timer, it is
			 * unlikely that the profiling timer will fire
			 * and update the time of day.  To account for this,
			 * get the time of day after polling with a timeout.
			 */
			gettimeofday((struct timeval *) &_sched_tod, NULL);
			
			/* Check once more for a runnable thread: */
			_queue_signals = 1;
			pthread_h = PTHREAD_PRIOQ_FIRST();
			_queue_signals = 0;
		}

		if (pthread_h != NULL) {
			/* Remove the thread from the ready queue: */
			PTHREAD_PRIOQ_REMOVE(pthread_h);

			/* Unprotect the scheduling queues: */
			_queue_signals = 0;

			/*
			 * Check for signals queued while the scheduling
			 * queues were protected:
			 */
			while (_sigq_check_reqd != 0) {
				/* Clear before handling queued signals: */
				_sigq_check_reqd = 0;

				/* Protect the scheduling queues again: */
				_queue_signals = 1;

				dequeue_signals();

				/*
				 * Check for a higher priority thread that
				 * became runnable due to signal handling.
				 */
				if (((pthread = PTHREAD_PRIOQ_FIRST()) != NULL) &&
				    (pthread->active_priority > pthread_h->active_priority)) {
					/* Remove the thread from the ready queue: */
					PTHREAD_PRIOQ_REMOVE(pthread);

					/*
					 * Insert the lower priority thread
					 * at the head of its priority list:
					 */
					PTHREAD_PRIOQ_INSERT_HEAD(pthread_h);

					/* There's a new thread in town: */
					pthread_h = pthread;
				}

				/* Unprotect the scheduling queues: */
				_queue_signals = 0;
			}

			/* Make the selected thread the current thread: */
			_set_curthread(pthread_h);
			curthread = pthread_h;

			/*
			 * Save the current time as the time that the thread
			 * became active:
			 */
			current_tick = _sched_ticks;
			curthread->last_active = (long) current_tick;

			/*
			 * Check if this thread is running for the first time
			 * or running again after using its full time slice
			 * allocation:
			 */
			if (curthread->slice_usec == -1) {
				/* Reset the accumulated time slice period: */
				curthread->slice_usec = 0;
			}

			/*
			 * If we had a context switch, run any
			 * installed switch hooks.
			 */
			if ((_sched_switch_hook != NULL) &&
			    (_last_user_thread != curthread)) {
				thread_run_switch_hook(_last_user_thread,
				    curthread);
			}
			/*
			 * Continue the thread at its current frame:
			 */
#if NOT_YET
			_setcontext(&curthread->ctx.uc);
#else
			___longjmp(curthread->ctx.jb, 1);
#endif
			/* This point should not be reached. */
			PANIC("Thread has returned from sigreturn or longjmp");
		}
	}

	/* There are no more threads, so exit this process: */
	exit(0);
}

void
_thread_kern_sched_state(enum pthread_state state, char *fname, int lineno)
{
	struct pthread	*curthread = _get_curthread();

	/*
	 * Flag the pthread kernel as executing scheduler code
	 * to avoid a scheduler signal from interrupting this
	 * execution and calling the scheduler again.
	 */
	_thread_kern_in_sched = 1;

	/*
	 * Prevent the signal handler from fiddling with this thread
	 * before its state is set and is placed into the proper queue.
	 */
	_queue_signals = 1;

	/* Change the state of the current thread: */
	curthread->state = state;
	curthread->fname = fname;
	curthread->lineno = lineno;

	/* Schedule the next thread that is ready: */
	_thread_kern_sched(NULL);
}

void
_thread_kern_sched_state_unlock(enum pthread_state state,
    spinlock_t *lock, char *fname, int lineno)
{
	struct pthread	*curthread = _get_curthread();

	/*
	 * Flag the pthread kernel as executing scheduler code
	 * to avoid a scheduler signal from interrupting this
	 * execution and calling the scheduler again.
	 */
	_thread_kern_in_sched = 1;

	/*
	 * Prevent the signal handler from fiddling with this thread
	 * before its state is set and it is placed into the proper
	 * queue(s).
	 */
	_queue_signals = 1;

	/* Change the state of the current thread: */
	curthread->state = state;
	curthread->fname = fname;
	curthread->lineno = lineno;

	_SPINUNLOCK(lock);

	/* Schedule the next thread that is ready: */
	_thread_kern_sched(NULL);
}

static void
thread_kern_poll(int wait_reqd)
{
	int             count = 0;
	int             i, found;
	int		kern_pipe_added = 0;
	int             nfds = 0;
	int		timeout_ms = 0;
	struct pthread	*pthread;
	struct timespec ts;
	struct timeval  tv;

	/* Check if the caller wants to wait: */
	if (wait_reqd == 0) {
		timeout_ms = 0;
	}
	else {
		/* Get the current time of day: */
		GET_CURRENT_TOD(tv);
		TIMEVAL_TO_TIMESPEC(&tv, &ts);

		_queue_signals = 1;
		pthread = TAILQ_FIRST(&_waitingq);
		_queue_signals = 0;

		if ((pthread == NULL) || (pthread->wakeup_time.tv_sec == -1)) {
			/*
			 * Either there are no threads in the waiting queue,
			 * or there are no threads that can timeout.
			 */
			timeout_ms = INFTIM;
		}
		else if (pthread->wakeup_time.tv_sec - ts.tv_sec > 60000)
			/* Limit maximum timeout to prevent rollover. */
			timeout_ms = 60000;
		else {
			/*
			 * Calculate the time left for the next thread to
			 * timeout:
			 */
			timeout_ms = ((pthread->wakeup_time.tv_sec - ts.tv_sec) *
			    1000) + ((pthread->wakeup_time.tv_nsec - ts.tv_nsec) /
			    1000000);
			/*
			 * Don't allow negative timeouts:
			 */
			if (timeout_ms < 0)
				timeout_ms = 0;
		}
	}
			
	/* Protect the scheduling queues: */
	_queue_signals = 1;

	/*
	 * Check to see if the signal queue needs to be walked to look
	 * for threads awoken by a signal while in the scheduler.
	 */
	if (_sigq_check_reqd != 0) {
		/* Reset flag before handling queued signals: */
		_sigq_check_reqd = 0;

		dequeue_signals();
	}

	/*
	 * Check for a thread that became runnable due to a signal:
	 */
	if (PTHREAD_PRIOQ_FIRST() != NULL) {
		/*
		 * Since there is at least one runnable thread,
		 * disable the wait.
		 */
		timeout_ms = 0;
	}

	/*
	 * Form the poll table:
	 */
	nfds = 0;
	if (timeout_ms != 0) {
		/* Add the kernel pipe to the poll table: */
		_thread_pfd_table[nfds].fd = _thread_kern_pipe[0];
		_thread_pfd_table[nfds].events = POLLRDNORM;
		_thread_pfd_table[nfds].revents = 0;
		nfds++;
		kern_pipe_added = 1;
	}

	PTHREAD_WAITQ_SETACTIVE();
	TAILQ_FOREACH(pthread, &_workq, qe) {
		switch (pthread->state) {
		case PS_SPINBLOCK:
			/*
			 * If the lock is available, let the thread run.
			 */
			if (pthread->data.spinlock->access_lock == 0) {
				PTHREAD_WAITQ_CLEARACTIVE();
				PTHREAD_WORKQ_REMOVE(pthread);
				PTHREAD_NEW_STATE(pthread,PS_RUNNING);
				PTHREAD_WAITQ_SETACTIVE();
				/* One less thread in a spinblock state: */
				_spinblock_count--;
				/*
				 * Since there is at least one runnable
				 * thread, disable the wait.
				 */
				timeout_ms = 0;
			}
			break;

		/* File descriptor read wait: */
		case PS_FDR_WAIT:
			/* Limit number of polled files to table size: */
			if (nfds < _thread_dtablesize) {
				_thread_pfd_table[nfds].events = POLLRDNORM;
				_thread_pfd_table[nfds].fd = pthread->data.fd.fd;
				nfds++;
			}
			break;

		/* File descriptor write wait: */
		case PS_FDW_WAIT:
			/* Limit number of polled files to table size: */
			if (nfds < _thread_dtablesize) {
				_thread_pfd_table[nfds].events = POLLWRNORM;
				_thread_pfd_table[nfds].fd = pthread->data.fd.fd;
				nfds++;
			}
			break;

		/* File descriptor poll or select wait: */
		case PS_POLL_WAIT:
		case PS_SELECT_WAIT:
			/* Limit number of polled files to table size: */
			if (pthread->data.poll_data->nfds + nfds <
			    _thread_dtablesize) {
				for (i = 0; i < pthread->data.poll_data->nfds; i++) {
					_thread_pfd_table[nfds + i].fd =
					    pthread->data.poll_data->fds[i].fd;
					_thread_pfd_table[nfds + i].events =
					    pthread->data.poll_data->fds[i].events;
				}
				nfds += pthread->data.poll_data->nfds;
			}
			break;

		/* Other states do not depend on file I/O. */
		default:
			break;
		}
	}
	PTHREAD_WAITQ_CLEARACTIVE();

	/*
	 * Wait for a file descriptor to be ready for read, write, or
	 * an exception, or a timeout to occur:
	 */
	count = __sys_poll(_thread_pfd_table, nfds, timeout_ms);

	if (kern_pipe_added != 0)
		/*
		 * Remove the pthread kernel pipe file descriptor
		 * from the pollfd table:
		 */
		nfds = 1;
	else
		nfds = 0;

	/*
	 * Check if it is possible that there are bytes in the kernel
	 * read pipe waiting to be read:
	 */
	if (count < 0 || ((kern_pipe_added != 0) &&
	    (_thread_pfd_table[0].revents & POLLRDNORM))) {
		/*
		 * If the kernel read pipe was included in the
		 * count:
		 */
		if (count > 0) {
			/* Decrement the count of file descriptors: */
			count--;
		}

		if (_sigq_check_reqd != 0) {
			/* Reset flag before handling signals: */
			_sigq_check_reqd = 0;

			dequeue_signals();
		}
	}

	/*
	 * Check if any file descriptors are ready:
	 */
	if (count > 0) {
		/*
		 * Enter a loop to look for threads waiting on file
		 * descriptors that are flagged as available by the
		 * _poll syscall:
		 */
		PTHREAD_WAITQ_SETACTIVE();
		TAILQ_FOREACH(pthread, &_workq, qe) {
			switch (pthread->state) {
			case PS_SPINBLOCK:
				/*
				 * If the lock is available, let the thread run.
				 */
				if (pthread->data.spinlock->access_lock == 0) {
					PTHREAD_WAITQ_CLEARACTIVE();
					PTHREAD_WORKQ_REMOVE(pthread);
					PTHREAD_NEW_STATE(pthread,PS_RUNNING);
					PTHREAD_WAITQ_SETACTIVE();

					/*
					 * One less thread in a spinblock state:
					 */
					_spinblock_count--;
				}
				break;

			/* File descriptor read wait: */
			case PS_FDR_WAIT:
				if ((nfds < _thread_dtablesize) &&
				    (_thread_pfd_table[nfds].revents
				       & (POLLRDNORM|POLLERR|POLLHUP|POLLNVAL))
				      != 0) {
					PTHREAD_WAITQ_CLEARACTIVE();
					PTHREAD_WORKQ_REMOVE(pthread);
					PTHREAD_NEW_STATE(pthread,PS_RUNNING);
					PTHREAD_WAITQ_SETACTIVE();
				}
				nfds++;
				break;

			/* File descriptor write wait: */
			case PS_FDW_WAIT:
				if ((nfds < _thread_dtablesize) &&
				    (_thread_pfd_table[nfds].revents
				       & (POLLWRNORM|POLLERR|POLLHUP|POLLNVAL))
				      != 0) {
					PTHREAD_WAITQ_CLEARACTIVE();
					PTHREAD_WORKQ_REMOVE(pthread);
					PTHREAD_NEW_STATE(pthread,PS_RUNNING);
					PTHREAD_WAITQ_SETACTIVE();
				}
				nfds++;
				break;

			/* File descriptor poll or select wait: */
			case PS_POLL_WAIT:
			case PS_SELECT_WAIT:
				if (pthread->data.poll_data->nfds + nfds <
				    _thread_dtablesize) {
					/*
					 * Enter a loop looking for I/O
					 * readiness:
					 */
					found = 0;
					for (i = 0; i < pthread->data.poll_data->nfds; i++) {
						if (_thread_pfd_table[nfds + i].revents != 0) {
							pthread->data.poll_data->fds[i].revents =
							    _thread_pfd_table[nfds + i].revents;
							found++;
						}
					}

					/* Increment before destroying: */
					nfds += pthread->data.poll_data->nfds;

					if (found != 0) {
						pthread->data.poll_data->nfds = found;
						PTHREAD_WAITQ_CLEARACTIVE();
						PTHREAD_WORKQ_REMOVE(pthread);
						PTHREAD_NEW_STATE(pthread,PS_RUNNING);
						PTHREAD_WAITQ_SETACTIVE();
					}
				}
				else
					nfds += pthread->data.poll_data->nfds;
				break;

			/* Other states do not depend on file I/O. */
			default:
				break;
			}
		}
		PTHREAD_WAITQ_CLEARACTIVE();
	}
	else if (_spinblock_count != 0) {
		/*
		 * Enter a loop to look for threads waiting on a spinlock
		 * that is now available.
		 */
		PTHREAD_WAITQ_SETACTIVE();
		TAILQ_FOREACH(pthread, &_workq, qe) {
			if (pthread->state == PS_SPINBLOCK) {
				/*
				 * If the lock is available, let the thread run.
				 */
				if (pthread->data.spinlock->access_lock == 0) {
					PTHREAD_WAITQ_CLEARACTIVE();
					PTHREAD_WORKQ_REMOVE(pthread);
					PTHREAD_NEW_STATE(pthread,PS_RUNNING);
					PTHREAD_WAITQ_SETACTIVE();

					/*
					 * One less thread in a spinblock state:
					 */
					_spinblock_count--;
				}
			}
		}
		PTHREAD_WAITQ_CLEARACTIVE();
	}

	/* Unprotect the scheduling queues: */
	_queue_signals = 0;

	while (_sigq_check_reqd != 0) {
		/* Handle queued signals: */
		_sigq_check_reqd = 0;

		/* Protect the scheduling queues: */
		_queue_signals = 1;

		dequeue_signals();

		/* Unprotect the scheduling queues: */
		_queue_signals = 0;
	}
}

void
_thread_kern_set_timeout(const struct timespec * timeout)
{
	struct pthread	*curthread = _get_curthread();
	struct timespec current_time;
	struct timeval  tv;

	/* Reset the timeout flag for the running thread: */
	curthread->timeout = 0;

	/* Check if the thread is to wait forever: */
	if (timeout == NULL) {
		/*
		 * Set the wakeup time to something that can be recognised as
		 * different to an actual time of day:
		 */
		curthread->wakeup_time.tv_sec = -1;
		curthread->wakeup_time.tv_nsec = -1;
	}
	/* Check if no waiting is required: */
	else if (timeout->tv_sec == 0 && timeout->tv_nsec == 0) {
		/* Set the wake up time to 'immediately': */
		curthread->wakeup_time.tv_sec = 0;
		curthread->wakeup_time.tv_nsec = 0;
	} else {
		/* Get the current time: */
		GET_CURRENT_TOD(tv);
		TIMEVAL_TO_TIMESPEC(&tv, &current_time);

		/* Calculate the time for the current thread to wake up: */
		curthread->wakeup_time.tv_sec = current_time.tv_sec + timeout->tv_sec;
		curthread->wakeup_time.tv_nsec = current_time.tv_nsec + timeout->tv_nsec;

		/* Check if the nanosecond field needs to wrap: */
		if (curthread->wakeup_time.tv_nsec >= 1000000000) {
			/* Wrap the nanosecond field: */
			curthread->wakeup_time.tv_sec += 1;
			curthread->wakeup_time.tv_nsec -= 1000000000;
		}
	}
}

void
_thread_kern_sig_defer(void)
{
	struct pthread	*curthread = _get_curthread();

	/* Allow signal deferral to be recursive. */
	curthread->sig_defer_count++;
}

void
_thread_kern_sig_undefer(void)
{
	struct pthread	*curthread = _get_curthread();

	/*
	 * Perform checks to yield only if we are about to undefer
	 * signals.
	 */
	if (curthread->sig_defer_count > 1) {
		/* Decrement the signal deferral count. */
		curthread->sig_defer_count--;
	}
	else if (curthread->sig_defer_count == 1) {
		/* Reenable signals: */
		curthread->sig_defer_count = 0;

		/*
		 * Check if there are queued signals:
		 */
		if (_sigq_check_reqd != 0)
			_thread_kern_sched(NULL);

		/*
		 * Check for asynchronous cancellation before delivering any
		 * pending signals:
		 */
		if (((curthread->cancelflags & PTHREAD_AT_CANCEL_POINT) == 0) &&
		    ((curthread->cancelflags & PTHREAD_CANCEL_ASYNCHRONOUS) != 0))
			pthread_testcancel();

		/*
		 * If there are pending signals or this thread has
		 * to yield the CPU, call the kernel scheduler:
		 *
		 * XXX - Come back and revisit the pending signal problem
		 */
		if ((curthread->yield_on_sig_undefer != 0) ||
		    SIGNOTEMPTY(curthread->sigpend)) {
			curthread->yield_on_sig_undefer = 0;
			_thread_kern_sched(NULL);
		}
	}
}

static void
dequeue_signals(void)
{
	char	bufr[128];
	int	num;

	/*
	 * Enter a loop to clear the pthread kernel pipe:
	 */
	while (((num = __sys_read(_thread_kern_pipe[0], bufr,
	    sizeof(bufr))) > 0) || (num == -1 && errno == EINTR)) {
	}
	if ((num < 0) && (errno != EAGAIN)) {
		/*
		 * The only error we should expect is if there is
		 * no data to read.
		 */
		PANIC("Unable to read from thread kernel pipe");
	}
	/* Handle any pending signals: */
	_thread_sig_handle_pending();
}

static inline void
thread_run_switch_hook(pthread_t thread_out, pthread_t thread_in)
{
	pthread_t tid_out = thread_out;
	pthread_t tid_in = thread_in;

	if ((tid_out != NULL) &&
	    (tid_out->flags & PTHREAD_FLAGS_PRIVATE) != 0)
		tid_out = NULL;
	if ((tid_in != NULL) &&
	    (tid_in->flags & PTHREAD_FLAGS_PRIVATE) != 0)
		tid_in = NULL;

	if ((_sched_switch_hook != NULL) && (tid_out != tid_in)) {
		/* Run the scheduler switch hook: */
		_sched_switch_hook(tid_out, tid_in);
	}
}

struct pthread *
_get_curthread(void)
{
	if (_thread_initial == NULL)
		_thread_init();

	return (_thread_run);
}

void
_set_curthread(struct pthread *newthread)
{
	_thread_run = newthread;
}
