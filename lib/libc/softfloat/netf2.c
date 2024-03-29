/* $NetBSD: netf2.c,v 1.1 2011/01/17 10:08:35 matt Exp $ */

/*
 * Written by Matt Thomas, 2011.  This file is in the Public Domain.
 */

#include "softfloat-for-gcc.h"
#include "milieu.h"
#include "softfloat.h"

#include <sys/cdefs.h>

#ifdef FLOAT128

flag __netf2(float128, float128);

flag
__netf2(float128 a, float128 b)
{

	/* libgcc1.c says a != b */
	return !float128_eq(a, b);
}

#endif /* FLOAT128 */
