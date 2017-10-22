/*
 * Copyright (c) 1994, 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 *
 *	$NetBSD: locore.s,v 1.47 1998/03/22 07:26:32 thorpej Exp $
 * $FreeBSD: src/sys/alpha/alpha/swtch.s,v 1.34 2003/08/12 19:33:35 jhb Exp $
 */

#define _LOCORE
#include <machine/asm.h>
#include <machine/mutex.h>
#include "assym.s"

/**************************************************************************/

/*
 * Perform actions necessary to switch to a new context.  The
 * hwpcb should be in a0.
 */
#define	SWITCH_CONTEXT							\
	/* Make a note of the context we're running on. */		\
	stq	a0, PC_CURPCB(pcpup);					\
									\
	/* Swap in the new context. */					\
	call_pal PAL_OSF1_swpctx
	
/*
 * savectx: save process context, i.e. callee-saved registers
 *
 * Note that savectx() only works for threads other than curthread,
 * since cpu_switch will copy over the info saved here.  (It _can_
 * sanely be used for curthread iff cpu_switch won't be called again, e.g.
 * from if called from boot().)
 *
 * Arguments:
 *	a0	'struct pcb *' of the process that needs its context saved
 *
 * Return:
 *	v0	0.  (note that for child processes, it seems
 *		like savectx() returns 1, because the return address
 *		in the PCB is set to the return address from savectx().)
 */

LEAF(savectx, 1)
	br	pv, Lsavectx1
Lsavectx1: LDGP(pv)
	stq	sp, PCB_HWPCB_KSP(a0)		/* store sp */
	stq	s0, PCB_CONTEXT+(0 * 8)(a0)	/* store s0 - s6 */
	stq	s1, PCB_CONTEXT+(1 * 8)(a0)
	stq	s2, PCB_CONTEXT+(2 * 8)(a0)
	stq	s3, PCB_CONTEXT+(3 * 8)(a0)
	stq	s4, PCB_CONTEXT+(4 * 8)(a0)
	stq	s5, PCB_CONTEXT+(5 * 8)(a0)
	stq	s6, PCB_CONTEXT+(6 * 8)(a0)
	stq	ra, PCB_CONTEXT+(7 * 8)(a0)	/* store ra */
	call_pal PAL_OSF1_rdps			/* NOTE: doesn't kill a0 */
	stq	v0, PCB_CONTEXT+(8 * 8)(a0)	/* store ps, for ipl */

	mov	zero, v0
	RET
	END(savectx)

/**************************************************************************/

IMPORT(Lev1map, 8)

/*
 * cpu_throw()
 * Switch to a new thread discarding our current state.
 *
 * Arguments:
 *	a0	'struct thread *' of the old thread
 *	a1	'struct thread *' of the new thread
 */
LEAF(cpu_throw, 0)
	LDGP(pv)
	CALL(Lcs1)
	END(cpu_throw)

/*
 * cpu_switch()
 * Switch to a new thread saving the current state in the old thread.
 *
 * Arguments:
 *	a0	'struct thread *' of the old thread
 *	a1	'struct thread *' of the new thread
 */
LEAF(cpu_switch, 1)
	LDGP(pv)
	/* do an inline savectx(), to save old context */
	ldq	a2, TD_PCB(a0)
	/* NOTE: ksp is stored by the swpctx */
	stq	s0, PCB_CONTEXT+(0 * 8)(a2)	/* store s0 - s6 */
	stq	s1, PCB_CONTEXT+(1 * 8)(a2)
	stq	s2, PCB_CONTEXT+(2 * 8)(a2)
	stq	s3, PCB_CONTEXT+(3 * 8)(a2)
	stq	s4, PCB_CONTEXT+(4 * 8)(a2)
	stq	s5, PCB_CONTEXT+(5 * 8)(a2)
	stq	s6, PCB_CONTEXT+(6 * 8)(a2)
	stq	ra, PCB_CONTEXT+(7 * 8)(a2)	/* store ra */
	call_pal PAL_OSF1_rdps			/* NOTE: doesn't kill a0 */
	stq	v0, PCB_CONTEXT+(8 * 8)(a2)	/* store ps, for ipl */

	mov	a0, s0				/* s0 = old curthread */
	mov	a2, s1				/* s1 = old pcb */

	/*
	 * Deactivate the old address space before activating the
	 * new one.  We need to do this before activating the
	 * new thread's address space in the event that new
	 * thread is using the same vmspace as the old.  If we
	 * do this after we activate, then we might end up
	 * incorrectly marking the pmap inactive!
	 *
	 * We don't deactivate if we came here from switch_exit
	 * (old pmap no longer exists; vmspace has been freed).
	 * oldproc will be NULL in this case.  We have actually
	 * taken care of calling pmap_deactivate() in cpu_exit(),
	 * before the vmspace went away.
	 */
	beq	a0, sw1
	CALL(pmap_deactivate)			/* pmap_deactivate(oldthread) */

sw1:
	br	pv, Lcs1
Lcs1:	LDGP(pv)
	mov	a1, s2				/* s2 = new thread */
	ldq	s3, TD_MD_PCBPADDR(s2)		/* s3 = new pcbpaddr */

#ifdef SMP
	/*
	 * Save fp state if we have some.
	 */
	mov	s0, a0				/* curthread */
	ldiq	a1, 1				/* clear fpcurthread */
	CALL(alpha_fpstate_save)
#endif

	/*
	 * Activate the new thread's address space and perform
	 * the actual context swap.
	 */

	mov	s2, a0				/* pmap_activate(newthread) */
	CALL(pmap_activate)			/* XXXKSE */

	mov	s3, a0				/* swap the context */
	SWITCH_CONTEXT

	/*
	 * Now that the switch is done, update curthread and other
	 * globals.
	 */
	stq	s2, PC_CURTHREAD(pcpup)		/* curthread = p */

	/*
	 * Now running on the new pcb.
	 * Restore registers and return.
	 */
	ldq	t0, TD_PCB(s2)

	/* NOTE: ksp is restored by the swpctx */
	ldq	s0, PCB_CONTEXT+(0 * 8)(t0)		/* restore s0 - s6 */
	ldq	s1, PCB_CONTEXT+(1 * 8)(t0)
	ldq	s2, PCB_CONTEXT+(2 * 8)(t0)
	ldq	s3, PCB_CONTEXT+(3 * 8)(t0)
	ldq	s4, PCB_CONTEXT+(4 * 8)(t0)
	ldq	s5, PCB_CONTEXT+(5 * 8)(t0)
	ldq	s6, PCB_CONTEXT+(6 * 8)(t0)
	ldq	ra, PCB_CONTEXT+(7 * 8)(t0)		/* restore ra */

	ldiq	v0, 1				/* possible ret to savectx() */
	RET
	END(cpu_switch)


/*
 * fork_trampoline()
 *
 * Arrange for a function to be invoked neatly, after a cpu_switch().
 *
 * Invokes fork_exit() passing in three arguments: a callout function,
 * an argument to the callout, and a trapframe pointer.  For child processes
 * returning from fork(2), the argument is a pointer to the child process.
 *
 * The callout function is in s0, the address to return to after executing
 * fork_exit() is in s1, and the argument is in s2.
 */
LEAF(fork_trampoline, 0)
	mov	s1, ra
	mov	s0, a0
	mov	s2, a1
	mov	sp, a2
	jmp	zero, fork_exit
	END(fork_trampoline)
