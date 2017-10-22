/*
 * David Leonard <d@openbsd.org>, 1999. Public Domain.
 *
 * $OpenBSD: uthread_msync.c,v 1.2 1999/06/09 07:16:17 d Exp $
 *
 * $FreeBSD: release/7.0.0/lib/libkse/thread/thr_msync.c 172491 2007-10-09 13:42:34Z obrien $
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <pthread.h>
#include "thr_private.h"

LT10_COMPAT_PRIVATE(__msync);
LT10_COMPAT_DEFAULT(msync);

__weak_reference(__msync, msync);

int
__msync(void *addr, size_t len, int flags)
{
	struct pthread *curthread = _get_curthread();
	int	ret;

	/*
	 * XXX This is quite pointless unless we know how to get the
	 * file descriptor associated with the memory, and lock it for
	 * write. The only real use of this wrapper is to guarantee
	 * a cancellation point, as per the standard. sigh.
	 */
	_thr_cancel_enter(curthread);
	ret = __sys_msync(addr, len, flags);
	_thr_cancel_leave(curthread, 1);

	return ret;
}
