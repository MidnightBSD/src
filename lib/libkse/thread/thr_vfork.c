/*
 * $FreeBSD: release/10.0.0/lib/libkse/thread/thr_vfork.c 174689 2007-12-16 23:29:57Z deischen $
 */

#include <unistd.h>
#include "thr_private.h"

int _vfork(void);

__weak_reference(_vfork, vfork);

int
_vfork(void)
{
	return (fork());
}
