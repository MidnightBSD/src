/* $MidnightBSD$ */

/* @(#)e_lgamma.c 1.3 95/01/18 */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice 
 * is preserved.
 * ====================================================
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: stable/10/lib/msun/src/e_lgamma.c 284810 2015-06-25 13:01:10Z tijl $");

/* __ieee754_lgamma(x)
 * Return the logarithm of the Gamma function of x.
 *
 * Method: call __ieee754_lgamma_r
 */

#include <float.h>

#include "math.h"
#include "math_private.h"

extern int signgam;

double
__ieee754_lgamma(double x)
{
	return __ieee754_lgamma_r(x,&signgam);
}

#if (LDBL_MANT_DIG == 53)
__weak_reference(lgamma, lgammal);
#endif
