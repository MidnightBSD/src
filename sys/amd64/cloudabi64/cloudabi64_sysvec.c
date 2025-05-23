/*-
 * Copyright (c) 2015 Nuxi, https://nuxi.nl/
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/imgact.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/sysent.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/frame.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/vmparam.h>

#include <compat/cloudabi/cloudabi_util.h>

#include <compat/cloudabi64/cloudabi64_syscall.h>
#include <compat/cloudabi64/cloudabi64_util.h>

extern const char *cloudabi64_syscallnames[];
extern struct sysent cloudabi64_sysent[];

static int
cloudabi64_fixup_tcb(uintptr_t *stack_base, struct image_params *imgp)
{
	int error;
	register_t tcbptr;

	/* Place auxiliary vector and TCB on the stack. */
	error = cloudabi64_fixup(stack_base, imgp);
	if (error != 0)
		return (error);

	/*
	 * On x86-64, the TCB is referred to by %fs:0. Take some space
	 * from the top of the stack to store a single element array,
	 * containing a pointer to the TCB. %fs base will point to this.
	 */
	tcbptr = (register_t)*stack_base;
	*stack_base -= sizeof(tcbptr);
	return (copyout(&tcbptr, (void *)*stack_base, sizeof(tcbptr)));
}

static void
cloudabi64_proc_setregs(struct thread *td, struct image_params *imgp,
    uintptr_t stack)
{
	struct trapframe *regs;

	exec_setregs(td, imgp, stack);

	/*
	 * The stack now contains a pointer to the TCB, the TCB itself,
	 * and the auxiliary vector. Let %rdx point to the auxiliary
	 * vector, and set %fs base to the address of the TCB.
	 */
	regs = td->td_frame;
	regs->tf_rdi = stack + sizeof(register_t) +
	    roundup(sizeof(cloudabi64_tcb_t), sizeof(register_t));
	(void)cpu_set_user_tls(td, TO_PTR(stack));
}

static int
cloudabi64_fetch_syscall_args(struct thread *td)
{
	struct trapframe *frame;
	struct syscall_args *sa;

	frame = td->td_frame;
	sa = &td->td_sa;

	/* Obtain system call number. */
	sa->code = frame->tf_rax;
	if (sa->code >= CLOUDABI64_SYS_MAXSYSCALL)
		return (ENOSYS);
	sa->callp = &cloudabi64_sysent[sa->code];

	/* Fetch system call arguments. */
	sa->args[0] = frame->tf_rdi;
	sa->args[1] = frame->tf_rsi;
	sa->args[2] = frame->tf_rdx;
	sa->args[3] = frame->tf_rcx; /* Actually %r10. */
	sa->args[4] = frame->tf_r8;
	sa->args[5] = frame->tf_r9;

	/* Default system call return values. */
	td->td_retval[0] = 0;
	td->td_retval[1] = frame->tf_rdx;
	return (0);
}

static void
cloudabi64_set_syscall_retval(struct thread *td, int error)
{
	struct trapframe *frame = td->td_frame;

	switch (error) {
	case 0:
		/* System call succeeded. */
		frame->tf_rax = td->td_retval[0];
		frame->tf_rdx = td->td_retval[1];
		frame->tf_rflags &= ~PSL_C;
		break;
	case ERESTART:
		/* Restart system call. */
		frame->tf_rip -= frame->tf_err;
		frame->tf_r10 = frame->tf_rcx;
		set_pcb_flags(td->td_pcb, PCB_FULL_IRET);
		break;
	case EJUSTRETURN:
		break;
	default:
		/* System call returned an error. */
		frame->tf_rax = cloudabi_convert_errno(error);
		frame->tf_rflags |= PSL_C;
		break;
	}
}

static void
cloudabi64_schedtail(struct thread *td)
{
	struct trapframe *frame = td->td_frame;

	/* Initial register values for processes returning from fork. */
	frame->tf_rax = CLOUDABI_PROCESS_CHILD;
	frame->tf_rdx = td->td_tid;
}

int
cloudabi64_thread_setregs(struct thread *td,
    const cloudabi64_threadattr_t *attr, uint64_t tcb)
{
	struct trapframe *frame;
	stack_t stack;
	uint64_t tcbptr;
	int error;

	/*
	 * On x86-64, the TCB is referred to by %fs:0. Take some space
	 * from the top of the stack to store a single element array,
	 * containing a pointer to the TCB. %fs base will point to this.
	 */
	tcbptr = rounddown(attr->stack + attr->stack_len - sizeof(tcbptr),
	    _Alignof(tcbptr));
	error = copyout(&tcb, (void *)tcbptr, sizeof(tcb));
	if (error != 0)
		return (error);

	/* Perform standard register initialization. */
	stack.ss_sp = TO_PTR(attr->stack);
	stack.ss_size = tcbptr - attr->stack;
	cpu_set_upcall(td, TO_PTR(attr->entry_point), NULL, &stack);

	/*
	 * Pass in the thread ID of the new thread and the argument
	 * pointer provided by the parent thread in as arguments to the
	 * entry point.
	 */
	frame = td->td_frame;
	frame->tf_rdi = td->td_tid;
	frame->tf_rsi = attr->argument;

	return (cpu_set_user_tls(td, TO_PTR(tcbptr)));
}

static struct sysentvec cloudabi64_elf_sysvec = {
	.sv_size		= CLOUDABI64_SYS_MAXSYSCALL,
	.sv_table		= cloudabi64_sysent,
	.sv_fixup		= cloudabi64_fixup_tcb,
	.sv_name		= "CloudABI ELF64",
	.sv_coredump		= elf64_coredump,
	.sv_elf_core_osabi	= ELFOSABI_FREEBSD,
	.sv_elf_core_abi_vendor	= FREEBSD_ABI_VENDOR,
	.sv_elf_core_prepare_notes = elf64_prepare_notes,
	.sv_minuser		= VM_MIN_ADDRESS,
	/* Keep top page reserved to work around AMD Ryzen stability issues. */
	.sv_maxuser		= VM_MAXUSER_ADDRESS - PAGE_SIZE,
	.sv_stackprot		= VM_PROT_READ | VM_PROT_WRITE,
	.sv_copyout_strings	= cloudabi64_copyout_strings,
	.sv_setregs		= cloudabi64_proc_setregs,
	.sv_flags		= SV_ABI_CLOUDABI | SV_CAPSICUM | SV_LP64,
	.sv_set_syscall_retval	= cloudabi64_set_syscall_retval,
	.sv_fetch_syscall_args	= cloudabi64_fetch_syscall_args,
	.sv_syscallnames	= cloudabi64_syscallnames,
	.sv_schedtail		= cloudabi64_schedtail,
	.sv_set_fork_retval	= x86_set_fork_retval,
};

INIT_SYSENTVEC(elf_sysvec, &cloudabi64_elf_sysvec);

Elf64_Brandinfo cloudabi64_brand = {
	.brand		= ELFOSABI_CLOUDABI,
	.machine	= EM_X86_64,
	.sysvec		= &cloudabi64_elf_sysvec,
	.flags		= BI_CAN_EXEC_DYN | BI_BRAND_ONLY_STATIC,
};
