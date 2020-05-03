#include <machine/asm.h>

.ident	"$FreeBSD: stable/11/lib/libc/arm/gen/_ctx_start.S 288373 2015-09-29 16:09:58Z kib $"
ENTRY(_ctx_start)
	mov	lr, pc
	mov	pc, r4
	mov	r0, r5
	bl	_C_LABEL(ctx_done)
	bl	_C_LABEL(abort)
END(_ctx_start)

	.section .note.GNU-stack,"",%progbits
