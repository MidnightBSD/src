/*-
 * Copyright (c) 2011 NetApp, Inc.
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
 *
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: release/10.0.0/sys/amd64/vmm/intel/vmx.c 256869 2013-10-22 00:58:51Z neel $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/10.0.0/sys/amd64/vmm/intel/vmx.c 256869 2013-10-22 00:58:51Z neel $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/smp.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/psl.h>
#include <machine/cpufunc.h>
#include <machine/md_var.h>
#include <machine/pmap.h>
#include <machine/segments.h>
#include <machine/specialreg.h>
#include <machine/vmparam.h>

#include <machine/vmm.h>
#include "vmm_host.h"
#include "vmm_lapic.h"
#include "vmm_msr.h"
#include "vmm_ktr.h"
#include "vmm_stat.h"

#include "vmx_msr.h"
#include "ept.h"
#include "vmx_cpufunc.h"
#include "vmx.h"
#include "x86.h"
#include "vmx_controls.h"

#define	PINBASED_CTLS_ONE_SETTING					\
	(PINBASED_EXTINT_EXITING	|				\
	 PINBASED_NMI_EXITING		|				\
	 PINBASED_VIRTUAL_NMI)
#define	PINBASED_CTLS_ZERO_SETTING	0

#define PROCBASED_CTLS_WINDOW_SETTING					\
	(PROCBASED_INT_WINDOW_EXITING	|				\
	 PROCBASED_NMI_WINDOW_EXITING)

#define	PROCBASED_CTLS_ONE_SETTING 					\
	(PROCBASED_SECONDARY_CONTROLS	|				\
	 PROCBASED_IO_EXITING		|				\
	 PROCBASED_MSR_BITMAPS		|				\
	 PROCBASED_CTLS_WINDOW_SETTING)
#define	PROCBASED_CTLS_ZERO_SETTING	\
	(PROCBASED_CR3_LOAD_EXITING |	\
	PROCBASED_CR3_STORE_EXITING |	\
	PROCBASED_IO_BITMAPS)

#define	PROCBASED_CTLS2_ONE_SETTING	PROCBASED2_ENABLE_EPT
#define	PROCBASED_CTLS2_ZERO_SETTING	0

#define VM_EXIT_CTLS_ONE_SETTING_NO_PAT					\
	(VM_EXIT_HOST_LMA			|			\
	VM_EXIT_SAVE_EFER			|			\
	VM_EXIT_LOAD_EFER)

#define	VM_EXIT_CTLS_ONE_SETTING					\
	(VM_EXIT_CTLS_ONE_SETTING_NO_PAT       	|			\
	VM_EXIT_SAVE_PAT			|			\
	VM_EXIT_LOAD_PAT)
#define	VM_EXIT_CTLS_ZERO_SETTING	VM_EXIT_SAVE_DEBUG_CONTROLS

#define	VM_ENTRY_CTLS_ONE_SETTING_NO_PAT	VM_ENTRY_LOAD_EFER

#define	VM_ENTRY_CTLS_ONE_SETTING					\
	(VM_ENTRY_CTLS_ONE_SETTING_NO_PAT     	|			\
	VM_ENTRY_LOAD_PAT)
#define	VM_ENTRY_CTLS_ZERO_SETTING					\
	(VM_ENTRY_LOAD_DEBUG_CONTROLS		|			\
	VM_ENTRY_INTO_SMM			|			\
	VM_ENTRY_DEACTIVATE_DUAL_MONITOR)

#define	guest_msr_rw(vmx, msr) \
	msr_bitmap_change_access((vmx)->msr_bitmap, (msr), MSR_BITMAP_ACCESS_RW)

#define	HANDLED		1
#define	UNHANDLED	0

MALLOC_DEFINE(M_VMX, "vmx", "vmx");

SYSCTL_DECL(_hw_vmm);
SYSCTL_NODE(_hw_vmm, OID_AUTO, vmx, CTLFLAG_RW, NULL, NULL);

int vmxon_enabled[MAXCPU];
static char vmxon_region[MAXCPU][PAGE_SIZE] __aligned(PAGE_SIZE);

static uint32_t pinbased_ctls, procbased_ctls, procbased_ctls2;
static uint32_t exit_ctls, entry_ctls;

static uint64_t cr0_ones_mask, cr0_zeros_mask;
SYSCTL_ULONG(_hw_vmm_vmx, OID_AUTO, cr0_ones_mask, CTLFLAG_RD,
	     &cr0_ones_mask, 0, NULL);
SYSCTL_ULONG(_hw_vmm_vmx, OID_AUTO, cr0_zeros_mask, CTLFLAG_RD,
	     &cr0_zeros_mask, 0, NULL);

static uint64_t cr4_ones_mask, cr4_zeros_mask;
SYSCTL_ULONG(_hw_vmm_vmx, OID_AUTO, cr4_ones_mask, CTLFLAG_RD,
	     &cr4_ones_mask, 0, NULL);
SYSCTL_ULONG(_hw_vmm_vmx, OID_AUTO, cr4_zeros_mask, CTLFLAG_RD,
	     &cr4_zeros_mask, 0, NULL);

static int vmx_no_patmsr;

static int vmx_initialized;
SYSCTL_INT(_hw_vmm_vmx, OID_AUTO, initialized, CTLFLAG_RD,
	   &vmx_initialized, 0, "Intel VMX initialized");

/*
 * Virtual NMI blocking conditions.
 *
 * Some processor implementations also require NMI to be blocked if
 * the STI_BLOCKING bit is set. It is possible to detect this at runtime
 * based on the (exit_reason,exit_qual) tuple being set to 
 * (EXIT_REASON_INVAL_VMCS, EXIT_QUAL_NMI_WHILE_STI_BLOCKING).
 *
 * We take the easy way out and also include STI_BLOCKING as one of the
 * gating items for vNMI injection.
 */
static uint64_t nmi_blocking_bits = VMCS_INTERRUPTIBILITY_MOVSS_BLOCKING |
				    VMCS_INTERRUPTIBILITY_NMI_BLOCKING |
				    VMCS_INTERRUPTIBILITY_STI_BLOCKING;

/*
 * Optional capabilities
 */
static int cap_halt_exit;
static int cap_pause_exit;
static int cap_unrestricted_guest;
static int cap_monitor_trap;
static int cap_invpcid;
 
static struct unrhdr *vpid_unr;
static u_int vpid_alloc_failed;
SYSCTL_UINT(_hw_vmm_vmx, OID_AUTO, vpid_alloc_failed, CTLFLAG_RD,
	    &vpid_alloc_failed, 0, NULL);

#ifdef KTR
static const char *
exit_reason_to_str(int reason)
{
	static char reasonbuf[32];

	switch (reason) {
	case EXIT_REASON_EXCEPTION:
		return "exception";
	case EXIT_REASON_EXT_INTR:
		return "extint";
	case EXIT_REASON_TRIPLE_FAULT:
		return "triplefault";
	case EXIT_REASON_INIT:
		return "init";
	case EXIT_REASON_SIPI:
		return "sipi";
	case EXIT_REASON_IO_SMI:
		return "iosmi";
	case EXIT_REASON_SMI:
		return "smi";
	case EXIT_REASON_INTR_WINDOW:
		return "intrwindow";
	case EXIT_REASON_NMI_WINDOW:
		return "nmiwindow";
	case EXIT_REASON_TASK_SWITCH:
		return "taskswitch";
	case EXIT_REASON_CPUID:
		return "cpuid";
	case EXIT_REASON_GETSEC:
		return "getsec";
	case EXIT_REASON_HLT:
		return "hlt";
	case EXIT_REASON_INVD:
		return "invd";
	case EXIT_REASON_INVLPG:
		return "invlpg";
	case EXIT_REASON_RDPMC:
		return "rdpmc";
	case EXIT_REASON_RDTSC:
		return "rdtsc";
	case EXIT_REASON_RSM:
		return "rsm";
	case EXIT_REASON_VMCALL:
		return "vmcall";
	case EXIT_REASON_VMCLEAR:
		return "vmclear";
	case EXIT_REASON_VMLAUNCH:
		return "vmlaunch";
	case EXIT_REASON_VMPTRLD:
		return "vmptrld";
	case EXIT_REASON_VMPTRST:
		return "vmptrst";
	case EXIT_REASON_VMREAD:
		return "vmread";
	case EXIT_REASON_VMRESUME:
		return "vmresume";
	case EXIT_REASON_VMWRITE:
		return "vmwrite";
	case EXIT_REASON_VMXOFF:
		return "vmxoff";
	case EXIT_REASON_VMXON:
		return "vmxon";
	case EXIT_REASON_CR_ACCESS:
		return "craccess";
	case EXIT_REASON_DR_ACCESS:
		return "draccess";
	case EXIT_REASON_INOUT:
		return "inout";
	case EXIT_REASON_RDMSR:
		return "rdmsr";
	case EXIT_REASON_WRMSR:
		return "wrmsr";
	case EXIT_REASON_INVAL_VMCS:
		return "invalvmcs";
	case EXIT_REASON_INVAL_MSR:
		return "invalmsr";
	case EXIT_REASON_MWAIT:
		return "mwait";
	case EXIT_REASON_MTF:
		return "mtf";
	case EXIT_REASON_MONITOR:
		return "monitor";
	case EXIT_REASON_PAUSE:
		return "pause";
	case EXIT_REASON_MCE:
		return "mce";
	case EXIT_REASON_TPR:
		return "tpr";
	case EXIT_REASON_APIC:
		return "apic";
	case EXIT_REASON_GDTR_IDTR:
		return "gdtridtr";
	case EXIT_REASON_LDTR_TR:
		return "ldtrtr";
	case EXIT_REASON_EPT_FAULT:
		return "eptfault";
	case EXIT_REASON_EPT_MISCONFIG:
		return "eptmisconfig";
	case EXIT_REASON_INVEPT:
		return "invept";
	case EXIT_REASON_RDTSCP:
		return "rdtscp";
	case EXIT_REASON_VMX_PREEMPT:
		return "vmxpreempt";
	case EXIT_REASON_INVVPID:
		return "invvpid";
	case EXIT_REASON_WBINVD:
		return "wbinvd";
	case EXIT_REASON_XSETBV:
		return "xsetbv";
	default:
		snprintf(reasonbuf, sizeof(reasonbuf), "%d", reason);
		return (reasonbuf);
	}
}

#ifdef SETJMP_TRACE
static const char *
vmx_setjmp_rc2str(int rc)
{
	switch (rc) {
	case VMX_RETURN_DIRECT:
		return "direct";
	case VMX_RETURN_LONGJMP:
		return "longjmp";
	case VMX_RETURN_VMRESUME:
		return "vmresume";
	case VMX_RETURN_VMLAUNCH:
		return "vmlaunch";
	case VMX_RETURN_AST:
		return "ast";
	default:
		return "unknown";
	}
}

#define	SETJMP_TRACE(vmx, vcpu, vmxctx, regname)			  \
	VMM_CTR1((vmx)->vm, (vcpu), "setjmp trace " #regname " 0x%016lx", \
		 (vmxctx)->regname)

static void
vmx_setjmp_trace(struct vmx *vmx, int vcpu, struct vmxctx *vmxctx, int rc)
{
	uint64_t host_rip, host_rsp;

	if (vmxctx != &vmx->ctx[vcpu])
		panic("vmx_setjmp_trace: invalid vmxctx %p; should be %p",
			vmxctx, &vmx->ctx[vcpu]);

	VMM_CTR1((vmx)->vm, (vcpu), "vmxctx = %p", vmxctx);
	VMM_CTR2((vmx)->vm, (vcpu), "setjmp return code %s(%d)",
		 vmx_setjmp_rc2str(rc), rc);

	host_rsp = host_rip = ~0;
	vmread(VMCS_HOST_RIP, &host_rip);
	vmread(VMCS_HOST_RSP, &host_rsp);
	VMM_CTR2((vmx)->vm, (vcpu), "vmcs host_rip 0x%016lx, host_rsp 0x%016lx",
		 host_rip, host_rsp);

	SETJMP_TRACE(vmx, vcpu, vmxctx, host_r15);
	SETJMP_TRACE(vmx, vcpu, vmxctx, host_r14);
	SETJMP_TRACE(vmx, vcpu, vmxctx, host_r13);
	SETJMP_TRACE(vmx, vcpu, vmxctx, host_r12);
	SETJMP_TRACE(vmx, vcpu, vmxctx, host_rbp);
	SETJMP_TRACE(vmx, vcpu, vmxctx, host_rsp);
	SETJMP_TRACE(vmx, vcpu, vmxctx, host_rbx);
	SETJMP_TRACE(vmx, vcpu, vmxctx, host_rip);

	SETJMP_TRACE(vmx, vcpu, vmxctx, guest_rdi);
	SETJMP_TRACE(vmx, vcpu, vmxctx, guest_rsi);
	SETJMP_TRACE(vmx, vcpu, vmxctx, guest_rdx);
	SETJMP_TRACE(vmx, vcpu, vmxctx, guest_rcx);
	SETJMP_TRACE(vmx, vcpu, vmxctx, guest_r8);
	SETJMP_TRACE(vmx, vcpu, vmxctx, guest_r9);
	SETJMP_TRACE(vmx, vcpu, vmxctx, guest_rax);
	SETJMP_TRACE(vmx, vcpu, vmxctx, guest_rbx);
	SETJMP_TRACE(vmx, vcpu, vmxctx, guest_rbp);
	SETJMP_TRACE(vmx, vcpu, vmxctx, guest_r10);
	SETJMP_TRACE(vmx, vcpu, vmxctx, guest_r11);
	SETJMP_TRACE(vmx, vcpu, vmxctx, guest_r12);
	SETJMP_TRACE(vmx, vcpu, vmxctx, guest_r13);
	SETJMP_TRACE(vmx, vcpu, vmxctx, guest_r14);
	SETJMP_TRACE(vmx, vcpu, vmxctx, guest_r15);
	SETJMP_TRACE(vmx, vcpu, vmxctx, guest_cr2);
}
#endif
#else
static void __inline
vmx_setjmp_trace(struct vmx *vmx, int vcpu, struct vmxctx *vmxctx, int rc)
{
	return;
}
#endif	/* KTR */

u_long
vmx_fix_cr0(u_long cr0)
{

	return ((cr0 | cr0_ones_mask) & ~cr0_zeros_mask);
}

u_long
vmx_fix_cr4(u_long cr4)
{

	return ((cr4 | cr4_ones_mask) & ~cr4_zeros_mask);
}

static void
vpid_free(int vpid)
{
	if (vpid < 0 || vpid > 0xffff)
		panic("vpid_free: invalid vpid %d", vpid);

	/*
	 * VPIDs [0,VM_MAXCPU] are special and are not allocated from
	 * the unit number allocator.
	 */

	if (vpid > VM_MAXCPU)
		free_unr(vpid_unr, vpid);
}

static void
vpid_alloc(uint16_t *vpid, int num)
{
	int i, x;

	if (num <= 0 || num > VM_MAXCPU)
		panic("invalid number of vpids requested: %d", num);

	/*
	 * If the "enable vpid" execution control is not enabled then the
	 * VPID is required to be 0 for all vcpus.
	 */
	if ((procbased_ctls2 & PROCBASED2_ENABLE_VPID) == 0) {
		for (i = 0; i < num; i++)
			vpid[i] = 0;
		return;
	}

	/*
	 * Allocate a unique VPID for each vcpu from the unit number allocator.
	 */
	for (i = 0; i < num; i++) {
		x = alloc_unr(vpid_unr);
		if (x == -1)
			break;
		else
			vpid[i] = x;
	}

	if (i < num) {
		atomic_add_int(&vpid_alloc_failed, 1);

		/*
		 * If the unit number allocator does not have enough unique
		 * VPIDs then we need to allocate from the [1,VM_MAXCPU] range.
		 *
		 * These VPIDs are not be unique across VMs but this does not
		 * affect correctness because the combined mappings are also
		 * tagged with the EP4TA which is unique for each VM.
		 *
		 * It is still sub-optimal because the invvpid will invalidate
		 * combined mappings for a particular VPID across all EP4TAs.
		 */
		while (i-- > 0)
			vpid_free(vpid[i]);

		for (i = 0; i < num; i++)
			vpid[i] = i + 1;
	}
}

static void
vpid_init(void)
{
	/*
	 * VPID 0 is required when the "enable VPID" execution control is
	 * disabled.
	 *
	 * VPIDs [1,VM_MAXCPU] are used as the "overflow namespace" when the
	 * unit number allocator does not have sufficient unique VPIDs to
	 * satisfy the allocation.
	 *
	 * The remaining VPIDs are managed by the unit number allocator.
	 */
	vpid_unr = new_unrhdr(VM_MAXCPU + 1, 0xffff, NULL);
}

static void
msr_save_area_init(struct msr_entry *g_area, int *g_count)
{
	int cnt;

	static struct msr_entry guest_msrs[] = {
		{ MSR_KGSBASE, 0, 0 },
	};

	cnt = sizeof(guest_msrs) / sizeof(guest_msrs[0]);
	if (cnt > GUEST_MSR_MAX_ENTRIES)
		panic("guest msr save area overrun");
	bcopy(guest_msrs, g_area, sizeof(guest_msrs));
	*g_count = cnt;
}

static void
vmx_disable(void *arg __unused)
{
	struct invvpid_desc invvpid_desc = { 0 };
	struct invept_desc invept_desc = { 0 };

	if (vmxon_enabled[curcpu]) {
		/*
		 * See sections 25.3.3.3 and 25.3.3.4 in Intel Vol 3b.
		 *
		 * VMXON or VMXOFF are not required to invalidate any TLB
		 * caching structures. This prevents potential retention of
		 * cached information in the TLB between distinct VMX episodes.
		 */
		invvpid(INVVPID_TYPE_ALL_CONTEXTS, invvpid_desc);
		invept(INVEPT_TYPE_ALL_CONTEXTS, invept_desc);
		vmxoff();
	}
	load_cr4(rcr4() & ~CR4_VMXE);
}

static int
vmx_cleanup(void)
{

	if (vpid_unr != NULL) {
		delete_unrhdr(vpid_unr);
		vpid_unr = NULL;
	}

	smp_rendezvous(NULL, vmx_disable, NULL, NULL);

	return (0);
}

static void
vmx_enable(void *arg __unused)
{
	int error;

	load_cr4(rcr4() | CR4_VMXE);

	*(uint32_t *)vmxon_region[curcpu] = vmx_revision();
	error = vmxon(vmxon_region[curcpu]);
	if (error == 0)
		vmxon_enabled[curcpu] = 1;
}

static int
vmx_init(void)
{
	int error;
	uint64_t fixed0, fixed1, feature_control;
	uint32_t tmp;

	/* CPUID.1:ECX[bit 5] must be 1 for processor to support VMX */
	if (!(cpu_feature2 & CPUID2_VMX)) {
		printf("vmx_init: processor does not support VMX operation\n");
		return (ENXIO);
	}

	/*
	 * Verify that MSR_IA32_FEATURE_CONTROL lock and VMXON enable bits
	 * are set (bits 0 and 2 respectively).
	 */
	feature_control = rdmsr(MSR_IA32_FEATURE_CONTROL);
	if ((feature_control & IA32_FEATURE_CONTROL_LOCK) == 0 ||
	    (feature_control & IA32_FEATURE_CONTROL_VMX_EN) == 0) {
		printf("vmx_init: VMX operation disabled by BIOS\n");
		return (ENXIO);
	}

	/* Check support for primary processor-based VM-execution controls */
	error = vmx_set_ctlreg(MSR_VMX_PROCBASED_CTLS,
			       MSR_VMX_TRUE_PROCBASED_CTLS,
			       PROCBASED_CTLS_ONE_SETTING,
			       PROCBASED_CTLS_ZERO_SETTING, &procbased_ctls);
	if (error) {
		printf("vmx_init: processor does not support desired primary "
		       "processor-based controls\n");
		return (error);
	}

	/* Clear the processor-based ctl bits that are set on demand */
	procbased_ctls &= ~PROCBASED_CTLS_WINDOW_SETTING;

	/* Check support for secondary processor-based VM-execution controls */
	error = vmx_set_ctlreg(MSR_VMX_PROCBASED_CTLS2,
			       MSR_VMX_PROCBASED_CTLS2,
			       PROCBASED_CTLS2_ONE_SETTING,
			       PROCBASED_CTLS2_ZERO_SETTING, &procbased_ctls2);
	if (error) {
		printf("vmx_init: processor does not support desired secondary "
		       "processor-based controls\n");
		return (error);
	}

	/* Check support for VPID */
	error = vmx_set_ctlreg(MSR_VMX_PROCBASED_CTLS2, MSR_VMX_PROCBASED_CTLS2,
			       PROCBASED2_ENABLE_VPID, 0, &tmp);
	if (error == 0)
		procbased_ctls2 |= PROCBASED2_ENABLE_VPID;

	/* Check support for pin-based VM-execution controls */
	error = vmx_set_ctlreg(MSR_VMX_PINBASED_CTLS,
			       MSR_VMX_TRUE_PINBASED_CTLS,
			       PINBASED_CTLS_ONE_SETTING,
			       PINBASED_CTLS_ZERO_SETTING, &pinbased_ctls);
	if (error) {
		printf("vmx_init: processor does not support desired "
		       "pin-based controls\n");
		return (error);
	}

	/* Check support for VM-exit controls */
	error = vmx_set_ctlreg(MSR_VMX_EXIT_CTLS, MSR_VMX_TRUE_EXIT_CTLS,
			       VM_EXIT_CTLS_ONE_SETTING,
			       VM_EXIT_CTLS_ZERO_SETTING,
			       &exit_ctls);
	if (error) {
		/* Try again without the PAT MSR bits */
		error = vmx_set_ctlreg(MSR_VMX_EXIT_CTLS,
				       MSR_VMX_TRUE_EXIT_CTLS,
				       VM_EXIT_CTLS_ONE_SETTING_NO_PAT,
				       VM_EXIT_CTLS_ZERO_SETTING,
				       &exit_ctls);
		if (error) {
			printf("vmx_init: processor does not support desired "
			       "exit controls\n");
			return (error);
		} else {
			if (bootverbose)
				printf("vmm: PAT MSR access not supported\n");
			guest_msr_valid(MSR_PAT);
			vmx_no_patmsr = 1;
		}
	}

	/* Check support for VM-entry controls */
	if (!vmx_no_patmsr) {
		error = vmx_set_ctlreg(MSR_VMX_ENTRY_CTLS,
				       MSR_VMX_TRUE_ENTRY_CTLS,
				       VM_ENTRY_CTLS_ONE_SETTING,
				       VM_ENTRY_CTLS_ZERO_SETTING,
				       &entry_ctls);
	} else {
		error = vmx_set_ctlreg(MSR_VMX_ENTRY_CTLS,
				       MSR_VMX_TRUE_ENTRY_CTLS,
				       VM_ENTRY_CTLS_ONE_SETTING_NO_PAT,
				       VM_ENTRY_CTLS_ZERO_SETTING,
				       &entry_ctls);
	}

	if (error) {
		printf("vmx_init: processor does not support desired "
		       "entry controls\n");
		       return (error);
	}

	/*
	 * Check support for optional features by testing them
	 * as individual bits
	 */
	cap_halt_exit = (vmx_set_ctlreg(MSR_VMX_PROCBASED_CTLS,
					MSR_VMX_TRUE_PROCBASED_CTLS,
					PROCBASED_HLT_EXITING, 0,
					&tmp) == 0);

	cap_monitor_trap = (vmx_set_ctlreg(MSR_VMX_PROCBASED_CTLS,
					MSR_VMX_PROCBASED_CTLS,
					PROCBASED_MTF, 0,
					&tmp) == 0);

	cap_pause_exit = (vmx_set_ctlreg(MSR_VMX_PROCBASED_CTLS,
					 MSR_VMX_TRUE_PROCBASED_CTLS,
					 PROCBASED_PAUSE_EXITING, 0,
					 &tmp) == 0);

	cap_unrestricted_guest = (vmx_set_ctlreg(MSR_VMX_PROCBASED_CTLS2,
					MSR_VMX_PROCBASED_CTLS2,
					PROCBASED2_UNRESTRICTED_GUEST, 0,
				        &tmp) == 0);

	cap_invpcid = (vmx_set_ctlreg(MSR_VMX_PROCBASED_CTLS2,
	    MSR_VMX_PROCBASED_CTLS2, PROCBASED2_ENABLE_INVPCID, 0,
	    &tmp) == 0);


	/* Initialize EPT */
	error = ept_init();
	if (error) {
		printf("vmx_init: ept initialization failed (%d)\n", error);
		return (error);
	}

	/*
	 * Stash the cr0 and cr4 bits that must be fixed to 0 or 1
	 */
	fixed0 = rdmsr(MSR_VMX_CR0_FIXED0);
	fixed1 = rdmsr(MSR_VMX_CR0_FIXED1);
	cr0_ones_mask = fixed0 & fixed1;
	cr0_zeros_mask = ~fixed0 & ~fixed1;

	/*
	 * CR0_PE and CR0_PG can be set to zero in VMX non-root operation
	 * if unrestricted guest execution is allowed.
	 */
	if (cap_unrestricted_guest)
		cr0_ones_mask &= ~(CR0_PG | CR0_PE);

	/*
	 * Do not allow the guest to set CR0_NW or CR0_CD.
	 */
	cr0_zeros_mask |= (CR0_NW | CR0_CD);

	fixed0 = rdmsr(MSR_VMX_CR4_FIXED0);
	fixed1 = rdmsr(MSR_VMX_CR4_FIXED1);
	cr4_ones_mask = fixed0 & fixed1;
	cr4_zeros_mask = ~fixed0 & ~fixed1;

	vpid_init();

	/* enable VMX operation */
	smp_rendezvous(NULL, vmx_enable, NULL, NULL);

	vmx_initialized = 1;

	return (0);
}

static int
vmx_setup_cr_shadow(int which, struct vmcs *vmcs, uint32_t initial)
{
	int error, mask_ident, shadow_ident;
	uint64_t mask_value;

	if (which != 0 && which != 4)
		panic("vmx_setup_cr_shadow: unknown cr%d", which);

	if (which == 0) {
		mask_ident = VMCS_CR0_MASK;
		mask_value = cr0_ones_mask | cr0_zeros_mask;
		shadow_ident = VMCS_CR0_SHADOW;
	} else {
		mask_ident = VMCS_CR4_MASK;
		mask_value = cr4_ones_mask | cr4_zeros_mask;
		shadow_ident = VMCS_CR4_SHADOW;
	}

	error = vmcs_setreg(vmcs, 0, VMCS_IDENT(mask_ident), mask_value);
	if (error)
		return (error);

	error = vmcs_setreg(vmcs, 0, VMCS_IDENT(shadow_ident), initial);
	if (error)
		return (error);

	return (0);
}
#define	vmx_setup_cr0_shadow(vmcs,init)	vmx_setup_cr_shadow(0, (vmcs), (init))
#define	vmx_setup_cr4_shadow(vmcs,init)	vmx_setup_cr_shadow(4, (vmcs), (init))

static void *
vmx_vminit(struct vm *vm, pmap_t pmap)
{
	uint16_t vpid[VM_MAXCPU];
	int i, error, guest_msr_count;
	struct vmx *vmx;

	vmx = malloc(sizeof(struct vmx), M_VMX, M_WAITOK | M_ZERO);
	if ((uintptr_t)vmx & PAGE_MASK) {
		panic("malloc of struct vmx not aligned on %d byte boundary",
		      PAGE_SIZE);
	}
	vmx->vm = vm;

	vmx->eptp = eptp(vtophys((vm_offset_t)pmap->pm_pml4));

	/*
	 * Clean up EPTP-tagged guest physical and combined mappings
	 *
	 * VMX transitions are not required to invalidate any guest physical
	 * mappings. So, it may be possible for stale guest physical mappings
	 * to be present in the processor TLBs.
	 *
	 * Combined mappings for this EP4TA are also invalidated for all VPIDs.
	 */
	ept_invalidate_mappings(vmx->eptp);

	msr_bitmap_initialize(vmx->msr_bitmap);

	/*
	 * It is safe to allow direct access to MSR_GSBASE and MSR_FSBASE.
	 * The guest FSBASE and GSBASE are saved and restored during
	 * vm-exit and vm-entry respectively. The host FSBASE and GSBASE are
	 * always restored from the vmcs host state area on vm-exit.
	 *
	 * The SYSENTER_CS/ESP/EIP MSRs are identical to FS/GSBASE in
	 * how they are saved/restored so can be directly accessed by the
	 * guest.
	 *
	 * Guest KGSBASE is saved and restored in the guest MSR save area.
	 * Host KGSBASE is restored before returning to userland from the pcb.
	 * There will be a window of time when we are executing in the host
	 * kernel context with a value of KGSBASE from the guest. This is ok
	 * because the value of KGSBASE is inconsequential in kernel context.
	 *
	 * MSR_EFER is saved and restored in the guest VMCS area on a
	 * VM exit and entry respectively. It is also restored from the
	 * host VMCS area on a VM exit.
	 */
	if (guest_msr_rw(vmx, MSR_GSBASE) ||
	    guest_msr_rw(vmx, MSR_FSBASE) ||
	    guest_msr_rw(vmx, MSR_SYSENTER_CS_MSR) ||
	    guest_msr_rw(vmx, MSR_SYSENTER_ESP_MSR) ||
	    guest_msr_rw(vmx, MSR_SYSENTER_EIP_MSR) ||
	    guest_msr_rw(vmx, MSR_KGSBASE) ||
	    guest_msr_rw(vmx, MSR_EFER))
		panic("vmx_vminit: error setting guest msr access");

	/*
	 * MSR_PAT is saved and restored in the guest VMCS are on a VM exit
	 * and entry respectively. It is also restored from the host VMCS
	 * area on a VM exit. However, if running on a system with no
	 * MSR_PAT save/restore support, leave access disabled so accesses
	 * will be trapped.
	 */
	if (!vmx_no_patmsr && guest_msr_rw(vmx, MSR_PAT))
		panic("vmx_vminit: error setting guest pat msr access");

	vpid_alloc(vpid, VM_MAXCPU);

	for (i = 0; i < VM_MAXCPU; i++) {
		vmx->vmcs[i].identifier = vmx_revision();
		error = vmclear(&vmx->vmcs[i]);
		if (error != 0) {
			panic("vmx_vminit: vmclear error %d on vcpu %d\n",
			      error, i);
		}

		error = vmcs_set_defaults(&vmx->vmcs[i],
					  (u_long)vmx_longjmp,
					  (u_long)&vmx->ctx[i],
					  vmx->eptp,
					  pinbased_ctls,
					  procbased_ctls,
					  procbased_ctls2,
					  exit_ctls, entry_ctls,
					  vtophys(vmx->msr_bitmap),
					  vpid[i]);

		if (error != 0)
			panic("vmx_vminit: vmcs_set_defaults error %d", error);

		vmx->cap[i].set = 0;
		vmx->cap[i].proc_ctls = procbased_ctls;
		vmx->cap[i].proc_ctls2 = procbased_ctls2;

		vmx->state[i].lastcpu = -1;
		vmx->state[i].vpid = vpid[i];

		msr_save_area_init(vmx->guest_msrs[i], &guest_msr_count);

		error = vmcs_set_msr_save(&vmx->vmcs[i],
					  vtophys(vmx->guest_msrs[i]),
					  guest_msr_count);
		if (error != 0)
			panic("vmcs_set_msr_save error %d", error);

		/*
		 * Set up the CR0/4 shadows, and init the read shadow
		 * to the power-on register value from the Intel Sys Arch.
		 *  CR0 - 0x60000010
		 *  CR4 - 0
		 */
		error = vmx_setup_cr0_shadow(&vmx->vmcs[i], 0x60000010);
		if (error != 0)
			panic("vmx_setup_cr0_shadow %d", error);

		error = vmx_setup_cr4_shadow(&vmx->vmcs[i], 0);
		if (error != 0)
			panic("vmx_setup_cr4_shadow %d", error);

		vmx->ctx[i].pmap = pmap;
		vmx->ctx[i].eptp = vmx->eptp;
	}

	return (vmx);
}

static int
vmx_handle_cpuid(struct vm *vm, int vcpu, struct vmxctx *vmxctx)
{
	int handled, func;
	
	func = vmxctx->guest_rax;

	handled = x86_emulate_cpuid(vm, vcpu,
				    (uint32_t*)(&vmxctx->guest_rax),
				    (uint32_t*)(&vmxctx->guest_rbx),
				    (uint32_t*)(&vmxctx->guest_rcx),
				    (uint32_t*)(&vmxctx->guest_rdx));
	return (handled);
}

static __inline void
vmx_run_trace(struct vmx *vmx, int vcpu)
{
#ifdef KTR
	VMM_CTR1(vmx->vm, vcpu, "Resume execution at 0x%0lx", vmcs_guest_rip());
#endif
}

static __inline void
vmx_exit_trace(struct vmx *vmx, int vcpu, uint64_t rip, uint32_t exit_reason,
	       int handled)
{
#ifdef KTR
	VMM_CTR3(vmx->vm, vcpu, "%s %s vmexit at 0x%0lx",
		 handled ? "handled" : "unhandled",
		 exit_reason_to_str(exit_reason), rip);
#endif
}

static __inline void
vmx_astpending_trace(struct vmx *vmx, int vcpu, uint64_t rip)
{
#ifdef KTR
	VMM_CTR1(vmx->vm, vcpu, "astpending vmexit at 0x%0lx", rip);
#endif
}

static int
vmx_set_pcpu_defaults(struct vmx *vmx, int vcpu)
{
	int error, lastcpu;
	struct vmxstate *vmxstate;
	struct invvpid_desc invvpid_desc = { 0 };

	vmxstate = &vmx->state[vcpu];
	lastcpu = vmxstate->lastcpu;
	vmxstate->lastcpu = curcpu;

	if (lastcpu == curcpu) {
		error = 0;
		goto done;
	}

	vmm_stat_incr(vmx->vm, vcpu, VCPU_MIGRATIONS, 1);

	error = vmwrite(VMCS_HOST_TR_BASE, vmm_get_host_trbase());
	if (error != 0)
		goto done;

	error = vmwrite(VMCS_HOST_GDTR_BASE, vmm_get_host_gdtrbase());
	if (error != 0)
		goto done;

	error = vmwrite(VMCS_HOST_GS_BASE, vmm_get_host_gsbase());
	if (error != 0)
		goto done;

	/*
	 * If we are using VPIDs then invalidate all mappings tagged with 'vpid'
	 *
	 * We do this because this vcpu was executing on a different host
	 * cpu when it last ran. We do not track whether it invalidated
	 * mappings associated with its 'vpid' during that run. So we must
	 * assume that the mappings associated with 'vpid' on 'curcpu' are
	 * stale and invalidate them.
	 *
	 * Note that we incur this penalty only when the scheduler chooses to
	 * move the thread associated with this vcpu between host cpus.
	 *
	 * Note also that this will invalidate mappings tagged with 'vpid'
	 * for "all" EP4TAs.
	 */
	if (vmxstate->vpid != 0) {
		invvpid_desc.vpid = vmxstate->vpid;
		invvpid(INVVPID_TYPE_SINGLE_CONTEXT, invvpid_desc);
	}
done:
	return (error);
}

static void 
vm_exit_update_rip(struct vm_exit *vmexit)
{
	int error;

	error = vmwrite(VMCS_GUEST_RIP, vmexit->rip + vmexit->inst_length);
	if (error)
		panic("vmx_run: error %d writing to VMCS_GUEST_RIP", error);
}

/*
 * We depend on 'procbased_ctls' to have the Interrupt Window Exiting bit set.
 */
CTASSERT((PROCBASED_CTLS_ONE_SETTING & PROCBASED_INT_WINDOW_EXITING) != 0);

static void __inline
vmx_set_int_window_exiting(struct vmx *vmx, int vcpu)
{
	int error;

	vmx->cap[vcpu].proc_ctls |= PROCBASED_INT_WINDOW_EXITING;

	error = vmwrite(VMCS_PRI_PROC_BASED_CTLS, vmx->cap[vcpu].proc_ctls);
	if (error)
		panic("vmx_set_int_window_exiting: vmwrite error %d", error);
}

static void __inline
vmx_clear_int_window_exiting(struct vmx *vmx, int vcpu)
{
	int error;

	vmx->cap[vcpu].proc_ctls &= ~PROCBASED_INT_WINDOW_EXITING;

	error = vmwrite(VMCS_PRI_PROC_BASED_CTLS, vmx->cap[vcpu].proc_ctls);
	if (error)
		panic("vmx_clear_int_window_exiting: vmwrite error %d", error);
}

static void __inline
vmx_set_nmi_window_exiting(struct vmx *vmx, int vcpu)
{
	int error;

	vmx->cap[vcpu].proc_ctls |= PROCBASED_NMI_WINDOW_EXITING;

	error = vmwrite(VMCS_PRI_PROC_BASED_CTLS, vmx->cap[vcpu].proc_ctls);
	if (error)
		panic("vmx_set_nmi_window_exiting: vmwrite error %d", error);
}

static void __inline
vmx_clear_nmi_window_exiting(struct vmx *vmx, int vcpu)
{
	int error;

	vmx->cap[vcpu].proc_ctls &= ~PROCBASED_NMI_WINDOW_EXITING;

	error = vmwrite(VMCS_PRI_PROC_BASED_CTLS, vmx->cap[vcpu].proc_ctls);
	if (error)
		panic("vmx_clear_nmi_window_exiting: vmwrite error %d", error);
}

static int
vmx_inject_nmi(struct vmx *vmx, int vcpu)
{
	int error;
	uint64_t info, interruptibility;

	/* Bail out if no NMI requested */
	if (!vm_nmi_pending(vmx->vm, vcpu))
		return (0);

	error = vmread(VMCS_GUEST_INTERRUPTIBILITY, &interruptibility);
	if (error) {
		panic("vmx_inject_nmi: vmread(interruptibility) %d",
			error);
	}
	if (interruptibility & nmi_blocking_bits)
		goto nmiblocked;

	/*
	 * Inject the virtual NMI. The vector must be the NMI IDT entry
	 * or the VMCS entry check will fail.
	 */
	info = VMCS_INTERRUPTION_INFO_NMI | VMCS_INTERRUPTION_INFO_VALID;
	info |= IDT_NMI;

	error = vmwrite(VMCS_ENTRY_INTR_INFO, info);
	if (error)
		panic("vmx_inject_nmi: vmwrite(intrinfo) %d", error);

	VMM_CTR0(vmx->vm, vcpu, "Injecting vNMI");

	/* Clear the request */
	vm_nmi_clear(vmx->vm, vcpu);
	return (1);

nmiblocked:
	/*
	 * Set the NMI Window Exiting execution control so we can inject
	 * the virtual NMI as soon as blocking condition goes away.
	 */
	vmx_set_nmi_window_exiting(vmx, vcpu);

	VMM_CTR0(vmx->vm, vcpu, "Enabling NMI window exiting");
	return (1);
}

static void
vmx_inject_interrupts(struct vmx *vmx, int vcpu)
{
	int error, vector;
	uint64_t info, rflags, interruptibility;

	const int HWINTR_BLOCKED = VMCS_INTERRUPTIBILITY_STI_BLOCKING |
				   VMCS_INTERRUPTIBILITY_MOVSS_BLOCKING;

	/*
	 * If there is already an interrupt pending then just return.
	 *
	 * This could happen if an interrupt was injected on a prior
	 * VM entry but the actual entry into guest mode was aborted
	 * because of a pending AST.
	 */
	error = vmread(VMCS_ENTRY_INTR_INFO, &info);
	if (error)
		panic("vmx_inject_interrupts: vmread(intrinfo) %d", error);
	if (info & VMCS_INTERRUPTION_INFO_VALID)
		return;

	/*
	 * NMI injection has priority so deal with those first
	 */
	if (vmx_inject_nmi(vmx, vcpu))
		return;

	/* Ask the local apic for a vector to inject */
	vector = lapic_pending_intr(vmx->vm, vcpu);
	if (vector < 0)
		return;

	if (vector < 32 || vector > 255)
		panic("vmx_inject_interrupts: invalid vector %d\n", vector);

	/* Check RFLAGS.IF and the interruptibility state of the guest */
	error = vmread(VMCS_GUEST_RFLAGS, &rflags);
	if (error)
		panic("vmx_inject_interrupts: vmread(rflags) %d", error);

	if ((rflags & PSL_I) == 0)
		goto cantinject;

	error = vmread(VMCS_GUEST_INTERRUPTIBILITY, &interruptibility);
	if (error) {
		panic("vmx_inject_interrupts: vmread(interruptibility) %d",
			error);
	}
	if (interruptibility & HWINTR_BLOCKED)
		goto cantinject;

	/* Inject the interrupt */
	info = VMCS_INTERRUPTION_INFO_HW_INTR | VMCS_INTERRUPTION_INFO_VALID;
	info |= vector;
	error = vmwrite(VMCS_ENTRY_INTR_INFO, info);
	if (error)
		panic("vmx_inject_interrupts: vmwrite(intrinfo) %d", error);

	/* Update the Local APIC ISR */
	lapic_intr_accepted(vmx->vm, vcpu, vector);

	VMM_CTR1(vmx->vm, vcpu, "Injecting hwintr at vector %d", vector);

	return;

cantinject:
	/*
	 * Set the Interrupt Window Exiting execution control so we can inject
	 * the interrupt as soon as blocking condition goes away.
	 */
	vmx_set_int_window_exiting(vmx, vcpu);

	VMM_CTR0(vmx->vm, vcpu, "Enabling interrupt window exiting");
}

static int
vmx_emulate_cr_access(struct vmx *vmx, int vcpu, uint64_t exitqual)
{
	int error, cr, vmcs_guest_cr, vmcs_shadow_cr;
	uint64_t crval, regval, ones_mask, zeros_mask;
	const struct vmxctx *vmxctx;

	/* We only handle mov to %cr0 or %cr4 at this time */
	if ((exitqual & 0xf0) != 0x00)
		return (UNHANDLED);

	cr = exitqual & 0xf;
	if (cr != 0 && cr != 4)
		return (UNHANDLED);

	vmxctx = &vmx->ctx[vcpu];

	/*
	 * We must use vmwrite() directly here because vmcs_setreg() will
	 * call vmclear(vmcs) as a side-effect which we certainly don't want.
	 */
	switch ((exitqual >> 8) & 0xf) {
	case 0:
		regval = vmxctx->guest_rax;
		break;
	case 1:
		regval = vmxctx->guest_rcx;
		break;
	case 2:
		regval = vmxctx->guest_rdx;
		break;
	case 3:
		regval = vmxctx->guest_rbx;
		break;
	case 4:
		error = vmread(VMCS_GUEST_RSP, &regval);
		if (error) {
			panic("vmx_emulate_cr_access: "
			      "error %d reading guest rsp", error);
		}
		break;
	case 5:
		regval = vmxctx->guest_rbp;
		break;
	case 6:
		regval = vmxctx->guest_rsi;
		break;
	case 7:
		regval = vmxctx->guest_rdi;
		break;
	case 8:
		regval = vmxctx->guest_r8;
		break;
	case 9:
		regval = vmxctx->guest_r9;
		break;
	case 10:
		regval = vmxctx->guest_r10;
		break;
	case 11:
		regval = vmxctx->guest_r11;
		break;
	case 12:
		regval = vmxctx->guest_r12;
		break;
	case 13:
		regval = vmxctx->guest_r13;
		break;
	case 14:
		regval = vmxctx->guest_r14;
		break;
	case 15:
		regval = vmxctx->guest_r15;
		break;
	}

	if (cr == 0) {
		ones_mask = cr0_ones_mask;
		zeros_mask = cr0_zeros_mask;
		vmcs_guest_cr = VMCS_GUEST_CR0;
		vmcs_shadow_cr = VMCS_CR0_SHADOW;
	} else {
		ones_mask = cr4_ones_mask;
		zeros_mask = cr4_zeros_mask;
		vmcs_guest_cr = VMCS_GUEST_CR4;
		vmcs_shadow_cr = VMCS_CR4_SHADOW;
	}

	error = vmwrite(vmcs_shadow_cr, regval);
	if (error) {
		panic("vmx_emulate_cr_access: error %d writing cr%d shadow",
		      error, cr);
	}

	crval = regval | ones_mask;
	crval &= ~zeros_mask;
	error = vmwrite(vmcs_guest_cr, crval);
	if (error) {
		panic("vmx_emulate_cr_access: error %d writing cr%d",
		      error, cr);
	}

	if (cr == 0 && regval & CR0_PG) {
		uint64_t efer, entry_ctls;

		/*
		 * If CR0.PG is 1 and EFER.LME is 1 then EFER.LMA and
		 * the "IA-32e mode guest" bit in VM-entry control must be
		 * equal.
		 */
		error = vmread(VMCS_GUEST_IA32_EFER, &efer);
		if (error) {
		  panic("vmx_emulate_cr_access: error %d efer read",
			error);			
		}
		if (efer & EFER_LME) {
			efer |= EFER_LMA;
			error = vmwrite(VMCS_GUEST_IA32_EFER, efer);
			if (error) {
				panic("vmx_emulate_cr_access: error %d"
				      " efer write", error);
			}
			error = vmread(VMCS_ENTRY_CTLS, &entry_ctls);
			if (error) {
				panic("vmx_emulate_cr_access: error %d"
				      " entry ctls read", error);
			}
			entry_ctls |= VM_ENTRY_GUEST_LMA;
			error = vmwrite(VMCS_ENTRY_CTLS, entry_ctls);
			if (error) {
				panic("vmx_emulate_cr_access: error %d"
				      " entry ctls write", error);
			}
		}
	}

	return (HANDLED);
}

static int
ept_fault_type(uint64_t ept_qual)
{
	int fault_type;

	if (ept_qual & EPT_VIOLATION_DATA_WRITE)
		fault_type = VM_PROT_WRITE;
	else if (ept_qual & EPT_VIOLATION_INST_FETCH)
		fault_type = VM_PROT_EXECUTE;
	else
		fault_type= VM_PROT_READ;

	return (fault_type);
}

static int
ept_protection(uint64_t ept_qual)
{
	int prot = 0;

	if (ept_qual & EPT_VIOLATION_GPA_READABLE)
		prot |= VM_PROT_READ;
	if (ept_qual & EPT_VIOLATION_GPA_WRITEABLE)
		prot |= VM_PROT_WRITE;
	if (ept_qual & EPT_VIOLATION_GPA_EXECUTABLE)
		prot |= VM_PROT_EXECUTE;

	return (prot);
}

static boolean_t
ept_emulation_fault(uint64_t ept_qual)
{
	int read, write;

	/* EPT fault on an instruction fetch doesn't make sense here */
	if (ept_qual & EPT_VIOLATION_INST_FETCH)
		return (FALSE);

	/* EPT fault must be a read fault or a write fault */
	read = ept_qual & EPT_VIOLATION_DATA_READ ? 1 : 0;
	write = ept_qual & EPT_VIOLATION_DATA_WRITE ? 1 : 0;
	if ((read | write) == 0)
		return (FALSE);

	/*
	 * The EPT violation must have been caused by accessing a
	 * guest-physical address that is a translation of a guest-linear
	 * address.
	 */
	if ((ept_qual & EPT_VIOLATION_GLA_VALID) == 0 ||
	    (ept_qual & EPT_VIOLATION_XLAT_VALID) == 0) {
		return (FALSE);
	}

	return (TRUE);
}

static int
vmx_exit_process(struct vmx *vmx, int vcpu, struct vm_exit *vmexit)
{
	int error, handled;
	struct vmcs *vmcs;
	struct vmxctx *vmxctx;
	uint32_t eax, ecx, edx, idtvec_info, idtvec_err, reason;
	uint64_t qual, gpa;

	handled = 0;
	vmcs = &vmx->vmcs[vcpu];
	vmxctx = &vmx->ctx[vcpu];
	qual = vmexit->u.vmx.exit_qualification;
	reason = vmexit->u.vmx.exit_reason;
	vmexit->exitcode = VM_EXITCODE_BOGUS;

	vmm_stat_incr(vmx->vm, vcpu, VMEXIT_COUNT, 1);

	/*
	 * VM exits that could be triggered during event injection on the
	 * previous VM entry need to be handled specially by re-injecting
	 * the event.
	 *
	 * See "Information for VM Exits During Event Delivery" in Intel SDM
	 * for details.
	 */
	switch (reason) {
	case EXIT_REASON_EPT_FAULT:
	case EXIT_REASON_EPT_MISCONFIG:
	case EXIT_REASON_APIC:
	case EXIT_REASON_TASK_SWITCH:
	case EXIT_REASON_EXCEPTION:
		idtvec_info = vmcs_idt_vectoring_info();
		if (idtvec_info & VMCS_IDT_VEC_VALID) {
			idtvec_info &= ~(1 << 12); /* clear undefined bit */
			vmwrite(VMCS_ENTRY_INTR_INFO, idtvec_info);
			if (idtvec_info & VMCS_IDT_VEC_ERRCODE_VALID) {
				idtvec_err = vmcs_idt_vectoring_err();
				vmwrite(VMCS_ENTRY_EXCEPTION_ERROR, idtvec_err);
			}
			vmwrite(VMCS_ENTRY_INST_LENGTH, vmexit->inst_length);
		}
	default:
		break;
	}

	switch (reason) {
	case EXIT_REASON_CR_ACCESS:
		vmm_stat_incr(vmx->vm, vcpu, VMEXIT_CR_ACCESS, 1);
		handled = vmx_emulate_cr_access(vmx, vcpu, qual);
		break;
	case EXIT_REASON_RDMSR:
		vmm_stat_incr(vmx->vm, vcpu, VMEXIT_RDMSR, 1);
		ecx = vmxctx->guest_rcx;
		error = emulate_rdmsr(vmx->vm, vcpu, ecx);
		if (error) {
			vmexit->exitcode = VM_EXITCODE_RDMSR;
			vmexit->u.msr.code = ecx;
		} else
			handled = 1;
		break;
	case EXIT_REASON_WRMSR:
		vmm_stat_incr(vmx->vm, vcpu, VMEXIT_WRMSR, 1);
		eax = vmxctx->guest_rax;
		ecx = vmxctx->guest_rcx;
		edx = vmxctx->guest_rdx;
		error = emulate_wrmsr(vmx->vm, vcpu, ecx,
					(uint64_t)edx << 32 | eax);
		if (error) {
			vmexit->exitcode = VM_EXITCODE_WRMSR;
			vmexit->u.msr.code = ecx;
			vmexit->u.msr.wval = (uint64_t)edx << 32 | eax;
		} else
			handled = 1;
		break;
	case EXIT_REASON_HLT:
		vmm_stat_incr(vmx->vm, vcpu, VMEXIT_HLT, 1);
		vmexit->exitcode = VM_EXITCODE_HLT;
		break;
	case EXIT_REASON_MTF:
		vmm_stat_incr(vmx->vm, vcpu, VMEXIT_MTRAP, 1);
		vmexit->exitcode = VM_EXITCODE_MTRAP;
		break;
	case EXIT_REASON_PAUSE:
		vmm_stat_incr(vmx->vm, vcpu, VMEXIT_PAUSE, 1);
		vmexit->exitcode = VM_EXITCODE_PAUSE;
		break;
	case EXIT_REASON_INTR_WINDOW:
		vmm_stat_incr(vmx->vm, vcpu, VMEXIT_INTR_WINDOW, 1);
		vmx_clear_int_window_exiting(vmx, vcpu);
		VMM_CTR0(vmx->vm, vcpu, "Disabling interrupt window exiting");
		return (1);
	case EXIT_REASON_EXT_INTR:
		/*
		 * External interrupts serve only to cause VM exits and allow
		 * the host interrupt handler to run.
		 *
		 * If this external interrupt triggers a virtual interrupt
		 * to a VM, then that state will be recorded by the
		 * host interrupt handler in the VM's softc. We will inject
		 * this virtual interrupt during the subsequent VM enter.
		 */

		/*
		 * This is special. We want to treat this as an 'handled'
		 * VM-exit but not increment the instruction pointer.
		 */
		vmm_stat_incr(vmx->vm, vcpu, VMEXIT_EXTINT, 1);
		return (1);
	case EXIT_REASON_NMI_WINDOW:
		/* Exit to allow the pending virtual NMI to be injected */
		vmm_stat_incr(vmx->vm, vcpu, VMEXIT_NMI_WINDOW, 1);
		vmx_clear_nmi_window_exiting(vmx, vcpu);
		VMM_CTR0(vmx->vm, vcpu, "Disabling NMI window exiting");
		return (1);
	case EXIT_REASON_INOUT:
		vmm_stat_incr(vmx->vm, vcpu, VMEXIT_INOUT, 1);
		vmexit->exitcode = VM_EXITCODE_INOUT;
		vmexit->u.inout.bytes = (qual & 0x7) + 1;
		vmexit->u.inout.in = (qual & 0x8) ? 1 : 0;
		vmexit->u.inout.string = (qual & 0x10) ? 1 : 0;
		vmexit->u.inout.rep = (qual & 0x20) ? 1 : 0;
		vmexit->u.inout.port = (uint16_t)(qual >> 16);
		vmexit->u.inout.eax = (uint32_t)(vmxctx->guest_rax);
		break;
	case EXIT_REASON_CPUID:
		vmm_stat_incr(vmx->vm, vcpu, VMEXIT_CPUID, 1);
		handled = vmx_handle_cpuid(vmx->vm, vcpu, vmxctx);
		break;
	case EXIT_REASON_EPT_FAULT:
		vmm_stat_incr(vmx->vm, vcpu, VMEXIT_EPT_FAULT, 1);
		/*
		 * If 'gpa' lies within the address space allocated to
		 * memory then this must be a nested page fault otherwise
		 * this must be an instruction that accesses MMIO space.
		 */
		gpa = vmcs_gpa();
		if (vm_mem_allocated(vmx->vm, gpa)) {
			vmexit->exitcode = VM_EXITCODE_PAGING;
			vmexit->u.paging.gpa = gpa;
			vmexit->u.paging.fault_type = ept_fault_type(qual);
			vmexit->u.paging.protection = ept_protection(qual);
		} else if (ept_emulation_fault(qual)) {
			vmexit->exitcode = VM_EXITCODE_INST_EMUL;
			vmexit->u.inst_emul.gpa = gpa;
			vmexit->u.inst_emul.gla = vmcs_gla();
			vmexit->u.inst_emul.cr3 = vmcs_guest_cr3();
		}
		break;
	default:
		vmm_stat_incr(vmx->vm, vcpu, VMEXIT_UNKNOWN, 1);
		break;
	}

	if (handled) {
		/*
		 * It is possible that control is returned to userland
		 * even though we were able to handle the VM exit in the
		 * kernel.
		 *
		 * In such a case we want to make sure that the userland
		 * restarts guest execution at the instruction *after*
		 * the one we just processed. Therefore we update the
		 * guest rip in the VMCS and in 'vmexit'.
		 */
		vm_exit_update_rip(vmexit);
		vmexit->rip += vmexit->inst_length;
		vmexit->inst_length = 0;
	} else {
		if (vmexit->exitcode == VM_EXITCODE_BOGUS) {
			/*
			 * If this VM exit was not claimed by anybody then
			 * treat it as a generic VMX exit.
			 */
			vmexit->exitcode = VM_EXITCODE_VMX;
			vmexit->u.vmx.error = 0;
		} else {
			/*
			 * The exitcode and collateral have been populated.
			 * The VM exit will be processed further in userland.
			 */
		}
	}
	return (handled);
}

static int
vmx_run(void *arg, int vcpu, register_t rip, pmap_t pmap)
{
	int error, vie, rc, handled, astpending;
	uint32_t exit_reason;
	struct vmx *vmx;
	struct vmxctx *vmxctx;
	struct vmcs *vmcs;
	struct vm_exit *vmexit;

	vmx = arg;
	vmcs = &vmx->vmcs[vcpu];
	vmxctx = &vmx->ctx[vcpu];
	vmxctx->launched = 0;

	astpending = 0;
	vmexit = vm_exitinfo(vmx->vm, vcpu);

	KASSERT(vmxctx->pmap == pmap,
	    ("pmap %p different than ctx pmap %p", pmap, vmxctx->pmap));
	KASSERT(vmxctx->eptp == vmx->eptp,
	    ("eptp %p different than ctx eptp %#lx", eptp, vmxctx->eptp));

	/*
	 * XXX Can we avoid doing this every time we do a vm run?
	 */
	VMPTRLD(vmcs);

	/*
	 * XXX
	 * We do this every time because we may setup the virtual machine
	 * from a different process than the one that actually runs it.
	 *
	 * If the life of a virtual machine was spent entirely in the context
	 * of a single process we could do this once in vmcs_set_defaults().
	 */
	if ((error = vmwrite(VMCS_HOST_CR3, rcr3())) != 0)
		panic("vmx_run: error %d writing to VMCS_HOST_CR3", error);

	if ((error = vmwrite(VMCS_GUEST_RIP, rip)) != 0)
		panic("vmx_run: error %d writing to VMCS_GUEST_RIP", error);

	if ((error = vmx_set_pcpu_defaults(vmx, vcpu)) != 0)
		panic("vmx_run: error %d setting up pcpu defaults", error);

	do {
		lapic_timer_tick(vmx->vm, vcpu);
		vmx_inject_interrupts(vmx, vcpu);
		vmx_run_trace(vmx, vcpu);
		rc = vmx_setjmp(vmxctx);
#ifdef SETJMP_TRACE
		vmx_setjmp_trace(vmx, vcpu, vmxctx, rc);
#endif
		switch (rc) {
		case VMX_RETURN_DIRECT:
			if (vmxctx->launched == 0) {
				vmxctx->launched = 1;
				vmx_launch(vmxctx);
			} else
				vmx_resume(vmxctx);
			panic("vmx_launch/resume should not return");
			break;
		case VMX_RETURN_LONGJMP:
			break;			/* vm exit */
		case VMX_RETURN_AST:
			astpending = 1;
			break;
		case VMX_RETURN_VMRESUME:
			vie = vmcs_instruction_error();
			if (vmxctx->launch_error == VM_FAIL_INVALID ||
			    vie != VMRESUME_WITH_NON_LAUNCHED_VMCS) {
				printf("vmresume error %d vmcs inst error %d\n",
					vmxctx->launch_error, vie);
				goto err_exit;
			}
			vmx_launch(vmxctx);	/* try to launch the guest */
			panic("vmx_launch should not return");
			break;
		case VMX_RETURN_VMLAUNCH:
			vie = vmcs_instruction_error();
#if 1
			printf("vmlaunch error %d vmcs inst error %d\n",
				vmxctx->launch_error, vie);
#endif
			goto err_exit;
		case VMX_RETURN_INVEPT:
			panic("vm %s:%d invept error %d",
			      vm_name(vmx->vm), vcpu, vmxctx->launch_error);
		default:
			panic("vmx_setjmp returned %d", rc);
		}
		
		/* enable interrupts */
		enable_intr();

		/* collect some basic information for VM exit processing */
		vmexit->rip = rip = vmcs_guest_rip();
		vmexit->inst_length = vmexit_instruction_length();
		vmexit->u.vmx.exit_reason = exit_reason = vmcs_exit_reason();
		vmexit->u.vmx.exit_qualification = vmcs_exit_qualification();

		if (astpending) {
			handled = 1;
			vmexit->inst_length = 0;
			vmexit->exitcode = VM_EXITCODE_BOGUS;
			vmx_astpending_trace(vmx, vcpu, rip);
			vmm_stat_incr(vmx->vm, vcpu, VMEXIT_ASTPENDING, 1);
			break;
		}

		handled = vmx_exit_process(vmx, vcpu, vmexit);
		vmx_exit_trace(vmx, vcpu, rip, exit_reason, handled);

	} while (handled);

	/*
	 * If a VM exit has been handled then the exitcode must be BOGUS
	 * If a VM exit is not handled then the exitcode must not be BOGUS
	 */
	if ((handled && vmexit->exitcode != VM_EXITCODE_BOGUS) ||
	    (!handled && vmexit->exitcode == VM_EXITCODE_BOGUS)) {
		panic("Mismatch between handled (%d) and exitcode (%d)",
		      handled, vmexit->exitcode);
	}

	if (!handled)
		vmm_stat_incr(vmx->vm, vcpu, VMEXIT_USERSPACE, 1);

	VMM_CTR1(vmx->vm, vcpu, "goto userland: exitcode %d",vmexit->exitcode);

	/*
	 * XXX
	 * We need to do this to ensure that any VMCS state cached by the
	 * processor is flushed to memory. We need to do this in case the
	 * VM moves to a different cpu the next time it runs.
	 *
	 * Can we avoid doing this?
	 */
	VMCLEAR(vmcs);
	return (0);

err_exit:
	vmexit->exitcode = VM_EXITCODE_VMX;
	vmexit->u.vmx.exit_reason = (uint32_t)-1;
	vmexit->u.vmx.exit_qualification = (uint32_t)-1;
	vmexit->u.vmx.error = vie;
	VMCLEAR(vmcs);
	return (ENOEXEC);
}

static void
vmx_vmcleanup(void *arg)
{
	int i, error;
	struct vmx *vmx = arg;

	for (i = 0; i < VM_MAXCPU; i++)
		vpid_free(vmx->state[i].vpid);

	/*
	 * XXXSMP we also need to clear the VMCS active on the other vcpus.
	 */
	error = vmclear(&vmx->vmcs[0]);
	if (error != 0)
		panic("vmx_vmcleanup: vmclear error %d on vcpu 0", error);

	free(vmx, M_VMX);

	return;
}

static register_t *
vmxctx_regptr(struct vmxctx *vmxctx, int reg)
{

	switch (reg) {
	case VM_REG_GUEST_RAX:
		return (&vmxctx->guest_rax);
	case VM_REG_GUEST_RBX:
		return (&vmxctx->guest_rbx);
	case VM_REG_GUEST_RCX:
		return (&vmxctx->guest_rcx);
	case VM_REG_GUEST_RDX:
		return (&vmxctx->guest_rdx);
	case VM_REG_GUEST_RSI:
		return (&vmxctx->guest_rsi);
	case VM_REG_GUEST_RDI:
		return (&vmxctx->guest_rdi);
	case VM_REG_GUEST_RBP:
		return (&vmxctx->guest_rbp);
	case VM_REG_GUEST_R8:
		return (&vmxctx->guest_r8);
	case VM_REG_GUEST_R9:
		return (&vmxctx->guest_r9);
	case VM_REG_GUEST_R10:
		return (&vmxctx->guest_r10);
	case VM_REG_GUEST_R11:
		return (&vmxctx->guest_r11);
	case VM_REG_GUEST_R12:
		return (&vmxctx->guest_r12);
	case VM_REG_GUEST_R13:
		return (&vmxctx->guest_r13);
	case VM_REG_GUEST_R14:
		return (&vmxctx->guest_r14);
	case VM_REG_GUEST_R15:
		return (&vmxctx->guest_r15);
	default:
		break;
	}
	return (NULL);
}

static int
vmxctx_getreg(struct vmxctx *vmxctx, int reg, uint64_t *retval)
{
	register_t *regp;

	if ((regp = vmxctx_regptr(vmxctx, reg)) != NULL) {
		*retval = *regp;
		return (0);
	} else
		return (EINVAL);
}

static int
vmxctx_setreg(struct vmxctx *vmxctx, int reg, uint64_t val)
{
	register_t *regp;

	if ((regp = vmxctx_regptr(vmxctx, reg)) != NULL) {
		*regp = val;
		return (0);
	} else
		return (EINVAL);
}

static int
vmx_shadow_reg(int reg)
{
	int shreg;

	shreg = -1;

	switch (reg) {
	case VM_REG_GUEST_CR0:
		shreg = VMCS_CR0_SHADOW;
                break;
        case VM_REG_GUEST_CR4:
		shreg = VMCS_CR4_SHADOW;
		break;
	default:
		break;
	}

	return (shreg);
}

static int
vmx_getreg(void *arg, int vcpu, int reg, uint64_t *retval)
{
	int running, hostcpu;
	struct vmx *vmx = arg;

	running = vcpu_is_running(vmx->vm, vcpu, &hostcpu);
	if (running && hostcpu != curcpu)
		panic("vmx_getreg: %s%d is running", vm_name(vmx->vm), vcpu);

	if (vmxctx_getreg(&vmx->ctx[vcpu], reg, retval) == 0)
		return (0);

	return (vmcs_getreg(&vmx->vmcs[vcpu], running, reg, retval));
}

static int
vmx_setreg(void *arg, int vcpu, int reg, uint64_t val)
{
	int error, hostcpu, running, shadow;
	uint64_t ctls;
	struct vmx *vmx = arg;

	running = vcpu_is_running(vmx->vm, vcpu, &hostcpu);
	if (running && hostcpu != curcpu)
		panic("vmx_setreg: %s%d is running", vm_name(vmx->vm), vcpu);

	if (vmxctx_setreg(&vmx->ctx[vcpu], reg, val) == 0)
		return (0);

	error = vmcs_setreg(&vmx->vmcs[vcpu], running, reg, val);

	if (error == 0) {
		/*
		 * If the "load EFER" VM-entry control is 1 then the
		 * value of EFER.LMA must be identical to "IA-32e mode guest"
		 * bit in the VM-entry control.
		 */
		if ((entry_ctls & VM_ENTRY_LOAD_EFER) != 0 &&
		    (reg == VM_REG_GUEST_EFER)) {
			vmcs_getreg(&vmx->vmcs[vcpu], running,
				    VMCS_IDENT(VMCS_ENTRY_CTLS), &ctls);
			if (val & EFER_LMA)
				ctls |= VM_ENTRY_GUEST_LMA;
			else
				ctls &= ~VM_ENTRY_GUEST_LMA;
			vmcs_setreg(&vmx->vmcs[vcpu], running,
				    VMCS_IDENT(VMCS_ENTRY_CTLS), ctls);
		}

		shadow = vmx_shadow_reg(reg);
		if (shadow > 0) {
			/*
			 * Store the unmodified value in the shadow
			 */			
			error = vmcs_setreg(&vmx->vmcs[vcpu], running,
				    VMCS_IDENT(shadow), val);
		}
	}

	return (error);
}

static int
vmx_getdesc(void *arg, int vcpu, int reg, struct seg_desc *desc)
{
	struct vmx *vmx = arg;

	return (vmcs_getdesc(&vmx->vmcs[vcpu], reg, desc));
}

static int
vmx_setdesc(void *arg, int vcpu, int reg, struct seg_desc *desc)
{
	struct vmx *vmx = arg;

	return (vmcs_setdesc(&vmx->vmcs[vcpu], reg, desc));
}

static int
vmx_inject(void *arg, int vcpu, int type, int vector, uint32_t code,
	   int code_valid)
{
	int error;
	uint64_t info;
	struct vmx *vmx = arg;
	struct vmcs *vmcs = &vmx->vmcs[vcpu];

	static uint32_t type_map[VM_EVENT_MAX] = {
		0x1,		/* VM_EVENT_NONE */
		0x0,		/* VM_HW_INTR */
		0x2,		/* VM_NMI */
		0x3,		/* VM_HW_EXCEPTION */
		0x4,		/* VM_SW_INTR */
		0x5,		/* VM_PRIV_SW_EXCEPTION */
		0x6,		/* VM_SW_EXCEPTION */
	};

	/*
	 * If there is already an exception pending to be delivered to the
	 * vcpu then just return.
	 */
	error = vmcs_getreg(vmcs, 0, VMCS_IDENT(VMCS_ENTRY_INTR_INFO), &info);
	if (error)
		return (error);

	if (info & VMCS_INTERRUPTION_INFO_VALID)
		return (EAGAIN);

	info = vector | (type_map[type] << 8) | (code_valid ? 1 << 11 : 0);
	info |= VMCS_INTERRUPTION_INFO_VALID;
	error = vmcs_setreg(vmcs, 0, VMCS_IDENT(VMCS_ENTRY_INTR_INFO), info);
	if (error != 0)
		return (error);

	if (code_valid) {
		error = vmcs_setreg(vmcs, 0,
				    VMCS_IDENT(VMCS_ENTRY_EXCEPTION_ERROR),
				    code);
	}
	return (error);
}

static int
vmx_getcap(void *arg, int vcpu, int type, int *retval)
{
	struct vmx *vmx = arg;
	int vcap;
	int ret;

	ret = ENOENT;

	vcap = vmx->cap[vcpu].set;

	switch (type) {
	case VM_CAP_HALT_EXIT:
		if (cap_halt_exit)
			ret = 0;
		break;
	case VM_CAP_PAUSE_EXIT:
		if (cap_pause_exit)
			ret = 0;
		break;
	case VM_CAP_MTRAP_EXIT:
		if (cap_monitor_trap)
			ret = 0;
		break;
	case VM_CAP_UNRESTRICTED_GUEST:
		if (cap_unrestricted_guest)
			ret = 0;
		break;
	case VM_CAP_ENABLE_INVPCID:
		if (cap_invpcid)
			ret = 0;
		break;
	default:
		break;
	}

	if (ret == 0)
		*retval = (vcap & (1 << type)) ? 1 : 0;

	return (ret);
}

static int
vmx_setcap(void *arg, int vcpu, int type, int val)
{
	struct vmx *vmx = arg;
	struct vmcs *vmcs = &vmx->vmcs[vcpu];
	uint32_t baseval;
	uint32_t *pptr;
	int error;
	int flag;
	int reg;
	int retval;

	retval = ENOENT;
	pptr = NULL;

	switch (type) {
	case VM_CAP_HALT_EXIT:
		if (cap_halt_exit) {
			retval = 0;
			pptr = &vmx->cap[vcpu].proc_ctls;
			baseval = *pptr;
			flag = PROCBASED_HLT_EXITING;
			reg = VMCS_PRI_PROC_BASED_CTLS;
		}
		break;
	case VM_CAP_MTRAP_EXIT:
		if (cap_monitor_trap) {
			retval = 0;
			pptr = &vmx->cap[vcpu].proc_ctls;
			baseval = *pptr;
			flag = PROCBASED_MTF;
			reg = VMCS_PRI_PROC_BASED_CTLS;
		}
		break;
	case VM_CAP_PAUSE_EXIT:
		if (cap_pause_exit) {
			retval = 0;
			pptr = &vmx->cap[vcpu].proc_ctls;
			baseval = *pptr;
			flag = PROCBASED_PAUSE_EXITING;
			reg = VMCS_PRI_PROC_BASED_CTLS;
		}
		break;
	case VM_CAP_UNRESTRICTED_GUEST:
		if (cap_unrestricted_guest) {
			retval = 0;
			pptr = &vmx->cap[vcpu].proc_ctls2;
			baseval = *pptr;
			flag = PROCBASED2_UNRESTRICTED_GUEST;
			reg = VMCS_SEC_PROC_BASED_CTLS;
		}
		break;
	case VM_CAP_ENABLE_INVPCID:
		if (cap_invpcid) {
			retval = 0;
			pptr = &vmx->cap[vcpu].proc_ctls2;
			baseval = *pptr;
			flag = PROCBASED2_ENABLE_INVPCID;
			reg = VMCS_SEC_PROC_BASED_CTLS;
		}
		break;
	default:
		break;
	}

	if (retval == 0) {
		if (val) {
			baseval |= flag;
		} else {
			baseval &= ~flag;
		}
		VMPTRLD(vmcs);
		error = vmwrite(reg, baseval);
		VMCLEAR(vmcs);

		if (error) {
			retval = error;
		} else {
			/*
			 * Update optional stored flags, and record
			 * setting
			 */
			if (pptr != NULL) {
				*pptr = baseval;
			}

			if (val) {
				vmx->cap[vcpu].set |= (1 << type);
			} else {
				vmx->cap[vcpu].set &= ~(1 << type);
			}
		}
	}

        return (retval);
}

struct vmm_ops vmm_ops_intel = {
	vmx_init,
	vmx_cleanup,
	vmx_vminit,
	vmx_run,
	vmx_vmcleanup,
	vmx_getreg,
	vmx_setreg,
	vmx_getdesc,
	vmx_setdesc,
	vmx_inject,
	vmx_getcap,
	vmx_setcap,
	ept_vmspace_alloc,
	ept_vmspace_free,
};
