/*
 * David Leonard <d@openbsd.org>, 1999. Public domain.
 * $FreeBSD$
 */
#include "namespace.h"
#include <sys/errno.h>
#include <pthread.h>
#include "un-namespace.h"
#include "thr_private.h"

__weak_reference(_pthread_cancel, pthread_cancel);
__weak_reference(_pthread_setcancelstate, pthread_setcancelstate);
__weak_reference(_pthread_setcanceltype, pthread_setcanceltype);
__weak_reference(_pthread_testcancel, pthread_testcancel);

static inline int
checkcancel(struct pthread *curthread)
{
	if ((curthread->cancelflags & THR_CANCELLING) != 0) {
		/*
		 * It is possible for this thread to be swapped out
		 * while performing cancellation; do not allow it
		 * to be cancelled again.
		 */
		if ((curthread->flags & THR_FLAGS_EXITING) != 0) {
			/*
			 * This may happen once, but after this, it
			 * shouldn't happen again.
			 */
			curthread->cancelflags &= ~THR_CANCELLING;
			return (0);
		}
		if ((curthread->cancelflags & PTHREAD_CANCEL_DISABLE) == 0) {
			curthread->cancelflags &= ~THR_CANCELLING;
			return (1);
		}
	}
	return (0);
}

static inline void
testcancel(struct pthread *curthread)
{
	if (checkcancel(curthread) != 0) {
		/* Unlock before exiting: */
		THR_THREAD_UNLOCK(curthread, curthread);

		_thr_exit_cleanup();
		_pthread_exit(PTHREAD_CANCELED);
		PANIC("cancel");
	}
}

int
_pthread_cancel(pthread_t pthread)
{
	struct pthread *curthread = _get_curthread();
	struct pthread *joinee = NULL;
	struct kse_mailbox *kmbx = NULL;
	int ret;

	if ((ret = _thr_ref_add(curthread, pthread, /*include dead*/0)) == 0) {
		/*
		 * Take the thread's lock while we change the cancel flags.
		 */
		THR_THREAD_LOCK(curthread, pthread);
		THR_SCHED_LOCK(curthread, pthread);
		if (pthread->flags & THR_FLAGS_EXITING) {
			THR_SCHED_UNLOCK(curthread, pthread);
			THR_THREAD_UNLOCK(curthread, pthread);
			_thr_ref_delete(curthread, pthread);
			return (ESRCH);
		}
		if (((pthread->cancelflags & PTHREAD_CANCEL_DISABLE) != 0) ||
		    (((pthread->cancelflags & THR_AT_CANCEL_POINT) == 0) &&
		    ((pthread->cancelflags & PTHREAD_CANCEL_ASYNCHRONOUS) == 0)))
			/* Just mark it for cancellation: */
			pthread->cancelflags |= THR_CANCELLING;
		else {
			/*
			 * Check if we need to kick it back into the
			 * run queue:
			 */
			switch (pthread->state) {
			case PS_RUNNING:
				/* No need to resume: */
				pthread->cancelflags |= THR_CANCELLING;
				break;

			case PS_LOCKWAIT:
				/*
				 * These can't be removed from the queue.
				 * Just mark it as cancelling and tell it
				 * to yield once it leaves the critical
				 * region.
				 */
				pthread->cancelflags |= THR_CANCELLING;
				pthread->critical_yield = 1;
				break;

			case PS_SLEEP_WAIT:
			case PS_SIGSUSPEND:
			case PS_SIGWAIT:
				/* Interrupt and resume: */
				pthread->interrupted = 1;
				pthread->cancelflags |= THR_CANCELLING;
				kmbx = _thr_setrunnable_unlocked(pthread);
				break;

			case PS_JOIN:
				/* Disconnect the thread from the joinee: */
				joinee = pthread->join_status.thread;
				pthread->join_status.thread = NULL;
				pthread->cancelflags |= THR_CANCELLING;
				kmbx = _thr_setrunnable_unlocked(pthread);
				if ((joinee != NULL) &&
				    (pthread->kseg == joinee->kseg)) {
					/* Remove the joiner from the joinee. */
					joinee->joiner = NULL;
					joinee = NULL;
				}
				break;

			case PS_SUSPENDED:
			case PS_MUTEX_WAIT:
			case PS_COND_WAIT:
				/*
				 * Threads in these states may be in queues.
				 * In order to preserve queue integrity, the
				 * cancelled thread must remove itself from the
				 * queue.  Mark the thread as interrupted and
				 * needing cancellation, and set the state to
				 * running.  When the thread resumes, it will
				 * remove itself from the queue and call the
				 * cancellation completion routine.
				 */
				pthread->interrupted = 1;
				pthread->cancelflags |= THR_CANCEL_NEEDED;
				kmbx = _thr_setrunnable_unlocked(pthread);
				pthread->continuation =
					_thr_finish_cancellation;
				break;

			case PS_DEAD:
			case PS_DEADLOCK:
			case PS_STATE_MAX:
				/* Ignore - only here to silence -Wall: */
				break;
			}
			if ((pthread->cancelflags & THR_AT_CANCEL_POINT) &&
			    (pthread->blocked != 0 ||
			     pthread->attr.flags & PTHREAD_SCOPE_SYSTEM))
				kse_thr_interrupt(&pthread->tcb->tcb_tmbx,
					KSE_INTR_INTERRUPT, 0);
		}

		/*
		 * Release the thread's lock and remove the
		 * reference:
		 */
		THR_SCHED_UNLOCK(curthread, pthread);
		THR_THREAD_UNLOCK(curthread, pthread);
		_thr_ref_delete(curthread, pthread);
		if (kmbx != NULL)
			kse_wakeup(kmbx);

		if ((joinee != NULL) &&
		    (_thr_ref_add(curthread, joinee, /* include dead */1) == 0)) {
			/* Remove the joiner from the joinee. */
			THR_SCHED_LOCK(curthread, joinee);
			joinee->joiner = NULL;
			THR_SCHED_UNLOCK(curthread, joinee);
			_thr_ref_delete(curthread, joinee);
		}
	}
	return (ret);
}

int
_pthread_setcancelstate(int state, int *oldstate)
{
	struct pthread	*curthread = _get_curthread();
	int ostate;
	int ret;
	int need_exit = 0;

	/* Take the thread's lock while fiddling with the state: */
	THR_THREAD_LOCK(curthread, curthread);

	ostate = curthread->cancelflags & PTHREAD_CANCEL_DISABLE;

	switch (state) {
	case PTHREAD_CANCEL_ENABLE:
		curthread->cancelflags &= ~PTHREAD_CANCEL_DISABLE;
		if ((curthread->cancelflags & PTHREAD_CANCEL_ASYNCHRONOUS) != 0)
			need_exit = checkcancel(curthread);
		ret = 0;
		break;
	case PTHREAD_CANCEL_DISABLE:
		curthread->cancelflags |= PTHREAD_CANCEL_DISABLE;
		ret = 0;
		break;
	default:
		ret = EINVAL;
	}

	THR_THREAD_UNLOCK(curthread, curthread);
	if (need_exit != 0) {
		_thr_exit_cleanup();
		_pthread_exit(PTHREAD_CANCELED);
		PANIC("cancel");
	}
	if (ret == 0 && oldstate != NULL)
		*oldstate = ostate;

	return (ret);
}

int
_pthread_setcanceltype(int type, int *oldtype)
{
	struct pthread	*curthread = _get_curthread();
	int otype;
	int ret;
	int need_exit = 0;

	/* Take the thread's lock while fiddling with the state: */
	THR_THREAD_LOCK(curthread, curthread);

	otype = curthread->cancelflags & PTHREAD_CANCEL_ASYNCHRONOUS;
	switch (type) {
	case PTHREAD_CANCEL_ASYNCHRONOUS:
		curthread->cancelflags |= PTHREAD_CANCEL_ASYNCHRONOUS;
		need_exit = checkcancel(curthread);
		ret = 0;
		break;
	case PTHREAD_CANCEL_DEFERRED:
		curthread->cancelflags &= ~PTHREAD_CANCEL_ASYNCHRONOUS;
		ret = 0;
		break;
	default:
		ret = EINVAL;
	}

	THR_THREAD_UNLOCK(curthread, curthread);
	if (need_exit != 0) {
		_thr_exit_cleanup();
		_pthread_exit(PTHREAD_CANCELED);
		PANIC("cancel");
	}
	if (ret == 0 && oldtype != NULL)
		*oldtype = otype;

	return (ret);
}

void
_pthread_testcancel(void)
{
	struct pthread	*curthread = _get_curthread();

	THR_THREAD_LOCK(curthread, curthread);
	testcancel(curthread);
	THR_THREAD_UNLOCK(curthread, curthread);
}

void
_thr_cancel_enter(struct pthread *thread)
{
	/* Look for a cancellation before we block: */
	THR_THREAD_LOCK(thread, thread);
	testcancel(thread);
	thread->cancelflags |= THR_AT_CANCEL_POINT;
	THR_THREAD_UNLOCK(thread, thread);
}

void
_thr_cancel_leave(struct pthread *thread, int check)
{
	THR_THREAD_LOCK(thread, thread);
	thread->cancelflags &= ~THR_AT_CANCEL_POINT;
	/* Look for a cancellation after we unblock: */
	if (check)
		testcancel(thread);
	THR_THREAD_UNLOCK(thread, thread);
}

void
_thr_finish_cancellation(void *arg __unused)
{
	struct pthread	*curthread = _get_curthread();

	curthread->continuation = NULL;
	curthread->interrupted = 0;

	THR_THREAD_LOCK(curthread, curthread);
	if ((curthread->cancelflags & THR_CANCEL_NEEDED) != 0) {
		curthread->cancelflags &= ~THR_CANCEL_NEEDED;
		THR_THREAD_UNLOCK(curthread, curthread);
		_thr_exit_cleanup();
		_pthread_exit(PTHREAD_CANCELED);
	}
	THR_THREAD_UNLOCK(curthread, curthread);
}
