/* $NetBSD: dec_eb164.c,v 1.26 1998/04/17 02:45:19 mjacob Exp $ */
/*-
 * Copyright (c) 1995, 1996, 1997 Carnegie-Mellon University.
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
/*-
 * Additional Copyright (c) 1997 by Matthew Jacob for NASA/Ames Research Center
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/alpha/alpha/dec_eb164.c,v 1.21 2005/01/05 20:05:48 imp Exp $");

#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/systm.h>
#include <sys/termios.h>
#include <sys/bus.h>

#include <machine/clock.h>
#include <machine/cpuconf.h>
#include <machine/md_var.h>
#include <machine/rpb.h>

#include <alpha/pci/ciavar.h>

void dec_eb164_init(void);
static void dec_eb164_cons_init(void);
static void eb164_intr_init(void);
extern void eb164_intr_enable(int irq);
extern void eb164_intr_disable(int irq);
extern void eb164_intr_enable_icsr(int irq);
extern void eb164_intr_disable_icsr(int irq);

void
dec_eb164_init()
{
	platform.family = "EB164";

	if ((platform.model = alpha_dsr_sysname()) == NULL) {
		/* XXX Don't know the system variations, yet. */
		platform.model = alpha_unknown_sysname();
	}

	platform.iobus = "cia";
	platform.cons_init = dec_eb164_cons_init;
	platform.pci_intr_init = eb164_intr_init;
	platform.pci_intr_map = NULL;
	if (strncmp(platform.model, "Digital AlphaPC 164 ", 20) == 0) {
		platform.pci_intr_disable = eb164_intr_disable_icsr;
		platform.pci_intr_enable = eb164_intr_enable_icsr;
	} else {
		platform.pci_intr_disable = eb164_intr_disable;
		platform.pci_intr_enable = eb164_intr_enable;
	}
}

static void
dec_eb164_cons_init()
{
	struct ctb *ctb;

	cia_init();

	ctb = (struct ctb *)(((caddr_t)hwrpb) + hwrpb->rpb_ctb_off);

	switch (ctb->ctb_term_type) {
	case 2:
		boothowto |= RB_SERIAL;
		break;

	case 3:
		boothowto &= ~RB_SERIAL;
		break;

	default:
		printf("ctb->ctb_term_type = 0x%lx\n", ctb->ctb_term_type);
		printf("ctb->ctb_turboslot = 0x%lx\n", ctb->ctb_turboslot);

		panic("consinit: unknown console type %d\n",
		    (int)ctb->ctb_term_type);
	}
}

static void
eb164_intr_init()
{
    /*
     * Enable ISA-PCI cascade interrupt.
     */
    eb164_intr_enable(4);
}
