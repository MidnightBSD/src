#include <machine/asm.h>
__FBSDID("$FreeBSD: stable/11/lib/csu/arm/crti.S 288373 2015-09-29 16:09:58Z kib $");

	.section .init,"ax",%progbits
	.align 4
	.globl	_init
	.type	_init,%function
_init:
	mov	ip, sp
	stmdb	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4

	.section .fini,"ax",%progbits
	.align 4
	.globl	_fini
	.type	_fini,%function
_fini:
	mov	ip, sp
	stmdb	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4

	.section .note.GNU-stack,"",%progbits
