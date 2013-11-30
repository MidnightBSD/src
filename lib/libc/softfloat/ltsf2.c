/* $NetBSD: ltsf2.c,v 1.1 2000/06/06 08:15:06 bjh21 Exp $ */

/*
 * Written by Ben Harris, 2000.  This file is in the Public Domain.
 */

#include "softfloat-for-gcc.h"
#include "milieu.h"
#include "softfloat.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libc/softfloat/ltsf2.c,v 1.1 2004/05/14 12:13:04 cognet Exp $");

flag __ltsf2(float32, float32);

flag
__ltsf2(float32 a, float32 b)
{

	/* libgcc1.c says -(a < b) */
	return -float32_lt(a, b);
}
