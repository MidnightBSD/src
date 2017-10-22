/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

__FBSDID("$FreeBSD: release/7.0.0/lib/msun/i387/s_significandf.S 140195 2005-01-13 18:58:25Z das $");
/* RCSID("$NetBSD: s_significandf.S,v 1.3 1995/05/09 00:24:07 jtc Exp $") */

ENTRY(significandf)
	flds	4(%esp)
	fxtract
	fstp	%st(1)
	ret
