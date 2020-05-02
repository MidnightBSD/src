/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>
__FBSDID("$FreeBSD: stable/11/lib/msun/i387/e_fmodf.S 324006 2017-09-26 09:01:56Z dim $")

ENTRY(fmodf)
	flds	8(%esp)
	flds	4(%esp)
1:	fprem
	fstsw	%ax
	sahf
	jp	1b
	fstp	%st(1)
	ret
END(fmodf)

	.section .note.GNU-stack,"",%progbits
