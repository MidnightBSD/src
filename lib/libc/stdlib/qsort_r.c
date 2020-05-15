/*
 * This file is in the public domain.  Originally written by Garrett
 * A. Wollman.
 *
 * $FreeBSD: stable/11/lib/libc/stdlib/qsort_r.c 264143 2014-04-05 08:17:48Z theraven $
 */
#include "block_abi.h"
#define I_AM_QSORT_R
#include "qsort.c"

typedef DECLARE_BLOCK(int, qsort_block, const void *, const void *);

void
qsort_b(void *base, size_t nel, size_t width, qsort_block compar)
{
	qsort_r(base, nel, width, compar,
		(int (*)(void *, const void *, const void *))
		GET_BLOCK_FUNCTION(compar));
}
