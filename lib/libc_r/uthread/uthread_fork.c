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
 * $FreeBSD: src/lib/libc_r/uthread/uthread_fork.c,v 1.29 2007/01/12 07:25:25 imp Exp $
 */
#include <sys/param.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include "pthread_private.h"

static void	free_thread_resources(struct pthread *thread);

__weak_reference(_fork, fork);

pid_t
_fork(void)
{
	struct pthread	*curthread = _get_curthread();
	struct pthread_atfork *af;
	int             i, flags, use_deadlist = 0;
	pid_t           ret;
	pthread_t	pthread;
	pthread_t	pthread_save;

	/*
	 * Defer signals to protect the scheduling queues from access
	 * by the signal handler:
	 */
	_thread_kern_sig_defer();

	_pthread_mutex_lock(&_atfork_mutex);

	/* Run down atfork prepare handlers. */
	TAILQ_FOREACH_REVERSE(af, &_atfork_list, atfork_head, qe) {
		if (af->prepare != NULL)
			af->prepare();
	}

	/* Fork a new process: */
	if ((ret = __sys_fork()) != 0) {
		/* Run down atfork parent handlers. */
		TAILQ_FOREACH(af, &_atfork_list, qe) {
			if (af->parent != NULL)
			af->parent();
		}
		_pthread_mutex_unlock(&_atfork_mutex);

	} else {
		/* Close the pthread kernel pipe: */
		__sys_close(_thread_kern_pipe[0]);
		__sys_close(_thread_kern_pipe[1]);

		/* Reset signals pending for the running thread: */
		sigemptyset(&curthread->sigpend);

		/*
		 * Create a pipe that is written to by the signal handler to
		 * prevent signals being missed in calls to
		 * __sys_select: 
		 */
		if (__sys_pipe(_thread_kern_pipe) != 0) {
			/* Cannot create pipe, so abort: */
			PANIC("Cannot create pthread kernel pipe for forked process");
		}
		/* Get the flags for the read pipe: */
		else if ((flags = __sys_fcntl(_thread_kern_pipe[0], F_GETFL, NULL)) == -1) {
			/* Abort this application: */
			abort();
		}
		/* Make the read pipe non-blocking: */
		else if (__sys_fcntl(_thread_kern_pipe[0], F_SETFL, flags | O_NONBLOCK) == -1) {
			/* Abort this application: */
			abort();
		}
		/* Get the flags for the write pipe: */
		else if ((flags = __sys_fcntl(_thread_kern_pipe[1], F_GETFL, NULL)) == -1) {
			/* Abort this application: */
			abort();
		}
		/* Make the write pipe non-blocking: */
		else if (__sys_fcntl(_thread_kern_pipe[1], F_SETFL, flags | O_NONBLOCK) == -1) {
			/* Abort this application: */
			abort();
		}
		/* Reinitialize the GC mutex: */
		else if (_mutex_reinit(&_gc_mutex) != 0) {
			/* Abort this application: */
			PANIC("Cannot initialize GC mutex for forked process");
		}
		/* Reinitialize the GC condition variable: */
		else if (_cond_reinit(&_gc_cond) != 0) {
			/* Abort this application: */
			PANIC("Cannot initialize GC condvar for forked process");
		}
		/* Initialize the ready queue: */
		else if (_pq_init(&_readyq) != 0) {
			/* Abort this application: */
			PANIC("Cannot initialize priority ready queue.");
		} else {
			/*
			 * Enter a loop to remove all threads other than
			 * the running thread from the thread list:
			 */
			if ((pthread = TAILQ_FIRST(&_thread_list)) == NULL) {
				pthread = TAILQ_FIRST(&_dead_list);
				use_deadlist = 1;
			}
			while (pthread != NULL) {
				/* Save the thread to be freed: */
				pthread_save = pthread;

				/*
				 * Advance to the next thread before
				 * destroying the current thread:
				 */
				if (use_deadlist != 0)
					pthread = TAILQ_NEXT(pthread, dle);
				else
					pthread = TAILQ_NEXT(pthread, tle);

				/* Make sure this isn't the running thread: */
				if (pthread_save != curthread) {
					/*
					 * Remove this thread from the
					 * appropriate list:
					 */
					if (use_deadlist != 0)
						TAILQ_REMOVE(&_thread_list,
						    pthread_save, dle);
					else
						TAILQ_REMOVE(&_thread_list,
						    pthread_save, tle);

					free_thread_resources(pthread_save);
				}

				/*
				 * Switch to the deadlist when the active
				 * thread list has been consumed.  This can't
				 * be at the top of the loop because it is
				 * used to determine to which list the thread
				 * belongs (when it is removed from the list).
				 */
				if (pthread == NULL) {
					pthread = TAILQ_FIRST(&_dead_list);
					use_deadlist = 1;
				}
			}

			/* Treat the current thread as the initial thread: */
			_thread_initial = curthread;

			/* Re-init the dead thread list: */
			TAILQ_INIT(&_dead_list);

			/* Re-init the waiting and work queues. */
			TAILQ_INIT(&_waitingq);
			TAILQ_INIT(&_workq);

			/* Re-init the threads mutex queue: */
			TAILQ_INIT(&curthread->mutexq);

			/* No spinlocks yet: */
			_spinblock_count = 0;

			/* Don't queue signals yet: */
			_queue_signals = 0;

			/* Initialize the scheduling switch hook routine: */
			_sched_switch_hook = NULL;

			/* Clear out any locks in the file descriptor table: */
			for (i = 0; i < _thread_dtablesize; i++) {
				if (_thread_fd_table[i] != NULL) {
					/* Initialise the file locks: */
					memset(&_thread_fd_table[i]->lock, 0,
					    sizeof(_thread_fd_table[i]->lock));
					_thread_fd_table[i]->r_owner = NULL;
					_thread_fd_table[i]->w_owner = NULL;
					_thread_fd_table[i]->r_fname = NULL;
					_thread_fd_table[i]->w_fname = NULL;
					_thread_fd_table[i]->r_lineno = 0;;
					_thread_fd_table[i]->w_lineno = 0;;
					_thread_fd_table[i]->r_lockcount = 0;;
					_thread_fd_table[i]->w_lockcount = 0;;

					/* Initialise the read/write queues: */
					TAILQ_INIT(&_thread_fd_table[i]->r_queue);
					TAILQ_INIT(&_thread_fd_table[i]->w_queue);
				}
			}
		}
		/* Run down atfork child handlers. */
		TAILQ_FOREACH(af, &_atfork_list, qe) {
			if (af->child != NULL)
				af->child();
		}
		_mutex_reinit(&_atfork_mutex);
	}


	/*
	 * Undefer and handle pending signals, yielding if necessary:
	 */
	_thread_kern_sig_undefer();

	/* Return the process ID: */
	return (ret);
}

static void
free_thread_resources(struct pthread *thread)
{

	/* Check to see if the threads library allocated the stack. */
	if ((thread->attr.stackaddr_attr == NULL) && (thread->stack != NULL)) {
		/*
		 * Since this is being called from fork, we are currently single
		 * threaded so there is no need to protect the call to
		 * _thread_stack_free() with _gc_mutex.
		 */
		_thread_stack_free(thread->stack, thread->attr.stacksize_attr,
		    thread->attr.guardsize_attr);
	}

	if (thread->specific != NULL)
		free(thread->specific);

	if (thread->poll_data.fds != NULL)
		free(thread->poll_data.fds);

	free(thread);
}
