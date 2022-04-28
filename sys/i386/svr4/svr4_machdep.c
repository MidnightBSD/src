/*-
 * Copyright (c) 1998 Mark Newton
 * Copyright (c) 1994 Christos Zoulas
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/exec.h>
#include <sys/filedesc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/signal.h>
#include <sys/signalvar.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/psl.h>
#include <machine/reg.h>
#include <machine/specialreg.h>
#include <machine/sysarch.h>
#include <machine/vm86.h>
#include <machine/vmparam.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <compat/svr4/svr4.h>
#include <compat/svr4/svr4_types.h>
#include <compat/svr4/svr4_signal.h>
#include <i386/svr4/svr4_machdep.h>
#include <compat/svr4/svr4_ucontext.h>
#include <compat/svr4/svr4_proto.h>
#include <compat/svr4/svr4_util.h>

#undef sigcode
#undef szsigcode

extern int svr4_szsigcode;
extern char svr4_sigcode[];

static void svr4_getsiginfo(union svr4_siginfo *, int, u_long, caddr_t);

#if !defined(__NetBSD__)
  /* taken from /sys/arch/i386/include/psl.h on NetBSD-1.3 */
# define PSL_MBZ 0xffc08028
# define PSL_USERSTATIC (PSL_USER | PSL_MBZ | PSL_IOPL | PSL_NT | PSL_VM | PSL_VIF | PSL_VIP)
# define USERMODE(c, f) (ISPL(c) == SEL_UPL)
#endif

#if defined(__NetBSD__)
void
svr4_setregs(td, epp, stack)
	struct thread *td;
	struct exec_package *epp;
	u_long stack;
{
	register struct pcb *pcb = td->td_pcb;

	pcb->pcb_savefpu.sv_env.en_cw = __SVR4_NPXCW__;
	setregs(td, epp, stack, 0UL);
}
#endif /* __NetBSD__ */

void
svr4_getcontext(td, uc, mask, oonstack)
	struct thread *td;
	struct svr4_ucontext *uc;
	sigset_t *mask;
	int oonstack;
{
	struct proc *p = td->td_proc;
	struct trapframe *tf = td->td_frame;
	svr4_greg_t *r = uc->uc_mcontext.greg;
	struct svr4_sigaltstack *s = &uc->uc_stack;
#if defined(DONE_MORE_SIGALTSTACK_WORK)
	struct sigacts *psp;
	struct sigaltstack *sf;
#endif

	PROC_LOCK(p);
#if defined(DONE_MORE_SIGALTSTACK_WORK)
	psp = p->p_sigacts;
	sf = &p->p_sigstk;
#endif

	memset(uc, 0, sizeof(struct svr4_ucontext));

	uc->uc_link = p->p_emuldata;
	/*
	 * Set the general purpose registers
	 */
#ifdef VM86
	if (tf->tf_eflags & PSL_VM) {
		r[SVR4_X86_GS] = tf->tf_vm86_gs;
		r[SVR4_X86_FS] = tf->tf_vm86_fs;
		r[SVR4_X86_ES] = tf->tf_vm86_es;
		r[SVR4_X86_DS] = tf->tf_vm86_ds;
		r[SVR4_X86_EFL] = get_vflags(td);
	} else
#endif
	{
#if defined(__NetBSD__)
	        __asm("movl %%gs,%w0" : "=r" (r[SVR4_X86_GS]));
		__asm("movl %%fs,%w0" : "=r" (r[SVR4_X86_FS]));
#else
	        r[SVR4_X86_GS] = rgs();
		r[SVR4_X86_FS] = tf->tf_fs;
#endif
		r[SVR4_X86_ES] = tf->tf_es;
		r[SVR4_X86_DS] = tf->tf_ds;
		r[SVR4_X86_EFL] = tf->tf_eflags;
	}
	r[SVR4_X86_EDI] = tf->tf_edi;
	r[SVR4_X86_ESI] = tf->tf_esi;
	r[SVR4_X86_EBP] = tf->tf_ebp;
	r[SVR4_X86_ESP] = tf->tf_esp;
	r[SVR4_X86_EBX] = tf->tf_ebx;
	r[SVR4_X86_EDX] = tf->tf_edx;
	r[SVR4_X86_ECX] = tf->tf_ecx;
	r[SVR4_X86_EAX] = tf->tf_eax;
	r[SVR4_X86_TRAPNO] = tf->tf_trapno;
	r[SVR4_X86_ERR] = tf->tf_err;
	r[SVR4_X86_EIP] = tf->tf_eip;
	r[SVR4_X86_CS] = tf->tf_cs;
	r[SVR4_X86_UESP] = 0;
	r[SVR4_X86_SS] = tf->tf_ss;

	/*
	 * Set the signal stack
	 */
#if defined(DONE_MORE_SIGALTSTACK_WORK)
	bsd_to_svr4_sigaltstack(sf, s);
#else
	s->ss_sp = (void *)rounddown2((u_long)tf->tf_esp, 16384);
	s->ss_size = 16384;
	s->ss_flags = 0;
#endif
	PROC_UNLOCK(p);

	/*
	 * Set the signal mask
	 */
	bsd_to_svr4_sigset(mask, &uc->uc_sigmask);

	/*
	 * Set the flags
	 */
	uc->uc_flags = SVR4_UC_SIGMASK|SVR4_UC_CPU|SVR4_UC_STACK;
}


/*
 * Set to ucontext specified. Reset signal mask and
 * stack state from context.
 * Return to previous pc and psl as specified by
 * context left by sendsig. Check carefully to
 * make sure that the user has not modified the
 * psl to gain improper privileges or to cause
 * a machine fault.
 */
int
svr4_setcontext(td, uc)
	struct thread *td;
	struct svr4_ucontext *uc;
{
#if defined(DONE_MORE_SIGALTSTACK_WORK)
	struct sigacts *psp;
#endif
	struct proc *p = td->td_proc;
	register struct trapframe *tf;
	svr4_greg_t *r = uc->uc_mcontext.greg;
	struct svr4_sigaltstack *s = &uc->uc_stack;
	struct sigaltstack *sf;
	sigset_t mask;

	PROC_LOCK(p);
#if defined(DONE_MORE_SIGALTSTACK_WORK)
	psp = p->p_sigacts;
#endif
	sf = &td->td_sigstk;

	/*
	 * XXX:
	 * Should we check the value of flags to determine what to restore?
	 * What to do with uc_link?
	 * What to do with floating point stuff?
	 * Should we bother with the rest of the registers that we
	 * set to 0 right now?
	 */

	if ((uc->uc_flags & SVR4_UC_CPU) == 0) {
		PROC_UNLOCK(p);
		return 0;
	}

	DPRINTF(("svr4_setcontext(%d)\n", p->p_pid));

	tf = td->td_frame;

	/*
	 * Restore register context.
	 */
#ifdef VM86
#warning "VM86 doesn't work yet, please don't try to use it."
	if (r[SVR4_X86_EFL] & PSL_VM) {
		tf->tf_vm86_gs = r[SVR4_X86_GS];
		tf->tf_vm86_fs = r[SVR4_X86_FS];
		tf->tf_vm86_es = r[SVR4_X86_ES];
		tf->tf_vm86_ds = r[SVR4_X86_DS];
		set_vflags(td, r[SVR4_X86_EFL]);
	} else
#endif
	{
		/*
		 * Check for security violations.  If we're returning to
		 * protected mode, the CPU will validate the segment registers
		 * automatically and generate a trap on violations.  We handle
		 * the trap, rather than doing all of the checking here.
		 */
		if (((r[SVR4_X86_EFL] ^ tf->tf_eflags) & PSL_USERSTATIC) != 0 ||
		    !USERMODE(r[SVR4_X86_CS], r[SVR4_X86_EFL])) {
			PROC_UNLOCK(p);
			return (EINVAL);
		}

#if defined(__NetBSD__)
		/* %fs and %gs were restored by the trampoline. */
#else
		/* %gs was restored by the trampoline. */
		tf->tf_fs = r[SVR4_X86_FS];
#endif
		tf->tf_es = r[SVR4_X86_ES];
		tf->tf_ds = r[SVR4_X86_DS];
		tf->tf_eflags = r[SVR4_X86_EFL];
	}
	tf->tf_edi = r[SVR4_X86_EDI];
	tf->tf_esi = r[SVR4_X86_ESI];
	tf->tf_ebp = r[SVR4_X86_EBP];
	tf->tf_ebx = r[SVR4_X86_EBX];
	tf->tf_edx = r[SVR4_X86_EDX];
	tf->tf_ecx = r[SVR4_X86_ECX];
	tf->tf_eax = r[SVR4_X86_EAX];
	tf->tf_trapno = r[SVR4_X86_TRAPNO];
	tf->tf_err = r[SVR4_X86_ERR];
	tf->tf_eip = r[SVR4_X86_EIP];
	tf->tf_cs = r[SVR4_X86_CS];
	tf->tf_ss = r[SVR4_X86_SS];
	tf->tf_esp = r[SVR4_X86_ESP];

	p->p_emuldata = uc->uc_link;
	/*
	 * restore signal stack
	 */
	if (uc->uc_flags & SVR4_UC_STACK) {
		svr4_to_bsd_sigaltstack(s, sf);
	}

	/*
	 * restore signal mask
	 */
	if (uc->uc_flags & SVR4_UC_SIGMASK) {
#if defined(DEBUG_SVR4)
		{
			int i;
			for (i = 0; i < 4; i++)
				DPRINTF(("\tuc_sigmask[%d] = %lx\n", i,
						uc->uc_sigmask.bits[i]));
		}
#endif
		svr4_to_bsd_sigset(&uc->uc_sigmask, &mask);
		SIG_CANTMASK(mask);
		td->td_sigmask = mask;
		signotify(td);
	}
	PROC_UNLOCK(p);

	return 0; /*EJUSTRETURN;*/
}


static void
svr4_getsiginfo(si, sig, code, addr)
	union svr4_siginfo	*si;
	int			 sig;
	u_long			 code;
	caddr_t			 addr;
{
	si->svr4_si_signo = bsd_to_svr4_sig[sig];
	si->svr4_si_errno = 0;
	si->svr4_si_addr  = addr;

	switch (code) {
	case T_PRIVINFLT:
		si->svr4_si_code = SVR4_ILL_PRVOPC;
		si->svr4_si_trap = SVR4_T_PRIVINFLT;
		break;

	case T_BPTFLT:
		si->svr4_si_code = SVR4_TRAP_BRKPT;
		si->svr4_si_trap = SVR4_T_BPTFLT;
		break;

	case T_ARITHTRAP:
		si->svr4_si_code = SVR4_FPE_INTOVF;
		si->svr4_si_trap = SVR4_T_DIVIDE;
		break;

	case T_PROTFLT:
		si->svr4_si_code = SVR4_SEGV_ACCERR;
		si->svr4_si_trap = SVR4_T_PROTFLT;
		break;

	case T_TRCTRAP:
		si->svr4_si_code = SVR4_TRAP_TRACE;
		si->svr4_si_trap = SVR4_T_TRCTRAP;
		break;

	case T_PAGEFLT:
		si->svr4_si_code = SVR4_SEGV_ACCERR;
		si->svr4_si_trap = SVR4_T_PAGEFLT;
		break;

	case T_ALIGNFLT:
		si->svr4_si_code = SVR4_BUS_ADRALN;
		si->svr4_si_trap = SVR4_T_ALIGNFLT;
		break;

	case T_DIVIDE:
		si->svr4_si_code = SVR4_FPE_FLTDIV;
		si->svr4_si_trap = SVR4_T_DIVIDE;
		break;

	case T_OFLOW:
		si->svr4_si_code = SVR4_FPE_FLTOVF;
		si->svr4_si_trap = SVR4_T_DIVIDE;
		break;

	case T_BOUND:
		si->svr4_si_code = SVR4_FPE_FLTSUB;
		si->svr4_si_trap = SVR4_T_BOUND;
		break;

	case T_DNA:
		si->svr4_si_code = SVR4_FPE_FLTINV;
		si->svr4_si_trap = SVR4_T_DNA;
		break;

	case T_FPOPFLT:
		si->svr4_si_code = SVR4_FPE_FLTINV;
		si->svr4_si_trap = SVR4_T_FPOPFLT;
		break;

	case T_SEGNPFLT:
		si->svr4_si_code = SVR4_SEGV_MAPERR;
		si->svr4_si_trap = SVR4_T_SEGNPFLT;
		break;

	case T_STKFLT:
		si->svr4_si_code = SVR4_ILL_BADSTK;
		si->svr4_si_trap = SVR4_T_STKFLT;
		break;

	default:
		si->svr4_si_code = 0;
		si->svr4_si_trap = 0;
#if defined(DEBUG_SVR4)
		printf("sig %d code %ld\n", sig, code);
/*		panic("svr4_getsiginfo");*/
#endif
		break;
	}
}


/*
 * Send an interrupt to process.
 *
 * Stack is set up to allow sigcode stored
 * in u. to call routine. After the handler is
 * done svr4 will call setcontext for us
 * with the user context we just set up, and we
 * will return to the user pc, psl.
 */
void
svr4_sendsig(catcher, ksi, mask)
	sig_t catcher;
	ksiginfo_t *ksi;
	sigset_t *mask;
{
	register struct thread *td = curthread;
	struct proc *p = td->td_proc;
	register struct trapframe *tf;
	struct svr4_sigframe *fp, frame;
	struct sigacts *psp;
	int oonstack;
	int sig;
	int code;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	sig = ksi->ksi_signo;
#if defined(DEBUG_SVR4)
	printf("svr4_sendsig(%d)\n", sig);
#endif
	code = ksi->ksi_trapno; /* use trap No. */
	psp = p->p_sigacts;
	mtx_assert(&psp->ps_mtx, MA_OWNED);

	tf = td->td_frame;
	oonstack = sigonstack(tf->tf_esp);

	/*
	 * Allocate space for the signal handler context.
	 */
	if ((td->td_pflags & TDP_ALTSTACK) && !oonstack &&
	    SIGISMEMBER(psp->ps_sigonstack, sig)) {
		fp = (struct svr4_sigframe *)((uintptr_t)td->td_sigstk.ss_sp +
		    td->td_sigstk.ss_size - sizeof(struct svr4_sigframe));
		td->td_sigstk.ss_flags |= SS_ONSTACK;
	} else {
		fp = (struct svr4_sigframe *)tf->tf_esp - 1;
	}
	mtx_unlock(&psp->ps_mtx);
	PROC_UNLOCK(p);

	/* 
	 * Build the argument list for the signal handler.
	 * Notes:
	 * 	- we always build the whole argument list, even when we
	 *	  don't need to [when SA_SIGINFO is not set, we don't need
	 *	  to pass all sf_si and sf_uc]
	 *	- we don't pass the correct signal address [we need to
	 *	  modify many kernel files to enable that]
	 */

	svr4_getcontext(td, &frame.sf_uc, mask, oonstack);
#if defined(DEBUG_SVR4)
	printf("obtained ucontext\n");
#endif
	svr4_getsiginfo(&frame.sf_si, sig, code, (caddr_t) tf->tf_eip);
#if defined(DEBUG_SVR4)
	printf("obtained siginfo\n");
#endif
	frame.sf_signum = frame.sf_si.svr4_si_signo;
	frame.sf_sip = &fp->sf_si;
	frame.sf_ucp = &fp->sf_uc;
	frame.sf_handler = catcher;
#if defined(DEBUG_SVR4)
	printf("sig = %d, sip %p, ucp = %p, handler = %p\n", 
	       frame.sf_signum, frame.sf_sip, frame.sf_ucp, frame.sf_handler);
#endif

	if (copyout(&frame, fp, sizeof(frame)) != 0) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
		PROC_LOCK(p);
		sigexit(td, SIGILL);
		/* NOTREACHED */
	}
#if defined(__NetBSD__)
	/*
	 * Build context to run handler in.
	 */
	tf->tf_es = GSEL(GUSERLDT_SEL, SEL_UPL);
	tf->tf_ds = GSEL(GUSERLDT_SEL, SEL_UPL);
	tf->tf_eip = (int)(((char *)PS_STRINGS) -
	     svr4_szsigcode);
	tf->tf_cs = GSEL(GUSERLDT_SEL, SEL_UPL);

	tf->tf_eflags &= ~(PSL_T|PSL_VM|PSL_AC|PSL_D);
	tf->tf_esp = (int)fp;
	tf->tf_ss = GSEL(GUSERLDT_SEL, SEL_UPL);
#else
	tf->tf_esp = (int)fp;
	tf->tf_eip = (int)(((char *)PS_STRINGS) - *(p->p_sysent->sv_szsigcode));
	tf->tf_eflags &= ~(PSL_T | PSL_D);
	tf->tf_cs = _ucodesel;
	tf->tf_ds = _udatasel;
	tf->tf_es = _udatasel;
	tf->tf_fs = _udatasel;
	load_gs(_udatasel);
	tf->tf_ss = _udatasel;
	PROC_LOCK(p);
	mtx_lock(&psp->ps_mtx);
#endif
}



int
svr4_sys_sysarch(td, v)
	struct thread *td;
	struct svr4_sys_sysarch_args *v;
{
	struct svr4_sys_sysarch_args *uap = v;
#if 0	/* USER_LDT */
#if defined(__NetBSD__)
	caddr_t sg = stackgap_init(p->p_emul);
#else
	caddr_t sg = stackgap_init();
#endif
	int error;
#endif

	switch (uap->op) {
	case SVR4_SYSARCH_FPHW:
		return 0;

	case SVR4_SYSARCH_DSCR:
#if 0	/* USER_LDT */
#warning "USER_LDT doesn't work - are you sure you want this?"
		{
			struct i386_set_ldt_args sa, *sap;
			struct sys_sysarch_args ua;

			struct svr4_ssd ssd;
			union descriptor bsd;

			if ((error = copyin(uap->a1, &ssd,
					    sizeof(ssd))) != 0) {
				printf("Cannot copy arg1\n");
				return error;
			}

			printf("s=%x, b=%x, l=%x, a1=%x a2=%x\n",
			       ssd.selector, ssd.base, ssd.limit,
			       ssd.access1, ssd.access2);

			/* We can only set ldt's for now. */
			if (!ISLDT(ssd.selector)) {
				printf("Not an ldt\n");
				return EPERM;
			}

			/* Oh, well we don't cleanup either */
			if (ssd.access1 == 0)
				return 0;

			bsd.sd.sd_lobase = ssd.base & 0xffffff;
			bsd.sd.sd_hibase = (ssd.base >> 24) & 0xff;

			bsd.sd.sd_lolimit = ssd.limit & 0xffff;
			bsd.sd.sd_hilimit = (ssd.limit >> 16) & 0xf;

			bsd.sd.sd_type = ssd.access1 & 0x1f;
			bsd.sd.sd_dpl =  (ssd.access1 >> 5) & 0x3;
			bsd.sd.sd_p = (ssd.access1 >> 7) & 0x1;

			bsd.sd.sd_xx = ssd.access2 & 0x3;
			bsd.sd.sd_def32 = (ssd.access2 >> 2) & 0x1;
			bsd.sd.sd_gran = (ssd.access2 >> 3)& 0x1;

			sa.start = IDXSEL(ssd.selector);
			sa.desc = stackgap_alloc(&sg, sizeof(union descriptor));
			sa.num = 1;
			sap = stackgap_alloc(&sg,
					     sizeof(struct i386_set_ldt_args));

			if ((error = copyout(&sa, sap, sizeof(sa))) != 0) {
				printf("Cannot copyout args\n");
				return error;
			}

			ua.op = I386_SET_LDT;
			ua.parms = (char *) sap;

			if ((error = copyout(&bsd, sa.desc, sizeof(bsd))) != 0) {
				printf("Cannot copyout desc\n");
				return error;
			}

			return sys_sysarch(td, &ua, retval);
		}
#endif

	default:
		printf("svr4_sysarch(%d), a1 %p\n", uap->op,
		       uap->a1);
		return 0;
	}
}
