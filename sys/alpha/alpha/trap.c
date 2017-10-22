/* $NetBSD: trap.c,v 1.31 1998/03/26 02:21:46 thorpej Exp $ */
/*-
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/alpha/alpha/trap.c,v 1.123 2005/04/12 23:18:53 jhb Exp $");

/* #include "opt_fix_unaligned_vax_fp.h" */
#include "opt_ddb.h"
#include "opt_ktrace.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kdb.h>
#include <sys/ktr.h>
#include <sys/sysproto.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/exec.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/smp.h>
#include <sys/vmmeter.h>
#include <sys/sysent.h>
#include <sys/signalvar.h>
#include <sys/syscall.h>
#include <sys/pioctl.h>
#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>
#include <vm/vm_param.h>
#include <sys/ptrace.h>
#include <machine/clock.h>
#include <machine/md_var.h>
#include <machine/reg.h>
#include <machine/pal.h>
#include <machine/pcb.h>
#include <machine/fpu.h>

#ifdef KTRACE
#include <sys/uio.h>
#include <sys/ktrace.h>
#endif

#include <alpha/alpha/db_instruction.h>		/* for handle_opdec() */

unsigned long	Sfloat_to_reg(unsigned int);
unsigned int	reg_to_Sfloat(unsigned long);
unsigned long	Tfloat_reg_cvt(unsigned long);
#ifdef FIX_UNALIGNED_VAX_FP
unsigned long	Ffloat_to_reg(unsigned int);
unsigned int	reg_to_Ffloat(unsigned long);
unsigned long	Gfloat_reg_cvt(unsigned long);
#endif

int		unaligned_fixup(unsigned long, unsigned long,
		    unsigned long, struct thread *);
int		handle_opdec(struct thread *td, u_int64_t *ucodep);

static void printtrap(const unsigned long, const unsigned long,
      const unsigned long, const unsigned long, struct trapframe *, int, int);

#ifdef WITNESS
extern char *syscallnames[];
#endif

static const char *arith_exceptions[] = {
	"software completion",
	"invalid operation",
	"division by zero",
	"overflow",
	"underflow",
	"inexact result",
	"integer overflow",
};

static const char *instruction_faults[] = {
	"bpt",
	"bugchk",
	"gentrap",
	"FEN",
	"opDec"
};

static const char *interrupt_types[] = {
	"interprocessor",
	"clock",
	"correctable error",
	"machine check",
	"I/O device",
	"performance counter"
};

static const char *mmfault_types[] = {
	"translation not valid",
	"access violation",
	"fault on read",
	"fault on execute",
	"fault on write"
};

static const char *mmfault_causes[] = {
	"instruction fetch",
	"load instructon",
	"store instruction"
};

static void
printtrap(a0, a1, a2, entry, framep, isfatal, user)
	const unsigned long a0, a1, a2, entry;
	struct trapframe *framep;
	int isfatal, user;
{
	char ubuf[64];
	const char *entryname;
	unsigned long i;

	switch (entry) {
	case ALPHA_KENTRY_INT:
		entryname = "interrupt";
		break;
	case ALPHA_KENTRY_ARITH:
		entryname = "arithmetic trap";
		break;
	case ALPHA_KENTRY_MM:
		entryname = "memory management fault";
		break;
	case ALPHA_KENTRY_IF:
		entryname = "instruction fault";
		break;
	case ALPHA_KENTRY_UNA:
		entryname = "unaligned access fault";
		break;
	case ALPHA_KENTRY_SYS:
		entryname = "system call";
		break;
	default:
		snprintf(ubuf, sizeof(ubuf), "type %lx", entry);
		entryname = (const char *) ubuf;
		break;
	}

	printf("\n");
	printf("%s %s trap:\n", isfatal? "fatal" : "handled",
	       user ? "user" : "kernel");
	printf("\n");
	printf("    trap entry     = 0x%lx (%s)\n", entry, entryname);
#ifdef SMP
	printf("    cpuid          = %d\n", PCPU_GET(cpuid));
#endif
	switch (entry) {
	case ALPHA_KENTRY_INT:
		printf("    interrupt type = ");
		if (a0 < 5) {
			printf("%s\n", interrupt_types[a0]);
			if (a0 > 1) {
				printf("    vector         = 0x%lx\n", a1);
				if (a0 < 3)
					printf("    logout area    = 0x%lx\n",
					    a2);
			}
		} else
			printf("0x%lx (unknown)\n", a0);
		break;
	case ALPHA_KENTRY_ARITH:
		printf("    exception type = ");
		for (i = 0; i < 7; i++)
			if (a0 & (1 << i)) {
				printf("%s", arith_exceptions[i]);
				if (a0 & (~0 - (1 << i)))
					printf(", ");
			}
		printf("\n");
		printf("    register mask  = 0x%lx", a1);
		break;
	case ALPHA_KENTRY_MM:
		printf("    faulting va    = 0x%lx\n", a0);
		printf("    type           = ");
		if (a1 < 5)
			printf("%s\n", mmfault_types[a1]);
		else
			printf("0x%lx (unknown)\n", a1);
		printf("    cause          = ");
		i = a2 + 1;
		if (i < 3)
			printf("%s\n", mmfault_causes[i]);
		else
			printf("0x%lx (unknown)\n", a2);
		break;
	case ALPHA_KENTRY_IF:
		printf("    fault type     = ");
		if (a0 < 5)
			printf("%s\n", instruction_faults[a0]);
		else
			printf("0x%lx (unknown)\n", a0);
		break;
	case ALPHA_KENTRY_UNA:
		printf("    faulting va    = 0x%lx\n", a0);
		printf("    opcode         = 0x%lx\n", a1);
		printf("    register       = 0x%lx\n", a2);
		break;
	default:
		printf("    a0             = 0x%lx\n", a0);
		printf("    a1             = 0x%lx\n", a1);
		printf("    a2             = 0x%lx\n", a2);
		break;
	}
	printf("    pc             = 0x%lx\n", framep->tf_regs[FRAME_PC]);
	printf("    ra             = 0x%lx\n", framep->tf_regs[FRAME_RA]);
	printf("    sp             = 0x%lx\n", framep->tf_regs[FRAME_SP]);
	if (curthread != NULL && (curthread->td_proc->p_flag & P_KTHREAD) == 0)
		printf("    usp            = 0x%lx\n", alpha_pal_rdusp());
	printf("    curthread      = %p\n", curthread);
	if (curthread != NULL)
		printf("        pid = %d, comm = %s\n",
		    curthread->td_proc->p_pid, curthread->td_proc->p_comm);
	printf("\n");
}

/*
 * Trap is called from locore to handle most types of processor traps.
 * System calls are broken out for efficiency and ASTs are broken out
 * to make the code a bit cleaner and more representative of the
 * Alpha architecture.
 */
/*ARGSUSED*/
void
trap(a0, a1, a2, entry, framep)
	const unsigned long a0, a1, a2, entry;
	struct trapframe *framep;
{
	register struct thread *td;
	register struct proc *p;
	register int i;
	u_int64_t ucode;
	u_int sticks;
	int user;
#ifdef SMP
	register_t s;
#endif

	/*
	 * Find our per-cpu globals.
	 */
#ifdef SMP
	s = intr_disable();
#endif
	pcpup = (struct pcpu *) alpha_pal_rdval();
	td = curthread;
#ifdef SMP
	if (td == NULL) {
		printtrap(a0, a1, a2, entry, framep, 1, 0);
		cpu_halt();
	}
	td->td_md.md_kernnest++;
	intr_restore(s);
#endif
	p = td->td_proc;

#ifdef KDB
	if (kdb_active) {
		kdb_reenter();
		return;
	}
#endif

	/*
	GIANT_REQUIRED;
	 * Giant hasn't been acquired yet.
	 */

	PCPU_LAZY_INC(cnt.v_trap);
	ucode = 0;
	user = (framep->tf_regs[FRAME_PS] & ALPHA_PSL_USERMODE) != 0;
	CTR5(KTR_TRAP, "%s trap: pid %d, (%lx, %lx, %lx)",
	    user ? "user" : "kernel", p->p_pid, a0, a1, a2);
	if (user)  {
		sticks = td->td_sticks;
		td->td_frame = framep;
		if (td->td_ucred != p->p_ucred)
			cred_update_thread(td);
	} else {
		sticks = 0;		/* XXX bogus -Wuninitialized warning */
		KASSERT(cold || td->td_ucred != NULL,
		    ("kernel trap doesn't have ucred"));
	}

#ifdef DIAGNOSTIC
	if (user)
		alpha_fpstate_check(td);
#endif

	switch (entry) {
	case ALPHA_KENTRY_UNA:
		/*
		 * If user-land, do whatever fixups, printing, and
		 * signalling is appropriate (based on system-wide
		 * and per-process unaligned-access-handling flags).
		 */
		if (user) {
			mtx_lock(&Giant);
			i = unaligned_fixup(a0, a1, a2, td);
			mtx_unlock(&Giant);
			if (i == 0)
				goto out;
			ucode = a0;		/* VA */
			break;
		}

		/*
		 * Unaligned access from kernel mode is always an error,
		 * EVEN IF A COPY FAULT HANDLER IS SET!
		 *
		 * It's an error if a copy fault handler is set because
		 * the various routines which do user-initiated copies
		 * do so in a bcopy-like manner.  In other words, the
		 * kernel never assumes that pointers provided by the
		 * user are properly aligned, and so if the kernel
		 * does cause an unaligned access it's a kernel bug.
		 */
		goto dopanic;

	case ALPHA_KENTRY_ARITH:
		/* 
		 * If user-land, give a SIGFPE if software completion
		 * is not requested or if the completion fails.
		 */
		if (user) {
			mtx_lock(&Giant);
			if (a0 & EXCSUM_SWC)
				if (fp_software_completion(a1, td)) {
					mtx_unlock(&Giant);
					goto out;
				}
			mtx_unlock(&Giant);
			i = SIGFPE;
			ucode =  a0;		/* exception summary */
			break;
		}

		/* Always fatal in kernel.  Should never happen. */
		goto dopanic;

	case ALPHA_KENTRY_IF:
		/*
		 * These are always fatal in kernel, and should never happen.
		 */
		if (!user) {
#ifdef KDB
			/*
			 * ...unless, of course, KDB is configured; BUGCHK
			 * is used to invoke the kernel debugger, and we
			 * might have set a breakpoint.
			 */
			if (a0 == ALPHA_IF_CODE_BUGCHK ||
			    a0 == ALPHA_IF_CODE_BPT) {
				if (kdb_trap(entry, a0, framep))
					goto out;
			}

			/*
			 * If we get here, KDB did _not_ handle the
			 * trap, and we need to PANIC!
			 */
#endif
			goto dopanic;
		}
		i = 0;
		switch (a0) {
		case ALPHA_IF_CODE_GENTRAP:
			if (framep->tf_regs[FRAME_A0] == -2) { /* weird! */
				i = SIGFPE;
				ucode =  a0;	/* exception summary */
				break;
			}
			/* FALLTHROUGH */
		case ALPHA_IF_CODE_BPT:
		case ALPHA_IF_CODE_BUGCHK:
			if (td->td_md.md_flags & (MDTD_STEP1|MDTD_STEP2)) {
				mtx_lock(&Giant);
				ptrace_clear_single_step(td);
				td->td_frame->tf_regs[FRAME_PC] -= 4;
				mtx_unlock(&Giant);
			}
			ucode = a0;		/* trap type */
			i = SIGTRAP;
			break;

		case ALPHA_IF_CODE_OPDEC:
			i = handle_opdec(td, &ucode);
			if (i == 0)
				goto out;
			break;

		case ALPHA_IF_CODE_FEN:
			/*
			 * on exit from the kernel, if thread == fpcurthread,
			 * FP is enabled.
			 */
			if (PCPU_GET(fpcurthread) == td) {
				printf("trap: fp disabled for fpcurthread == %p",
				    td);
				goto dopanic;
			}
	
			alpha_fpstate_switch(td);
			goto out;

		default:
			printf("trap: unknown IF type 0x%lx\n", a0);
			goto dopanic;
		}
		break;

	case ALPHA_KENTRY_MM:
		switch (a1) {
		case ALPHA_MMCSR_FOR:
		case ALPHA_MMCSR_FOE:
		case ALPHA_MMCSR_FOW:
			pmap_emulate_reference(p->p_vmspace, a0, user,
			    a1 == ALPHA_MMCSR_FOW);
			goto out;

		case ALPHA_MMCSR_INVALTRANS:
		case ALPHA_MMCSR_ACCESS:
	    	{
			register vm_offset_t va;
			register struct vmspace *vm = NULL;
			register vm_map_t map;
			vm_prot_t ftype = 0;
			int rv;

			/*
			 * If it was caused by fuswintr or suswintr,
			 * just punt.  Note that we check the faulting
			 * address against the address accessed by
			 * [fs]uswintr, in case another fault happens
			 * when they are running.
			 */
			if (!user &&
			    td != NULL &&
			    td->td_pcb->pcb_onfault ==
			      (unsigned long)fswintrberr &&
			    td->td_pcb->pcb_accessaddr == a0) {
				framep->tf_regs[FRAME_PC] =
				    td->td_pcb->pcb_onfault;
				td->td_pcb->pcb_onfault = 0;
				goto out;
			}

			/*
			 * It is only a kernel address space fault iff:
			 *	1. !user and
			 *	2. pcb_onfault not set or
			 *	3. pcb_onfault set but kernel space data fault
			 * The last can occur during an exec() copyin where the
			 * argument space is lazy-allocated.
			 *
			 * For the purposes of the Linux emulator, we allow
			 * kernel accesses to a small region of the
			 * user stack which the emulator uses to
			 * translate syscall arguments.
			 */
			if (!user 
			    && ((a0 >= VM_MIN_KERNEL_ADDRESS) 
				|| (td == NULL) 
				|| (td->td_pcb->pcb_onfault == 0))) {
				if (a0 >= trunc_page(PS_STRINGS
						     - szsigcode
						     - SPARE_USRSPACE)
				    && a0 < round_page(PS_STRINGS
						       - szsigcode)) {
					vm = p->p_vmspace;
					map = &vm->vm_map;
				} else {
					map = kernel_map;
				}
			} else {
				vm = p->p_vmspace;
				map = &vm->vm_map;
			}
	
			switch (a2) {
			case -1:		/* instruction fetch fault */
			case 0:			/* load instruction */
				ftype = VM_PROT_READ;
				break;
			case 1:			/* store instruction */
				ftype = VM_PROT_WRITE;
				break;
#ifdef DIAGNOSTIC
			default:		/* XXX gcc -Wuninitialized */
				goto dopanic;
#endif
			}
	
			va = trunc_page((vm_offset_t)a0);

			if (map != kernel_map) {
				/*
				 * Keep swapout from messing with us
				 * during thiscritical time.
				 */
				PROC_LOCK(p);
				++p->p_lock;
				PROC_UNLOCK(p);

				/* Fault in the user page: */
				rv = vm_fault(map, va, ftype,
					      (ftype & VM_PROT_WRITE)
						      ? VM_FAULT_DIRTY
						      : VM_FAULT_NORMAL);

				PROC_LOCK(p);
				--p->p_lock;
				PROC_UNLOCK(p);
			} else {
				/*
				 * Don't have to worry about process
				 * locking or stacks in the kernel.
				 */
				rv = vm_fault(map, va, ftype, VM_FAULT_NORMAL);
			}
			if (rv == KERN_SUCCESS)
				goto out;

			if (!user) {
				/* Check for copyin/copyout fault */
				if (td != NULL &&
				    td->td_pcb->pcb_onfault != 0) {
					framep->tf_regs[FRAME_PC] =
					    td->td_pcb->pcb_onfault;
					td->td_pcb->pcb_onfault = 0;
					goto out;
				}
				goto dopanic;
			}
			ucode = a0;
			i = SIGSEGV;
#ifdef DEBUG
			printtrap(a0, a1, a2, entry, framep, 1, user);
#endif
			break;
		    }

		default:
			printf("trap: unknown MMCSR value 0x%lx\n", a1);
			goto dopanic;
		}
		break;

	default:
		goto dopanic;
	}

#ifdef DEBUG
	printtrap(a0, a1, a2, entry, framep, 1, user);
#endif
	framep->tf_regs[FRAME_TRAPARG_A0] = a0;
	framep->tf_regs[FRAME_TRAPARG_A1] = a1;
	framep->tf_regs[FRAME_TRAPARG_A2] = a2;
	trapsignal(td, i, ucode);
out:
	if (user) {
		framep->tf_regs[FRAME_SP] = alpha_pal_rdusp();
		userret(td, framep, sticks);
		mtx_assert(&Giant, MA_NOTOWNED);
	}
	return;

dopanic:
	printtrap(a0, a1, a2, entry, framep, 1, user);
	/* XXX dump registers */
#ifdef KDB
	kdb_trap(entry, a0, framep);
#endif
	panic("trap");
}

/*
 * Process a system call.
 *
 * System calls are strange beasts.  They are passed the syscall number
 * in v0, and the arguments in the registers (as normal).  They return
 * an error flag in a3 (if a3 != 0 on return, the syscall had an error),
 * and the return value (if any) in v0.
 *
 * The assembly stub takes care of moving the call number into a register
 * we can get to, and moves all of the argument registers into their places
 * in the trap frame.  On return, it restores the callee-saved registers,
 * a3, and v0 from the frame before returning to the user process.
 */
void
syscall(code, framep)
	u_int64_t code;
	struct trapframe *framep;
{
	struct sysent *callp;
	struct thread *td;
	struct proc *p;
	int error = 0;
	u_int64_t opc;
	u_int sticks;
	u_int64_t args[10];					/* XXX */
	u_int hidden = 0, nargs;
#ifdef SMP
	register_t s;
#endif

	/*
	 * Find our per-cpu globals.
	 */
#ifdef SMP
	s = intr_disable();
#endif
	pcpup = (struct pcpu *) alpha_pal_rdval();
	td = curthread;
#ifdef SMP
	td->td_md.md_kernnest++;
	intr_restore(s);
#endif
	p = td->td_proc;

	framep->tf_regs[FRAME_TRAPARG_A0] = 0;
	framep->tf_regs[FRAME_TRAPARG_A1] = 0;
	framep->tf_regs[FRAME_TRAPARG_A2] = 0;
#if notdef				/* can't happen, ever. */
	if ((framep->tf_regs[FRAME_PS] & ALPHA_PSL_USERMODE) == 0)
		panic("syscall");
#endif

	PCPU_LAZY_INC(cnt.v_syscall);
	td->td_frame = framep;
	opc = framep->tf_regs[FRAME_PC] - 4;
	sticks = td->td_sticks;
	if (td->td_ucred != p->p_ucred)
		cred_update_thread(td);
	if (p->p_flag & P_SA)
		thread_user_enter(td);
#ifdef DIAGNOSTIC
	alpha_fpstate_check(td);
#endif

	if (p->p_sysent->sv_prepsyscall) {
		/* (*p->p_sysent->sv_prepsyscall)(framep, args, &code, &params); */
		panic("prepsyscall");
	} else {
		/*
		 * syscall() and __syscall() are handled the same on
		 * the alpha, as everything is 64-bit aligned, anyway.
		 */
		if (code == SYS_syscall || code == SYS___syscall) {
			/*
			 * Code is first argument, followed by actual args.
			 */
			code = framep->tf_regs[FRAME_A0];
			hidden = 1;
		}
	}

 	if (p->p_sysent->sv_mask)
 		code &= p->p_sysent->sv_mask;

 	if (code >= p->p_sysent->sv_size)
 		callp = &p->p_sysent->sv_table[0];
  	else
 		callp = &p->p_sysent->sv_table[code];

	nargs = (callp->sy_narg & SYF_ARGMASK) + hidden;
	switch (nargs) {
	default:
		if (nargs > 10)		/* XXX */
			panic("syscall: too many args (%d)", nargs);
		error = copyin((caddr_t)(alpha_pal_rdusp()), &args[6],
		    (nargs - 6) * sizeof(u_int64_t));
	case 6:	
		args[5] = framep->tf_regs[FRAME_A5];
	case 5:	
		args[4] = framep->tf_regs[FRAME_A4];
	case 4:	
		args[3] = framep->tf_regs[FRAME_A3];
	case 3:	
		args[2] = framep->tf_regs[FRAME_A2];
	case 2:	
		args[1] = framep->tf_regs[FRAME_A1];
	case 1:	
		args[0] = framep->tf_regs[FRAME_A0];
	case 0:
		break;
	}

#ifdef KTRACE
	if (KTRPOINT(td, KTR_SYSCALL))
	    ktrsyscall(code, (callp->sy_narg & SYF_ARGMASK), args + hidden);
#endif
	/*
	 * Try to run the syscall without the MP lock if the syscall
	 * is MP safe
	 */
	if ((callp->sy_narg & SYF_MPSAFE) == 0)
		mtx_lock(&Giant);

	if (error == 0) {
		td->td_retval[0] = 0;
		td->td_retval[1] = 0;

		STOPEVENT(p, S_SCE, (callp->sy_narg & SYF_ARGMASK));

		PTRACESTOP_SC(p, td, S_PT_SCE);

		error = (*callp->sy_call)(td, args + hidden);
	}


	switch (error) {
	case 0:
		framep->tf_regs[FRAME_V0] = td->td_retval[0];
		framep->tf_regs[FRAME_A4] = td->td_retval[1];
		framep->tf_regs[FRAME_A3] = 0;
		break;
	case ERESTART:
		framep->tf_regs[FRAME_PC] = opc;
		break;
	case EJUSTRETURN:
		break;
	default:
		if (p->p_sysent->sv_errsize) {
			if (error >= p->p_sysent->sv_errsize)
				error = -1; /* XXX */
			else
				error = p->p_sysent->sv_errtbl[error];
		}
		framep->tf_regs[FRAME_V0] = error;
		framep->tf_regs[FRAME_A3] = 1;
		break;
	}

	/*
	 * Release Giant if we had to get it.
	 */
	if ((callp->sy_narg & SYF_MPSAFE) == 0)
		mtx_unlock(&Giant);

	userret(td, framep, sticks);
	
#ifdef KTRACE
	if (KTRPOINT(td, KTR_SYSRET))
		ktrsysret(code, error, td->td_retval[0]);
#endif
	/*
	 * This works because errno is findable through the
	 * register set.  If we ever support an emulation where this
	 * is not the case, this code will need to be revisited.
	 */
	STOPEVENT(p, S_SCX, code);

	PTRACESTOP_SC(p, td, S_PT_SCX);

	WITNESS_WARN(WARN_PANIC, NULL, "System call %s returning",
	    (code >= 0 && code < SYS_MAXSYSCALL) ? syscallnames[code] : "???");
	mtx_assert(&sched_lock, MA_NOTOWNED);
	mtx_assert(&Giant, MA_NOTOWNED);
}

/*
 * Unaligned access handler.  It's not clear that this can get much slower...
 *
 */
const static int reg_to_framereg[32] = {
	FRAME_V0,	FRAME_T0,	FRAME_T1,	FRAME_T2,
	FRAME_T3,	FRAME_T4,	FRAME_T5,	FRAME_T6,
	FRAME_T7,	FRAME_S0,	FRAME_S1,	FRAME_S2,
	FRAME_S3,	FRAME_S4,	FRAME_S5,	FRAME_S6,
	FRAME_A0,	FRAME_A1,	FRAME_A2,	FRAME_A3,
	FRAME_A4,	FRAME_A5,	FRAME_T8,	FRAME_T9,
	FRAME_T10,	FRAME_T11,	FRAME_RA,	FRAME_T12,
	FRAME_AT,	FRAME_GP,	FRAME_SP,	-1,
};

#define	irp(td, reg)							\
	((reg_to_framereg[(reg)] == -1) ? NULL :			\
	    &(td)->td_frame->tf_regs[reg_to_framereg[(reg)]])

#define	frp(td, reg)							\
	(&(td)->td_pcb->pcb_fp.fpr_regs[(reg)])

#define	unaligned_load(storage, ptrf, mod)				\
	if (copyin((caddr_t)va, &(storage), sizeof (storage)) == 0 &&	\
	    (regptr = ptrf(td, reg)) != NULL)				\
		signal = 0;						\
	else								\
		break;							\
	*regptr = mod (storage);

#define	unaligned_store(storage, ptrf, mod)				\
	if ((regptr = ptrf(td, reg)) == NULL)				\
		(storage) = 0;						\
	else								\
		(storage) = mod (*regptr);				\
	if (copyout(&(storage), (caddr_t)va, sizeof (storage)) == 0)	\
		signal = 0;						\
	else								\
		break;

#define	unaligned_load_integer(storage)					\
	unaligned_load(storage, irp, )

#define	unaligned_store_integer(storage)				\
	unaligned_store(storage, irp, )

#define	unaligned_load_floating(storage, mod)				\
	alpha_fpstate_save(td, 1);					\
	unaligned_load(storage, frp, mod)

#define	unaligned_store_floating(storage, mod)				\
	alpha_fpstate_save(td, 0);					\
	unaligned_store(storage, frp, mod)

unsigned long
Sfloat_to_reg(s)
	unsigned int s;
{
	unsigned long sign, expn, frac;
	unsigned long result;

	sign = (s & 0x80000000) >> 31;
	expn = (s & 0x7f800000) >> 23;
	frac = (s & 0x007fffff) >>  0;

	/* map exponent part, as appropriate. */
	if (expn == 0xff)
		expn = 0x7ff;
	else if ((expn & 0x80) != 0)
		expn = (0x400 | (expn & ~0x80));
	else if ((expn & 0x80) == 0 && expn != 0)
		expn = (0x380 | (expn & ~0x80));

	result = (sign << 63) | (expn << 52) | (frac << 29);
	return (result);
}

unsigned int
reg_to_Sfloat(r)
	unsigned long r;
{
	unsigned long sign, expn, frac;
	unsigned int result;

	sign = (r & 0x8000000000000000) >> 63;
	expn = (r & 0x7ff0000000000000) >> 52;
	frac = (r & 0x000fffffe0000000) >> 29;

	/* map exponent part, as appropriate. */
	expn = (expn & 0x7f) | ((expn & 0x400) != 0 ? 0x80 : 0x00);

	result = (sign << 31) | (expn << 23) | (frac << 0);
	return (result);
}

/*
 * Conversion of T floating datums to and from register format
 * requires no bit reordering whatsoever.
 */
unsigned long
Tfloat_reg_cvt(input)
	unsigned long input;
{

	return (input);
}

#ifdef FIX_UNALIGNED_VAX_FP
unsigned long
Ffloat_to_reg(f)
	unsigned int f;
{
	unsigned long sign, expn, frlo, frhi;
	unsigned long result;

	sign = (f & 0x00008000) >> 15;
	expn = (f & 0x00007f80) >>  7;
	frhi = (f & 0x0000007f) >>  0;
	frlo = (f & 0xffff0000) >> 16;

	/* map exponent part, as appropriate. */
	if ((expn & 0x80) != 0)
		expn = (0x400 | (expn & ~0x80));
	else if ((expn & 0x80) == 0 && expn != 0)
		expn = (0x380 | (expn & ~0x80));

	result = (sign << 63) | (expn << 52) | (frhi << 45) | (frlo << 29);
	return (result);
}

unsigned int
reg_to_Ffloat(r)
	unsigned long r;
{
	unsigned long sign, expn, frhi, frlo;
	unsigned int result;

	sign = (r & 0x8000000000000000) >> 63;
	expn = (r & 0x7ff0000000000000) >> 52;
	frhi = (r & 0x000fe00000000000) >> 45;
	frlo = (r & 0x00001fffe0000000) >> 29;

	/* map exponent part, as appropriate. */
	expn = (expn & 0x7f) | ((expn & 0x400) != 0 ? 0x80 : 0x00);

	result = (sign << 15) | (expn << 7) | (frhi << 0) | (frlo << 16);
	return (result);
}

/*
 * Conversion of G floating datums to and from register format is
 * symmetrical.  Just swap shorts in the quad...
 */
unsigned long
Gfloat_reg_cvt(input)
	unsigned long input;
{
	unsigned long a, b, c, d;
	unsigned long result;

	a = (input & 0x000000000000ffff) >> 0;
	b = (input & 0x00000000ffff0000) >> 16;
	c = (input & 0x0000ffff00000000) >> 32;
	d = (input & 0xffff000000000000) >> 48;

	result = (a << 48) | (b << 32) | (c << 16) | (d << 0);
	return (result);
}
#endif /* FIX_UNALIGNED_VAX_FP */

extern int	alpha_unaligned_print, alpha_unaligned_fix;
extern int	alpha_unaligned_sigbus;

struct unaligned_fixup_data {
	const char *type;	/* opcode name */
	int size;		/* size, 0 if fixup not supported */
};

int
unaligned_fixup(va, opcode, reg, td)
	unsigned long va, opcode, reg;
	struct thread *td;
{
	int doprint, dofix, dosigbus;
	int signal, size;
	const char *type;
	struct proc *p;
	unsigned long *regptr, longdata, uac;
	int intdata;		/* signed to get extension when storing */
	u_int16_t worddata;	/* unsigned to _avoid_ extension */
	const struct unaligned_fixup_data tab_0c[0x2] = {
		{ "ldwu",	2 },	{ "stw",	2 },
	};
	const struct unaligned_fixup_data tab_20[0x10] = {
#ifdef FIX_UNALIGNED_VAX_FP
		{ "ldf",	4 },	{ "ldg",	8 },
#else
		{ "ldf",	0 },	{ "ldg",	0 },
#endif
		{ "lds",	4 },	{ "ldt",	8 },
#ifdef FIX_UNALIGNED_VAX_FP
		{ "stf",	4 },	{ "stg",	8 },
#else
		{ "stf",	0 },	{ "stg",	0 },
#endif
		{ "sts",	4 },	{ "stt",	8 },
		{ "ldl",	4 },	{ "ldq",	8 },
		{ "ldl_l",	0 },	{ "ldq_l",	0 },	/* can't fix */
		{ "stl",	4 },	{ "stq",	8 },
		{ "stl_c",	0 },	{ "stq_c",	0 },	/* can't fix */
	};

	/*
	 * Figure out what actions to take.
	 *
	 */

	if (td) {
		p = td->td_proc;
		uac = p->p_md.md_uac;
	} else {
		uac = 0;
		p = NULL;
	}

	doprint = alpha_unaligned_print && !(uac & MDP_UAC_NOPRINT);
	dofix = alpha_unaligned_fix && !(uac & MDP_UAC_NOFIX);
	dosigbus = alpha_unaligned_sigbus | (uac & MDP_UAC_SIGBUS);

	/*
	 * Find out which opcode it is.  Arrange to have the opcode
	 * printed if it's an unknown opcode.
	 */
	if (opcode >= 0x0c && opcode <= 0x0d) {
		type = tab_0c[opcode - 0x0c].type;
		size = tab_0c[opcode - 0x0c].size;
	} else if (opcode >= 0x20 && opcode <= 0x2f) {
		type = tab_20[opcode - 0x20].type;
		size = tab_20[opcode - 0x20].size;
	} else {
		type = "0x%lx";
		size = 0;
	}

	/*
	 * See if the user can access the memory in question.
	 * Even if it's an unknown opcode, SEGV if the access
	 * should have failed.
	 */
	if (!useracc((caddr_t)va, size ? size : 1, VM_PROT_WRITE)) {
		signal = SIGSEGV;
		goto out;
	}

	/*
	 * If we're supposed to be noisy, squawk now.
	 */
	if (doprint) {
		uprintf(
		"pid %d (%s): unaligned access: va=0x%lx pc=0x%lx ra=0x%lx op=",
		    p->p_pid, p->p_comm, va, td->td_frame->tf_regs[FRAME_PC],
		    td->td_frame->tf_regs[FRAME_RA]);
		uprintf(type,opcode);
		uprintf("\n");
	}

	/*
	 * If we should try to fix it and know how, give it a shot.
	 *
	 * We never allow bad data to be unknowingly used by the
	 * user process.  That is, if we decide not to fix up an
	 * access we cause a SIGBUS rather than letting the user
	 * process go on without warning.
	 *
	 * If we're trying to do a fixup, we assume that things
	 * will be botched.  If everything works out OK, 
	 * unaligned_{load,store}_* clears the signal flag.
	 */
	signal = SIGBUS;
	if (dofix && size != 0) {
		switch (opcode) {
		case 0x0c:                      /* ldwu */
			/* XXX ONLY WORKS ON LITTLE-ENDIAN ALPHA */
			unaligned_load_integer(worddata);
			break;

		case 0x0d:                      /* stw */
			/* XXX ONLY WORKS ON LITTLE-ENDIAN ALPHA */
			unaligned_store_integer(worddata);
			break;
#ifdef FIX_UNALIGNED_VAX_FP
		case 0x20:			/* ldf */
			unaligned_load_floating(intdata, Ffloat_to_reg);
			break;

		case 0x21:			/* ldg */
			unaligned_load_floating(longdata, Gfloat_reg_cvt);
			break;
#endif

		case 0x22:			/* lds */
			unaligned_load_floating(intdata, Sfloat_to_reg);
			break;

		case 0x23:			/* ldt */
			unaligned_load_floating(longdata, Tfloat_reg_cvt);
			break;

#ifdef FIX_UNALIGNED_VAX_FP
		case 0x24:			/* stf */
			unaligned_store_floating(intdata, reg_to_Ffloat);
			break;

		case 0x25:			/* stg */
			unaligned_store_floating(longdata, Gfloat_reg_cvt);
			break;
#endif

		case 0x26:			/* sts */
			unaligned_store_floating(intdata, reg_to_Sfloat);
			break;

		case 0x27:			/* stt */
			unaligned_store_floating(longdata, Tfloat_reg_cvt);
			break;

		case 0x28:			/* ldl */
			unaligned_load_integer(intdata);
			break;

		case 0x29:			/* ldq */
			unaligned_load_integer(longdata);
			break;

		case 0x2c:			/* stl */
			unaligned_store_integer(intdata);
			break;

		case 0x2d:			/* stq */
			unaligned_store_integer(longdata);
			break;

#ifdef DIAGNOSTIC
		default:
			panic("unaligned_fixup: can't get here");
#endif
		}
	} 

	/*
	 * Force SIGBUS if requested.
	 */
	if (dosigbus)
		signal = SIGBUS;

out:
	return (signal);
}


/*
 * Reserved/unimplemented instruction (opDec fault) handler
 *
 * Argument is the process that caused it.  No useful information
 * is passed to the trap handler other than the fault type.  The
 * address of the instruction that caused the fault is 4 less than
 * the PC stored in the trap frame.
 *
 * If the instruction is emulated successfully, this function returns 0.
 * Otherwise, this function returns the signal to deliver to the process,
 * and fills in *ucodep with the code to be delivered.
 */
int
handle_opdec(td, ucodep)
	struct thread *td;
	u_int64_t *ucodep;
{
	alpha_instruction inst;
	register_t *regptr, memaddr;
	u_int64_t inst_pc;
	int sig;

	/*
	 * Read USP into frame in case it's going to be used or modified.
	 * This keeps us from having to check for it in lots of places
	 * later.
	 */
	td->td_frame->tf_regs[FRAME_SP] = alpha_pal_rdusp();

	inst_pc = memaddr = td->td_frame->tf_regs[FRAME_PC] - 4;
	if (copyin((caddr_t)inst_pc, &inst, sizeof (inst)) != 0) {
		/*
		 * really, this should never happen, but in case it
		 * does we handle it.
		 */
		printf("WARNING: handle_opdec() couldn't fetch instruction\n");
		goto sigsegv;
	}

	switch (inst.generic_format.opcode) {
	case op_ldbu:
	case op_ldwu:
	case op_stw:
	case op_stb:
		regptr = irp(td, inst.mem_format.rs);
		if (regptr != NULL)
			memaddr = *regptr;
		else
			memaddr = 0;
		memaddr += inst.mem_format.displacement;

		regptr = irp(td, inst.mem_format.rd);

		if (inst.mem_format.opcode == op_ldwu ||
		    inst.mem_format.opcode == op_stw) {
			if (memaddr & 0x01) {
				sig = unaligned_fixup(memaddr,
				    inst.mem_format.opcode,
				    inst.mem_format.rd, td);
				if (sig)
					goto unaligned_fixup_sig;
				break;
			}
		}

		if (inst.mem_format.opcode == op_ldbu) {
			u_int8_t b;

			/* XXX ONLY WORKS ON LITTLE-ENDIAN ALPHA */
			if (copyin((caddr_t)memaddr, &b, sizeof (b)) != 0)
				goto sigsegv;
			if (regptr != NULL)
				*regptr = b;
		} else if (inst.mem_format.opcode == op_ldwu) {
			u_int16_t w;

			/* XXX ONLY WORKS ON LITTLE-ENDIAN ALPHA */
			if (copyin((caddr_t)memaddr, &w, sizeof (w)) != 0)
				goto sigsegv;
			if (regptr != NULL)
				*regptr = w;
		} else if (inst.mem_format.opcode == op_stw) {
			u_int16_t w;

			/* XXX ONLY WORKS ON LITTLE-ENDIAN ALPHA */
			w = (regptr != NULL) ? *regptr : 0;
			if (copyout(&w, (caddr_t)memaddr, sizeof (w)) != 0)
				goto sigsegv;
		} else if (inst.mem_format.opcode == op_stb) {
			u_int8_t b;

			/* XXX ONLY WORKS ON LITTLE-ENDIAN ALPHA */
			b = (regptr != NULL) ? *regptr : 0;
			if (copyout(&b, (caddr_t)memaddr, sizeof (b)) != 0)
				goto sigsegv;
		}
		break;

	case op_intmisc:
		if (inst.operate_generic_format.function == op_sextb &&
		    inst.operate_generic_format.ra == 31) {
			int8_t b;

			if (inst.operate_generic_format.is_lit) {
				b = inst.operate_lit_format.literal;
			} else {
				if (inst.operate_reg_format.sbz != 0)
					goto sigill;
				regptr = irp(td, inst.operate_reg_format.rt);
				b = (regptr != NULL) ? *regptr : 0;
			}

			regptr = irp(td, inst.operate_generic_format.rc);
			if (regptr != NULL)
				*regptr = b;
			break;
		}
		if (inst.operate_generic_format.function == op_sextw &&
		    inst.operate_generic_format.ra == 31) {
			int16_t w;

			if (inst.operate_generic_format.is_lit) {
				w = inst.operate_lit_format.literal;
			} else {
				if (inst.operate_reg_format.sbz != 0)
					goto sigill;
				regptr = irp(td, inst.operate_reg_format.rt);
				w = (regptr != NULL) ? *regptr : 0;
			}

			regptr = irp(td, inst.operate_generic_format.rc);
			if (regptr != NULL)
				*regptr = w;
			break;
		}
		goto sigill;

	default:
		goto sigill;
	}

	/*
	 * Write back USP.  Note that in the error cases below,
	 * nothing will have been successfully modified so we don't
	 * have to write it out.
	 */
	alpha_pal_wrusp(td->td_frame->tf_regs[FRAME_SP]);

	return (0);

sigill:
	*ucodep = ALPHA_IF_CODE_OPDEC;			/* trap type */
	return (SIGILL);

sigsegv:
	sig = SIGSEGV;
	td->td_frame->tf_regs[FRAME_PC] = inst_pc;	/* re-run instr. */
unaligned_fixup_sig:
	*ucodep = memaddr;				/* faulting address */
	return (sig);
}
