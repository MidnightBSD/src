#include <machine/asm.h>
__FBSDID("$FreeBSD: release/10.0.0/lib/csu/mips/crti.S 205112 2010-03-13 02:10:20Z jmallett $");

	.section .init,"ax",%progbits
	.align 4
	.globl	_init
	.type	_init,%function
_init:
	.set noreorder
#if defined(__ABICALLS__) && (defined(__mips_o32) || defined(__mips_o64))
	SETUP_GP
#endif
	PTR_ADDU sp, sp, -CALLFRAME_SIZ
	REG_S	ra, CALLFRAME_RA(sp)
#if defined(__ABICALLS__)
#if defined(__mips_o32) || defined(__mips_o64)
	SAVE_GP(CALLFRAME_GP)
#else
	SETUP_GP64(CALLFRAME_GP, _init)
#endif
#else /* __ABICALLS__ */
#if defined(__mips_n32) || defined(__mips_n64)
	REG_S	gp, CALLFRAME_GP(sp)
#endif
#endif
	.set reorder

	.section .fini,"ax",%progbits
	.align 4
	.globl	_fini
	.type	_fini,%function
_fini:
	.set noreorder
#if defined(__ABICALLS__) && (defined(__mips_o32) || defined(__mips_o64))
	SETUP_GP
#endif
	PTR_ADDU sp, sp, -CALLFRAME_SIZ
	REG_S	ra, CALLFRAME_RA(sp)
#if defined(__ABICALLS__)
#if defined(__mips_o32) || defined(__mips_o64)
	SAVE_GP(CALLFRAME_GP)
#else
	SETUP_GP64(CALLFRAME_GP, _fini)
#endif
#else /* __ABICALLS__ */
#if defined(__mips_n32) || defined(__mips_n64)
	REG_S	gp, CALLFRAME_GP(sp)
#endif
#endif
	.set reorder
