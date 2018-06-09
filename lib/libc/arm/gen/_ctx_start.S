/* $MidnightBSD$ */
#include <machine/asm.h>

.ident	"$FreeBSD: stable/10/lib/libc/arm/gen/_ctx_start.S 271337 2014-09-09 22:24:01Z ian $"
ENTRY(_ctx_start)
	mov	lr, pc
	mov	pc, r4
	mov	r0, r5
	bl	_C_LABEL(ctx_done)
	bl	_C_LABEL(abort)
END(_ctx_start)
