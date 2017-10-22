#include <machine/asm.h>
__FBSDID("$FreeBSD: release/7.0.0/lib/csu/arm/crtn.S 135679 2004-09-23 23:00:51Z cognet $");
	.section .init,"ax",%progbits
	ldmea	fp, {fp, sp, pc}
	mov	pc, lr

	.section .fini,"ax",%progbits
	ldmea	fp, {fp, sp, pc}
	mov	pc, lr
