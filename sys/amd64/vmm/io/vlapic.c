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
 * $FreeBSD: release/10.0.0/sys/amd64/vmm/io/vlapic.c 251976 2013-06-18 23:31:09Z pluknet $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/10.0.0/sys/amd64/vmm/io/vlapic.c 251976 2013-06-18 23:31:09Z pluknet $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/smp.h>

#include <machine/clock.h>
#include <x86/specialreg.h>
#include <x86/apicreg.h>

#include <machine/vmm.h>

#include "vmm_stat.h"
#include "vmm_lapic.h"
#include "vmm_ktr.h"
#include "vdev.h"
#include "vlapic.h"

#define	VLAPIC_CTR0(vlapic, format)					\
	VMM_CTR0((vlapic)->vm, (vlapic)->vcpuid, format)

#define	VLAPIC_CTR1(vlapic, format, p1)					\
	VMM_CTR1((vlapic)->vm, (vlapic)->vcpuid, format, p1)

#define	VLAPIC_CTR_IRR(vlapic, msg)					\
do {									\
	uint32_t *irrptr = &(vlapic)->apic.irr0;			\
	irrptr[0] = irrptr[0];	/* silence compiler */			\
	VLAPIC_CTR1((vlapic), msg " irr0 0x%08x", irrptr[0 << 2]);	\
	VLAPIC_CTR1((vlapic), msg " irr1 0x%08x", irrptr[1 << 2]);	\
	VLAPIC_CTR1((vlapic), msg " irr2 0x%08x", irrptr[2 << 2]);	\
	VLAPIC_CTR1((vlapic), msg " irr3 0x%08x", irrptr[3 << 2]);	\
	VLAPIC_CTR1((vlapic), msg " irr4 0x%08x", irrptr[4 << 2]);	\
	VLAPIC_CTR1((vlapic), msg " irr5 0x%08x", irrptr[5 << 2]);	\
	VLAPIC_CTR1((vlapic), msg " irr6 0x%08x", irrptr[6 << 2]);	\
	VLAPIC_CTR1((vlapic), msg " irr7 0x%08x", irrptr[7 << 2]);	\
} while (0)

#define	VLAPIC_CTR_ISR(vlapic, msg)					\
do {									\
	uint32_t *isrptr = &(vlapic)->apic.isr0;			\
	isrptr[0] = isrptr[0];	/* silence compiler */			\
	VLAPIC_CTR1((vlapic), msg " isr0 0x%08x", isrptr[0 << 2]);	\
	VLAPIC_CTR1((vlapic), msg " isr1 0x%08x", isrptr[1 << 2]);	\
	VLAPIC_CTR1((vlapic), msg " isr2 0x%08x", isrptr[2 << 2]);	\
	VLAPIC_CTR1((vlapic), msg " isr3 0x%08x", isrptr[3 << 2]);	\
	VLAPIC_CTR1((vlapic), msg " isr4 0x%08x", isrptr[4 << 2]);	\
	VLAPIC_CTR1((vlapic), msg " isr5 0x%08x", isrptr[5 << 2]);	\
	VLAPIC_CTR1((vlapic), msg " isr6 0x%08x", isrptr[6 << 2]);	\
	VLAPIC_CTR1((vlapic), msg " isr7 0x%08x", isrptr[7 << 2]);	\
} while (0)

static MALLOC_DEFINE(M_VLAPIC, "vlapic", "vlapic");

#define	PRIO(x)			((x) >> 4)

#define VLAPIC_VERSION		(16)
#define VLAPIC_MAXLVT_ENTRIES	(5)

#define	x2apic(vlapic)	(((vlapic)->msr_apicbase & APICBASE_X2APIC) ? 1 : 0)

enum boot_state {
	BS_INIT,
	BS_SIPI,
	BS_RUNNING
};

struct vlapic {
	struct vm		*vm;
	int			vcpuid;

	struct io_region	*mmio;
	struct vdev_ops		*ops;
	struct LAPIC		 apic;

	int			 esr_update;

	int			 divisor;
	int			 ccr_ticks;

	/*
	 * The 'isrvec_stk' is a stack of vectors injected by the local apic.
	 * A vector is popped from the stack when the processor does an EOI.
	 * The vector on the top of the stack is used to compute the
	 * Processor Priority in conjunction with the TPR.
	 */
	uint8_t			 isrvec_stk[ISRVEC_STK_SIZE];
	int			 isrvec_stk_top;

	uint64_t		msr_apicbase;
	enum boot_state		boot_state;
};

#define VLAPIC_BUS_FREQ	tsc_freq

static int
vlapic_timer_divisor(uint32_t dcr)
{
	switch (dcr & 0xB) {
	case APIC_TDCR_1:
		return (1);
	case APIC_TDCR_2:
		return (2);
	case APIC_TDCR_4:
		return (4);
	case APIC_TDCR_8:
		return (8);
	case APIC_TDCR_16:
		return (16);
	case APIC_TDCR_32:
		return (32);
	case APIC_TDCR_64:
		return (64);
	case APIC_TDCR_128:
		return (128);
	default:
		panic("vlapic_timer_divisor: invalid dcr 0x%08x", dcr);
	}
}

static void
vlapic_mask_lvts(uint32_t *lvts, int num_lvt)
{
	int i;
	for (i = 0; i < num_lvt; i++) {
		*lvts |= APIC_LVT_M;
		lvts += 4;
	}
}

#if 0
static inline void
vlapic_dump_lvt(uint32_t offset, uint32_t *lvt)
{
	printf("Offset %x: lvt %08x (V:%02x DS:%x M:%x)\n", offset,
	    *lvt, *lvt & APIC_LVTT_VECTOR, *lvt & APIC_LVTT_DS,
	    *lvt & APIC_LVTT_M);
}
#endif

static uint64_t
vlapic_get_ccr(struct vlapic *vlapic)
{
	struct LAPIC    *lapic = &vlapic->apic;
	return lapic->ccr_timer;
}

static void
vlapic_update_errors(struct vlapic *vlapic)
{
	struct LAPIC    *lapic = &vlapic->apic;
	lapic->esr = 0; // XXX 
}

static void
vlapic_init_ipi(struct vlapic *vlapic)
{
	struct LAPIC    *lapic = &vlapic->apic;
	lapic->version = VLAPIC_VERSION;
	lapic->version |= (VLAPIC_MAXLVT_ENTRIES < MAXLVTSHIFT);
	lapic->dfr = 0xffffffff;
	lapic->svr = APIC_SVR_VECTOR;
	vlapic_mask_lvts(&lapic->lvt_timer, VLAPIC_MAXLVT_ENTRIES+1);
}

static int
vlapic_op_reset(void* dev)
{
	struct vlapic 	*vlapic = (struct vlapic*)dev;
	struct LAPIC	*lapic = &vlapic->apic;

	memset(lapic, 0, sizeof(*lapic));
	lapic->apr = vlapic->vcpuid;
	vlapic_init_ipi(vlapic);
	vlapic->divisor = vlapic_timer_divisor(lapic->dcr_timer);

	if (vlapic->vcpuid == 0)
		vlapic->boot_state = BS_RUNNING;	/* BSP */
	else
		vlapic->boot_state = BS_INIT;		/* AP */
	
	return 0;

}

static int
vlapic_op_init(void* dev)
{
	struct vlapic *vlapic = (struct vlapic*)dev;
	vdev_register_region(vlapic->ops, vlapic, vlapic->mmio);
	return vlapic_op_reset(dev);
}

static int
vlapic_op_halt(void* dev)
{
	struct vlapic *vlapic = (struct vlapic*)dev;
	vdev_unregister_region(vlapic, vlapic->mmio);
	return 0;

}

void
vlapic_set_intr_ready(struct vlapic *vlapic, int vector)
{
	struct LAPIC	*lapic = &vlapic->apic;
	uint32_t	*irrptr;
	int		idx;

	if (vector < 0 || vector >= 256)
		panic("vlapic_set_intr_ready: invalid vector %d\n", vector);

	idx = (vector / 32) * 4;
	irrptr = &lapic->irr0;
	atomic_set_int(&irrptr[idx], 1 << (vector % 32));
	VLAPIC_CTR_IRR(vlapic, "vlapic_set_intr_ready");
}

static void
vlapic_start_timer(struct vlapic *vlapic, uint32_t elapsed)
{
	uint32_t icr_timer;

	icr_timer = vlapic->apic.icr_timer;

	vlapic->ccr_ticks = ticks;
	if (elapsed < icr_timer)
		vlapic->apic.ccr_timer = icr_timer - elapsed;
	else {
		/*
		 * This can happen when the guest is trying to run its local
		 * apic timer higher that the setting of 'hz' in the host.
		 *
		 * We deal with this by running the guest local apic timer
		 * at the rate of the host's 'hz' setting.
		 */
		vlapic->apic.ccr_timer = 0;
	}
}

static __inline uint32_t *
vlapic_get_lvt(struct vlapic *vlapic, uint32_t offset)
{
	struct LAPIC	*lapic = &vlapic->apic;
	int 		 i;

	if (offset < APIC_OFFSET_TIMER_LVT || offset > APIC_OFFSET_ERROR_LVT) {
		panic("vlapic_get_lvt: invalid LVT\n");
	}
	i = (offset - APIC_OFFSET_TIMER_LVT) >> 2;
	return ((&lapic->lvt_timer) + i);;
}

#if 1
static void
dump_isrvec_stk(struct vlapic *vlapic)
{
	int i;
	uint32_t *isrptr;

	isrptr = &vlapic->apic.isr0;
	for (i = 0; i < 8; i++)
		printf("ISR%d 0x%08x\n", i, isrptr[i * 4]);

	for (i = 0; i <= vlapic->isrvec_stk_top; i++)
		printf("isrvec_stk[%d] = %d\n", i, vlapic->isrvec_stk[i]);
}
#endif

/*
 * Algorithm adopted from section "Interrupt, Task and Processor Priority"
 * in Intel Architecture Manual Vol 3a.
 */
static void
vlapic_update_ppr(struct vlapic *vlapic)
{
	int isrvec, tpr, ppr;

	/*
	 * Note that the value on the stack at index 0 is always 0.
	 *
	 * This is a placeholder for the value of ISRV when none of the
	 * bits is set in the ISRx registers.
	 */
	isrvec = vlapic->isrvec_stk[vlapic->isrvec_stk_top];
	tpr = vlapic->apic.tpr;

#if 1
	{
		int i, lastprio, curprio, vector, idx;
		uint32_t *isrptr;

		if (vlapic->isrvec_stk_top == 0 && isrvec != 0)
			panic("isrvec_stk is corrupted: %d", isrvec);

		/*
		 * Make sure that the priority of the nested interrupts is
		 * always increasing.
		 */
		lastprio = -1;
		for (i = 1; i <= vlapic->isrvec_stk_top; i++) {
			curprio = PRIO(vlapic->isrvec_stk[i]);
			if (curprio <= lastprio) {
				dump_isrvec_stk(vlapic);
				panic("isrvec_stk does not satisfy invariant");
			}
			lastprio = curprio;
		}

		/*
		 * Make sure that each bit set in the ISRx registers has a
		 * corresponding entry on the isrvec stack.
		 */
		i = 1;
		isrptr = &vlapic->apic.isr0;
		for (vector = 0; vector < 256; vector++) {
			idx = (vector / 32) * 4;
			if (isrptr[idx] & (1 << (vector % 32))) {
				if (i > vlapic->isrvec_stk_top ||
				    vlapic->isrvec_stk[i] != vector) {
					dump_isrvec_stk(vlapic);
					panic("ISR and isrvec_stk out of sync");
				}
				i++;
			}
		}
	}
#endif

	if (PRIO(tpr) >= PRIO(isrvec))
		ppr = tpr;
	else
		ppr = isrvec & 0xf0;

	vlapic->apic.ppr = ppr;
	VLAPIC_CTR1(vlapic, "vlapic_update_ppr 0x%02x", ppr);
}

static void
vlapic_process_eoi(struct vlapic *vlapic)
{
	struct LAPIC	*lapic = &vlapic->apic;
	uint32_t	*isrptr;
	int		i, idx, bitpos;

	isrptr = &lapic->isr0;

	/*
	 * The x86 architecture reserves the the first 32 vectors for use
	 * by the processor.
	 */
	for (i = 7; i > 0; i--) {
		idx = i * 4;
		bitpos = fls(isrptr[idx]);
		if (bitpos != 0) {
			if (vlapic->isrvec_stk_top <= 0) {
				panic("invalid vlapic isrvec_stk_top %d",
				      vlapic->isrvec_stk_top);
			}
			isrptr[idx] &= ~(1 << (bitpos - 1));
			VLAPIC_CTR_ISR(vlapic, "vlapic_process_eoi");
			vlapic->isrvec_stk_top--;
			vlapic_update_ppr(vlapic);
			return;
		}
	}
}

static __inline int
vlapic_get_lvt_field(uint32_t *lvt, uint32_t mask)
{
	return (*lvt & mask);
}

static __inline int
vlapic_periodic_timer(struct vlapic *vlapic)
{
	uint32_t *lvt;
	
	lvt = vlapic_get_lvt(vlapic, APIC_OFFSET_TIMER_LVT);

	return (vlapic_get_lvt_field(lvt, APIC_LVTT_TM_PERIODIC));
}

static VMM_STAT(VLAPIC_INTR_TIMER, "timer interrupts generated by vlapic");

static void
vlapic_fire_timer(struct vlapic *vlapic)
{
	int vector;
	uint32_t *lvt;
	
	lvt = vlapic_get_lvt(vlapic, APIC_OFFSET_TIMER_LVT);

	if (!vlapic_get_lvt_field(lvt, APIC_LVTT_M)) {
		vmm_stat_incr(vlapic->vm, vlapic->vcpuid, VLAPIC_INTR_TIMER, 1);
		vector = vlapic_get_lvt_field(lvt,APIC_LVTT_VECTOR);
		vlapic_set_intr_ready(vlapic, vector);
	}
}

static VMM_STAT_ARRAY(IPIS_SENT, VM_MAXCPU, "ipis sent to vcpu");

static int
lapic_process_icr(struct vlapic *vlapic, uint64_t icrval)
{
	int i;
	cpuset_t dmask;
	uint32_t dest, vec, mode;
	struct vlapic *vlapic2;
	struct vm_exit *vmexit;
	
	if (x2apic(vlapic))
		dest = icrval >> 32;
	else
		dest = icrval >> (32 + 24);
	vec = icrval & APIC_VECTOR_MASK;
	mode = icrval & APIC_DELMODE_MASK;

	if (mode == APIC_DELMODE_FIXED || mode == APIC_DELMODE_NMI) {
		switch (icrval & APIC_DEST_MASK) {
		case APIC_DEST_DESTFLD:
			CPU_SETOF(dest, &dmask);
			break;
		case APIC_DEST_SELF:
			CPU_SETOF(vlapic->vcpuid, &dmask);
			break;
		case APIC_DEST_ALLISELF:
			dmask = vm_active_cpus(vlapic->vm);
			break;
		case APIC_DEST_ALLESELF:
			dmask = vm_active_cpus(vlapic->vm);
			CPU_CLR(vlapic->vcpuid, &dmask);
			break;
		default:
			CPU_ZERO(&dmask);	/* satisfy gcc */
			break;
		}

		while ((i = CPU_FFS(&dmask)) != 0) {
			i--;
			CPU_CLR(i, &dmask);
			if (mode == APIC_DELMODE_FIXED) {
				lapic_set_intr(vlapic->vm, i, vec);
				vmm_stat_array_incr(vlapic->vm, vlapic->vcpuid,
						    IPIS_SENT, i, 1);
			} else
				vm_inject_nmi(vlapic->vm, i);
		}

		return (0);	/* handled completely in the kernel */
	}

	if (mode == APIC_DELMODE_INIT) {
		if ((icrval & APIC_LEVEL_MASK) == APIC_LEVEL_DEASSERT)
			return (0);

		if (vlapic->vcpuid == 0 && dest != 0 && dest < VM_MAXCPU) {
			vlapic2 = vm_lapic(vlapic->vm, dest);

			/* move from INIT to waiting-for-SIPI state */
			if (vlapic2->boot_state == BS_INIT) {
				vlapic2->boot_state = BS_SIPI;
			}

			return (0);
		}
	}

	if (mode == APIC_DELMODE_STARTUP) {
		if (vlapic->vcpuid == 0 && dest != 0 && dest < VM_MAXCPU) {
			vlapic2 = vm_lapic(vlapic->vm, dest);

			/*
			 * Ignore SIPIs in any state other than wait-for-SIPI
			 */
			if (vlapic2->boot_state != BS_SIPI)
				return (0);

			vmexit = vm_exitinfo(vlapic->vm, vlapic->vcpuid);
			vmexit->exitcode = VM_EXITCODE_SPINUP_AP;
			vmexit->u.spinup_ap.vcpu = dest;
			vmexit->u.spinup_ap.rip = vec << PAGE_SHIFT;

			/*
			 * XXX this assumes that the startup IPI always succeeds
			 */
			vlapic2->boot_state = BS_RUNNING;
			vm_activate_cpu(vlapic2->vm, dest);

			return (0);
		}
	}

	/*
	 * This will cause a return to userland.
	 */
	return (1);
}

int
vlapic_pending_intr(struct vlapic *vlapic)
{
	struct LAPIC	*lapic = &vlapic->apic;
	int	  	 idx, i, bitpos, vector;
	uint32_t	*irrptr, val;

	irrptr = &lapic->irr0;

	/*
	 * The x86 architecture reserves the the first 32 vectors for use
	 * by the processor.
	 */
	for (i = 7; i > 0; i--) {
		idx = i * 4;
		val = atomic_load_acq_int(&irrptr[idx]);
		bitpos = fls(val);
		if (bitpos != 0) {
			vector = i * 32 + (bitpos - 1);
			if (PRIO(vector) > PRIO(lapic->ppr)) {
				VLAPIC_CTR1(vlapic, "pending intr %d", vector);
				return (vector);
			} else 
				break;
		}
	}
	VLAPIC_CTR0(vlapic, "no pending intr");
	return (-1);
}

void
vlapic_intr_accepted(struct vlapic *vlapic, int vector)
{
	struct LAPIC	*lapic = &vlapic->apic;
	uint32_t	*irrptr, *isrptr;
	int		idx, stk_top;

	/*
	 * clear the ready bit for vector being accepted in irr 
	 * and set the vector as in service in isr.
	 */
	idx = (vector / 32) * 4;

	irrptr = &lapic->irr0;
	atomic_clear_int(&irrptr[idx], 1 << (vector % 32));
	VLAPIC_CTR_IRR(vlapic, "vlapic_intr_accepted");

	isrptr = &lapic->isr0;
	isrptr[idx] |= 1 << (vector % 32);
	VLAPIC_CTR_ISR(vlapic, "vlapic_intr_accepted");

	/*
	 * Update the PPR
	 */
	vlapic->isrvec_stk_top++;

	stk_top = vlapic->isrvec_stk_top;
	if (stk_top >= ISRVEC_STK_SIZE)
		panic("isrvec_stk_top overflow %d", stk_top);

	vlapic->isrvec_stk[stk_top] = vector;
	vlapic_update_ppr(vlapic);
}

int
vlapic_op_mem_read(void* dev, uint64_t gpa, opsize_t size, uint64_t *data)
{
	struct vlapic 	*vlapic = (struct vlapic*)dev;
	struct LAPIC	*lapic = &vlapic->apic;
	uint64_t	 offset = gpa & ~(PAGE_SIZE);
	uint32_t	*reg;
	int		 i;

	if (offset > sizeof(*lapic)) {
		*data = 0;
		return 0;
	}
	
	offset &= ~3;
	switch(offset)
	{
		case APIC_OFFSET_ID:
			if (x2apic(vlapic))
				*data = vlapic->vcpuid;
			else
				*data = vlapic->vcpuid << 24;
			break;
		case APIC_OFFSET_VER:
			*data = lapic->version;
			break;
		case APIC_OFFSET_TPR:
			*data = lapic->tpr;
			break;
		case APIC_OFFSET_APR:
			*data = lapic->apr;
			break;
		case APIC_OFFSET_PPR:
			*data = lapic->ppr;
			break;
		case APIC_OFFSET_EOI:
			*data = lapic->eoi;
			break;
		case APIC_OFFSET_LDR:
			*data = lapic->ldr;
			break;
		case APIC_OFFSET_DFR:
			*data = lapic->dfr;
			break;
		case APIC_OFFSET_SVR:
			*data = lapic->svr;
			break;
		case APIC_OFFSET_ISR0 ... APIC_OFFSET_ISR7:
			i = (offset - APIC_OFFSET_ISR0) >> 2;
			reg = &lapic->isr0;
			*data = *(reg + i);
			break;
		case APIC_OFFSET_TMR0 ... APIC_OFFSET_TMR7:
			i = (offset - APIC_OFFSET_TMR0) >> 2;
			reg = &lapic->tmr0;
			*data = *(reg + i);
			break;
		case APIC_OFFSET_IRR0 ... APIC_OFFSET_IRR7:
			i = (offset - APIC_OFFSET_IRR0) >> 2;
			reg = &lapic->irr0;
			*data = atomic_load_acq_int(reg + i);
			break;
		case APIC_OFFSET_ESR:
			*data = lapic->esr;
			break;
		case APIC_OFFSET_ICR_LOW: 
			*data = lapic->icr_lo;
			break;
		case APIC_OFFSET_ICR_HI: 
			*data = lapic->icr_hi;
			break;
		case APIC_OFFSET_TIMER_LVT ... APIC_OFFSET_ERROR_LVT:
			reg = vlapic_get_lvt(vlapic, offset);	
			*data = *(reg);
			break;
		case APIC_OFFSET_ICR:
			*data = lapic->icr_timer;
			break;
		case APIC_OFFSET_CCR:
			*data = vlapic_get_ccr(vlapic);
			break;
		case APIC_OFFSET_DCR:
			*data = lapic->dcr_timer;
			break;
		case APIC_OFFSET_RRR:
		default:
			*data = 0;
			break;
	}
	return 0;
}

int
vlapic_op_mem_write(void* dev, uint64_t gpa, opsize_t size, uint64_t data)
{
	struct vlapic 	*vlapic = (struct vlapic*)dev;
	struct LAPIC	*lapic = &vlapic->apic;
	uint64_t	 offset = gpa & ~(PAGE_SIZE);
	uint32_t	*reg;
	int		retval;

	if (offset > sizeof(*lapic)) {
		return 0;
	}

	retval = 0;
	offset &= ~3;
	switch(offset)
	{
		case APIC_OFFSET_ID:
			break;
		case APIC_OFFSET_TPR:
			lapic->tpr = data & 0xff;
			vlapic_update_ppr(vlapic);
			break;
		case APIC_OFFSET_EOI:
			vlapic_process_eoi(vlapic);
			break;
		case APIC_OFFSET_LDR:
			break;
		case APIC_OFFSET_DFR:
			break;
		case APIC_OFFSET_SVR:
			lapic->svr = data;
			break;
		case APIC_OFFSET_ICR_LOW: 
			if (!x2apic(vlapic)) {
				data &= 0xffffffff;
				data |= (uint64_t)lapic->icr_hi << 32;
			}
			retval = lapic_process_icr(vlapic, data);
			break;
		case APIC_OFFSET_ICR_HI:
			if (!x2apic(vlapic)) {
				retval = 0;
				lapic->icr_hi = data;
			}
			break;
		case APIC_OFFSET_TIMER_LVT ... APIC_OFFSET_ERROR_LVT:
			reg = vlapic_get_lvt(vlapic, offset);	
			if (!(lapic->svr & APIC_SVR_ENABLE)) {
				data |= APIC_LVT_M;
			}
			*reg = data;
			// vlapic_dump_lvt(offset, reg);
			break;
		case APIC_OFFSET_ICR:
			lapic->icr_timer = data;
			vlapic_start_timer(vlapic, 0);
			break;

		case APIC_OFFSET_DCR:
			lapic->dcr_timer = data;
			vlapic->divisor = vlapic_timer_divisor(data);
			break;

		case APIC_OFFSET_ESR:
			vlapic_update_errors(vlapic);
			break;
		case APIC_OFFSET_VER:
		case APIC_OFFSET_APR:
		case APIC_OFFSET_PPR:
		case APIC_OFFSET_RRR:
		case APIC_OFFSET_ISR0 ... APIC_OFFSET_ISR7:
		case APIC_OFFSET_TMR0 ... APIC_OFFSET_TMR7:
		case APIC_OFFSET_IRR0 ... APIC_OFFSET_IRR7:
		case APIC_OFFSET_CCR:
		default:
			// Read only.
			break;
	}

	return (retval);
}

int
vlapic_timer_tick(struct vlapic *vlapic)
{
	int curticks, delta, periodic, fired;
	uint32_t ccr;
	uint32_t decrement, leftover;

restart:
	curticks = ticks;
	delta = curticks - vlapic->ccr_ticks;

	/* Local APIC timer is disabled */
	if (vlapic->apic.icr_timer == 0)
		return (-1);

	/* One-shot mode and timer has already counted down to zero */
	periodic = vlapic_periodic_timer(vlapic);
	if (!periodic && vlapic->apic.ccr_timer == 0)
		return (-1);
	/*
	 * The 'curticks' and 'ccr_ticks' are out of sync by more than
	 * 2^31 ticks. We deal with this by restarting the timer.
	 */
	if (delta < 0) {
		vlapic_start_timer(vlapic, 0);
		goto restart;
	}

	fired = 0;
	decrement = (VLAPIC_BUS_FREQ / vlapic->divisor) / hz;

	vlapic->ccr_ticks = curticks;
	ccr = vlapic->apic.ccr_timer;

	while (delta-- > 0) {
		if (ccr > decrement) {
			ccr -= decrement;
			continue;
		}

		/* Trigger the local apic timer interrupt */
		vlapic_fire_timer(vlapic);
		if (periodic) {
			leftover = decrement - ccr;
			vlapic_start_timer(vlapic, leftover);
			ccr = vlapic->apic.ccr_timer;
		} else {
			/*
			 * One-shot timer has counted down to zero.
			 */
			ccr = 0;
		}
		fired = 1;
		break;
	}

	vlapic->apic.ccr_timer = ccr;

	if (!fired)
		return ((ccr / decrement) + 1);
	else
		return (0);
}

struct vdev_ops vlapic_dev_ops = {
	.name = "vlapic",
	.init = vlapic_op_init,
	.reset = vlapic_op_reset,
	.halt = vlapic_op_halt,
	.memread = vlapic_op_mem_read,
	.memwrite = vlapic_op_mem_write,
};
static struct io_region vlapic_mmio[VM_MAXCPU];

struct vlapic *
vlapic_init(struct vm *vm, int vcpuid)
{
	struct vlapic 		*vlapic;

	vlapic = malloc(sizeof(struct vlapic), M_VLAPIC, M_WAITOK | M_ZERO);
	vlapic->vm = vm;
	vlapic->vcpuid = vcpuid;

	vlapic->msr_apicbase = DEFAULT_APIC_BASE | APICBASE_ENABLED;

	if (vcpuid == 0)
		vlapic->msr_apicbase |= APICBASE_BSP;

	vlapic->ops = &vlapic_dev_ops;

	vlapic->mmio = vlapic_mmio + vcpuid;
	vlapic->mmio->base = DEFAULT_APIC_BASE;
	vlapic->mmio->len = PAGE_SIZE;
	vlapic->mmio->attr = MMIO_READ|MMIO_WRITE;
	vlapic->mmio->vcpu = vcpuid;

	vdev_register(&vlapic_dev_ops, vlapic);

	vlapic_op_init(vlapic);

	return (vlapic);
}

void
vlapic_cleanup(struct vlapic *vlapic)
{
	vlapic_op_halt(vlapic);
	vdev_unregister(vlapic);
	free(vlapic, M_VLAPIC);
}

uint64_t
vlapic_get_apicbase(struct vlapic *vlapic)
{

	return (vlapic->msr_apicbase);
}

void
vlapic_set_apicbase(struct vlapic *vlapic, uint64_t val)
{
	int err;
	enum x2apic_state state;

	err = vm_get_x2apic_state(vlapic->vm, vlapic->vcpuid, &state);
	if (err)
		panic("vlapic_set_apicbase: err %d fetching x2apic state", err);

	if (state == X2APIC_DISABLED)
		val &= ~APICBASE_X2APIC;

	vlapic->msr_apicbase = val;
}

void
vlapic_set_x2apic_state(struct vm *vm, int vcpuid, enum x2apic_state state)
{
	struct vlapic *vlapic;

	vlapic = vm_lapic(vm, vcpuid);

	if (state == X2APIC_DISABLED)
		vlapic->msr_apicbase &= ~APICBASE_X2APIC;
}
