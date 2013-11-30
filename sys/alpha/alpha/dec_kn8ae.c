/* $NetBSD: dec_kn8ae.c,v 1.15 1998/02/13 00:12:50 thorpej Exp $ */
/*-
 * Copyright (c) 1997 by Matthew Jacob
 * NASA AMES Research Center.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/alpha/alpha/dec_kn8ae.c,v 1.15 2005/01/05 20:05:49 imp Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/termios.h>
#include <sys/cons.h>
#include <sys/reboot.h>
#include <sys/bus.h>

#include <machine/cpuconf.h>
#include <machine/md_var.h>
#include <machine/rpb.h>

#include <alpha/tlsb/tlsbreg.h>
#include <alpha/tlsb/gbusreg.h>
#include <alpha/tlsb/zsvar.h>

void dec_kn8ae_init(int);
static void dec_kn8ae_cons_init(void);

static const struct alpha_variation_table dec_kn8ae_variations[] = {
	{ 0, "AlphaServer 8400" },
	{ 0, NULL },
};

void
dec_kn8ae_init(int cputype)
{
	u_int64_t variation;

	platform.family = "AlphaServer 8400";

	if ((platform.model = alpha_dsr_sysname()) == NULL) {
		variation = hwrpb->rpb_variation & SV_ST_MASK;
		if ((platform.model = alpha_variation_name(variation,
		    dec_kn8ae_variations)) == NULL)
			platform.model = alpha_unknown_sysname();
	}

	platform.iobus = "tlsb";
	platform.cons_init = dec_kn8ae_cons_init;
}

/*
 * Info to retain:
 *
 *	The AXP 8X00 seems to encode the
 *	type of console in the ctb_type field,
 *	not the ctb_term_type field.
 */
static void
dec_kn8ae_cons_init(void)
{

	boothowto |= RB_SERIAL;
	zs_cnattach(TLSB_GBUS_BASE, GBUS_DUART0_OFFSET);
}
