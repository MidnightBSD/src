/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

__FBSDID("$FreeBSD: release/7.0.0/lib/msun/i387/e_logf.S 141280 2005-02-04 14:08:32Z das $");
/* RCSID("$NetBSD: e_logf.S,v 1.2 1996/07/06 00:15:45 jtc Exp $") */

ENTRY(logf)
	fldln2
	flds	4(%esp)
	fyl2x
	ret
