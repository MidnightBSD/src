/*	$NetBSD: ldiv.S,v 1.1 2001/06/19 00:25:04 fvdl Exp $	*/

/*-
 * Written by gcc 3.0.
 * Copy/pasted by Frank van der Linden (fvdl@wasabisystems.com)
 */

#include <machine/asm.h>
__FBSDID("$FreeBSD: release/10.0.0/lib/libc/amd64/stdlib/ldiv.S 217106 2011-01-07 16:08:40Z kib $");

ENTRY(ldiv)
	movq	%rdi,%rax
	cqto
	idivq	%rsi
	ret
END(ldiv)

	.section .note.GNU-stack,"",%progbits
