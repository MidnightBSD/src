/*
 * $FreeBSD: release/7.0.0/lib/libkse/thread/thr_vfork.c 172491 2007-10-09 13:42:34Z obrien $
 */
#include <unistd.h>

#include "thr_private.h"

LT10_COMPAT_PRIVATE(_vfork);
LT10_COMPAT_DEFAULT(vfork);

__weak_reference(_vfork, vfork);

int
_vfork(void)
{
	return (fork());
}
