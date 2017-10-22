/* $FreeBSD: release/10.0.0/sys/powerpc/aim/locore.S 209975 2010-07-13 05:32:19Z nwhitehorn $ */

#ifdef __powerpc64__
#include <powerpc/aim/locore64.S>
#else
#include <powerpc/aim/locore32.S>
#endif

