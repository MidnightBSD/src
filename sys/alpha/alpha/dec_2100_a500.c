/*-
 * Copyright (c) 2000 Andrew Gallatin
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
__FBSDID("$FreeBSD: src/sys/alpha/alpha/dec_2100_a500.c,v 1.19 2005/01/05 20:05:48 imp Exp $");

#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/systm.h>
#include <sys/termios.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <machine/clock.h>
#include <machine/cpuconf.h>
#include <machine/md_var.h>
#include <machine/rpb.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <alpha/pci/t2var.h>
#include <alpha/pci/t2reg.h>

void dec_2100_a500_init(int);
static void dec_2100_a500_cons_init(void);
static void dec_2100_a500_intr_init(void);

void
dec_2100_a500_init(int cputype)
{
	/*
	 * See if we're a `Sable' or a `Lynx'.
	 */
	if (cputype == ST_DEC_2100_A500) {
		if (alpha_implver() == ALPHA_IMPLVER_EV5)
			sable_lynx_base = LYNX_BASE;
		else
			sable_lynx_base = SABLE_BASE;
		platform.family = "DEC AlphaServer 2100";
	} else if (cputype == ST_DEC_2100A_A500) {
		sable_lynx_base = LYNX_BASE;
		platform.family = "DEC AlphaServer 2100A";
	} else {
		sable_lynx_base = SABLE_BASE;
		platform.family = "DEC AlphaServer 2100?????";
	}

	if ((platform.model = alpha_dsr_sysname()) == NULL) {
		platform.model = alpha_unknown_sysname();
	}

	platform.iobus = "t2";
	platform.cons_init = dec_2100_a500_cons_init;
	platform.pci_intr_init = dec_2100_a500_intr_init;

	t2_init();
}


static void
dec_2100_a500_cons_init()
{
	struct ctb *ctb;
	t2_init();

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
		panic("consinit: unknown console type");
	}
}


void
dec_2100_a500_intr_init(void)
{

	outb(SLAVE0_ICU, 0);
	outb(SLAVE1_ICU, 0);
	outb(SLAVE2_ICU, 0);
	outb(MASTER_ICU, 0x44);
}
