/*-
 * Copyright (c) 1992, 1994, 1995, 1996 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 *
 */

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */
/* __KERNEL_RCSID(0, "$NetBSD: prom.c,v 1.22 1998/02/27 04:03:00 thorpej Exp $"); */
__FBSDID("$FreeBSD: src/sys/alpha/alpha/prom.c,v 1.30 2005/01/05 20:05:49 imp Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <sys/proc.h>
#include <sys/cons.h>

#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/prom.h>
#include <machine/rpb.h>
#include <machine/vmparam.h>

struct rpb	*hwrpb;

extern struct prom_vec prom_dispatch_v;

int		prom_mapped = 1;	/* Is PROM still mapped? */
pt_entry_t	rom_pte, saved_pte[1];	/* XXX */

static pt_entry_t *rom_lev1map(void);
extern pt_entry_t* Lev1map;

static void prom_cache_sync(void);

static pt_entry_t *
rom_lev1map()
{
	struct alpha_pcb *apcb;
	struct pcb *cpcb;

	/*
	 * We may be called before the first context switch
	 * after alpha_init(), in which case we just need
	 * to use the kernel Lev1map.
	 */
	if (PCPU_GET(curpcb) == 0)
		return (Lev1map);

	/*
	 * Find the level 1 map that we're currently running on.
	 */
	cpcb = PCPU_GET(curpcb);
	apcb = (struct alpha_pcb *)ALPHA_PHYS_TO_K0SEG((vm_offset_t)cpcb);

	return ((pt_entry_t *)ALPHA_PHYS_TO_K0SEG(alpha_ptob(apcb->apcb_ptbr)));
}

void
init_prom_interface(rpb)
	struct rpb *rpb;
{
	struct crb *c;

	c = (struct crb *)((char *)rpb + rpb->rpb_crb_off);

        prom_dispatch_v.routine_arg = c->crb_v_dispatch;
        prom_dispatch_v.routine = c->crb_v_dispatch->entry_va;
}

static int alpha_console;

void
init_bootstrap_console()
{
	char buf[4];

	init_prom_interface(hwrpb);

	prom_getenv(PROM_E_TTY_DEV, buf, 4);
	alpha_console = buf[0] - '0';
	promcnattach(alpha_console);
}

static register_t  enter_prom(void);
static void leave_prom(critical_t);


/*
 * promcnputc:
 *
 * Remap char before passing off to prom.
 *
 * Prom only takes 32 bit addresses. Copy char somewhere prom can
 * find it. This routine will stop working after pmap_rid_of_console 
 * is called in alpha_init. This is due to the hard coded address
 * of the console area.
 */
void
promcnputc(struct consdev *cp, int c)
{
        prom_return_t ret;
	unsigned char *to = (unsigned char *)0x20000000;
	register_t s;

	s = enter_prom();	/* critical_enter() and map prom */
	*to = c;

	do {
		ret.bits = prom_putstr(alpha_console, to, 1);
	} while ((ret.u.retval & 1) == 0);

	leave_prom(s);		/* unmap prom and critical_exit(s) */
}

/*
 * promcngetc:
 *
 * Wait for the prom to get a real char and pass it back.
 */
int
promcngetc(struct consdev *cp)
{
        prom_return_t ret;
	register_t s;

        for (;;) {
		s = enter_prom();
                ret.bits = prom_getc(alpha_console);
		leave_prom(s);
                if (ret.u.status == 0 || ret.u.status == 1)
                        return (ret.u.retval);
        }
}

/*
 * promcncheckc
 *
 * If a char is ready, return it, otherwise return -1.
 */
int
promcncheckc(struct consdev *cp)
{
        prom_return_t ret;
	register_t s;

	s = enter_prom();
	ret.bits = prom_getc(alpha_console);
	leave_prom(s);
	if (ret.u.status == 0 || ret.u.status == 1)
		return (ret.u.retval);
	else
		return (-1);
}

static register_t
enter_prom()
{
	pt_entry_t *lev1map;
	register_t s;
	
	s = intr_disable();

	if (!prom_mapped) {
		if (!pmap_uses_prom_console())
			panic("enter_prom");
		lev1map = rom_lev1map();	/* XXX */
		saved_pte[0] = lev1map[0];	/* XXX */
		lev1map[0] = rom_pte;		/* XXX */
		prom_cache_sync();		/* XXX */
	}
	return s;
}

static void
leave_prom(s)
	register_t s;
{

	pt_entry_t *lev1map;

	if (!prom_mapped) {
		if (!pmap_uses_prom_console())
			panic("leave_prom");
		lev1map = rom_lev1map();	/* XXX */
		lev1map[0] = saved_pte[0];	/* XXX */
		prom_cache_sync();		/* XXX */
	}
	intr_restore(s);
}

static void
prom_cache_sync(void)
{
	ALPHA_TBIA();
	alpha_pal_imb();
}

int
prom_getenv(id, buf, len)
	int id, len;
	char *buf;
{
	unsigned char *to = (unsigned char *)0x20000000;
	prom_return_t ret;
	register_t s;

	s = enter_prom();
	ret.bits = prom_getenv_disp(id, to, len);
	bcopy(to, buf, len);
	leave_prom(s);

	if (ret.u.status & 0x4)
		ret.u.retval = 0;
	buf[ret.u.retval] = '\0';

	return (ret.bits);
}

void
prom_halt(halt)
	int halt;
{
	struct pcs *p;

	/*
	 * Turn off interrupts, for sanity.
	 */
	intr_disable();

	/*
	 * Set "boot request" part of the CPU state depending on what
	 * we want to happen when we halt.
	 */
	p = LOCATE_PCS(hwrpb, PCPU_GET(cpuid));
	p->pcs_flags &= ~(PCS_RC | PCS_HALT_REQ);
	if (halt)
		p->pcs_flags |= PCS_HALT_STAY_HALTED;
	else
		p->pcs_flags |= PCS_HALT_WARM_BOOT;

	/*
	 * Halt the machine.
	 */
	for (;;)
		alpha_pal_halt();
}

u_int64_t
hwrpb_checksum()
{
	u_int64_t *p, sum;
	int i;

	for (i = 0, p = (u_int64_t *)hwrpb, sum = 0;
	    i < (offsetof(struct rpb, rpb_checksum) / sizeof (u_int64_t));
	    i++, p++)
		sum += *p;

	return (sum);
}

void
hwrpb_restart_setup()
{
	struct pcs *p;

	/* Clear bootstrap-in-progress flag since we're done bootstrapping */
	p = (struct pcs *)((char *)hwrpb + hwrpb->rpb_pcs_off);
	p->pcs_flags &= ~PCS_BIP;

	bcopy(&thread0.td_pcb->pcb_hw, p->pcs_hwpcb,
	    sizeof thread0.td_pcb->pcb_hw);
	hwrpb->rpb_vptb = VPTBASE;

	/* when 'c'ontinuing from console halt, do a dump */
	hwrpb->rpb_rest_term = (u_int64_t)&XentRestart;
	hwrpb->rpb_rest_term_val = 0x1;

#if 0
	/* don't know what this is really used by, so don't mess with it. */
	hwrpb->rpb_restart = (u_int64_t)&XentRestart;
	hwrpb->rpb_restart_val = 0x2;
#endif

	hwrpb->rpb_checksum = hwrpb_checksum();

	p->pcs_flags |= (PCS_RC | PCS_CV);
}

u_int64_t
console_restart(ra, ai, pv)
	u_int64_t ra, ai, pv;
{
	struct pcs *p;

	/* Clear restart-capable flag, since we can no longer restart. */
	p = (struct pcs *)((char *)hwrpb + hwrpb->rpb_pcs_off);
	p->pcs_flags &= ~PCS_RC;

	panic("user requested console halt");

	return (1);
}
