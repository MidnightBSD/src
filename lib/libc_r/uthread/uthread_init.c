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
 * $FreeBSD: release/7.0.0/lib/libc_r/uthread/uthread_init.c 165967 2007-01-12 07:26:21Z imp $
 */

/* Allocate space for global thread variables here: */
#define GLOBAL_PTHREAD_PRIVATE

#include "namespace.h"
#include <sys/param.h>
#include <sys/types.h>
#include <machine/reg.h>

#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/event.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/ttycom.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <poll.h>
#include <pthread.h>
#include <pthread_np.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "un-namespace.h"

#include "libc_private.h"
#include "pthread_private.h"

int	__pthread_cond_wait(pthread_cond_t *, pthread_mutex_t *);
int	__pthread_mutex_lock(pthread_mutex_t *);
int	__pthread_mutex_trylock(pthread_mutex_t *);

/*
 * All weak references used within libc should be in this table.
 * This allows static libraries to work.
 */
static void *references[] = {
	&_accept,
	&_bind,
	&_close,
	&_connect,
	&_dup,
	&_dup2,
	&_execve,
	&_fcntl,
	&_flock,
	&_flockfile,
	&_fstat,
	&_fstatfs,
	&_fsync,
	&_funlockfile,
	&_getdirentries,
	&_getlogin,
	&_getpeername,
	&_getsockname,
	&_getsockopt,
	&_ioctl,
	&_kevent,
	&_listen,
	&_nanosleep,
	&_open,
	&_pthread_cond_destroy,
	&_pthread_cond_init,
	&_pthread_cond_signal,
	&_pthread_cond_wait,
	&_pthread_getspecific,
	&_pthread_key_create,
	&_pthread_key_delete,
	&_pthread_mutex_destroy,
	&_pthread_mutex_init,
	&_pthread_mutex_lock,
	&_pthread_mutex_trylock,
	&_pthread_mutex_unlock,
	&_pthread_mutexattr_init,
	&_pthread_mutexattr_destroy,
	&_pthread_mutexattr_settype,
	&_pthread_once,
	&_pthread_setspecific,
	&_read,
	&_readv,
	&_recvfrom,
	&_recvmsg,
	&_select,
	&_sendmsg,
	&_sendto,
	&_setsockopt,
	&_sigaction,
	&_sigprocmask,
	&_sigsuspend,
	&_socket,
	&_socketpair,
	&_wait4,
	&_write,
	&_writev
};

/*
 * These are needed when linking statically.  All references within
 * libgcc (and libc) to these routines are weak, but if they are not
 * (strongly) referenced by the application or other libraries, then
 * the actual functions will not be loaded.
 */
static void *libgcc_references[] = {
	&_pthread_once,
	&_pthread_key_create,
	&_pthread_key_delete,
	&_pthread_getspecific,
	&_pthread_setspecific,
	&_pthread_mutex_init,
	&_pthread_mutex_destroy,
	&_pthread_mutex_lock,
	&_pthread_mutex_trylock,
	&_pthread_mutex_unlock
};

#define	DUAL_ENTRY(entry)	\
	(pthread_func_t)entry, (pthread_func_t)entry

static pthread_func_t jmp_table[][2] = {
	{DUAL_ENTRY(_pthread_atfork)},	/* PJT_ATFORK */
	{DUAL_ENTRY(_pthread_attr_destroy)},	/* PJT_ATTR_DESTROY */
	{DUAL_ENTRY(_pthread_attr_getdetachstate)},	/* PJT_ATTR_GETDETACHSTATE */
	{DUAL_ENTRY(_pthread_attr_getguardsize)},	/* PJT_ATTR_GETGUARDSIZE */
	{DUAL_ENTRY(_pthread_attr_getinheritsched)},	/* PJT_ATTR_GETINHERITSCHED */
	{DUAL_ENTRY(_pthread_attr_getschedparam)},	/* PJT_ATTR_GETSCHEDPARAM */
	{DUAL_ENTRY(_pthread_attr_getschedpolicy)},	/* PJT_ATTR_GETSCHEDPOLICY */
	{DUAL_ENTRY(_pthread_attr_getscope)},	/* PJT_ATTR_GETSCOPE */
	{DUAL_ENTRY(_pthread_attr_getstackaddr)},	/* PJT_ATTR_GETSTACKADDR */
	{DUAL_ENTRY(_pthread_attr_getstacksize)},	/* PJT_ATTR_GETSTACKSIZE */
	{DUAL_ENTRY(_pthread_attr_init)},	/* PJT_ATTR_INIT */
	{DUAL_ENTRY(_pthread_attr_setdetachstate)},	/* PJT_ATTR_SETDETACHSTATE */
	{DUAL_ENTRY(_pthread_attr_setguardsize)},	/* PJT_ATTR_SETGUARDSIZE */
	{DUAL_ENTRY(_pthread_attr_setinheritsched)},	/* PJT_ATTR_SETINHERITSCHED */
	{DUAL_ENTRY(_pthread_attr_setschedparam)},	/* PJT_ATTR_SETSCHEDPARAM */
	{DUAL_ENTRY(_pthread_attr_setschedpolicy)},	/* PJT_ATTR_SETSCHEDPOLICY */
	{DUAL_ENTRY(_pthread_attr_setscope)},	/* PJT_ATTR_SETSCOPE */
	{DUAL_ENTRY(_pthread_attr_setstackaddr)},	/* PJT_ATTR_SETSTACKADDR */
	{DUAL_ENTRY(_pthread_attr_setstacksize)},	/* PJT_ATTR_SETSTACKSIZE */
	{DUAL_ENTRY(_pthread_cancel)},	/* PJT_CANCEL */
	{DUAL_ENTRY(_pthread_cleanup_pop)},	/* PJT_CLEANUP_POP */
	{DUAL_ENTRY(_pthread_cleanup_push)},	/* PJT_CLEANUP_PUSH */
	{DUAL_ENTRY(_pthread_cond_broadcast)},	/* PJT_COND_BROADCAST */
	{DUAL_ENTRY(_pthread_cond_destroy)},	/* PJT_COND_DESTROY */
	{DUAL_ENTRY(_pthread_cond_init)},	/* PJT_COND_INIT */
	{DUAL_ENTRY(_pthread_cond_signal)},	/* PJT_COND_SIGNAL */
	{DUAL_ENTRY(_pthread_cond_timedwait)},	/* PJT_COND_TIMEDWAIT */
	{(pthread_func_t)__pthread_cond_wait,
	 (pthread_func_t)_pthread_cond_wait},	/* PJT_COND_WAIT */
	{DUAL_ENTRY(_pthread_detach)},	/* PJT_DETACH */
	{DUAL_ENTRY(_pthread_equal)},	/* PJT_EQUAL */
	{DUAL_ENTRY(_pthread_exit)},	/* PJT_EXIT */
	{DUAL_ENTRY(_pthread_getspecific)},	/* PJT_GETSPECIFIC */
	{DUAL_ENTRY(_pthread_join)},	/* PJT_JOIN */
	{DUAL_ENTRY(_pthread_key_create)},	/* PJT_KEY_CREATE */
	{DUAL_ENTRY(_pthread_key_delete)},	/* PJT_KEY_DELETE*/
	{DUAL_ENTRY(_pthread_kill)},	/* PJT_KILL */
	{DUAL_ENTRY(_pthread_main_np)},		/* PJT_MAIN_NP */
	{DUAL_ENTRY(_pthread_mutexattr_destroy)}, /* PJT_MUTEXATTR_DESTROY */
	{DUAL_ENTRY(_pthread_mutexattr_init)},	/* PJT_MUTEXATTR_INIT */
	{DUAL_ENTRY(_pthread_mutexattr_settype)}, /* PJT_MUTEXATTR_SETTYPE */
	{DUAL_ENTRY(_pthread_mutex_destroy)},	/* PJT_MUTEX_DESTROY */
	{DUAL_ENTRY(_pthread_mutex_init)},	/* PJT_MUTEX_INIT */
	{(pthread_func_t)__pthread_mutex_lock,
	 (pthread_func_t)_pthread_mutex_lock},	/* PJT_MUTEX_LOCK */
	{(pthread_func_t)__pthread_mutex_trylock,
	 (pthread_func_t)_pthread_mutex_trylock},/* PJT_MUTEX_TRYLOCK */
	{DUAL_ENTRY(_pthread_mutex_unlock)},	/* PJT_MUTEX_UNLOCK */
	{DUAL_ENTRY(_pthread_once)},		/* PJT_ONCE */
	{DUAL_ENTRY(_pthread_rwlock_destroy)},	/* PJT_RWLOCK_DESTROY */
	{DUAL_ENTRY(_pthread_rwlock_init)},	/* PJT_RWLOCK_INIT */
	{DUAL_ENTRY(_pthread_rwlock_rdlock)},	/* PJT_RWLOCK_RDLOCK */
	{DUAL_ENTRY(_pthread_rwlock_tryrdlock)},/* PJT_RWLOCK_TRYRDLOCK */
	{DUAL_ENTRY(_pthread_rwlock_trywrlock)},/* PJT_RWLOCK_TRYWRLOCK */
	{DUAL_ENTRY(_pthread_rwlock_unlock)},	/* PJT_RWLOCK_UNLOCK */
	{DUAL_ENTRY(_pthread_rwlock_wrlock)},	/* PJT_RWLOCK_WRLOCK */
	{DUAL_ENTRY(_pthread_self)},		/* PJT_SELF */
	{DUAL_ENTRY(_pthread_setcancelstate)},	/* PJT_SETCANCELSTATE */
	{DUAL_ENTRY(_pthread_setcanceltype)},	/* PJT_SETCANCELTYPE */
	{DUAL_ENTRY(_pthread_setspecific)},	/* PJT_SETSPECIFIC */
	{DUAL_ENTRY(_pthread_sigmask)},		/* PJT_SIGMASK */
	{DUAL_ENTRY(_pthread_testcancel)}	/* PJT_TESTCANCEL */
};

int _pthread_guard_default;
int _pthread_page_size;
int _pthread_stack_default;
int _pthread_stack_initial;

/*
 * Threaded process initialization
 */
void
_thread_init(void)
{
	int		fd;
	int             flags;
	int             i;
	size_t		len;
	int		mib[2];
	int		sched_stack_size;	/* Size of scheduler stack. */
#if !defined(__ia64__)
	u_long		stackp;
#endif

	struct clockinfo clockinfo;
	struct sigaction act;


	/* Check if this function has already been called: */
	if (_thread_initial)
		/* Only initialise the threaded application once. */
		return;

	_pthread_page_size = getpagesize();;
	_pthread_guard_default = _pthread_page_size;
	sched_stack_size = 4 * _pthread_page_size;
	if (sizeof(void *) == 8) {
		_pthread_stack_default = PTHREAD_STACK64_DEFAULT;
		_pthread_stack_initial = PTHREAD_STACK64_INITIAL;
	}
	else {
		_pthread_stack_default = PTHREAD_STACK32_DEFAULT;
		_pthread_stack_initial = PTHREAD_STACK32_INITIAL;
	}

	_pthread_attr_default.guardsize_attr = _pthread_guard_default;
	_pthread_attr_default.stacksize_attr = _pthread_stack_default;

	/*
	 * Make gcc quiescent about {,libgcc_}references not being
	 * referenced:
	 */
	if ((references[0] == NULL) || (libgcc_references[0] == NULL))
		PANIC("Failed loading mandatory references in _thread_init");

	/*
	 * Check the size of the jump table to make sure it is preset
	 * with the correct number of entries.
	 */
	if (sizeof(jmp_table) != (sizeof(pthread_func_t) * PJT_MAX * 2))
		PANIC("Thread jump table not properly initialized");
	memcpy(__thr_jtable, jmp_table, sizeof(jmp_table));

	/*
	 * Check for the special case of this process running as
	 * or in place of init as pid = 1:
	 */
	if (getpid() == 1) {
		/*
		 * Setup a new session for this process which is
		 * assumed to be running as root.
		 */
		if (setsid() == -1)
			PANIC("Can't set session ID");
		if (revoke(_PATH_CONSOLE) != 0)
			PANIC("Can't revoke console");
		if ((fd = __sys_open(_PATH_CONSOLE, O_RDWR)) < 0)
			PANIC("Can't open console");
		if (setlogin("root") == -1)
			PANIC("Can't set login to root");
		if (__sys_ioctl(fd, TIOCSCTTY, (char *) NULL) == -1)
			PANIC("Can't set controlling terminal");
		if (__sys_dup2(fd, 0) == -1 ||
		    __sys_dup2(fd, 1) == -1 ||
		    __sys_dup2(fd, 2) == -1)
			PANIC("Can't dup2");
	}

	/* Get the standard I/O flags before messing with them : */
	for (i = 0; i < 3; i++) {
		if (((_pthread_stdio_flags[i] =
		    __sys_fcntl(i, F_GETFL, NULL)) == -1) &&
		    (errno != EBADF))
			PANIC("Cannot get stdio flags");
	}

	/*
	 * Create a pipe that is written to by the signal handler to prevent
	 * signals being missed in calls to _select:
	 */
	if (__sys_pipe(_thread_kern_pipe) != 0) {
		/* Cannot create pipe, so abort: */
		PANIC("Cannot create kernel pipe");
	}

	/*
	 * Make sure the pipe does not get in the way of stdio:
	 */
	for (i = 0; i < 2; i++) {
		if (_thread_kern_pipe[i] < 3) {
			fd = __sys_fcntl(_thread_kern_pipe[i], F_DUPFD, 3);
			if (fd == -1)
			    PANIC("Cannot create kernel pipe");
			__sys_close(_thread_kern_pipe[i]);
			_thread_kern_pipe[i] = fd;
		}
	}
	/* Get the flags for the read pipe: */
	if ((flags = __sys_fcntl(_thread_kern_pipe[0], F_GETFL, NULL)) == -1) {
		/* Abort this application: */
		PANIC("Cannot get kernel read pipe flags");
	}
	/* Make the read pipe non-blocking: */
	else if (__sys_fcntl(_thread_kern_pipe[0], F_SETFL, flags | O_NONBLOCK) == -1) {
		/* Abort this application: */
		PANIC("Cannot make kernel read pipe non-blocking");
	}
	/* Get the flags for the write pipe: */
	else if ((flags = __sys_fcntl(_thread_kern_pipe[1], F_GETFL, NULL)) == -1) {
		/* Abort this application: */
		PANIC("Cannot get kernel write pipe flags");
	}
	/* Make the write pipe non-blocking: */
	else if (__sys_fcntl(_thread_kern_pipe[1], F_SETFL, flags | O_NONBLOCK) == -1) {
		/* Abort this application: */
		PANIC("Cannot get kernel write pipe flags");
	}
	/* Allocate and initialize the ready queue: */
	else if (_pq_alloc(&_readyq, PTHREAD_MIN_PRIORITY, PTHREAD_LAST_PRIORITY) != 0) {
		/* Abort this application: */
		PANIC("Cannot allocate priority ready queue.");
	}
	/* Allocate memory for the thread structure of the initial thread: */
	else if ((_thread_initial = (pthread_t) malloc(sizeof(struct pthread))) == NULL) {
		/*
		 * Insufficient memory to initialise this application, so
		 * abort:
		 */
		PANIC("Cannot allocate memory for initial thread");
	}
	/* Allocate memory for the scheduler stack: */
	else if ((_thread_kern_sched_stack = malloc(sched_stack_size)) == NULL)
		PANIC("Failed to allocate stack for scheduler");
	else {
		/* Zero the global kernel thread structure: */
		memset(&_thread_kern_thread, 0, sizeof(struct pthread));
		_thread_kern_thread.flags = PTHREAD_FLAGS_PRIVATE;
		memset(_thread_initial, 0, sizeof(struct pthread));

		/* Initialize the waiting and work queues: */
		TAILQ_INIT(&_waitingq);
		TAILQ_INIT(&_workq);

		/* Initialize the scheduling switch hook routine: */
		_sched_switch_hook = NULL;

		/* Give this thread default attributes: */
		memcpy((void *) &_thread_initial->attr, &_pthread_attr_default,
		    sizeof(struct pthread_attr));

		/* Find the stack top */
		mib[0] = CTL_KERN;
		mib[1] = KERN_USRSTACK;
		len = sizeof (_usrstack);
		if (sysctl(mib, 2, &_usrstack, &len, NULL, 0) == -1)
			_usrstack = (void *)USRSTACK;
		/*
		 * Create a red zone below the main stack.  All other stacks are
		 * constrained to a maximum size by the paramters passed to
		 * mmap(), but this stack is only limited by resource limits, so
		 * this stack needs an explicitly mapped red zone to protect the
		 * thread stack that is just beyond.
		 */
		if (mmap(_usrstack - _pthread_stack_initial -
		    _pthread_guard_default, _pthread_guard_default, 0,
		    MAP_ANON, -1, 0) == MAP_FAILED)
			PANIC("Cannot allocate red zone for initial thread");

		/* Set the main thread stack pointer. */
		_thread_initial->stack = _usrstack - _pthread_stack_initial;

		/* Set the stack attributes: */
		_thread_initial->attr.stackaddr_attr = _thread_initial->stack;
		_thread_initial->attr.stacksize_attr = _pthread_stack_initial;

		/* Setup the context for the scheduler: */
		_setjmp(_thread_kern_sched_jb);
#if !defined(__ia64__)
		stackp = (long)_thread_kern_sched_stack + sched_stack_size - sizeof(double);
#if defined(__amd64__)
		stackp &= ~0xFUL;
#endif
		SET_STACK_JB(_thread_kern_sched_jb, stackp);
#else
		SET_STACK_JB(_thread_kern_sched_jb, _thread_kern_sched_stack,
		    sched_stack_size);
#endif
		SET_RETURN_ADDR_JB(_thread_kern_sched_jb, _thread_kern_scheduler);

		/*
		 * Write a magic value to the thread structure
		 * to help identify valid ones:
		 */
		_thread_initial->magic = PTHREAD_MAGIC;

		/* Set the initial cancel state */
		_thread_initial->cancelflags = PTHREAD_CANCEL_ENABLE |
		    PTHREAD_CANCEL_DEFERRED;

		/* Default the priority of the initial thread: */
		_thread_initial->base_priority = PTHREAD_DEFAULT_PRIORITY;
		_thread_initial->active_priority = PTHREAD_DEFAULT_PRIORITY;
		_thread_initial->inherited_priority = 0;

		/* Initialise the state of the initial thread: */
		_thread_initial->state = PS_RUNNING;

		/* Set the name of the thread: */
		_thread_initial->name = strdup("_thread_initial");

		/* Initialize joiner to NULL (no joiner): */
		_thread_initial->joiner = NULL;

		/* Initialize the owned mutex queue and count: */
		TAILQ_INIT(&(_thread_initial->mutexq));
		_thread_initial->priority_mutex_count = 0;

		/* Initialize the global scheduling time: */
		_sched_ticks = 0;
		gettimeofday((struct timeval *) &_sched_tod, NULL);

		/* Initialize last active: */
		_thread_initial->last_active = (long) _sched_ticks;

		/* Initialize the initial context: */
		_thread_initial->curframe = NULL;

		/* Initialise the rest of the fields: */
		_thread_initial->poll_data.nfds = 0;
		_thread_initial->poll_data.fds = NULL;
		_thread_initial->sig_defer_count = 0;
		_thread_initial->yield_on_sig_undefer = 0;
		_thread_initial->specific = NULL;
		_thread_initial->cleanup = NULL;
		_thread_initial->flags = 0;
		_thread_initial->error = 0;
		TAILQ_INIT(&_thread_list);
		TAILQ_INSERT_HEAD(&_thread_list, _thread_initial, tle);
		_set_curthread(_thread_initial);
		TAILQ_INIT(&_atfork_list);
		_pthread_mutex_init(&_atfork_mutex, NULL);

		/* Initialise the global signal action structure: */
		sigfillset(&act.sa_mask);
		act.sa_handler = (void (*) ()) _thread_sig_handler;
		act.sa_flags = SA_SIGINFO | SA_RESTART;

		/* Clear pending signals for the process: */
		sigemptyset(&_process_sigpending);

		/* Clear the signal queue: */
		memset(_thread_sigq, 0, sizeof(_thread_sigq));

		/* Enter a loop to get the existing signal status: */
		for (i = 1; i < NSIG; i++) {
			/* Check for signals which cannot be trapped: */
			if (i == SIGKILL || i == SIGSTOP) {
			}

			/* Get the signal handler details: */
			else if (__sys_sigaction(i, NULL,
			    &_thread_sigact[i - 1]) != 0) {
				/*
				 * Abort this process if signal
				 * initialisation fails:
				 */
				PANIC("Cannot read signal handler info");
			}

			/* Initialize the SIG_DFL dummy handler count. */
			_thread_dfl_count[i] = 0;
		}

		/*
		 * Install the signal handler for the most important
		 * signals that the user-thread kernel needs. Actually
		 * SIGINFO isn't really needed, but it is nice to have.
		 */
		if (__sys_sigaction(_SCHED_SIGNAL, &act, NULL) != 0 ||
		    __sys_sigaction(SIGINFO,       &act, NULL) != 0 ||
		    __sys_sigaction(SIGCHLD,       &act, NULL) != 0) {
			/*
			 * Abort this process if signal initialisation fails:
			 */
			PANIC("Cannot initialise signal handler");
		}
		_thread_sigact[_SCHED_SIGNAL - 1].sa_flags = SA_SIGINFO;
		_thread_sigact[SIGINFO - 1].sa_flags = SA_SIGINFO;
		_thread_sigact[SIGCHLD - 1].sa_flags = SA_SIGINFO;

		/* Get the process signal mask: */
		__sys_sigprocmask(SIG_SETMASK, NULL, &_process_sigmask);

		/* Get the kernel clockrate: */
		mib[0] = CTL_KERN;
		mib[1] = KERN_CLOCKRATE;
		len = sizeof (struct clockinfo);
		if (sysctl(mib, 2, &clockinfo, &len, NULL, 0) == 0)
			_clock_res_usec = clockinfo.tick > CLOCK_RES_USEC_MIN ?
			    clockinfo.tick : CLOCK_RES_USEC_MIN;

		/* Get the table size: */
		if ((_thread_dtablesize = getdtablesize()) < 0) {
			/*
			 * Cannot get the system defined table size, so abort
			 * this process.
			 */
			PANIC("Cannot get dtablesize");
		}
		/* Allocate memory for the file descriptor table: */
		if ((_thread_fd_table = (struct fd_table_entry **) malloc(sizeof(struct fd_table_entry *) * _thread_dtablesize)) == NULL) {
			/* Avoid accesses to file descriptor table on exit: */
			_thread_dtablesize = 0;

			/*
			 * Cannot allocate memory for the file descriptor
			 * table, so abort this process.
			 */
			PANIC("Cannot allocate memory for file descriptor table");
		}
		/* Allocate memory for the pollfd table: */
		if ((_thread_pfd_table = (struct pollfd *) malloc(sizeof(struct pollfd) * _thread_dtablesize)) == NULL) {
			/*
			 * Cannot allocate memory for the file descriptor
			 * table, so abort this process.
			 */
			PANIC("Cannot allocate memory for pollfd table");
		} else {
			/*
			 * Enter a loop to initialise the file descriptor
			 * table:
			 */
			for (i = 0; i < _thread_dtablesize; i++) {
				/* Initialise the file descriptor table: */
				_thread_fd_table[i] = NULL;
			}

			/* Initialize stdio file descriptor table entries: */
			for (i = 0; i < 3; i++) {
				if ((_thread_fd_table_init(i) != 0) &&
				    (errno != EBADF))
					PANIC("Cannot initialize stdio file "
					    "descriptor table entry");
			}
		}
	}

	/* Initialise the garbage collector mutex and condition variable. */
	if (_pthread_mutex_init(&_gc_mutex,NULL) != 0 ||
	    _pthread_cond_init(&_gc_cond,NULL) != 0)
		PANIC("Failed to initialise garbage collector mutex or condvar");
}


/*
 * Special start up code for NetBSD/Alpha
 */
#if	defined(__NetBSD__) && defined(__alpha__)
int
main(int argc, char *argv[], char *env);

int
_thread_main(int argc, char *argv[], char *env)
{
	_thread_init();
	return (main(argc, argv, env));
}
#endif
