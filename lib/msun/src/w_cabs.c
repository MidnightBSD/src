/*
 * cabs() wrapper for hypot().
 *
 * Written by J.T. Conklin, <jtc@wimsey.com>
 * Placed into the Public Domain, 1994.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: stable/9/lib/msun/src/w_cabs.c 177758 2008-03-30 20:03:06Z das $");

#include <complex.h>
#include <float.h>
#include <math.h>

double
cabs(double complex z)
{
	return hypot(creal(z), cimag(z));
}

#if LDBL_MANT_DIG == 53
__weak_reference(cabs, cabsl);
#endif
