#include <machine/asm.h>
__FBSDID("$FreeBSD: release/10.0.0/lib/libc/amd64/string/bcmp.S 217106 2011-01-07 16:08:40Z kib $");

#if 0
	RCSID("$NetBSD: bcmp.S,v 1.1 2001/06/19 00:25:04 fvdl Exp $")
#endif

ENTRY(bcmp)
	cld				/* set compare direction forward */

	movq	%rdx,%rcx		/* compare by words */
	shrq	$3,%rcx
	repe
	cmpsq
	jne	L1

	movq	%rdx,%rcx		/* compare remainder by bytes */
	andq	$7,%rcx
	repe
	cmpsb
L1:
	setne	%al
	movsbl	%al,%eax
	ret
END(bcmp)

	.section .note.GNU-stack,"",%progbits
