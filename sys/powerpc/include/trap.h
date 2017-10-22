/* $FreeBSD: release/10.0.0/sys/powerpc/include/trap.h 236141 2012-05-27 10:25:20Z raj $ */

#if defined(AIM)
#include <machine/trap_aim.h>
#elif defined(BOOKE)
#include <machine/trap_booke.h>
#endif

