/*	$NetBSD: memcpy.S,v 1.4 2003/10/14 07:51:45 scw Exp $	*/

#include <machine/asm.h>
__FBSDID("$FreeBSD: release/10.0.0/lib/libc/arm/string/memcpy.S 172616 2007-10-13 12:06:31Z cognet $");
#if !defined(_ARM_ARCH_5E) || defined(_STANDALONE)
#include "memcpy_arm.S"
#else
#include "memcpy_xscale.S"
#endif
