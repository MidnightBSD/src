/* $FreeBSD: src/lib/libc/amd64/gen/fpgetmask.c,v 1.1 2003/07/22 06:46:17 peter Exp $ */
#define __IEEEFP_NOINLINES__ 1
#include <ieeefp.h>

fp_except_t fpgetmask(void)
{
	return __fpgetmask();
}
