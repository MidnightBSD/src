/* $FreeBSD: stable/9/sys/powerpc/include/trap.h 236511 2012-06-03 11:54:26Z marius $ */

#if defined(AIM)
#include <machine/trap_aim.h>
#elif defined(E500)
#include <machine/trap_booke.h>
#endif

