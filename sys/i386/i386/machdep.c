/*-
 * Copyright (c) 1992 Terrence R. Lambert.
 * Copyright (c) 1982, 1987, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)machdep.c	7.4 (Berkeley) 6/3/91
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_apic.h"
#include "opt_atalk.h"
#include "opt_atpic.h"
#include "opt_compat.h"
#include "opt_cpu.h"
#include "opt_ddb.h"
#include "opt_inet.h"
#include "opt_ipx.h"
#include "opt_isa.h"
#include "opt_kstack_pages.h"
#include "opt_maxmem.h"
#include "opt_mp_watchdog.h"
#include "opt_npx.h"
#include "opt_perfmon.h"
#include "opt_xbox.h"
#include "opt_kdtrace.h"

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/bus.h>
#include <sys/callout.h>
#include <sys/cons.h>
#include <sys/cpu.h>
#include <sys/eventhandler.h>
#include <sys/exec.h>
#include <sys/imgact.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/msgbuf.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/ptrace.h>
#include <sys/reboot.h>
#include <sys/sched.h>
#include <sys/signalvar.h>
#ifdef SMP
#include <sys/smp.h>
#endif
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/ucontext.h>
#include <sys/vmmeter.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_pager.h>
#include <vm/vm_param.h>

#ifdef DDB
#ifndef KDB
#error KDB must be enabled in order for DDB to work!
#endif
#include <ddb/ddb.h>
#include <ddb/db_sym.h>
#endif

#include <isa/rtc.h>

#include <net/netisr.h>

#include <machine/bootinfo.h>
#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/cputypes.h>
#include <machine/intr_machdep.h>
#include <x86/mca.h>
#include <machine/md_var.h>
#include <machine/metadata.h>
#include <machine/mp_watchdog.h>
#include <machine/pc/bios.h>
#include <machine/pcb.h>
#include <machine/pcb_ext.h>
#include <machine/proc.h>
#include <machine/reg.h>
#include <machine/sigframe.h>
#include <machine/specialreg.h>
#include <machine/vm86.h>
#ifdef PERFMON
#include <machine/perfmon.h>
#endif
#ifdef SMP
#include <machine/smp.h>
#endif

#ifdef DEV_APIC
#include <machine/apicvar.h>
#endif

#ifdef DEV_ISA
#include <x86/isa/icu.h>
#endif

#ifdef XBOX
#include <machine/xbox.h>

int arch_i386_is_xbox = 0;
uint32_t arch_i386_xbox_memsize = 0;
#endif

#ifdef XEN
/* XEN includes */
#include <machine/xen/xen-os.h>
#include <xen/hypervisor.h>
#include <machine/xen/xen-os.h>
#include <machine/xen/xenvar.h>
#include <machine/xen/xenfunc.h>
#include <xen/xen_intr.h>

void Xhypervisor_callback(void);
void failsafe_callback(void);

extern trap_info_t trap_table[];
struct proc_ldt default_proc_ldt;
extern int init_first;
int running_xen = 1;
extern unsigned long physfree;
#endif /* XEN */

/* Sanity check for __curthread() */
CTASSERT(offsetof(struct pcpu, pc_curthread) == 0);

extern void init386(int first);
extern void dblfault_handler(void);

extern void printcpuinfo(void);	/* XXX header file */
extern void finishidentcpu(void);
extern void panicifcpuunsupported(void);

#define	CS_SECURE(cs)		(ISPL(cs) == SEL_UPL)
#define	EFL_SECURE(ef, oef)	((((ef) ^ (oef)) & ~PSL_USERCHANGE) == 0)

#if !defined(CPU_DISABLE_SSE) && defined(I686_CPU)
#define CPU_ENABLE_SSE
#endif

static void cpu_startup(void *);
static void fpstate_drop(struct thread *td);
static void get_fpcontext(struct thread *td, mcontext_t *mcp);
static int  set_fpcontext(struct thread *td, const mcontext_t *mcp);
#ifdef CPU_ENABLE_SSE
static void set_fpregs_xmm(struct save87 *, struct savexmm *);
static void fill_fpregs_xmm(struct savexmm *, struct save87 *);
#endif /* CPU_ENABLE_SSE */
SYSINIT(cpu, SI_SUB_CPU, SI_ORDER_FIRST, cpu_startup, NULL);

#ifdef DDB
extern vm_offset_t ksym_start, ksym_end;
#endif

/* Intel ICH registers */
#define ICH_PMBASE	0x400
#define ICH_SMI_EN	ICH_PMBASE + 0x30

int	_udatasel, _ucodesel;
u_int	basemem;

int cold = 1;

#ifdef COMPAT_43
static void osendsig(sig_t catcher, ksiginfo_t *, sigset_t *mask);
#endif
#ifdef COMPAT_FREEBSD4
static void freebsd4_sendsig(sig_t catcher, ksiginfo_t *, sigset_t *mask);
#endif

long Maxmem = 0;
long realmem = 0;

#ifdef PAE
FEATURE(pae, "Physical Address Extensions");
#endif

/*
 * The number of PHYSMAP entries must be one less than the number of
 * PHYSSEG entries because the PHYSMAP entry that spans the largest
 * physical address that is accessible by ISA DMA is split into two
 * PHYSSEG entries.
 */
#define	PHYSMAP_SIZE	(2 * (VM_PHYSSEG_MAX - 1))

vm_paddr_t phys_avail[PHYSMAP_SIZE + 2];
vm_paddr_t dump_avail[PHYSMAP_SIZE + 2];

/* must be 2 less so 0 0 can signal end of chunks */
#define PHYS_AVAIL_ARRAY_END ((sizeof(phys_avail) / sizeof(phys_avail[0])) - 2)
#define DUMP_AVAIL_ARRAY_END ((sizeof(dump_avail) / sizeof(dump_avail[0])) - 2)

struct kva_md_info kmi;

static struct trapframe proc0_tf;
struct pcpu __pcpu[MAXCPU];

struct mtx icu_lock;

static void
cpu_startup(dummy)
	void *dummy;
{
	uintmax_t memsize;
	char *sysenv;
	
	/*
	 * On MacBooks, we need to disallow the legacy USB circuit to
	 * generate an SMI# because this can cause several problems,
	 * namely: incorrect CPU frequency detection and failure to
	 * start the APs.
	 * We do this by disabling a bit in the SMI_EN (SMI Control and
	 * Enable register) of the Intel ICH LPC Interface Bridge.
	 */
	sysenv = getenv("smbios.system.product");
	if (sysenv != NULL) {
		if (strncmp(sysenv, "MacBook1,1", 10) == 0 ||
		    strncmp(sysenv, "MacBook3,1", 10) == 0 ||
		    strncmp(sysenv, "MacBookPro1,1", 13) == 0 ||
		    strncmp(sysenv, "MacBookPro1,2", 13) == 0 ||
		    strncmp(sysenv, "MacBookPro3,1", 13) == 0 ||
		    strncmp(sysenv, "Macmini1,1", 10) == 0) {
			if (bootverbose)
				printf("Disabling LEGACY_USB_EN bit on "
				    "Intel ICH.\n");
			outl(ICH_SMI_EN, inl(ICH_SMI_EN) & ~0x8);
		}
		freeenv(sysenv);
	}

	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	startrtclock();
	printcpuinfo();
	panicifcpuunsupported();
#ifdef PERFMON
	perfmon_init();
#endif
	realmem = Maxmem;

	/*
	 * Display physical memory if SMBIOS reports reasonable amount.
	 */
	memsize = 0;
	sysenv = getenv("smbios.memory.enabled");
	if (sysenv != NULL) {
		memsize = (uintmax_t)strtoul(sysenv, (char **)NULL, 10) << 10;
		freeenv(sysenv);
	}
	if (memsize < ptoa((uintmax_t)cnt.v_free_count))
		memsize = ptoa((uintmax_t)Maxmem);
	printf("real memory  = %ju (%ju MB)\n", memsize, memsize >> 20);

	/*
	 * Display any holes after the first chunk of extended memory.
	 */
	if (bootverbose) {
		int indx;

		printf("Physical memory chunk(s):\n");
		for (indx = 0; phys_avail[indx + 1] != 0; indx += 2) {
			vm_paddr_t size;

			size = phys_avail[indx + 1] - phys_avail[indx];
			printf(
			    "0x%016jx - 0x%016jx, %ju bytes (%ju pages)\n",
			    (uintmax_t)phys_avail[indx],
			    (uintmax_t)phys_avail[indx + 1] - 1,
			    (uintmax_t)size, (uintmax_t)size / PAGE_SIZE);
		}
	}

	vm_ksubmap_init(&kmi);

	printf("avail memory = %ju (%ju MB)\n",
	    ptoa((uintmax_t)cnt.v_free_count),
	    ptoa((uintmax_t)cnt.v_free_count) / 1048576);

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */
	bufinit();
	vm_pager_bufferinit();
#ifndef XEN
	cpu_setregs();
#endif

	/*
	 * Add BSP as an interrupt target.
	 */
	intr_add_cpu(0);
}

/*
 * Send an interrupt to process.
 *
 * Stack is set up to allow sigcode stored
 * at top to call routine, followed by kcall
 * to sigreturn routine below.  After sigreturn
 * resets the signal mask, the stack, and the
 * frame pointer, it returns to the user
 * specified pc, psl.
 */
#ifdef COMPAT_43
static void
osendsig(sig_t catcher, ksiginfo_t *ksi, sigset_t *mask)
{
	struct osigframe sf, *fp;
	struct proc *p;
	struct thread *td;
	struct sigacts *psp;
	struct trapframe *regs;
	int sig;
	int oonstack;

	td = curthread;
	p = td->td_proc;
	PROC_LOCK_ASSERT(p, MA_OWNED);
	sig = ksi->ksi_signo;
	psp = p->p_sigacts;
	mtx_assert(&psp->ps_mtx, MA_OWNED);
	regs = td->td_frame;
	oonstack = sigonstack(regs->tf_esp);

	/* Allocate space for the signal handler context. */
	if ((td->td_pflags & TDP_ALTSTACK) && !oonstack &&
	    SIGISMEMBER(psp->ps_sigonstack, sig)) {
		fp = (struct osigframe *)(td->td_sigstk.ss_sp +
		    td->td_sigstk.ss_size - sizeof(struct osigframe));
#if defined(COMPAT_43)
		td->td_sigstk.ss_flags |= SS_ONSTACK;
#endif
	} else
		fp = (struct osigframe *)regs->tf_esp - 1;

	/* Translate the signal if appropriate. */
	if (p->p_sysent->sv_sigtbl && sig <= p->p_sysent->sv_sigsize)
		sig = p->p_sysent->sv_sigtbl[_SIG_IDX(sig)];

	/* Build the argument list for the signal handler. */
	sf.sf_signum = sig;
	sf.sf_scp = (register_t)&fp->sf_siginfo.si_sc;
	bzero(&sf.sf_siginfo, sizeof(sf.sf_siginfo));
	if (SIGISMEMBER(psp->ps_siginfo, sig)) {
		/* Signal handler installed with SA_SIGINFO. */
		sf.sf_arg2 = (register_t)&fp->sf_siginfo;
		sf.sf_siginfo.si_signo = sig;
		sf.sf_siginfo.si_code = ksi->ksi_code;
		sf.sf_ahu.sf_action = (__osiginfohandler_t *)catcher;
		sf.sf_addr = 0;
	} else {
		/* Old FreeBSD-style arguments. */
		sf.sf_arg2 = ksi->ksi_code;
		sf.sf_addr = (register_t)ksi->ksi_addr;
		sf.sf_ahu.sf_handler = catcher;
	}
	mtx_unlock(&psp->ps_mtx);
	PROC_UNLOCK(p);

	/* Save most if not all of trap frame. */
	sf.sf_siginfo.si_sc.sc_eax = regs->tf_eax;
	sf.sf_siginfo.si_sc.sc_ebx = regs->tf_ebx;
	sf.sf_siginfo.si_sc.sc_ecx = regs->tf_ecx;
	sf.sf_siginfo.si_sc.sc_edx = regs->tf_edx;
	sf.sf_siginfo.si_sc.sc_esi = regs->tf_esi;
	sf.sf_siginfo.si_sc.sc_edi = regs->tf_edi;
	sf.sf_siginfo.si_sc.sc_cs = regs->tf_cs;
	sf.sf_siginfo.si_sc.sc_ds = regs->tf_ds;
	sf.sf_siginfo.si_sc.sc_ss = regs->tf_ss;
	sf.sf_siginfo.si_sc.sc_es = regs->tf_es;
	sf.sf_siginfo.si_sc.sc_fs = regs->tf_fs;
	sf.sf_siginfo.si_sc.sc_gs = rgs();
	sf.sf_siginfo.si_sc.sc_isp = regs->tf_isp;

	/* Build the signal context to be used by osigreturn(). */
	sf.sf_siginfo.si_sc.sc_onstack = (oonstack) ? 1 : 0;
	SIG2OSIG(*mask, sf.sf_siginfo.si_sc.sc_mask);
	sf.sf_siginfo.si_sc.sc_sp = regs->tf_esp;
	sf.sf_siginfo.si_sc.sc_fp = regs->tf_ebp;
	sf.sf_siginfo.si_sc.sc_pc = regs->tf_eip;
	sf.sf_siginfo.si_sc.sc_ps = regs->tf_eflags;
	sf.sf_siginfo.si_sc.sc_trapno = regs->tf_trapno;
	sf.sf_siginfo.si_sc.sc_err = regs->tf_err;

	/*
	 * If we're a vm86 process, we want to save the segment registers.
	 * We also change eflags to be our emulated eflags, not the actual
	 * eflags.
	 */
	if (regs->tf_eflags & PSL_VM) {
		/* XXX confusing names: `tf' isn't a trapframe; `regs' is. */
		struct trapframe_vm86 *tf = (struct trapframe_vm86 *)regs;
		struct vm86_kernel *vm86 = &td->td_pcb->pcb_ext->ext_vm86;

		sf.sf_siginfo.si_sc.sc_gs = tf->tf_vm86_gs;
		sf.sf_siginfo.si_sc.sc_fs = tf->tf_vm86_fs;
		sf.sf_siginfo.si_sc.sc_es = tf->tf_vm86_es;
		sf.sf_siginfo.si_sc.sc_ds = tf->tf_vm86_ds;

		if (vm86->vm86_has_vme == 0)
			sf.sf_siginfo.si_sc.sc_ps =
			    (tf->tf_eflags & ~(PSL_VIF | PSL_VIP)) |
			    (vm86->vm86_eflags & (PSL_VIF | PSL_VIP));

		/* See sendsig() for comments. */
		tf->tf_eflags &= ~(PSL_VM | PSL_NT | PSL_VIF | PSL_VIP);
	}

	/*
	 * Copy the sigframe out to the user's stack.
	 */
	if (copyout(&sf, fp, sizeof(*fp)) != 0) {
#ifdef DEBUG
		printf("process %ld has trashed its stack\n", (long)p->p_pid);
#endif
		PROC_LOCK(p);
		sigexit(td, SIGILL);
	}

	regs->tf_esp = (int)fp;
	regs->tf_eip = PS_STRINGS - szosigcode;
	regs->tf_eflags &= ~(PSL_T | PSL_D);
	regs->tf_cs = _ucodesel;
	regs->tf_ds = _udatasel;
	regs->tf_es = _udatasel;
	regs->tf_fs = _udatasel;
	load_gs(_udatasel);
	regs->tf_ss = _udatasel;
	PROC_LOCK(p);
	mtx_lock(&psp->ps_mtx);
}
#endif /* COMPAT_43 */

#ifdef COMPAT_FREEBSD4
static void
freebsd4_sendsig(sig_t catcher, ksiginfo_t *ksi, sigset_t *mask)
{
	struct sigframe4 sf, *sfp;
	struct proc *p;
	struct thread *td;
	struct sigacts *psp;
	struct trapframe *regs;
	int sig;
	int oonstack;

	td = curthread;
	p = td->td_proc;
	PROC_LOCK_ASSERT(p, MA_OWNED);
	sig = ksi->ksi_signo;
	psp = p->p_sigacts;
	mtx_assert(&psp->ps_mtx, MA_OWNED);
	regs = td->td_frame;
	oonstack = sigonstack(regs->tf_esp);

	/* Save user context. */
	bzero(&sf, sizeof(sf));
	sf.sf_uc.uc_sigmask = *mask;
	sf.sf_uc.uc_stack = td->td_sigstk;
	sf.sf_uc.uc_stack.ss_flags = (td->td_pflags & TDP_ALTSTACK)
	    ? ((oonstack) ? SS_ONSTACK : 0) : SS_DISABLE;
	sf.sf_uc.uc_mcontext.mc_onstack = (oonstack) ? 1 : 0;
	sf.sf_uc.uc_mcontext.mc_gs = rgs();
	bcopy(regs, &sf.sf_uc.uc_mcontext.mc_fs, sizeof(*regs));
	bzero(sf.sf_uc.uc_mcontext.mc_fpregs,
	    sizeof(sf.sf_uc.uc_mcontext.mc_fpregs));
	bzero(sf.sf_uc.uc_mcontext.__spare__,
	    sizeof(sf.sf_uc.uc_mcontext.__spare__));
	bzero(sf.sf_uc.__spare__, sizeof(sf.sf_uc.__spare__));

	/* Allocate space for the signal handler context. */
	if ((td->td_pflags & TDP_ALTSTACK) != 0 && !oonstack &&
	    SIGISMEMBER(psp->ps_sigonstack, sig)) {
		sfp = (struct sigframe4 *)(td->td_sigstk.ss_sp +
		    td->td_sigstk.ss_size - sizeof(struct sigframe4));
#if defined(COMPAT_43)
		td->td_sigstk.ss_flags |= SS_ONSTACK;
#endif
	} else
		sfp = (struct sigframe4 *)regs->tf_esp - 1;

	/* Translate the signal if appropriate. */
	if (p->p_sysent->sv_sigtbl && sig <= p->p_sysent->sv_sigsize)
		sig = p->p_sysent->sv_sigtbl[_SIG_IDX(sig)];

	/* Build the argument list for the signal handler. */
	sf.sf_signum = sig;
	sf.sf_ucontext = (register_t)&sfp->sf_uc;
	bzero(&sf.sf_si, sizeof(sf.sf_si));
	if (SIGISMEMBER(psp->ps_siginfo, sig)) {
		/* Signal handler installed with SA_SIGINFO. */
		sf.sf_siginfo = (register_t)&sfp->sf_si;
		sf.sf_ahu.sf_action = (__siginfohandler_t *)catcher;

		/* Fill in POSIX parts */
		sf.sf_si.si_signo = sig;
		sf.sf_si.si_code = ksi->ksi_code;
		sf.sf_si.si_addr = ksi->ksi_addr;
	} else {
		/* Old FreeBSD-style arguments. */
		sf.sf_siginfo = ksi->ksi_code;
		sf.sf_addr = (register_t)ksi->ksi_addr;
		sf.sf_ahu.sf_handler = catcher;
	}
	mtx_unlock(&psp->ps_mtx);
	PROC_UNLOCK(p);

	/*
	 * If we're a vm86 process, we want to save the segment registers.
	 * We also change eflags to be our emulated eflags, not the actual
	 * eflags.
	 */
	if (regs->tf_eflags & PSL_VM) {
		struct trapframe_vm86 *tf = (struct trapframe_vm86 *)regs;
		struct vm86_kernel *vm86 = &td->td_pcb->pcb_ext->ext_vm86;

		sf.sf_uc.uc_mcontext.mc_gs = tf->tf_vm86_gs;
		sf.sf_uc.uc_mcontext.mc_fs = tf->tf_vm86_fs;
		sf.sf_uc.uc_mcontext.mc_es = tf->tf_vm86_es;
		sf.sf_uc.uc_mcontext.mc_ds = tf->tf_vm86_ds;

		if (vm86->vm86_has_vme == 0)
			sf.sf_uc.uc_mcontext.mc_eflags =
			    (tf->tf_eflags & ~(PSL_VIF | PSL_VIP)) |
			    (vm86->vm86_eflags & (PSL_VIF | PSL_VIP));

		/*
		 * Clear PSL_NT to inhibit T_TSSFLT faults on return from
		 * syscalls made by the signal handler.  This just avoids
		 * wasting time for our lazy fixup of such faults.  PSL_NT
		 * does nothing in vm86 mode, but vm86 programs can set it
		 * almost legitimately in probes for old cpu types.
		 */
		tf->tf_eflags &= ~(PSL_VM | PSL_NT | PSL_VIF | PSL_VIP);
	}

	/*
	 * Copy the sigframe out to the user's stack.
	 */
	if (copyout(&sf, sfp, sizeof(*sfp)) != 0) {
#ifdef DEBUG
		printf("process %ld has trashed its stack\n", (long)p->p_pid);
#endif
		PROC_LOCK(p);
		sigexit(td, SIGILL);
	}

	regs->tf_esp = (int)sfp;
	regs->tf_eip = PS_STRINGS - szfreebsd4_sigcode;
	regs->tf_eflags &= ~(PSL_T | PSL_D);
	regs->tf_cs = _ucodesel;
	regs->tf_ds = _udatasel;
	regs->tf_es = _udatasel;
	regs->tf_fs = _udatasel;
	regs->tf_ss = _udatasel;
	PROC_LOCK(p);
	mtx_lock(&psp->ps_mtx);
}
#endif	/* COMPAT_FREEBSD4 */

void
sendsig(sig_t catcher, ksiginfo_t *ksi, sigset_t *mask)
{
	struct sigframe sf, *sfp;
	struct proc *p;
	struct thread *td;
	struct sigacts *psp;
	char *sp;
	struct trapframe *regs;
	struct segment_descriptor *sdp;
	int sig;
	int oonstack;

	td = curthread;
	p = td->td_proc;
	PROC_LOCK_ASSERT(p, MA_OWNED);
	sig = ksi->ksi_signo;
	psp = p->p_sigacts;
	mtx_assert(&psp->ps_mtx, MA_OWNED);
#ifdef COMPAT_FREEBSD4
	if (SIGISMEMBER(psp->ps_freebsd4, sig)) {
		freebsd4_sendsig(catcher, ksi, mask);
		return;
	}
#endif
#ifdef COMPAT_43
	if (SIGISMEMBER(psp->ps_osigset, sig)) {
		osendsig(catcher, ksi, mask);
		return;
	}
#endif
	regs = td->td_frame;
	oonstack = sigonstack(regs->tf_esp);

	/* Save user context. */
	bzero(&sf, sizeof(sf));
	sf.sf_uc.uc_sigmask = *mask;
	sf.sf_uc.uc_stack = td->td_sigstk;
	sf.sf_uc.uc_stack.ss_flags = (td->td_pflags & TDP_ALTSTACK)
	    ? ((oonstack) ? SS_ONSTACK : 0) : SS_DISABLE;
	sf.sf_uc.uc_mcontext.mc_onstack = (oonstack) ? 1 : 0;
	sf.sf_uc.uc_mcontext.mc_gs = rgs();
	bcopy(regs, &sf.sf_uc.uc_mcontext.mc_fs, sizeof(*regs));
	sf.sf_uc.uc_mcontext.mc_len = sizeof(sf.sf_uc.uc_mcontext); /* magic */
	get_fpcontext(td, &sf.sf_uc.uc_mcontext);
	fpstate_drop(td);
	/*
	 * Unconditionally fill the fsbase and gsbase into the mcontext.
	 */
	sdp = &td->td_pcb->pcb_fsd;
	sf.sf_uc.uc_mcontext.mc_fsbase = sdp->sd_hibase << 24 |
	    sdp->sd_lobase;
	sdp = &td->td_pcb->pcb_gsd;
	sf.sf_uc.uc_mcontext.mc_gsbase = sdp->sd_hibase << 24 |
	    sdp->sd_lobase;
	sf.sf_uc.uc_mcontext.mc_flags = 0;
	bzero(sf.sf_uc.uc_mcontext.mc_spare2,
	    sizeof(sf.sf_uc.uc_mcontext.mc_spare2));
	bzero(sf.sf_uc.__spare__, sizeof(sf.sf_uc.__spare__));

	/* Allocate space for the signal handler context. */
	if ((td->td_pflags & TDP_ALTSTACK) != 0 && !oonstack &&
	    SIGISMEMBER(psp->ps_sigonstack, sig)) {
		sp = td->td_sigstk.ss_sp +
		    td->td_sigstk.ss_size - sizeof(struct sigframe);
#if defined(COMPAT_43)
		td->td_sigstk.ss_flags |= SS_ONSTACK;
#endif
	} else
		sp = (char *)regs->tf_esp - sizeof(struct sigframe);
	/* Align to 16 bytes. */
	sfp = (struct sigframe *)((unsigned int)sp & ~0xF);

	/* Translate the signal if appropriate. */
	if (p->p_sysent->sv_sigtbl && sig <= p->p_sysent->sv_sigsize)
		sig = p->p_sysent->sv_sigtbl[_SIG_IDX(sig)];

	/* Build the argument list for the signal handler. */
	sf.sf_signum = sig;
	sf.sf_ucontext = (register_t)&sfp->sf_uc;
	bzero(&sf.sf_si, sizeof(sf.sf_si));
	if (SIGISMEMBER(psp->ps_siginfo, sig)) {
		/* Signal handler installed with SA_SIGINFO. */
		sf.sf_siginfo = (register_t)&sfp->sf_si;
		sf.sf_ahu.sf_action = (__siginfohandler_t *)catcher;

		/* Fill in POSIX parts */
		sf.sf_si = ksi->ksi_info;
		sf.sf_si.si_signo = sig; /* maybe a translated signal */
	} else {
		/* Old FreeBSD-style arguments. */
		sf.sf_siginfo = ksi->ksi_code;
		sf.sf_addr = (register_t)ksi->ksi_addr;
		sf.sf_ahu.sf_handler = catcher;
	}
	mtx_unlock(&psp->ps_mtx);
	PROC_UNLOCK(p);

	/*
	 * If we're a vm86 process, we want to save the segment registers.
	 * We also change eflags to be our emulated eflags, not the actual
	 * eflags.
	 */
	if (regs->tf_eflags & PSL_VM) {
		struct trapframe_vm86 *tf = (struct trapframe_vm86 *)regs;
		struct vm86_kernel *vm86 = &td->td_pcb->pcb_ext->ext_vm86;

		sf.sf_uc.uc_mcontext.mc_gs = tf->tf_vm86_gs;
		sf.sf_uc.uc_mcontext.mc_fs = tf->tf_vm86_fs;
		sf.sf_uc.uc_mcontext.mc_es = tf->tf_vm86_es;
		sf.sf_uc.uc_mcontext.mc_ds = tf->tf_vm86_ds;

		if (vm86->vm86_has_vme == 0)
			sf.sf_uc.uc_mcontext.mc_eflags =
			    (tf->tf_eflags & ~(PSL_VIF | PSL_VIP)) |
			    (vm86->vm86_eflags & (PSL_VIF | PSL_VIP));

		/*
		 * Clear PSL_NT to inhibit T_TSSFLT faults on return from
		 * syscalls made by the signal handler.  This just avoids
		 * wasting time for our lazy fixup of such faults.  PSL_NT
		 * does nothing in vm86 mode, but vm86 programs can set it
		 * almost legitimately in probes for old cpu types.
		 */
		tf->tf_eflags &= ~(PSL_VM | PSL_NT | PSL_VIF | PSL_VIP);
	}

	/*
	 * Copy the sigframe out to the user's stack.
	 */
	if (copyout(&sf, sfp, sizeof(*sfp)) != 0) {
#ifdef DEBUG
		printf("process %ld has trashed its stack\n", (long)p->p_pid);
#endif
		PROC_LOCK(p);
		sigexit(td, SIGILL);
	}

	regs->tf_esp = (int)sfp;
	regs->tf_eip = PS_STRINGS - *(p->p_sysent->sv_szsigcode);
	regs->tf_eflags &= ~(PSL_T | PSL_D);
	regs->tf_cs = _ucodesel;
	regs->tf_ds = _udatasel;
	regs->tf_es = _udatasel;
	regs->tf_fs = _udatasel;
	regs->tf_ss = _udatasel;
	PROC_LOCK(p);
	mtx_lock(&psp->ps_mtx);
}

/*
 * System call to cleanup state after a signal
 * has been taken.  Reset signal mask and
 * stack state from context left by sendsig (above).
 * Return to previous pc and psl as specified by
 * context left by sendsig. Check carefully to
 * make sure that the user has not modified the
 * state to gain improper privileges.
 *
 * MPSAFE
 */
#ifdef COMPAT_43
int
osigreturn(td, uap)
	struct thread *td;
	struct osigreturn_args /* {
		struct osigcontext *sigcntxp;
	} */ *uap;
{
	struct osigcontext sc;
	struct trapframe *regs;
	struct osigcontext *scp;
	int eflags, error;
	ksiginfo_t ksi;

	regs = td->td_frame;
	error = copyin(uap->sigcntxp, &sc, sizeof(sc));
	if (error != 0)
		return (error);
	scp = &sc;
	eflags = scp->sc_ps;
	if (eflags & PSL_VM) {
		struct trapframe_vm86 *tf = (struct trapframe_vm86 *)regs;
		struct vm86_kernel *vm86;

		/*
		 * if pcb_ext == 0 or vm86_inited == 0, the user hasn't
		 * set up the vm86 area, and we can't enter vm86 mode.
		 */
		if (td->td_pcb->pcb_ext == 0)
			return (EINVAL);
		vm86 = &td->td_pcb->pcb_ext->ext_vm86;
		if (vm86->vm86_inited == 0)
			return (EINVAL);

		/* Go back to user mode if both flags are set. */
		if ((eflags & PSL_VIP) && (eflags & PSL_VIF)) {
			ksiginfo_init_trap(&ksi);
			ksi.ksi_signo = SIGBUS;
			ksi.ksi_code = BUS_OBJERR;
			ksi.ksi_addr = (void *)regs->tf_eip;
			trapsignal(td, &ksi);
		}

		if (vm86->vm86_has_vme) {
			eflags = (tf->tf_eflags & ~VME_USERCHANGE) |
			    (eflags & VME_USERCHANGE) | PSL_VM;
		} else {
			vm86->vm86_eflags = eflags;	/* save VIF, VIP */
			eflags = (tf->tf_eflags & ~VM_USERCHANGE) |
			    (eflags & VM_USERCHANGE) | PSL_VM;
		}
		tf->tf_vm86_ds = scp->sc_ds;
		tf->tf_vm86_es = scp->sc_es;
		tf->tf_vm86_fs = scp->sc_fs;
		tf->tf_vm86_gs = scp->sc_gs;
		tf->tf_ds = _udatasel;
		tf->tf_es = _udatasel;
		tf->tf_fs = _udatasel;
	} else {
		/*
		 * Don't allow users to change privileged or reserved flags.
		 */
		/*
		 * XXX do allow users to change the privileged flag PSL_RF.
		 * The cpu sets PSL_RF in tf_eflags for faults.  Debuggers
		 * should sometimes set it there too.  tf_eflags is kept in
		 * the signal context during signal handling and there is no
		 * other place to remember it, so the PSL_RF bit may be
		 * corrupted by the signal handler without us knowing.
		 * Corruption of the PSL_RF bit at worst causes one more or
		 * one less debugger trap, so allowing it is fairly harmless.
		 */
		if (!EFL_SECURE(eflags & ~PSL_RF, regs->tf_eflags & ~PSL_RF)) {
	    		return (EINVAL);
		}

		/*
		 * Don't allow users to load a valid privileged %cs.  Let the
		 * hardware check for invalid selectors, excess privilege in
		 * other selectors, invalid %eip's and invalid %esp's.
		 */
		if (!CS_SECURE(scp->sc_cs)) {
			ksiginfo_init_trap(&ksi);
			ksi.ksi_signo = SIGBUS;
			ksi.ksi_code = BUS_OBJERR;
			ksi.ksi_trapno = T_PROTFLT;
			ksi.ksi_addr = (void *)regs->tf_eip;
			trapsignal(td, &ksi);
			return (EINVAL);
		}
		regs->tf_ds = scp->sc_ds;
		regs->tf_es = scp->sc_es;
		regs->tf_fs = scp->sc_fs;
	}

	/* Restore remaining registers. */
	regs->tf_eax = scp->sc_eax;
	regs->tf_ebx = scp->sc_ebx;
	regs->tf_ecx = scp->sc_ecx;
	regs->tf_edx = scp->sc_edx;
	regs->tf_esi = scp->sc_esi;
	regs->tf_edi = scp->sc_edi;
	regs->tf_cs = scp->sc_cs;
	regs->tf_ss = scp->sc_ss;
	regs->tf_isp = scp->sc_isp;
	regs->tf_ebp = scp->sc_fp;
	regs->tf_esp = scp->sc_sp;
	regs->tf_eip = scp->sc_pc;
	regs->tf_eflags = eflags;

#if defined(COMPAT_43)
	if (scp->sc_onstack & 1)
		td->td_sigstk.ss_flags |= SS_ONSTACK;
	else
		td->td_sigstk.ss_flags &= ~SS_ONSTACK;
#endif
	kern_sigprocmask(td, SIG_SETMASK, (sigset_t *)&scp->sc_mask, NULL,
	    SIGPROCMASK_OLD);
	return (EJUSTRETURN);
}
#endif /* COMPAT_43 */

#ifdef COMPAT_FREEBSD4
/*
 * MPSAFE
 */
int
freebsd4_sigreturn(td, uap)
	struct thread *td;
	struct freebsd4_sigreturn_args /* {
		const ucontext4 *sigcntxp;
	} */ *uap;
{
	struct ucontext4 uc;
	struct trapframe *regs;
	struct ucontext4 *ucp;
	int cs, eflags, error;
	ksiginfo_t ksi;

	error = copyin(uap->sigcntxp, &uc, sizeof(uc));
	if (error != 0)
		return (error);
	ucp = &uc;
	regs = td->td_frame;
	eflags = ucp->uc_mcontext.mc_eflags;
	if (eflags & PSL_VM) {
		struct trapframe_vm86 *tf = (struct trapframe_vm86 *)regs;
		struct vm86_kernel *vm86;

		/*
		 * if pcb_ext == 0 or vm86_inited == 0, the user hasn't
		 * set up the vm86 area, and we can't enter vm86 mode.
		 */
		if (td->td_pcb->pcb_ext == 0)
			return (EINVAL);
		vm86 = &td->td_pcb->pcb_ext->ext_vm86;
		if (vm86->vm86_inited == 0)
			return (EINVAL);

		/* Go back to user mode if both flags are set. */
		if ((eflags & PSL_VIP) && (eflags & PSL_VIF)) {
			ksiginfo_init_trap(&ksi);
			ksi.ksi_signo = SIGBUS;
			ksi.ksi_code = BUS_OBJERR;
			ksi.ksi_addr = (void *)regs->tf_eip;
			trapsignal(td, &ksi);
		}
		if (vm86->vm86_has_vme) {
			eflags = (tf->tf_eflags & ~VME_USERCHANGE) |
			    (eflags & VME_USERCHANGE) | PSL_VM;
		} else {
			vm86->vm86_eflags = eflags;	/* save VIF, VIP */
			eflags = (tf->tf_eflags & ~VM_USERCHANGE) |
			    (eflags & VM_USERCHANGE) | PSL_VM;
		}
		bcopy(&ucp->uc_mcontext.mc_fs, tf, sizeof(struct trapframe));
		tf->tf_eflags = eflags;
		tf->tf_vm86_ds = tf->tf_ds;
		tf->tf_vm86_es = tf->tf_es;
		tf->tf_vm86_fs = tf->tf_fs;
		tf->tf_vm86_gs = ucp->uc_mcontext.mc_gs;
		tf->tf_ds = _udatasel;
		tf->tf_es = _udatasel;
		tf->tf_fs = _udatasel;
	} else {
		/*
		 * Don't allow users to change privileged or reserved flags.
		 */
		/*
		 * XXX do allow users to change the privileged flag PSL_RF.
		 * The cpu sets PSL_RF in tf_eflags for faults.  Debuggers
		 * should sometimes set it there too.  tf_eflags is kept in
		 * the signal context during signal handling and there is no
		 * other place to remember it, so the PSL_RF bit may be
		 * corrupted by the signal handler without us knowing.
		 * Corruption of the PSL_RF bit at worst causes one more or
		 * one less debugger trap, so allowing it is fairly harmless.
		 */
		if (!EFL_SECURE(eflags & ~PSL_RF, regs->tf_eflags & ~PSL_RF)) {
			uprintf("pid %d (%s): freebsd4_sigreturn eflags = 0x%x\n",
			    td->td_proc->p_pid, td->td_name, eflags);
	    		return (EINVAL);
		}

		/*
		 * Don't allow users to load a valid privileged %cs.  Let the
		 * hardware check for invalid selectors, excess privilege in
		 * other selectors, invalid %eip's and invalid %esp's.
		 */
		cs = ucp->uc_mcontext.mc_cs;
		if (!CS_SECURE(cs)) {
			uprintf("pid %d (%s): freebsd4_sigreturn cs = 0x%x\n",
			    td->td_proc->p_pid, td->td_name, cs);
			ksiginfo_init_trap(&ksi);
			ksi.ksi_signo = SIGBUS;
			ksi.ksi_code = BUS_OBJERR;
			ksi.ksi_trapno = T_PROTFLT;
			ksi.ksi_addr = (void *)regs->tf_eip;
			trapsignal(td, &ksi);
			return (EINVAL);
		}

		bcopy(&ucp->uc_mcontext.mc_fs, regs, sizeof(*regs));
	}

#if defined(COMPAT_43)
	if (ucp->uc_mcontext.mc_onstack & 1)
		td->td_sigstk.ss_flags |= SS_ONSTACK;
	else
		td->td_sigstk.ss_flags &= ~SS_ONSTACK;
#endif
	kern_sigprocmask(td, SIG_SETMASK, &ucp->uc_sigmask, NULL, 0);
	return (EJUSTRETURN);
}
#endif	/* COMPAT_FREEBSD4 */

/*
 * MPSAFE
 */
int
sys_sigreturn(td, uap)
	struct thread *td;
	struct sigreturn_args /* {
		const struct __ucontext *sigcntxp;
	} */ *uap;
{
	ucontext_t uc;
	struct trapframe *regs;
	ucontext_t *ucp;
	int cs, eflags, error, ret;
	ksiginfo_t ksi;

	error = copyin(uap->sigcntxp, &uc, sizeof(uc));
	if (error != 0)
		return (error);
	ucp = &uc;
	regs = td->td_frame;
	eflags = ucp->uc_mcontext.mc_eflags;
	if (eflags & PSL_VM) {
		struct trapframe_vm86 *tf = (struct trapframe_vm86 *)regs;
		struct vm86_kernel *vm86;

		/*
		 * if pcb_ext == 0 or vm86_inited == 0, the user hasn't
		 * set up the vm86 area, and we can't enter vm86 mode.
		 */
		if (td->td_pcb->pcb_ext == 0)
			return (EINVAL);
		vm86 = &td->td_pcb->pcb_ext->ext_vm86;
		if (vm86->vm86_inited == 0)
			return (EINVAL);

		/* Go back to user mode if both flags are set. */
		if ((eflags & PSL_VIP) && (eflags & PSL_VIF)) {
			ksiginfo_init_trap(&ksi);
			ksi.ksi_signo = SIGBUS;
			ksi.ksi_code = BUS_OBJERR;
			ksi.ksi_addr = (void *)regs->tf_eip;
			trapsignal(td, &ksi);
		}

		if (vm86->vm86_has_vme) {
			eflags = (tf->tf_eflags & ~VME_USERCHANGE) |
			    (eflags & VME_USERCHANGE) | PSL_VM;
		} else {
			vm86->vm86_eflags = eflags;	/* save VIF, VIP */
			eflags = (tf->tf_eflags & ~VM_USERCHANGE) |
			    (eflags & VM_USERCHANGE) | PSL_VM;
		}
		bcopy(&ucp->uc_mcontext.mc_fs, tf, sizeof(struct trapframe));
		tf->tf_eflags = eflags;
		tf->tf_vm86_ds = tf->tf_ds;
		tf->tf_vm86_es = tf->tf_es;
		tf->tf_vm86_fs = tf->tf_fs;
		tf->tf_vm86_gs = ucp->uc_mcontext.mc_gs;
		tf->tf_ds = _udatasel;
		tf->tf_es = _udatasel;
		tf->tf_fs = _udatasel;
	} else {
		/*
		 * Don't allow users to change privileged or reserved flags.
		 */
		/*
		 * XXX do allow users to change the privileged flag PSL_RF.
		 * The cpu sets PSL_RF in tf_eflags for faults.  Debuggers
		 * should sometimes set it there too.  tf_eflags is kept in
		 * the signal context during signal handling and there is no
		 * other place to remember it, so the PSL_RF bit may be
		 * corrupted by the signal handler without us knowing.
		 * Corruption of the PSL_RF bit at worst causes one more or
		 * one less debugger trap, so allowing it is fairly harmless.
		 */
		if (!EFL_SECURE(eflags & ~PSL_RF, regs->tf_eflags & ~PSL_RF)) {
			uprintf("pid %d (%s): sigreturn eflags = 0x%x\n",
			    td->td_proc->p_pid, td->td_name, eflags);
	    		return (EINVAL);
		}

		/*
		 * Don't allow users to load a valid privileged %cs.  Let the
		 * hardware check for invalid selectors, excess privilege in
		 * other selectors, invalid %eip's and invalid %esp's.
		 */
		cs = ucp->uc_mcontext.mc_cs;
		if (!CS_SECURE(cs)) {
			uprintf("pid %d (%s): sigreturn cs = 0x%x\n",
			    td->td_proc->p_pid, td->td_name, cs);
			ksiginfo_init_trap(&ksi);
			ksi.ksi_signo = SIGBUS;
			ksi.ksi_code = BUS_OBJERR;
			ksi.ksi_trapno = T_PROTFLT;
			ksi.ksi_addr = (void *)regs->tf_eip;
			trapsignal(td, &ksi);
			return (EINVAL);
		}

		ret = set_fpcontext(td, &ucp->uc_mcontext);
		if (ret != 0)
			return (ret);
		bcopy(&ucp->uc_mcontext.mc_fs, regs, sizeof(*regs));
	}

#if defined(COMPAT_43)
	if (ucp->uc_mcontext.mc_onstack & 1)
		td->td_sigstk.ss_flags |= SS_ONSTACK;
	else
		td->td_sigstk.ss_flags &= ~SS_ONSTACK;
#endif

	kern_sigprocmask(td, SIG_SETMASK, &ucp->uc_sigmask, NULL, 0);
	return (EJUSTRETURN);
}

/*
 * Machine dependent boot() routine
 *
 * I haven't seen anything to put here yet
 * Possibly some stuff might be grafted back here from boot()
 */
void
cpu_boot(int howto)
{
}

/*
 * Flush the D-cache for non-DMA I/O so that the I-cache can
 * be made coherent later.
 */
void
cpu_flush_dcache(void *ptr, size_t len)
{
	/* Not applicable */
}

/* Get current clock frequency for the given cpu id. */
int
cpu_est_clockrate(int cpu_id, uint64_t *rate)
{
	uint64_t tsc1, tsc2;
	uint64_t acnt, mcnt, perf;
	register_t reg;

	if (pcpu_find(cpu_id) == NULL || rate == NULL)
		return (EINVAL);
	if ((cpu_feature & CPUID_TSC) == 0)
		return (EOPNOTSUPP);

	/*
	 * If TSC is P-state invariant and APERF/MPERF MSRs do not exist,
	 * DELAY(9) based logic fails.
	 */
	if (tsc_is_invariant && !tsc_perf_stat)
		return (EOPNOTSUPP);

#ifdef SMP
	if (smp_cpus > 1) {
		/* Schedule ourselves on the indicated cpu. */
		thread_lock(curthread);
		sched_bind(curthread, cpu_id);
		thread_unlock(curthread);
	}
#endif

	/* Calibrate by measuring a short delay. */
	reg = intr_disable();
	if (tsc_is_invariant) {
		wrmsr(MSR_MPERF, 0);
		wrmsr(MSR_APERF, 0);
		tsc1 = rdtsc();
		DELAY(1000);
		mcnt = rdmsr(MSR_MPERF);
		acnt = rdmsr(MSR_APERF);
		tsc2 = rdtsc();
		intr_restore(reg);
		perf = 1000 * acnt / mcnt;
		*rate = (tsc2 - tsc1) * perf;
	} else {
		tsc1 = rdtsc();
		DELAY(1000);
		tsc2 = rdtsc();
		intr_restore(reg);
		*rate = (tsc2 - tsc1) * 1000;
	}

#ifdef SMP
	if (smp_cpus > 1) {
		thread_lock(curthread);
		sched_unbind(curthread);
		thread_unlock(curthread);
	}
#endif

	return (0);
}

#ifdef XEN

void
cpu_halt(void)
{
	HYPERVISOR_shutdown(SHUTDOWN_poweroff);
}

int scheduler_running;

static void
cpu_idle_hlt(int busy)
{

	scheduler_running = 1;
	enable_intr();
	idle_block();
}

#else
/*
 * Shutdown the CPU as much as possible
 */
void
cpu_halt(void)
{
	for (;;)
		__asm__ ("hlt");
}

#endif

void (*cpu_idle_hook)(void) = NULL;	/* ACPI idle hook. */
static int	cpu_ident_amdc1e = 0;	/* AMD C1E supported. */
static int	idle_mwait = 1;		/* Use MONITOR/MWAIT for short idle. */
TUNABLE_INT("machdep.idle_mwait", &idle_mwait);
SYSCTL_INT(_machdep, OID_AUTO, idle_mwait, CTLFLAG_RW, &idle_mwait,
    0, "Use MONITOR/MWAIT for short idle");

#define	STATE_RUNNING	0x0
#define	STATE_MWAIT	0x1
#define	STATE_SLEEPING	0x2

static void
cpu_idle_acpi(int busy)
{
	int *state;

	state = (int *)PCPU_PTR(monitorbuf);
	*state = STATE_SLEEPING;
	disable_intr();
	if (sched_runnable())
		enable_intr();
	else if (cpu_idle_hook)
		cpu_idle_hook();
	else
		__asm __volatile("sti; hlt");
	*state = STATE_RUNNING;
}

#ifndef XEN
static void
cpu_idle_hlt(int busy)
{
	int *state;

	state = (int *)PCPU_PTR(monitorbuf);
	*state = STATE_SLEEPING;
	/*
	 * We must absolutely guarentee that hlt is the next instruction
	 * after sti or we introduce a timing window.
	 */
	disable_intr();
	if (sched_runnable())
		enable_intr();
	else
		__asm __volatile("sti; hlt");
	*state = STATE_RUNNING;
}
#endif

/*
 * MWAIT cpu power states.  Lower 4 bits are sub-states.
 */
#define	MWAIT_C0	0xf0
#define	MWAIT_C1	0x00
#define	MWAIT_C2	0x10
#define	MWAIT_C3	0x20
#define	MWAIT_C4	0x30

static void
cpu_idle_mwait(int busy)
{
	int *state;

	state = (int *)PCPU_PTR(monitorbuf);
	*state = STATE_MWAIT;
	if (!sched_runnable()) {
		cpu_monitor(state, 0, 0);
		if (*state == STATE_MWAIT)
			cpu_mwait(0, MWAIT_C1);
	}
	*state = STATE_RUNNING;
}

static void
cpu_idle_spin(int busy)
{
	int *state;
	int i;

	state = (int *)PCPU_PTR(monitorbuf);
	*state = STATE_RUNNING;
	for (i = 0; i < 1000; i++) {
		if (sched_runnable())
			return;
		cpu_spinwait();
	}
}

/*
 * C1E renders the local APIC timer dead, so we disable it by
 * reading the Interrupt Pending Message register and clearing
 * both C1eOnCmpHalt (bit 28) and SmiOnCmpHalt (bit 27).
 * 
 * Reference:
 *   "BIOS and Kernel Developer's Guide for AMD NPT Family 0Fh Processors"
 *   #32559 revision 3.00+
 */
#define	MSR_AMDK8_IPM		0xc0010055
#define	AMDK8_SMIONCMPHALT	(1ULL << 27)
#define	AMDK8_C1EONCMPHALT	(1ULL << 28)
#define	AMDK8_CMPHALT		(AMDK8_SMIONCMPHALT | AMDK8_C1EONCMPHALT)

static void
cpu_probe_amdc1e(void)
{

	/*
	 * Detect the presence of C1E capability mostly on latest
	 * dual-cores (or future) k8 family.
	 */
	if (cpu_vendor_id == CPU_VENDOR_AMD &&
	    (cpu_id & 0x00000f00) == 0x00000f00 &&
	    (cpu_id & 0x0fff0000) >=  0x00040000) {
		cpu_ident_amdc1e = 1;
	}
}

#ifdef XEN
void (*cpu_idle_fn)(int) = cpu_idle_hlt;
#else
void (*cpu_idle_fn)(int) = cpu_idle_acpi;
#endif

void
cpu_idle(int busy)
{
#ifndef XEN
	uint64_t msr;
#endif

	CTR2(KTR_SPARE2, "cpu_idle(%d) at %d",
	    busy, curcpu);
#if defined(MP_WATCHDOG) && !defined(XEN)
	ap_watchdog(PCPU_GET(cpuid));
#endif
#ifndef XEN
	/* If we are busy - try to use fast methods. */
	if (busy) {
		if ((cpu_feature2 & CPUID2_MON) && idle_mwait) {
			cpu_idle_mwait(busy);
			goto out;
		}
	}
#endif

	/* If we have time - switch timers into idle mode. */
	if (!busy) {
		critical_enter();
		cpu_idleclock();
	}

#ifndef XEN
	/* Apply AMD APIC timer C1E workaround. */
	if (cpu_ident_amdc1e && cpu_disable_deep_sleep) {
		msr = rdmsr(MSR_AMDK8_IPM);
		if (msr & AMDK8_CMPHALT)
			wrmsr(MSR_AMDK8_IPM, msr & ~AMDK8_CMPHALT);
	}
#endif

	/* Call main idle method. */
	cpu_idle_fn(busy);

	/* Switch timers mack into active mode. */
	if (!busy) {
		cpu_activeclock();
		critical_exit();
	}
#ifndef XEN
out:
#endif
	CTR2(KTR_SPARE2, "cpu_idle(%d) at %d done",
	    busy, curcpu);
}

int
cpu_idle_wakeup(int cpu)
{
	struct pcpu *pcpu;
	int *state;

	pcpu = pcpu_find(cpu);
	state = (int *)pcpu->pc_monitorbuf;
	/*
	 * This doesn't need to be atomic since missing the race will
	 * simply result in unnecessary IPIs.
	 */
	if (*state == STATE_SLEEPING)
		return (0);
	if (*state == STATE_MWAIT)
		*state = STATE_RUNNING;
	return (1);
}

/*
 * Ordered by speed/power consumption.
 */
struct {
	void	*id_fn;
	char	*id_name;
} idle_tbl[] = {
	{ cpu_idle_spin, "spin" },
	{ cpu_idle_mwait, "mwait" },
	{ cpu_idle_hlt, "hlt" },
	{ cpu_idle_acpi, "acpi" },
	{ NULL, NULL }
};

static int
idle_sysctl_available(SYSCTL_HANDLER_ARGS)
{
	char *avail, *p;
	int error;
	int i;

	avail = malloc(256, M_TEMP, M_WAITOK);
	p = avail;
	for (i = 0; idle_tbl[i].id_name != NULL; i++) {
		if (strstr(idle_tbl[i].id_name, "mwait") &&
		    (cpu_feature2 & CPUID2_MON) == 0)
			continue;
		if (strcmp(idle_tbl[i].id_name, "acpi") == 0 &&
		    cpu_idle_hook == NULL)
			continue;
		p += sprintf(p, "%s%s", p != avail ? ", " : "",
		    idle_tbl[i].id_name);
	}
	error = sysctl_handle_string(oidp, avail, 0, req);
	free(avail, M_TEMP);
	return (error);
}

SYSCTL_PROC(_machdep, OID_AUTO, idle_available, CTLTYPE_STRING | CTLFLAG_RD,
    0, 0, idle_sysctl_available, "A", "list of available idle functions");

static int
idle_sysctl(SYSCTL_HANDLER_ARGS)
{
	char buf[16];
	int error;
	char *p;
	int i;

	p = "unknown";
	for (i = 0; idle_tbl[i].id_name != NULL; i++) {
		if (idle_tbl[i].id_fn == cpu_idle_fn) {
			p = idle_tbl[i].id_name;
			break;
		}
	}
	strncpy(buf, p, sizeof(buf));
	error = sysctl_handle_string(oidp, buf, sizeof(buf), req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	for (i = 0; idle_tbl[i].id_name != NULL; i++) {
		if (strstr(idle_tbl[i].id_name, "mwait") &&
		    (cpu_feature2 & CPUID2_MON) == 0)
			continue;
		if (strcmp(idle_tbl[i].id_name, "acpi") == 0 &&
		    cpu_idle_hook == NULL)
			continue;
		if (strcmp(idle_tbl[i].id_name, buf))
			continue;
		cpu_idle_fn = idle_tbl[i].id_fn;
		return (0);
	}
	return (EINVAL);
}

SYSCTL_PROC(_machdep, OID_AUTO, idle, CTLTYPE_STRING | CTLFLAG_RW, 0, 0,
    idle_sysctl, "A", "currently selected idle function");

uint64_t (*atomic_load_acq_64)(volatile uint64_t *) =
    atomic_load_acq_64_i386;
void (*atomic_store_rel_64)(volatile uint64_t *, uint64_t) =
    atomic_store_rel_64_i386;

static void
cpu_probe_cmpxchg8b(void)
{

	if ((cpu_feature & CPUID_CX8) != 0 ||
	    cpu_vendor_id == CPU_VENDOR_RISE) {
		atomic_load_acq_64 = atomic_load_acq_64_i586;
		atomic_store_rel_64 = atomic_store_rel_64_i586;
	}
}

/*
 * Reset registers to default values on exec.
 */
void
exec_setregs(struct thread *td, struct image_params *imgp, u_long stack)
{
	struct trapframe *regs = td->td_frame;
	struct pcb *pcb = td->td_pcb;

	/* Reset pc->pcb_gs and %gs before possibly invalidating it. */
	pcb->pcb_gs = _udatasel;
	load_gs(_udatasel);

	mtx_lock_spin(&dt_lock);
	if (td->td_proc->p_md.md_ldt)
		user_ldt_free(td);
	else
		mtx_unlock_spin(&dt_lock);
  
	bzero((char *)regs, sizeof(struct trapframe));
	regs->tf_eip = imgp->entry_addr;
	regs->tf_esp = stack;
	regs->tf_eflags = PSL_USER | (regs->tf_eflags & PSL_T);
	regs->tf_ss = _udatasel;
	regs->tf_ds = _udatasel;
	regs->tf_es = _udatasel;
	regs->tf_fs = _udatasel;
	regs->tf_cs = _ucodesel;

	/* PS_STRINGS value for BSD/OS binaries.  It is 0 for non-BSD/OS. */
	regs->tf_ebx = imgp->ps_strings;

        /*
         * Reset the hardware debug registers if they were in use.
         * They won't have any meaning for the newly exec'd process.  
         */
        if (pcb->pcb_flags & PCB_DBREGS) {
                pcb->pcb_dr0 = 0;
                pcb->pcb_dr1 = 0;
                pcb->pcb_dr2 = 0;
                pcb->pcb_dr3 = 0;
                pcb->pcb_dr6 = 0;
                pcb->pcb_dr7 = 0;
                if (pcb == PCPU_GET(curpcb)) {
		        /*
			 * Clear the debug registers on the running
			 * CPU, otherwise they will end up affecting
			 * the next process we switch to.
			 */
		        reset_dbregs();
                }
                pcb->pcb_flags &= ~PCB_DBREGS;
        }

	/*
	 * Initialize the math emulator (if any) for the current process.
	 * Actually, just clear the bit that says that the emulator has
	 * been initialized.  Initialization is delayed until the process
	 * traps to the emulator (if it is done at all) mainly because
	 * emulators don't provide an entry point for initialization.
	 */
	td->td_pcb->pcb_flags &= ~FP_SOFTFP;
	pcb->pcb_initial_npxcw = __INITIAL_NPXCW__;

	/*
	 * Drop the FP state if we hold it, so that the process gets a
	 * clean FP state if it uses the FPU again.
	 */
	fpstate_drop(td);

	/*
	 * XXX - Linux emulator
	 * Make sure sure edx is 0x0 on entry. Linux binaries depend
	 * on it.
	 */
	td->td_retval[1] = 0;
}

void
cpu_setregs(void)
{
	unsigned int cr0;

	cr0 = rcr0();

	/*
	 * CR0_MP, CR0_NE and CR0_TS are set for NPX (FPU) support:
	 *
	 * Prepare to trap all ESC (i.e., NPX) instructions and all WAIT
	 * instructions.  We must set the CR0_MP bit and use the CR0_TS
	 * bit to control the trap, because setting the CR0_EM bit does
	 * not cause WAIT instructions to trap.  It's important to trap
	 * WAIT instructions - otherwise the "wait" variants of no-wait
	 * control instructions would degenerate to the "no-wait" variants
	 * after FP context switches but work correctly otherwise.  It's
	 * particularly important to trap WAITs when there is no NPX -
	 * otherwise the "wait" variants would always degenerate.
	 *
	 * Try setting CR0_NE to get correct error reporting on 486DX's.
	 * Setting it should fail or do nothing on lesser processors.
	 */
	cr0 |= CR0_MP | CR0_NE | CR0_TS | CR0_WP | CR0_AM;
	load_cr0(cr0);
	load_gs(_udatasel);
}

u_long bootdev;		/* not a struct cdev *- encoding is different */
SYSCTL_ULONG(_machdep, OID_AUTO, guessed_bootdev,
	CTLFLAG_RD, &bootdev, 0, "Maybe the Boot device (not in struct cdev *format)");

/*
 * Initialize 386 and configure to run kernel
 */

/*
 * Initialize segments & interrupt table
 */

int _default_ldt;

#ifdef XEN
union descriptor *gdt;
union descriptor *ldt;
#else
union descriptor gdt[NGDT * MAXCPU];	/* global descriptor table */
union descriptor ldt[NLDT];		/* local descriptor table */
#endif
static struct gate_descriptor idt0[NIDT];
struct gate_descriptor *idt = &idt0[0];	/* interrupt descriptor table */
struct region_descriptor r_gdt, r_idt;	/* table descriptors */
struct mtx dt_lock;			/* lock for GDT and LDT */

#if defined(I586_CPU) && !defined(NO_F00F_HACK)
extern int has_f00f_bug;
#endif

static struct i386tss dblfault_tss;
static char dblfault_stack[PAGE_SIZE];

extern  vm_offset_t	proc0kstack;


/*
 * software prototypes -- in more palatable form.
 *
 * GCODE_SEL through GUDATA_SEL must be in this order for syscall/sysret
 * GUFS_SEL and GUGS_SEL must be in this order (swtch.s knows it)
 */
struct soft_segment_descriptor gdt_segs[] = {
/* GNULL_SEL	0 Null Descriptor */
{	.ssd_base = 0x0,
	.ssd_limit = 0x0,
	.ssd_type = 0,
	.ssd_dpl = SEL_KPL,
	.ssd_p = 0,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 0,
	.ssd_gran = 0		},
/* GPRIV_SEL	1 SMP Per-Processor Private Data Descriptor */
{	.ssd_base = 0x0,
	.ssd_limit = 0xfffff,
	.ssd_type = SDT_MEMRWA,
	.ssd_dpl = SEL_KPL,
	.ssd_p = 1,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 1,
	.ssd_gran = 1		},
/* GUFS_SEL	2 %fs Descriptor for user */
{	.ssd_base = 0x0,
	.ssd_limit = 0xfffff,
	.ssd_type = SDT_MEMRWA,
	.ssd_dpl = SEL_UPL,
	.ssd_p = 1,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 1,
	.ssd_gran = 1		},
/* GUGS_SEL	3 %gs Descriptor for user */
{	.ssd_base = 0x0,
	.ssd_limit = 0xfffff,
	.ssd_type = SDT_MEMRWA,
	.ssd_dpl = SEL_UPL,
	.ssd_p = 1,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 1,
	.ssd_gran = 1		},
/* GCODE_SEL	4 Code Descriptor for kernel */
{	.ssd_base = 0x0,
	.ssd_limit = 0xfffff,
	.ssd_type = SDT_MEMERA,
	.ssd_dpl = SEL_KPL,
	.ssd_p = 1,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 1,
	.ssd_gran = 1		},
/* GDATA_SEL	5 Data Descriptor for kernel */
{	.ssd_base = 0x0,
	.ssd_limit = 0xfffff,
	.ssd_type = SDT_MEMRWA,
	.ssd_dpl = SEL_KPL,
	.ssd_p = 1,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 1,
	.ssd_gran = 1		},
/* GUCODE_SEL	6 Code Descriptor for user */
{	.ssd_base = 0x0,
	.ssd_limit = 0xfffff,
	.ssd_type = SDT_MEMERA,
	.ssd_dpl = SEL_UPL,
	.ssd_p = 1,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 1,
	.ssd_gran = 1		},
/* GUDATA_SEL	7 Data Descriptor for user */
{	.ssd_base = 0x0,
	.ssd_limit = 0xfffff,
	.ssd_type = SDT_MEMRWA,
	.ssd_dpl = SEL_UPL,
	.ssd_p = 1,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 1,
	.ssd_gran = 1		},
/* GBIOSLOWMEM_SEL 8 BIOS access to realmode segment 0x40, must be #8 in GDT */
{	.ssd_base = 0x400,
	.ssd_limit = 0xfffff,
	.ssd_type = SDT_MEMRWA,
	.ssd_dpl = SEL_KPL,
	.ssd_p = 1,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 1,
	.ssd_gran = 1		},
#ifndef XEN
/* GPROC0_SEL	9 Proc 0 Tss Descriptor */
{
	.ssd_base = 0x0,
	.ssd_limit = sizeof(struct i386tss)-1,
	.ssd_type = SDT_SYS386TSS,
	.ssd_dpl = 0,
	.ssd_p = 1,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 0,
	.ssd_gran = 0		},
/* GLDT_SEL	10 LDT Descriptor */
{	.ssd_base = (int) ldt,
	.ssd_limit = sizeof(ldt)-1,
	.ssd_type = SDT_SYSLDT,
	.ssd_dpl = SEL_UPL,
	.ssd_p = 1,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 0,
	.ssd_gran = 0		},
/* GUSERLDT_SEL	11 User LDT Descriptor per process */
{	.ssd_base = (int) ldt,
	.ssd_limit = (512 * sizeof(union descriptor)-1),
	.ssd_type = SDT_SYSLDT,
	.ssd_dpl = 0,
	.ssd_p = 1,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 0,
	.ssd_gran = 0		},
/* GPANIC_SEL	12 Panic Tss Descriptor */
{	.ssd_base = (int) &dblfault_tss,
	.ssd_limit = sizeof(struct i386tss)-1,
	.ssd_type = SDT_SYS386TSS,
	.ssd_dpl = 0,
	.ssd_p = 1,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 0,
	.ssd_gran = 0		},
/* GBIOSCODE32_SEL 13 BIOS 32-bit interface (32bit Code) */
{	.ssd_base = 0,
	.ssd_limit = 0xfffff,
	.ssd_type = SDT_MEMERA,
	.ssd_dpl = 0,
	.ssd_p = 1,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 0,
	.ssd_gran = 1		},
/* GBIOSCODE16_SEL 14 BIOS 32-bit interface (16bit Code) */
{	.ssd_base = 0,
	.ssd_limit = 0xfffff,
	.ssd_type = SDT_MEMERA,
	.ssd_dpl = 0,
	.ssd_p = 1,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 0,
	.ssd_gran = 1		},
/* GBIOSDATA_SEL 15 BIOS 32-bit interface (Data) */
{	.ssd_base = 0,
	.ssd_limit = 0xfffff,
	.ssd_type = SDT_MEMRWA,
	.ssd_dpl = 0,
	.ssd_p = 1,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 1,
	.ssd_gran = 1		},
/* GBIOSUTIL_SEL 16 BIOS 16-bit interface (Utility) */
{	.ssd_base = 0,
	.ssd_limit = 0xfffff,
	.ssd_type = SDT_MEMRWA,
	.ssd_dpl = 0,
	.ssd_p = 1,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 0,
	.ssd_gran = 1		},
/* GBIOSARGS_SEL 17 BIOS 16-bit interface (Arguments) */
{	.ssd_base = 0,
	.ssd_limit = 0xfffff,
	.ssd_type = SDT_MEMRWA,
	.ssd_dpl = 0,
	.ssd_p = 1,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 0,
	.ssd_gran = 1		},
/* GNDIS_SEL	18 NDIS Descriptor */
{	.ssd_base = 0x0,
	.ssd_limit = 0x0,
	.ssd_type = 0,
	.ssd_dpl = 0,
	.ssd_p = 0,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 0,
	.ssd_gran = 0		},
#endif /* !XEN */
};

static struct soft_segment_descriptor ldt_segs[] = {
	/* Null Descriptor - overwritten by call gate */
{	.ssd_base = 0x0,
	.ssd_limit = 0x0,
	.ssd_type = 0,
	.ssd_dpl = 0,
	.ssd_p = 0,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 0,
	.ssd_gran = 0		},
	/* Null Descriptor - overwritten by call gate */
{	.ssd_base = 0x0,
	.ssd_limit = 0x0,
	.ssd_type = 0,
	.ssd_dpl = 0,
	.ssd_p = 0,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 0,
	.ssd_gran = 0		},
	/* Null Descriptor - overwritten by call gate */
{	.ssd_base = 0x0,
	.ssd_limit = 0x0,
	.ssd_type = 0,
	.ssd_dpl = 0,
	.ssd_p = 0,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 0,
	.ssd_gran = 0		},
	/* Code Descriptor for user */
{	.ssd_base = 0x0,
	.ssd_limit = 0xfffff,
	.ssd_type = SDT_MEMERA,
	.ssd_dpl = SEL_UPL,
	.ssd_p = 1,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 1,
	.ssd_gran = 1		},
	/* Null Descriptor - overwritten by call gate */
{	.ssd_base = 0x0,
	.ssd_limit = 0x0,
	.ssd_type = 0,
	.ssd_dpl = 0,
	.ssd_p = 0,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 0,
	.ssd_gran = 0		},
	/* Data Descriptor for user */
{	.ssd_base = 0x0,
	.ssd_limit = 0xfffff,
	.ssd_type = SDT_MEMRWA,
	.ssd_dpl = SEL_UPL,
	.ssd_p = 1,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 1,
	.ssd_gran = 1		},
};

void
setidt(idx, func, typ, dpl, selec)
	int idx;
	inthand_t *func;
	int typ;
	int dpl;
	int selec;
{
	struct gate_descriptor *ip;

	ip = idt + idx;
	ip->gd_looffset = (int)func;
	ip->gd_selector = selec;
	ip->gd_stkcpy = 0;
	ip->gd_xx = 0;
	ip->gd_type = typ;
	ip->gd_dpl = dpl;
	ip->gd_p = 1;
	ip->gd_hioffset = ((int)func)>>16 ;
}

extern inthand_t
	IDTVEC(div), IDTVEC(dbg), IDTVEC(nmi), IDTVEC(bpt), IDTVEC(ofl),
	IDTVEC(bnd), IDTVEC(ill), IDTVEC(dna), IDTVEC(fpusegm),
	IDTVEC(tss), IDTVEC(missing), IDTVEC(stk), IDTVEC(prot),
	IDTVEC(page), IDTVEC(mchk), IDTVEC(rsvd), IDTVEC(fpu), IDTVEC(align),
	IDTVEC(xmm),
#ifdef KDTRACE_HOOKS
	IDTVEC(dtrace_ret),
#endif
	IDTVEC(lcall_syscall), IDTVEC(int0x80_syscall);

#ifdef DDB
/*
 * Display the index and function name of any IDT entries that don't use
 * the default 'rsvd' entry point.
 */
DB_SHOW_COMMAND(idt, db_show_idt)
{
	struct gate_descriptor *ip;
	int idx;
	uintptr_t func;

	ip = idt;
	for (idx = 0; idx < NIDT && !db_pager_quit; idx++) {
		func = (ip->gd_hioffset << 16 | ip->gd_looffset);
		if (func != (uintptr_t)&IDTVEC(rsvd)) {
			db_printf("%3d\t", idx);
			db_printsym(func, DB_STGY_PROC);
			db_printf("\n");
		}
		ip++;
	}
}

/* Show privileged registers. */
DB_SHOW_COMMAND(sysregs, db_show_sysregs)
{
	uint64_t idtr, gdtr;

	idtr = ridt();
	db_printf("idtr\t0x%08x/%04x\n",
	    (u_int)(idtr >> 16), (u_int)idtr & 0xffff);
	gdtr = rgdt();
	db_printf("gdtr\t0x%08x/%04x\n",
	    (u_int)(gdtr >> 16), (u_int)gdtr & 0xffff);
	db_printf("ldtr\t0x%04x\n", rldt());
	db_printf("tr\t0x%04x\n", rtr());
	db_printf("cr0\t0x%08x\n", rcr0());
	db_printf("cr2\t0x%08x\n", rcr2());
	db_printf("cr3\t0x%08x\n", rcr3());
	db_printf("cr4\t0x%08x\n", rcr4());
}
#endif

void
sdtossd(sd, ssd)
	struct segment_descriptor *sd;
	struct soft_segment_descriptor *ssd;
{
	ssd->ssd_base  = (sd->sd_hibase << 24) | sd->sd_lobase;
	ssd->ssd_limit = (sd->sd_hilimit << 16) | sd->sd_lolimit;
	ssd->ssd_type  = sd->sd_type;
	ssd->ssd_dpl   = sd->sd_dpl;
	ssd->ssd_p     = sd->sd_p;
	ssd->ssd_def32 = sd->sd_def32;
	ssd->ssd_gran  = sd->sd_gran;
}

#ifndef XEN
static int
add_smap_entry(struct bios_smap *smap, vm_paddr_t *physmap, int *physmap_idxp)
{
	int i, insert_idx, physmap_idx;

	physmap_idx = *physmap_idxp;
	
	if (boothowto & RB_VERBOSE)
		printf("SMAP type=%02x base=%016llx len=%016llx\n",
		    smap->type, smap->base, smap->length);

	if (smap->type != SMAP_TYPE_MEMORY)
		return (1);

	if (smap->length == 0)
		return (1);

#ifndef PAE
	if (smap->base > 0xffffffff) {
		printf("%uK of memory above 4GB ignored\n",
		    (u_int)(smap->length / 1024));
		return (1);
	}
#endif

	/*
	 * Find insertion point while checking for overlap.  Start off by
	 * assuming the new entry will be added to the end.
	 */
	insert_idx = physmap_idx + 2;
	for (i = 0; i <= physmap_idx; i += 2) {
		if (smap->base < physmap[i + 1]) {
			if (smap->base + smap->length <= physmap[i]) {
				insert_idx = i;
				break;
			}
			if (boothowto & RB_VERBOSE)
				printf(
		    "Overlapping memory regions, ignoring second region\n");
			return (1);
		}
	}

	/* See if we can prepend to the next entry. */
	if (insert_idx <= physmap_idx &&
	    smap->base + smap->length == physmap[insert_idx]) {
		physmap[insert_idx] = smap->base;
		return (1);
	}

	/* See if we can append to the previous entry. */
	if (insert_idx > 0 && smap->base == physmap[insert_idx - 1]) {
		physmap[insert_idx - 1] += smap->length;
		return (1);
	}

	physmap_idx += 2;
	*physmap_idxp = physmap_idx;
	if (physmap_idx == PHYSMAP_SIZE) {
		printf(
		"Too many segments in the physical address map, giving up\n");
		return (0);
	}

	/*
	 * Move the last 'N' entries down to make room for the new
	 * entry if needed.
	 */
	for (i = physmap_idx; i > insert_idx; i -= 2) {
		physmap[i] = physmap[i - 2];
		physmap[i + 1] = physmap[i - 1];
	}

	/* Insert the new entry. */
	physmap[insert_idx] = smap->base;
	physmap[insert_idx + 1] = smap->base + smap->length;
	return (1);
}

static void
basemem_setup(void)
{
	vm_paddr_t pa;
	pt_entry_t *pte;
	int i;

	if (basemem > 640) {
		printf("Preposterous BIOS basemem of %uK, truncating to 640K\n",
			basemem);
		basemem = 640;
	}

	/*
	 * XXX if biosbasemem is now < 640, there is a `hole'
	 * between the end of base memory and the start of
	 * ISA memory.  The hole may be empty or it may
	 * contain BIOS code or data.  Map it read/write so
	 * that the BIOS can write to it.  (Memory from 0 to
	 * the physical end of the kernel is mapped read-only
	 * to begin with and then parts of it are remapped.
	 * The parts that aren't remapped form holes that
	 * remain read-only and are unused by the kernel.
	 * The base memory area is below the physical end of
	 * the kernel and right now forms a read-only hole.
	 * The part of it from PAGE_SIZE to
	 * (trunc_page(biosbasemem * 1024) - 1) will be
	 * remapped and used by the kernel later.)
	 *
	 * This code is similar to the code used in
	 * pmap_mapdev, but since no memory needs to be
	 * allocated we simply change the mapping.
	 */
	for (pa = trunc_page(basemem * 1024);
	     pa < ISA_HOLE_START; pa += PAGE_SIZE)
		pmap_kenter(KERNBASE + pa, pa);

	/*
	 * Map pages between basemem and ISA_HOLE_START, if any, r/w into
	 * the vm86 page table so that vm86 can scribble on them using
	 * the vm86 map too.  XXX: why 2 ways for this and only 1 way for
	 * page 0, at least as initialized here?
	 */
	pte = (pt_entry_t *)vm86paddr;
	for (i = basemem / 4; i < 160; i++)
		pte[i] = (i << PAGE_SHIFT) | PG_V | PG_RW | PG_U;
}
#endif

/*
 * Populate the (physmap) array with base/bound pairs describing the
 * available physical memory in the system, then test this memory and
 * build the phys_avail array describing the actually-available memory.
 *
 * If we cannot accurately determine the physical memory map, then use
 * value from the 0xE801 call, and failing that, the RTC.
 *
 * Total memory size may be set by the kernel environment variable
 * hw.physmem or the compile-time define MAXMEM.
 *
 * XXX first should be vm_paddr_t.
 */
static void
getmemsize(int first)
{
	int has_smap, off, physmap_idx, pa_indx, da_indx;
	u_long physmem_tunable, memtest;
	vm_paddr_t physmap[PHYSMAP_SIZE];
	pt_entry_t *pte;
	quad_t dcons_addr, dcons_size;
#ifndef XEN
	int hasbrokenint12, i;
	u_int extmem;
	struct vm86frame vmf;
	struct vm86context vmc;
	vm_paddr_t pa;
	struct bios_smap *smap, *smapbase, *smapend;
	u_int32_t smapsize;
	caddr_t kmdp;
#endif

	has_smap = 0;
#if defined(XEN)
	Maxmem = xen_start_info->nr_pages - init_first;
	physmem = Maxmem;
	basemem = 0;
	physmap[0] = init_first << PAGE_SHIFT;
	physmap[1] = ptoa(Maxmem) - round_page(msgbufsize);
	physmap_idx = 0;
#else
#ifdef XBOX
	if (arch_i386_is_xbox) {
		/*
		 * We queried the memory size before, so chop off 4MB for
		 * the framebuffer and inform the OS of this.
		 */
		physmap[0] = 0;
		physmap[1] = (arch_i386_xbox_memsize * 1024 * 1024) - XBOX_FB_SIZE;
		physmap_idx = 0;
		goto physmap_done;
	}
#endif
	bzero(&vmf, sizeof(vmf));
	bzero(physmap, sizeof(physmap));
	basemem = 0;

	/*
	 * Check if the loader supplied an SMAP memory map.  If so,
	 * use that and do not make any VM86 calls.
	 */
	physmap_idx = 0;
	smapbase = NULL;
	kmdp = preload_search_by_type("elf kernel");
	if (kmdp == NULL)
		kmdp = preload_search_by_type("elf32 kernel");
	if (kmdp != NULL)
		smapbase = (struct bios_smap *)preload_search_info(kmdp,
		    MODINFO_METADATA | MODINFOMD_SMAP);
	if (smapbase != NULL) {
		/*
		 * subr_module.c says:
		 * "Consumer may safely assume that size value precedes data."
		 * ie: an int32_t immediately precedes SMAP.
		 */
		smapsize = *((u_int32_t *)smapbase - 1);
		smapend = (struct bios_smap *)((uintptr_t)smapbase + smapsize);
		has_smap = 1;

		for (smap = smapbase; smap < smapend; smap++)
			if (!add_smap_entry(smap, physmap, &physmap_idx))
				break;
		goto have_smap;
	}

	/*
	 * Some newer BIOSes have a broken INT 12H implementation
	 * which causes a kernel panic immediately.  In this case, we
	 * need use the SMAP to determine the base memory size.
	 */
	hasbrokenint12 = 0;
	TUNABLE_INT_FETCH("hw.hasbrokenint12", &hasbrokenint12);
	if (hasbrokenint12 == 0) {
		/* Use INT12 to determine base memory size. */
		vm86_intcall(0x12, &vmf);
		basemem = vmf.vmf_ax;
		basemem_setup();
	}

	/*
	 * Fetch the memory map with INT 15:E820.  Map page 1 R/W into
	 * the kernel page table so we can use it as a buffer.  The
	 * kernel will unmap this page later.
	 */
	pmap_kenter(KERNBASE + (1 << PAGE_SHIFT), 1 << PAGE_SHIFT);
	vmc.npages = 0;
	smap = (void *)vm86_addpage(&vmc, 1, KERNBASE + (1 << PAGE_SHIFT));
	vm86_getptr(&vmc, (vm_offset_t)smap, &vmf.vmf_es, &vmf.vmf_di);

	vmf.vmf_ebx = 0;
	do {
		vmf.vmf_eax = 0xE820;
		vmf.vmf_edx = SMAP_SIG;
		vmf.vmf_ecx = sizeof(struct bios_smap);
		i = vm86_datacall(0x15, &vmf, &vmc);
		if (i || vmf.vmf_eax != SMAP_SIG)
			break;
		has_smap = 1;
		if (!add_smap_entry(smap, physmap, &physmap_idx))
			break;
	} while (vmf.vmf_ebx != 0);

have_smap:
	/*
	 * If we didn't fetch the "base memory" size from INT12,
	 * figure it out from the SMAP (or just guess).
	 */
	if (basemem == 0) {
		for (i = 0; i <= physmap_idx; i += 2) {
			if (physmap[i] == 0x00000000) {
				basemem = physmap[i + 1] / 1024;
				break;
			}
		}

		/* XXX: If we couldn't find basemem from SMAP, just guess. */
		if (basemem == 0)
			basemem = 640;
		basemem_setup();
	}

	if (physmap[1] != 0)
		goto physmap_done;

	/*
	 * If we failed to find an SMAP, figure out the extended
	 * memory size.  We will then build a simple memory map with
	 * two segments, one for "base memory" and the second for
	 * "extended memory".  Note that "extended memory" starts at a
	 * physical address of 1MB and that both basemem and extmem
	 * are in units of 1KB.
	 *
	 * First, try to fetch the extended memory size via INT 15:E801.
	 */
	vmf.vmf_ax = 0xE801;
	if (vm86_intcall(0x15, &vmf) == 0) {
		extmem = vmf.vmf_cx + vmf.vmf_dx * 64;
	} else {
		/*
		 * If INT15:E801 fails, this is our last ditch effort
		 * to determine the extended memory size.  Currently
		 * we prefer the RTC value over INT15:88.
		 */
#if 0
		vmf.vmf_ah = 0x88;
		vm86_intcall(0x15, &vmf);
		extmem = vmf.vmf_ax;
#else
		extmem = rtcin(RTC_EXTLO) + (rtcin(RTC_EXTHI) << 8);
#endif
	}

	/*
	 * Special hack for chipsets that still remap the 384k hole when
	 * there's 16MB of memory - this really confuses people that
	 * are trying to use bus mastering ISA controllers with the
	 * "16MB limit"; they only have 16MB, but the remapping puts
	 * them beyond the limit.
	 *
	 * If extended memory is between 15-16MB (16-17MB phys address range),
	 *	chop it to 15MB.
	 */
	if ((extmem > 15 * 1024) && (extmem < 16 * 1024))
		extmem = 15 * 1024;

	physmap[0] = 0;
	physmap[1] = basemem * 1024;
	physmap_idx = 2;
	physmap[physmap_idx] = 0x100000;
	physmap[physmap_idx + 1] = physmap[physmap_idx] + extmem * 1024;

physmap_done:
#endif	
	/*
	 * Now, physmap contains a map of physical memory.
	 */

#ifdef SMP
	/* make hole for AP bootstrap code */
	physmap[1] = mp_bootaddress(physmap[1]);
#endif

	/*
	 * Maxmem isn't the "maximum memory", it's one larger than the
	 * highest page of the physical address space.  It should be
	 * called something like "Maxphyspage".  We may adjust this 
	 * based on ``hw.physmem'' and the results of the memory test.
	 */
	Maxmem = atop(physmap[physmap_idx + 1]);

#ifdef MAXMEM
	Maxmem = MAXMEM / 4;
#endif

	if (TUNABLE_ULONG_FETCH("hw.physmem", &physmem_tunable))
		Maxmem = atop(physmem_tunable);

	/*
	 * If we have an SMAP, don't allow MAXMEM or hw.physmem to extend
	 * the amount of memory in the system.
	 */
	if (has_smap && Maxmem > atop(physmap[physmap_idx + 1]))
		Maxmem = atop(physmap[physmap_idx + 1]);

	/*
	 * By default enable the memory test on real hardware, and disable
	 * it if we appear to be running in a VM.  This avoids touching all
	 * pages unnecessarily, which doesn't matter on real hardware but is
	 * bad for shared VM hosts.  Use a general name so that
	 * one could eventually do more with the code than just disable it.
	 */
	memtest = (vm_guest > VM_GUEST_NO) ? 0 : 1;
	TUNABLE_ULONG_FETCH("hw.memtest.tests", &memtest);

	if (atop(physmap[physmap_idx + 1]) != Maxmem &&
	    (boothowto & RB_VERBOSE))
		printf("Physical memory use set to %ldK\n", Maxmem * 4);

	/*
	 * If Maxmem has been increased beyond what the system has detected,
	 * extend the last memory segment to the new limit.
	 */ 
	if (atop(physmap[physmap_idx + 1]) < Maxmem)
		physmap[physmap_idx + 1] = ptoa((vm_paddr_t)Maxmem);

	/* call pmap initialization to make new kernel address space */
	pmap_bootstrap(first);

	/*
	 * Size up each available chunk of physical memory.
	 */
	physmap[0] = PAGE_SIZE;		/* mask off page 0 */
	pa_indx = 0;
	da_indx = 1;
	phys_avail[pa_indx++] = physmap[0];
	phys_avail[pa_indx] = physmap[0];
	dump_avail[da_indx] = physmap[0];
	pte = CMAP1;

	/*
	 * Get dcons buffer address
	 */
	if (getenv_quad("dcons.addr", &dcons_addr) == 0 ||
	    getenv_quad("dcons.size", &dcons_size) == 0)
		dcons_addr = 0;

#ifndef XEN
	/*
	 * physmap is in bytes, so when converting to page boundaries,
	 * round up the start address and round down the end address.
	 */
	for (i = 0; i <= physmap_idx; i += 2) {
		vm_paddr_t end;

		end = ptoa((vm_paddr_t)Maxmem);
		if (physmap[i + 1] < end)
			end = trunc_page(physmap[i + 1]);
		for (pa = round_page(physmap[i]); pa < end; pa += PAGE_SIZE) {
			int tmp, page_bad, full;
			int *ptr = (int *)CADDR1;

			full = FALSE;
			/*
			 * block out kernel memory as not available.
			 */
			if (pa >= KERNLOAD && pa < first)
				goto do_dump_avail;

			/*
			 * block out dcons buffer
			 */
			if (dcons_addr > 0
			    && pa >= trunc_page(dcons_addr)
			    && pa < dcons_addr + dcons_size)
				goto do_dump_avail;

			page_bad = FALSE;
			if (memtest == 0)
				goto skip_memtest;

			/*
			 * map page into kernel: valid, read/write,non-cacheable
			 */
			*pte = pa | PG_V | PG_RW | PG_N;
			invltlb();

			tmp = *(int *)ptr;
			/*
			 * Test for alternating 1's and 0's
			 */
			*(volatile int *)ptr = 0xaaaaaaaa;
			if (*(volatile int *)ptr != 0xaaaaaaaa)
				page_bad = TRUE;
			/*
			 * Test for alternating 0's and 1's
			 */
			*(volatile int *)ptr = 0x55555555;
			if (*(volatile int *)ptr != 0x55555555)
				page_bad = TRUE;
			/*
			 * Test for all 1's
			 */
			*(volatile int *)ptr = 0xffffffff;
			if (*(volatile int *)ptr != 0xffffffff)
				page_bad = TRUE;
			/*
			 * Test for all 0's
			 */
			*(volatile int *)ptr = 0x0;
			if (*(volatile int *)ptr != 0x0)
				page_bad = TRUE;
			/*
			 * Restore original value.
			 */
			*(int *)ptr = tmp;

skip_memtest:
			/*
			 * Adjust array of valid/good pages.
			 */
			if (page_bad == TRUE)
				continue;
			/*
			 * If this good page is a continuation of the
			 * previous set of good pages, then just increase
			 * the end pointer. Otherwise start a new chunk.
			 * Note that "end" points one higher than end,
			 * making the range >= start and < end.
			 * If we're also doing a speculative memory
			 * test and we at or past the end, bump up Maxmem
			 * so that we keep going. The first bad page
			 * will terminate the loop.
			 */
			if (phys_avail[pa_indx] == pa) {
				phys_avail[pa_indx] += PAGE_SIZE;
			} else {
				pa_indx++;
				if (pa_indx == PHYS_AVAIL_ARRAY_END) {
					printf(
		"Too many holes in the physical address space, giving up\n");
					pa_indx--;
					full = TRUE;
					goto do_dump_avail;
				}
				phys_avail[pa_indx++] = pa;	/* start */
				phys_avail[pa_indx] = pa + PAGE_SIZE; /* end */
			}
			physmem++;
do_dump_avail:
			if (dump_avail[da_indx] == pa) {
				dump_avail[da_indx] += PAGE_SIZE;
			} else {
				da_indx++;
				if (da_indx == DUMP_AVAIL_ARRAY_END) {
					da_indx--;
					goto do_next;
				}
				dump_avail[da_indx++] = pa;	/* start */
				dump_avail[da_indx] = pa + PAGE_SIZE; /* end */
			}
do_next:
			if (full)
				break;
		}
	}
	*pte = 0;
	invltlb();
#else
	phys_avail[0] = physfree;
	phys_avail[1] = xen_start_info->nr_pages*PAGE_SIZE;
	dump_avail[0] = 0;	
	dump_avail[1] = xen_start_info->nr_pages*PAGE_SIZE;
	
#endif
	
	/*
	 * XXX
	 * The last chunk must contain at least one page plus the message
	 * buffer to avoid complicating other code (message buffer address
	 * calculation, etc.).
	 */
	while (phys_avail[pa_indx - 1] + PAGE_SIZE +
	    round_page(msgbufsize) >= phys_avail[pa_indx]) {
		physmem -= atop(phys_avail[pa_indx] - phys_avail[pa_indx - 1]);
		phys_avail[pa_indx--] = 0;
		phys_avail[pa_indx--] = 0;
	}

	Maxmem = atop(phys_avail[pa_indx]);

	/* Trim off space for the message buffer. */
	phys_avail[pa_indx] -= round_page(msgbufsize);

	/* Map the message buffer. */
	for (off = 0; off < round_page(msgbufsize); off += PAGE_SIZE)
		pmap_kenter((vm_offset_t)msgbufp + off, phys_avail[pa_indx] +
		    off);

	PT_UPDATES_FLUSH();
}

#ifdef XEN
#define MTOPSIZE (1<<(14 + PAGE_SHIFT))

void
init386(first)
	int first;
{
	unsigned long gdtmachpfn;
	int error, gsel_tss, metadata_missing, x, pa;
	size_t kstack0_sz;
	struct pcpu *pc;
	struct callback_register event = {
		.type = CALLBACKTYPE_event,
		.address = {GSEL(GCODE_SEL, SEL_KPL), (unsigned long)Xhypervisor_callback },
	};
	struct callback_register failsafe = {
		.type = CALLBACKTYPE_failsafe,
		.address = {GSEL(GCODE_SEL, SEL_KPL), (unsigned long)failsafe_callback },
	};

	thread0.td_kstack = proc0kstack;
	thread0.td_kstack_pages = KSTACK_PAGES;
	kstack0_sz = thread0.td_kstack_pages * PAGE_SIZE;
	thread0.td_pcb = (struct pcb *)(thread0.td_kstack + kstack0_sz) - 1;

	/*
 	 * This may be done better later if it gets more high level
 	 * components in it. If so just link td->td_proc here.
	 */
	proc_linkup0(&proc0, &thread0);

	metadata_missing = 0;
	if (xen_start_info->mod_start) {
		preload_metadata = (caddr_t)xen_start_info->mod_start;
		preload_bootstrap_relocate(KERNBASE);
	} else {
		metadata_missing = 1;
	}
	if (envmode == 1)
		kern_envp = static_env;
	else if ((caddr_t)xen_start_info->cmd_line)
	        kern_envp = xen_setbootenv((caddr_t)xen_start_info->cmd_line);

	boothowto |= xen_boothowto(kern_envp);
	
	/* Init basic tunables, hz etc */
	init_param1();

	/*
	 * XEN occupies a portion of the upper virtual address space 
	 * At its base it manages an array mapping machine page frames 
	 * to physical page frames - hence we need to be able to 
	 * access 4GB - (64MB  - 4MB + 64k) 
	 */
	gdt_segs[GPRIV_SEL].ssd_limit = atop(HYPERVISOR_VIRT_START + MTOPSIZE);
	gdt_segs[GUFS_SEL].ssd_limit = atop(HYPERVISOR_VIRT_START + MTOPSIZE);
	gdt_segs[GUGS_SEL].ssd_limit = atop(HYPERVISOR_VIRT_START + MTOPSIZE);
	gdt_segs[GCODE_SEL].ssd_limit = atop(HYPERVISOR_VIRT_START + MTOPSIZE);
	gdt_segs[GDATA_SEL].ssd_limit = atop(HYPERVISOR_VIRT_START + MTOPSIZE);
	gdt_segs[GUCODE_SEL].ssd_limit = atop(HYPERVISOR_VIRT_START + MTOPSIZE);
	gdt_segs[GUDATA_SEL].ssd_limit = atop(HYPERVISOR_VIRT_START + MTOPSIZE);
	gdt_segs[GBIOSLOWMEM_SEL].ssd_limit = atop(HYPERVISOR_VIRT_START + MTOPSIZE);

	pc = &__pcpu[0];
	gdt_segs[GPRIV_SEL].ssd_base = (int) pc;
	gdt_segs[GPROC0_SEL].ssd_base = (int) &pc->pc_common_tss;

	PT_SET_MA(gdt, xpmap_ptom(VTOP(gdt)) | PG_V | PG_RW);
	bzero(gdt, PAGE_SIZE);
	for (x = 0; x < NGDT; x++)
		ssdtosd(&gdt_segs[x], &gdt[x].sd);

	mtx_init(&dt_lock, "descriptor tables", NULL, MTX_SPIN);

	gdtmachpfn = vtomach(gdt) >> PAGE_SHIFT;
	PT_SET_MA(gdt, xpmap_ptom(VTOP(gdt)) | PG_V);
	PANIC_IF(HYPERVISOR_set_gdt(&gdtmachpfn, 512) != 0);	
	lgdt(&r_gdt);
	gdtset = 1;

	if ((error = HYPERVISOR_set_trap_table(trap_table)) != 0) {
		panic("set_trap_table failed - error %d\n", error);
	}
	
	error = HYPERVISOR_callback_op(CALLBACKOP_register, &event);
	if (error == 0)
		error = HYPERVISOR_callback_op(CALLBACKOP_register, &failsafe);
#if	CONFIG_XEN_COMPAT <= 0x030002
	if (error == -ENOXENSYS)
		HYPERVISOR_set_callbacks(GSEL(GCODE_SEL, SEL_KPL),
		    (unsigned long)Xhypervisor_callback,
		    GSEL(GCODE_SEL, SEL_KPL), (unsigned long)failsafe_callback);
#endif
	pcpu_init(pc, 0, sizeof(struct pcpu));
	for (pa = first; pa < first + DPCPU_SIZE; pa += PAGE_SIZE)
		pmap_kenter(pa + KERNBASE, pa);
	dpcpu_init((void *)(first + KERNBASE), 0);
	first += DPCPU_SIZE;
	physfree += DPCPU_SIZE;
	init_first += DPCPU_SIZE / PAGE_SIZE;

	PCPU_SET(prvspace, pc);
	PCPU_SET(curthread, &thread0);
	PCPU_SET(curpcb, thread0.td_pcb);

	/*
	 * Initialize mutexes.
	 *
	 * icu_lock: in order to allow an interrupt to occur in a critical
	 * 	     section, to set pcpu->ipending (etc...) properly, we
	 *	     must be able to get the icu lock, so it can't be
	 *	     under witness.
	 */
	mutex_init();
	mtx_init(&icu_lock, "icu", NULL, MTX_SPIN | MTX_NOWITNESS | MTX_NOPROFILE);

	/* make ldt memory segments */
	PT_SET_MA(ldt, xpmap_ptom(VTOP(ldt)) | PG_V | PG_RW);
	bzero(ldt, PAGE_SIZE);
	ldt_segs[LUCODE_SEL].ssd_limit = atop(0 - 1);
	ldt_segs[LUDATA_SEL].ssd_limit = atop(0 - 1);
	for (x = 0; x < sizeof ldt_segs / sizeof ldt_segs[0]; x++)
		ssdtosd(&ldt_segs[x], &ldt[x].sd);

	default_proc_ldt.ldt_base = (caddr_t)ldt;
	default_proc_ldt.ldt_len = 6;
	_default_ldt = (int)&default_proc_ldt;
	PCPU_SET(currentldt, _default_ldt);
	PT_SET_MA(ldt, *vtopte((unsigned long)ldt) & ~PG_RW);
	xen_set_ldt((unsigned long) ldt, (sizeof ldt_segs / sizeof ldt_segs[0]));
	
#if defined(XEN_PRIVILEGED)
	/*
	 * Initialize the i8254 before the console so that console
	 * initialization can use DELAY().
	 */
	i8254_init();
#endif
	
	/*
	 * Initialize the console before we print anything out.
	 */
	cninit();

	if (metadata_missing)
		printf("WARNING: loader(8) metadata is missing!\n");

#ifdef DEV_ISA
#ifdef DEV_ATPIC
	elcr_probe();
	atpic_startup();
#else
	/* Reset and mask the atpics and leave them shut down. */
	atpic_reset();

	/*
	 * Point the ICU spurious interrupt vectors at the APIC spurious
	 * interrupt handler.
	 */
	setidt(IDT_IO_INTS + 7, IDTVEC(spuriousint), SDT_SYS386IGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
	setidt(IDT_IO_INTS + 15, IDTVEC(spuriousint), SDT_SYS386IGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
#endif
#endif

#ifdef DDB
	ksym_start = bootinfo.bi_symtab;
	ksym_end = bootinfo.bi_esymtab;
#endif

	kdb_init();

#ifdef KDB
	if (boothowto & RB_KDB)
		kdb_enter(KDB_WHY_BOOTFLAGS, "Boot flags requested debugger");
#endif

	finishidentcpu();	/* Final stage of CPU initialization */
	setidt(IDT_UD, &IDTVEC(ill),  SDT_SYS386TGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
	setidt(IDT_GP, &IDTVEC(prot),  SDT_SYS386TGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
	initializecpu();	/* Initialize CPU registers */

	/* make an initial tss so cpu can get interrupt stack on syscall! */
	/* Note: -16 is so we can grow the trapframe if we came from vm86 */
	PCPU_SET(common_tss.tss_esp0, thread0.td_kstack +
	    kstack0_sz - sizeof(struct pcb) - 16);
	PCPU_SET(common_tss.tss_ss0, GSEL(GDATA_SEL, SEL_KPL));
	gsel_tss = GSEL(GPROC0_SEL, SEL_KPL);
	HYPERVISOR_stack_switch(GSEL(GDATA_SEL, SEL_KPL),
	    PCPU_GET(common_tss.tss_esp0));
	
	/* pointer to selector slot for %fs/%gs */
	PCPU_SET(fsgs_gdt, &gdt[GUFS_SEL].sd);

	dblfault_tss.tss_esp = dblfault_tss.tss_esp0 = dblfault_tss.tss_esp1 =
	    dblfault_tss.tss_esp2 = (int)&dblfault_stack[sizeof(dblfault_stack)];
	dblfault_tss.tss_ss = dblfault_tss.tss_ss0 = dblfault_tss.tss_ss1 =
	    dblfault_tss.tss_ss2 = GSEL(GDATA_SEL, SEL_KPL);
#ifdef PAE
	dblfault_tss.tss_cr3 = (int)IdlePDPT;
#else
	dblfault_tss.tss_cr3 = (int)IdlePTD;
#endif
	dblfault_tss.tss_eip = (int)dblfault_handler;
	dblfault_tss.tss_eflags = PSL_KERNEL;
	dblfault_tss.tss_ds = dblfault_tss.tss_es =
	    dblfault_tss.tss_gs = GSEL(GDATA_SEL, SEL_KPL);
	dblfault_tss.tss_fs = GSEL(GPRIV_SEL, SEL_KPL);
	dblfault_tss.tss_cs = GSEL(GCODE_SEL, SEL_KPL);
	dblfault_tss.tss_ldt = GSEL(GLDT_SEL, SEL_KPL);

	vm86_initialize();
	getmemsize(first);
	init_param2(physmem);

	/* now running on new page tables, configured,and u/iom is accessible */

	msgbufinit(msgbufp, msgbufsize);
	/* transfer to user mode */

	_ucodesel = GSEL(GUCODE_SEL, SEL_UPL);
	_udatasel = GSEL(GUDATA_SEL, SEL_UPL);

	/* setup proc 0's pcb */
	thread0.td_pcb->pcb_flags = 0;
#ifdef PAE
	thread0.td_pcb->pcb_cr3 = (int)IdlePDPT;
#else
	thread0.td_pcb->pcb_cr3 = (int)IdlePTD;
#endif
	thread0.td_pcb->pcb_ext = 0;
	thread0.td_frame = &proc0_tf;
	thread0.td_pcb->pcb_fsd = PCPU_GET(fsgs_gdt)[0];
	thread0.td_pcb->pcb_gsd = PCPU_GET(fsgs_gdt)[1];

	cpu_probe_amdc1e();
	cpu_probe_cmpxchg8b();
}

#else
void
init386(first)
	int first;
{
	struct gate_descriptor *gdp;
	int gsel_tss, metadata_missing, x, pa;
	size_t kstack0_sz;
	struct pcpu *pc;

	thread0.td_kstack = proc0kstack;
	thread0.td_kstack_pages = KSTACK_PAGES;
	kstack0_sz = thread0.td_kstack_pages * PAGE_SIZE;
	thread0.td_pcb = (struct pcb *)(thread0.td_kstack + kstack0_sz) - 1;

	/*
 	 * This may be done better later if it gets more high level
 	 * components in it. If so just link td->td_proc here.
	 */
	proc_linkup0(&proc0, &thread0);

	metadata_missing = 0;
	if (bootinfo.bi_modulep) {
		preload_metadata = (caddr_t)bootinfo.bi_modulep + KERNBASE;
		preload_bootstrap_relocate(KERNBASE);
	} else {
		metadata_missing = 1;
	}
	if (envmode == 1)
		kern_envp = static_env;
	else if (bootinfo.bi_envp)
		kern_envp = (caddr_t)bootinfo.bi_envp + KERNBASE;

	/* Init basic tunables, hz etc */
	init_param1();

	/*
	 * Make gdt memory segments.  All segments cover the full 4GB
	 * of address space and permissions are enforced at page level.
	 */
	gdt_segs[GCODE_SEL].ssd_limit = atop(0 - 1);
	gdt_segs[GDATA_SEL].ssd_limit = atop(0 - 1);
	gdt_segs[GUCODE_SEL].ssd_limit = atop(0 - 1);
	gdt_segs[GUDATA_SEL].ssd_limit = atop(0 - 1);
	gdt_segs[GUFS_SEL].ssd_limit = atop(0 - 1);
	gdt_segs[GUGS_SEL].ssd_limit = atop(0 - 1);

	pc = &__pcpu[0];
	gdt_segs[GPRIV_SEL].ssd_limit = atop(0 - 1);
	gdt_segs[GPRIV_SEL].ssd_base = (int) pc;
	gdt_segs[GPROC0_SEL].ssd_base = (int) &pc->pc_common_tss;

	for (x = 0; x < NGDT; x++)
		ssdtosd(&gdt_segs[x], &gdt[x].sd);

	r_gdt.rd_limit = NGDT * sizeof(gdt[0]) - 1;
	r_gdt.rd_base =  (int) gdt;
	mtx_init(&dt_lock, "descriptor tables", NULL, MTX_SPIN);
	lgdt(&r_gdt);

	pcpu_init(pc, 0, sizeof(struct pcpu));
	for (pa = first; pa < first + DPCPU_SIZE; pa += PAGE_SIZE)
		pmap_kenter(pa + KERNBASE, pa);
	dpcpu_init((void *)(first + KERNBASE), 0);
	first += DPCPU_SIZE;
	PCPU_SET(prvspace, pc);
	PCPU_SET(curthread, &thread0);
	PCPU_SET(curpcb, thread0.td_pcb);

	/*
	 * Initialize mutexes.
	 *
	 * icu_lock: in order to allow an interrupt to occur in a critical
	 * 	     section, to set pcpu->ipending (etc...) properly, we
	 *	     must be able to get the icu lock, so it can't be
	 *	     under witness.
	 */
	mutex_init();
	mtx_init(&icu_lock, "icu", NULL, MTX_SPIN | MTX_NOWITNESS | MTX_NOPROFILE);

	/* make ldt memory segments */
	ldt_segs[LUCODE_SEL].ssd_limit = atop(0 - 1);
	ldt_segs[LUDATA_SEL].ssd_limit = atop(0 - 1);
	for (x = 0; x < sizeof ldt_segs / sizeof ldt_segs[0]; x++)
		ssdtosd(&ldt_segs[x], &ldt[x].sd);

	_default_ldt = GSEL(GLDT_SEL, SEL_KPL);
	lldt(_default_ldt);
	PCPU_SET(currentldt, _default_ldt);

	/* exceptions */
	for (x = 0; x < NIDT; x++)
		setidt(x, &IDTVEC(rsvd), SDT_SYS386TGT, SEL_KPL,
		    GSEL(GCODE_SEL, SEL_KPL));
	setidt(IDT_DE, &IDTVEC(div),  SDT_SYS386TGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
	setidt(IDT_DB, &IDTVEC(dbg),  SDT_SYS386IGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
	setidt(IDT_NMI, &IDTVEC(nmi),  SDT_SYS386IGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
 	setidt(IDT_BP, &IDTVEC(bpt),  SDT_SYS386IGT, SEL_UPL,
	    GSEL(GCODE_SEL, SEL_KPL));
	setidt(IDT_OF, &IDTVEC(ofl),  SDT_SYS386TGT, SEL_UPL,
	    GSEL(GCODE_SEL, SEL_KPL));
	setidt(IDT_BR, &IDTVEC(bnd),  SDT_SYS386TGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
	setidt(IDT_UD, &IDTVEC(ill),  SDT_SYS386TGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
	setidt(IDT_NM, &IDTVEC(dna),  SDT_SYS386TGT, SEL_KPL
	    , GSEL(GCODE_SEL, SEL_KPL));
	setidt(IDT_DF, 0,  SDT_SYSTASKGT, SEL_KPL, GSEL(GPANIC_SEL, SEL_KPL));
	setidt(IDT_FPUGP, &IDTVEC(fpusegm),  SDT_SYS386TGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
	setidt(IDT_TS, &IDTVEC(tss),  SDT_SYS386TGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
	setidt(IDT_NP, &IDTVEC(missing),  SDT_SYS386TGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
	setidt(IDT_SS, &IDTVEC(stk),  SDT_SYS386TGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
	setidt(IDT_GP, &IDTVEC(prot),  SDT_SYS386TGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
	setidt(IDT_PF, &IDTVEC(page),  SDT_SYS386IGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
	setidt(IDT_MF, &IDTVEC(fpu),  SDT_SYS386TGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
	setidt(IDT_AC, &IDTVEC(align), SDT_SYS386TGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
	setidt(IDT_MC, &IDTVEC(mchk),  SDT_SYS386TGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
	setidt(IDT_XF, &IDTVEC(xmm), SDT_SYS386TGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
 	setidt(IDT_SYSCALL, &IDTVEC(int0x80_syscall), SDT_SYS386TGT, SEL_UPL,
	    GSEL(GCODE_SEL, SEL_KPL));
#ifdef KDTRACE_HOOKS
	setidt(IDT_DTRACE_RET, &IDTVEC(dtrace_ret), SDT_SYS386TGT, SEL_UPL,
	    GSEL(GCODE_SEL, SEL_KPL));
#endif

	r_idt.rd_limit = sizeof(idt0) - 1;
	r_idt.rd_base = (int) idt;
	lidt(&r_idt);

#ifdef XBOX
	/*
	 * The following code queries the PCI ID of 0:0:0. For the XBOX,
	 * This should be 0x10de / 0x02a5.
	 *
	 * This is exactly what Linux does.
	 */
	outl(0xcf8, 0x80000000);
	if (inl(0xcfc) == 0x02a510de) {
		arch_i386_is_xbox = 1;
		pic16l_setled(XBOX_LED_GREEN);

		/*
		 * We are an XBOX, but we may have either 64MB or 128MB of
		 * memory. The PCI host bridge should be programmed for this,
		 * so we just query it. 
		 */
		outl(0xcf8, 0x80000084);
		arch_i386_xbox_memsize = (inl(0xcfc) == 0x7FFFFFF) ? 128 : 64;
	}
#endif /* XBOX */

	/*
	 * Initialize the i8254 before the console so that console
	 * initialization can use DELAY().
	 */
	i8254_init();

	/*
	 * Initialize the console before we print anything out.
	 */
	cninit();

	if (metadata_missing)
		printf("WARNING: loader(8) metadata is missing!\n");

#ifdef DEV_ISA
#ifdef DEV_ATPIC
	elcr_probe();
	atpic_startup();
#else
	/* Reset and mask the atpics and leave them shut down. */
	atpic_reset();

	/*
	 * Point the ICU spurious interrupt vectors at the APIC spurious
	 * interrupt handler.
	 */
	setidt(IDT_IO_INTS + 7, IDTVEC(spuriousint), SDT_SYS386IGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
	setidt(IDT_IO_INTS + 15, IDTVEC(spuriousint), SDT_SYS386IGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
#endif
#endif

#ifdef DDB
	ksym_start = bootinfo.bi_symtab;
	ksym_end = bootinfo.bi_esymtab;
#endif

	kdb_init();

#ifdef KDB
	if (boothowto & RB_KDB)
		kdb_enter(KDB_WHY_BOOTFLAGS, "Boot flags requested debugger");
#endif

	finishidentcpu();	/* Final stage of CPU initialization */
	setidt(IDT_UD, &IDTVEC(ill),  SDT_SYS386TGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
	setidt(IDT_GP, &IDTVEC(prot),  SDT_SYS386TGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
	initializecpu();	/* Initialize CPU registers */

	/* make an initial tss so cpu can get interrupt stack on syscall! */
	/* Note: -16 is so we can grow the trapframe if we came from vm86 */
	PCPU_SET(common_tss.tss_esp0, thread0.td_kstack +
	    kstack0_sz - sizeof(struct pcb) - 16);
	PCPU_SET(common_tss.tss_ss0, GSEL(GDATA_SEL, SEL_KPL));
	gsel_tss = GSEL(GPROC0_SEL, SEL_KPL);
	PCPU_SET(tss_gdt, &gdt[GPROC0_SEL].sd);
	PCPU_SET(common_tssd, *PCPU_GET(tss_gdt));
	PCPU_SET(common_tss.tss_ioopt, (sizeof (struct i386tss)) << 16);
	ltr(gsel_tss);

	/* pointer to selector slot for %fs/%gs */
	PCPU_SET(fsgs_gdt, &gdt[GUFS_SEL].sd);

	dblfault_tss.tss_esp = dblfault_tss.tss_esp0 = dblfault_tss.tss_esp1 =
	    dblfault_tss.tss_esp2 = (int)&dblfault_stack[sizeof(dblfault_stack)];
	dblfault_tss.tss_ss = dblfault_tss.tss_ss0 = dblfault_tss.tss_ss1 =
	    dblfault_tss.tss_ss2 = GSEL(GDATA_SEL, SEL_KPL);
#ifdef PAE
	dblfault_tss.tss_cr3 = (int)IdlePDPT;
#else
	dblfault_tss.tss_cr3 = (int)IdlePTD;
#endif
	dblfault_tss.tss_eip = (int)dblfault_handler;
	dblfault_tss.tss_eflags = PSL_KERNEL;
	dblfault_tss.tss_ds = dblfault_tss.tss_es =
	    dblfault_tss.tss_gs = GSEL(GDATA_SEL, SEL_KPL);
	dblfault_tss.tss_fs = GSEL(GPRIV_SEL, SEL_KPL);
	dblfault_tss.tss_cs = GSEL(GCODE_SEL, SEL_KPL);
	dblfault_tss.tss_ldt = GSEL(GLDT_SEL, SEL_KPL);

	vm86_initialize();
	getmemsize(first);
	init_param2(physmem);

	/* now running on new page tables, configured,and u/iom is accessible */

	msgbufinit(msgbufp, msgbufsize);

	/* make a call gate to reenter kernel with */
	gdp = &ldt[LSYS5CALLS_SEL].gd;

	x = (int) &IDTVEC(lcall_syscall);
	gdp->gd_looffset = x;
	gdp->gd_selector = GSEL(GCODE_SEL,SEL_KPL);
	gdp->gd_stkcpy = 1;
	gdp->gd_type = SDT_SYS386CGT;
	gdp->gd_dpl = SEL_UPL;
	gdp->gd_p = 1;
	gdp->gd_hioffset = x >> 16;

	/* XXX does this work? */
	/* XXX yes! */
	ldt[LBSDICALLS_SEL] = ldt[LSYS5CALLS_SEL];
	ldt[LSOL26CALLS_SEL] = ldt[LSYS5CALLS_SEL];

	/* transfer to user mode */

	_ucodesel = GSEL(GUCODE_SEL, SEL_UPL);
	_udatasel = GSEL(GUDATA_SEL, SEL_UPL);

	/* setup proc 0's pcb */
	thread0.td_pcb->pcb_flags = 0;
#ifdef PAE
	thread0.td_pcb->pcb_cr3 = (int)IdlePDPT;
#else
	thread0.td_pcb->pcb_cr3 = (int)IdlePTD;
#endif
	thread0.td_pcb->pcb_ext = 0;
	thread0.td_frame = &proc0_tf;

	cpu_probe_amdc1e();
	cpu_probe_cmpxchg8b();
}
#endif

void
cpu_pcpu_init(struct pcpu *pcpu, int cpuid, size_t size)
{

	pcpu->pc_acpi_id = 0xffffffff;
}

void
spinlock_enter(void)
{
	struct thread *td;
	register_t flags;

	td = curthread;
	if (td->td_md.md_spinlock_count == 0) {
		flags = intr_disable();
		td->td_md.md_spinlock_count = 1;
		td->td_md.md_saved_flags = flags;
	} else
		td->td_md.md_spinlock_count++;
	critical_enter();
}

void
spinlock_exit(void)
{
	struct thread *td;
	register_t flags;

	td = curthread;
	critical_exit();
	flags = td->td_md.md_saved_flags;
	td->td_md.md_spinlock_count--;
	if (td->td_md.md_spinlock_count == 0)
		intr_restore(flags);
}

#if defined(I586_CPU) && !defined(NO_F00F_HACK)
static void f00f_hack(void *unused);
SYSINIT(f00f_hack, SI_SUB_INTRINSIC, SI_ORDER_FIRST, f00f_hack, NULL);

static void
f00f_hack(void *unused)
{
	struct gate_descriptor *new_idt;
	vm_offset_t tmp;

	if (!has_f00f_bug)
		return;

	GIANT_REQUIRED;

	printf("Intel Pentium detected, installing workaround for F00F bug\n");

	tmp = kmem_alloc(kernel_map, PAGE_SIZE * 2);
	if (tmp == 0)
		panic("kmem_alloc returned 0");

	/* Put the problematic entry (#6) at the end of the lower page. */
	new_idt = (struct gate_descriptor*)
	    (tmp + PAGE_SIZE - 7 * sizeof(struct gate_descriptor));
	bcopy(idt, new_idt, sizeof(idt0));
	r_idt.rd_base = (u_int)new_idt;
	lidt(&r_idt);
	idt = new_idt;
	if (vm_map_protect(kernel_map, tmp, tmp + PAGE_SIZE,
			   VM_PROT_READ, FALSE) != KERN_SUCCESS)
		panic("vm_map_protect failed");
}
#endif /* defined(I586_CPU) && !NO_F00F_HACK */

/*
 * Construct a PCB from a trapframe. This is called from kdb_trap() where
 * we want to start a backtrace from the function that caused us to enter
 * the debugger. We have the context in the trapframe, but base the trace
 * on the PCB. The PCB doesn't have to be perfect, as long as it contains
 * enough for a backtrace.
 */
void
makectx(struct trapframe *tf, struct pcb *pcb)
{

	pcb->pcb_edi = tf->tf_edi;
	pcb->pcb_esi = tf->tf_esi;
	pcb->pcb_ebp = tf->tf_ebp;
	pcb->pcb_ebx = tf->tf_ebx;
	pcb->pcb_eip = tf->tf_eip;
	pcb->pcb_esp = (ISPL(tf->tf_cs)) ? tf->tf_esp : (int)(tf + 1) - 8;
}

int
ptrace_set_pc(struct thread *td, u_long addr)
{

	td->td_frame->tf_eip = addr;
	return (0);
}

int
ptrace_single_step(struct thread *td)
{
	td->td_frame->tf_eflags |= PSL_T;
	return (0);
}

int
ptrace_clear_single_step(struct thread *td)
{
	td->td_frame->tf_eflags &= ~PSL_T;
	return (0);
}

int
fill_regs(struct thread *td, struct reg *regs)
{
	struct pcb *pcb;
	struct trapframe *tp;

	tp = td->td_frame;
	pcb = td->td_pcb;
	regs->r_gs = pcb->pcb_gs;
	return (fill_frame_regs(tp, regs));
}

int
fill_frame_regs(struct trapframe *tp, struct reg *regs)
{
	regs->r_fs = tp->tf_fs;
	regs->r_es = tp->tf_es;
	regs->r_ds = tp->tf_ds;
	regs->r_edi = tp->tf_edi;
	regs->r_esi = tp->tf_esi;
	regs->r_ebp = tp->tf_ebp;
	regs->r_ebx = tp->tf_ebx;
	regs->r_edx = tp->tf_edx;
	regs->r_ecx = tp->tf_ecx;
	regs->r_eax = tp->tf_eax;
	regs->r_eip = tp->tf_eip;
	regs->r_cs = tp->tf_cs;
	regs->r_eflags = tp->tf_eflags;
	regs->r_esp = tp->tf_esp;
	regs->r_ss = tp->tf_ss;
	return (0);
}

int
set_regs(struct thread *td, struct reg *regs)
{
	struct pcb *pcb;
	struct trapframe *tp;

	tp = td->td_frame;
	if (!EFL_SECURE(regs->r_eflags, tp->tf_eflags) ||
	    !CS_SECURE(regs->r_cs))
		return (EINVAL);
	pcb = td->td_pcb;
	tp->tf_fs = regs->r_fs;
	tp->tf_es = regs->r_es;
	tp->tf_ds = regs->r_ds;
	tp->tf_edi = regs->r_edi;
	tp->tf_esi = regs->r_esi;
	tp->tf_ebp = regs->r_ebp;
	tp->tf_ebx = regs->r_ebx;
	tp->tf_edx = regs->r_edx;
	tp->tf_ecx = regs->r_ecx;
	tp->tf_eax = regs->r_eax;
	tp->tf_eip = regs->r_eip;
	tp->tf_cs = regs->r_cs;
	tp->tf_eflags = regs->r_eflags;
	tp->tf_esp = regs->r_esp;
	tp->tf_ss = regs->r_ss;
	pcb->pcb_gs = regs->r_gs;
	return (0);
}

#ifdef CPU_ENABLE_SSE
static void
fill_fpregs_xmm(sv_xmm, sv_87)
	struct savexmm *sv_xmm;
	struct save87 *sv_87;
{
	register struct env87 *penv_87 = &sv_87->sv_env;
	register struct envxmm *penv_xmm = &sv_xmm->sv_env;
	int i;

	bzero(sv_87, sizeof(*sv_87));

	/* FPU control/status */
	penv_87->en_cw = penv_xmm->en_cw;
	penv_87->en_sw = penv_xmm->en_sw;
	penv_87->en_tw = penv_xmm->en_tw;
	penv_87->en_fip = penv_xmm->en_fip;
	penv_87->en_fcs = penv_xmm->en_fcs;
	penv_87->en_opcode = penv_xmm->en_opcode;
	penv_87->en_foo = penv_xmm->en_foo;
	penv_87->en_fos = penv_xmm->en_fos;

	/* FPU registers */
	for (i = 0; i < 8; ++i)
		sv_87->sv_ac[i] = sv_xmm->sv_fp[i].fp_acc;
}

static void
set_fpregs_xmm(sv_87, sv_xmm)
	struct save87 *sv_87;
	struct savexmm *sv_xmm;
{
	register struct env87 *penv_87 = &sv_87->sv_env;
	register struct envxmm *penv_xmm = &sv_xmm->sv_env;
	int i;

	/* FPU control/status */
	penv_xmm->en_cw = penv_87->en_cw;
	penv_xmm->en_sw = penv_87->en_sw;
	penv_xmm->en_tw = penv_87->en_tw;
	penv_xmm->en_fip = penv_87->en_fip;
	penv_xmm->en_fcs = penv_87->en_fcs;
	penv_xmm->en_opcode = penv_87->en_opcode;
	penv_xmm->en_foo = penv_87->en_foo;
	penv_xmm->en_fos = penv_87->en_fos;

	/* FPU registers */
	for (i = 0; i < 8; ++i)
		sv_xmm->sv_fp[i].fp_acc = sv_87->sv_ac[i];
}
#endif /* CPU_ENABLE_SSE */

int
fill_fpregs(struct thread *td, struct fpreg *fpregs)
{

	KASSERT(td == curthread || TD_IS_SUSPENDED(td) ||
	    P_SHOULDSTOP(td->td_proc),
	    ("not suspended thread %p", td));
#ifdef DEV_NPX
	npxgetregs(td);
#else
	bzero(fpregs, sizeof(*fpregs));
#endif
#ifdef CPU_ENABLE_SSE
	if (cpu_fxsr)
		fill_fpregs_xmm(&td->td_pcb->pcb_user_save.sv_xmm,
		    (struct save87 *)fpregs);
	else
#endif /* CPU_ENABLE_SSE */
		bcopy(&td->td_pcb->pcb_user_save.sv_87, fpregs,
		    sizeof(*fpregs));
	return (0);
}

int
set_fpregs(struct thread *td, struct fpreg *fpregs)
{

#ifdef CPU_ENABLE_SSE
	if (cpu_fxsr)
		set_fpregs_xmm((struct save87 *)fpregs,
		    &td->td_pcb->pcb_user_save.sv_xmm);
	else
#endif /* CPU_ENABLE_SSE */
		bcopy(fpregs, &td->td_pcb->pcb_user_save.sv_87,
		    sizeof(*fpregs));
#ifdef DEV_NPX
	npxuserinited(td);
#endif
	return (0);
}

/*
 * Get machine context.
 */
int
get_mcontext(struct thread *td, mcontext_t *mcp, int flags)
{
	struct trapframe *tp;
	struct segment_descriptor *sdp;

	tp = td->td_frame;

	PROC_LOCK(curthread->td_proc);
	mcp->mc_onstack = sigonstack(tp->tf_esp);
	PROC_UNLOCK(curthread->td_proc);
	mcp->mc_gs = td->td_pcb->pcb_gs;
	mcp->mc_fs = tp->tf_fs;
	mcp->mc_es = tp->tf_es;
	mcp->mc_ds = tp->tf_ds;
	mcp->mc_edi = tp->tf_edi;
	mcp->mc_esi = tp->tf_esi;
	mcp->mc_ebp = tp->tf_ebp;
	mcp->mc_isp = tp->tf_isp;
	mcp->mc_eflags = tp->tf_eflags;
	if (flags & GET_MC_CLEAR_RET) {
		mcp->mc_eax = 0;
		mcp->mc_edx = 0;
		mcp->mc_eflags &= ~PSL_C;
	} else {
		mcp->mc_eax = tp->tf_eax;
		mcp->mc_edx = tp->tf_edx;
	}
	mcp->mc_ebx = tp->tf_ebx;
	mcp->mc_ecx = tp->tf_ecx;
	mcp->mc_eip = tp->tf_eip;
	mcp->mc_cs = tp->tf_cs;
	mcp->mc_esp = tp->tf_esp;
	mcp->mc_ss = tp->tf_ss;
	mcp->mc_len = sizeof(*mcp);
	get_fpcontext(td, mcp);
	sdp = &td->td_pcb->pcb_fsd;
	mcp->mc_fsbase = sdp->sd_hibase << 24 | sdp->sd_lobase;
	sdp = &td->td_pcb->pcb_gsd;
	mcp->mc_gsbase = sdp->sd_hibase << 24 | sdp->sd_lobase;
	mcp->mc_flags = 0;
	bzero(mcp->mc_spare2, sizeof(mcp->mc_spare2));
	return (0);
}

/*
 * Set machine context.
 *
 * However, we don't set any but the user modifiable flags, and we won't
 * touch the cs selector.
 */
int
set_mcontext(struct thread *td, const mcontext_t *mcp)
{
	struct trapframe *tp;
	int eflags, ret;

	tp = td->td_frame;
	if (mcp->mc_len != sizeof(*mcp))
		return (EINVAL);
	eflags = (mcp->mc_eflags & PSL_USERCHANGE) |
	    (tp->tf_eflags & ~PSL_USERCHANGE);
	if ((ret = set_fpcontext(td, mcp)) == 0) {
		tp->tf_fs = mcp->mc_fs;
		tp->tf_es = mcp->mc_es;
		tp->tf_ds = mcp->mc_ds;
		tp->tf_edi = mcp->mc_edi;
		tp->tf_esi = mcp->mc_esi;
		tp->tf_ebp = mcp->mc_ebp;
		tp->tf_ebx = mcp->mc_ebx;
		tp->tf_edx = mcp->mc_edx;
		tp->tf_ecx = mcp->mc_ecx;
		tp->tf_eax = mcp->mc_eax;
		tp->tf_eip = mcp->mc_eip;
		tp->tf_eflags = eflags;
		tp->tf_esp = mcp->mc_esp;
		tp->tf_ss = mcp->mc_ss;
		td->td_pcb->pcb_gs = mcp->mc_gs;
		ret = 0;
	}
	return (ret);
}

static void
get_fpcontext(struct thread *td, mcontext_t *mcp)
{

#ifndef DEV_NPX
	mcp->mc_fpformat = _MC_FPFMT_NODEV;
	mcp->mc_ownedfp = _MC_FPOWNED_NONE;
	bzero(mcp->mc_fpstate, sizeof(mcp->mc_fpstate));
#else
	mcp->mc_ownedfp = npxgetregs(td);
	bcopy(&td->td_pcb->pcb_user_save, &mcp->mc_fpstate,
	    sizeof(mcp->mc_fpstate));
	mcp->mc_fpformat = npxformat();
#endif
}

static int
set_fpcontext(struct thread *td, const mcontext_t *mcp)
{

	if (mcp->mc_fpformat == _MC_FPFMT_NODEV)
		return (0);
	else if (mcp->mc_fpformat != _MC_FPFMT_387 &&
	    mcp->mc_fpformat != _MC_FPFMT_XMM)
		return (EINVAL);
	else if (mcp->mc_ownedfp == _MC_FPOWNED_NONE)
		/* We don't care what state is left in the FPU or PCB. */
		fpstate_drop(td);
	else if (mcp->mc_ownedfp == _MC_FPOWNED_FPU ||
	    mcp->mc_ownedfp == _MC_FPOWNED_PCB) {
#ifdef DEV_NPX
#ifdef CPU_ENABLE_SSE
		if (cpu_fxsr)
			((union savefpu *)&mcp->mc_fpstate)->sv_xmm.sv_env.
			    en_mxcsr &= cpu_mxcsr_mask;
#endif
		npxsetregs(td, (union savefpu *)&mcp->mc_fpstate);
#endif
	} else
		return (EINVAL);
	return (0);
}

static void
fpstate_drop(struct thread *td)
{

	KASSERT(PCB_USER_FPU(td->td_pcb), ("fpstate_drop: kernel-owned fpu"));
	critical_enter();
#ifdef DEV_NPX
	if (PCPU_GET(fpcurthread) == td)
		npxdrop();
#endif
	/*
	 * XXX force a full drop of the npx.  The above only drops it if we
	 * owned it.  npxgetregs() has the same bug in the !cpu_fxsr case.
	 *
	 * XXX I don't much like npxgetregs()'s semantics of doing a full
	 * drop.  Dropping only to the pcb matches fnsave's behaviour.
	 * We only need to drop to !PCB_INITDONE in sendsig().  But
	 * sendsig() is the only caller of npxgetregs()... perhaps we just
	 * have too many layers.
	 */
	curthread->td_pcb->pcb_flags &= ~(PCB_NPXINITDONE |
	    PCB_NPXUSERINITDONE);
	critical_exit();
}

int
fill_dbregs(struct thread *td, struct dbreg *dbregs)
{
	struct pcb *pcb;

	if (td == NULL) {
		dbregs->dr[0] = rdr0();
		dbregs->dr[1] = rdr1();
		dbregs->dr[2] = rdr2();
		dbregs->dr[3] = rdr3();
		dbregs->dr[4] = rdr4();
		dbregs->dr[5] = rdr5();
		dbregs->dr[6] = rdr6();
		dbregs->dr[7] = rdr7();
	} else {
		pcb = td->td_pcb;
		dbregs->dr[0] = pcb->pcb_dr0;
		dbregs->dr[1] = pcb->pcb_dr1;
		dbregs->dr[2] = pcb->pcb_dr2;
		dbregs->dr[3] = pcb->pcb_dr3;
		dbregs->dr[4] = 0;
		dbregs->dr[5] = 0;
		dbregs->dr[6] = pcb->pcb_dr6;
		dbregs->dr[7] = pcb->pcb_dr7;
	}
	return (0);
}

int
set_dbregs(struct thread *td, struct dbreg *dbregs)
{
	struct pcb *pcb;
	int i;

	if (td == NULL) {
		load_dr0(dbregs->dr[0]);
		load_dr1(dbregs->dr[1]);
		load_dr2(dbregs->dr[2]);
		load_dr3(dbregs->dr[3]);
		load_dr4(dbregs->dr[4]);
		load_dr5(dbregs->dr[5]);
		load_dr6(dbregs->dr[6]);
		load_dr7(dbregs->dr[7]);
	} else {
		/*
		 * Don't let an illegal value for dr7 get set.	Specifically,
		 * check for undefined settings.  Setting these bit patterns
		 * result in undefined behaviour and can lead to an unexpected
		 * TRCTRAP.
		 */
		for (i = 0; i < 4; i++) {
			if (DBREG_DR7_ACCESS(dbregs->dr[7], i) == 0x02)
				return (EINVAL);
			if (DBREG_DR7_LEN(dbregs->dr[7], i) == 0x02)
				return (EINVAL);
		}
		
		pcb = td->td_pcb;
		
		/*
		 * Don't let a process set a breakpoint that is not within the
		 * process's address space.  If a process could do this, it
		 * could halt the system by setting a breakpoint in the kernel
		 * (if ddb was enabled).  Thus, we need to check to make sure
		 * that no breakpoints are being enabled for addresses outside
		 * process's address space.
		 *
		 * XXX - what about when the watched area of the user's
		 * address space is written into from within the kernel
		 * ... wouldn't that still cause a breakpoint to be generated
		 * from within kernel mode?
		 */

		if (DBREG_DR7_ENABLED(dbregs->dr[7], 0)) {
			/* dr0 is enabled */
			if (dbregs->dr[0] >= VM_MAXUSER_ADDRESS)
				return (EINVAL);
		}
			
		if (DBREG_DR7_ENABLED(dbregs->dr[7], 1)) {
			/* dr1 is enabled */
			if (dbregs->dr[1] >= VM_MAXUSER_ADDRESS)
				return (EINVAL);
		}
			
		if (DBREG_DR7_ENABLED(dbregs->dr[7], 2)) {
			/* dr2 is enabled */
			if (dbregs->dr[2] >= VM_MAXUSER_ADDRESS)
				return (EINVAL);
		}
			
		if (DBREG_DR7_ENABLED(dbregs->dr[7], 3)) {
			/* dr3 is enabled */
			if (dbregs->dr[3] >= VM_MAXUSER_ADDRESS)
				return (EINVAL);
		}

		pcb->pcb_dr0 = dbregs->dr[0];
		pcb->pcb_dr1 = dbregs->dr[1];
		pcb->pcb_dr2 = dbregs->dr[2];
		pcb->pcb_dr3 = dbregs->dr[3];
		pcb->pcb_dr6 = dbregs->dr[6];
		pcb->pcb_dr7 = dbregs->dr[7];

		pcb->pcb_flags |= PCB_DBREGS;
	}

	return (0);
}

/*
 * Return > 0 if a hardware breakpoint has been hit, and the
 * breakpoint was in user space.  Return 0, otherwise.
 */
int
user_dbreg_trap(void)
{
        u_int32_t dr7, dr6; /* debug registers dr6 and dr7 */
        u_int32_t bp;       /* breakpoint bits extracted from dr6 */
        int nbp;            /* number of breakpoints that triggered */
        caddr_t addr[4];    /* breakpoint addresses */
        int i;
        
        dr7 = rdr7();
        if ((dr7 & 0x000000ff) == 0) {
                /*
                 * all GE and LE bits in the dr7 register are zero,
                 * thus the trap couldn't have been caused by the
                 * hardware debug registers
                 */
                return 0;
        }

        nbp = 0;
        dr6 = rdr6();
        bp = dr6 & 0x0000000f;

        if (!bp) {
                /*
                 * None of the breakpoint bits are set meaning this
                 * trap was not caused by any of the debug registers
                 */
                return 0;
        }

        /*
         * at least one of the breakpoints were hit, check to see
         * which ones and if any of them are user space addresses
         */

        if (bp & 0x01) {
                addr[nbp++] = (caddr_t)rdr0();
        }
        if (bp & 0x02) {
                addr[nbp++] = (caddr_t)rdr1();
        }
        if (bp & 0x04) {
                addr[nbp++] = (caddr_t)rdr2();
        }
        if (bp & 0x08) {
                addr[nbp++] = (caddr_t)rdr3();
        }

        for (i = 0; i < nbp; i++) {
                if (addr[i] < (caddr_t)VM_MAXUSER_ADDRESS) {
                        /*
                         * addr[i] is in user space
                         */
                        return nbp;
                }
        }

        /*
         * None of the breakpoints are in user space.
         */
        return 0;
}

#ifdef KDB

/*
 * Provide inb() and outb() as functions.  They are normally only available as
 * inline functions, thus cannot be called from the debugger.
 */

/* silence compiler warnings */
u_char inb_(u_short);
void outb_(u_short, u_char);

u_char
inb_(u_short port)
{
	return inb(port);
}

void
outb_(u_short port, u_char data)
{
	outb(port, data);
}

#endif /* KDB */
