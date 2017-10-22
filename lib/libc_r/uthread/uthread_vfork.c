/*
 * $FreeBSD: release/7.0.0/lib/libc_r/uthread/uthread_vfork.c 75369 2001-04-10 04:19:21Z deischen $
 */
#include <unistd.h>

__weak_reference(_vfork, vfork);

int
_vfork(void)
{
	return (fork());
}
