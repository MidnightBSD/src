#include <machine/asm.h>
__FBSDID("$FreeBSD: stable/11/lib/csu/arm/crtn.S 288373 2015-09-29 16:09:58Z kib $");

	.section .init,"ax",%progbits
	ldmea	fp, {fp, sp, pc}
	mov	pc, lr

	.section .fini,"ax",%progbits
	ldmea	fp, {fp, sp, pc}
	mov	pc, lr

	.section .note.GNU-stack,"",%progbits
