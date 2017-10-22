/*-
 * Copyright (c) 2009-2010 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Semihalf under sponsorship from
 * the FreeBSD Foundation.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <machine/intr_machdep.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/openfirm.h>

#include "ofw_bus_if.h"
#include "fdt_common.h"

static void
fdt_fixup_busfreq(phandle_t root)
{
	phandle_t sb, cpus, child;
	pcell_t freq;

	/*
	 * Do a strict check so as to skip non-SOC nodes, which also claim
	 * simple-bus compatibility such as eLBC etc.
	 */
	if ((sb = fdt_find_compatible(root, "simple-bus", 1)) == 0)
		return;

	/*
	 * This fixup uses /cpus/ bus-frequency prop value to set simple-bus
	 * bus-frequency property.
	 */
	if ((cpus = OF_finddevice("/cpus")) == 0)
		return;

	if ((child = OF_child(cpus)) == 0)
		return;

	if (OF_getprop(child, "bus-frequency", (void *)&freq,
	    sizeof(freq)) <= 0)
		return;

	OF_setprop(sb, "bus-frequency", (void *)&freq, sizeof(freq));
}

struct fdt_fixup_entry fdt_fixup_table[] = {
	{ "fsl,MPC8572DS", &fdt_fixup_busfreq },
	{ "MPC8555CDS", &fdt_fixup_busfreq },
	{ NULL, NULL }
};

static int
fdt_pic_decode_iic(phandle_t node, pcell_t *intr, int *interrupt, int *trig,
    int *pol)
{
	if (!fdt_is_compatible(node, "chrp,iic"))
		return (ENXIO);

	*interrupt = intr[0];

	switch (intr[1]) {
	case 0:
		/* Active L level */
		*trig = INTR_TRIGGER_LEVEL;
		*pol = INTR_POLARITY_LOW;
		break;
	case 1:
		/* Active H level */
		*trig = INTR_TRIGGER_LEVEL;
		*pol = INTR_POLARITY_HIGH;
		break;
	case 2:
		/* H to L edge */
		*trig = INTR_TRIGGER_EDGE;
		*pol = INTR_POLARITY_LOW;
		break;
	case 3:
		/* L to H edge */
		*trig = INTR_TRIGGER_EDGE;
		*pol = INTR_POLARITY_HIGH;
		break;
	default:
		*trig = INTR_TRIGGER_CONFORM;
		*pol = INTR_POLARITY_CONFORM;
	}
	return (0);
}

static int
fdt_pic_decode_openpic(phandle_t node, pcell_t *intr, int *interrupt,
    int *trig, int *pol)
{

	if (!fdt_is_compatible(node, "chrp,open-pic"))
		return (ENXIO);

	/*
	 * XXX The interrupt number read out from the MPC85XX device tree is
	 * already offset by 16 to reflect the 'internal' IRQ range shift on
	 * the OpenPIC.
	 */
	*interrupt = intr[0];

	switch (intr[1]) {
	case 0:
		/* L to H edge */
		*trig = INTR_TRIGGER_EDGE;
		*pol = INTR_POLARITY_HIGH;
		break;
	case 1:
		/* Active L level */
		*trig = INTR_TRIGGER_LEVEL;
		*pol = INTR_POLARITY_LOW;
		break;
	case 2:
		/* Active H level */
		*trig = INTR_TRIGGER_LEVEL;
		*pol = INTR_POLARITY_HIGH;
		break;
	case 3:
		/* H to L edge */
		*trig = INTR_TRIGGER_EDGE;
		*pol = INTR_POLARITY_LOW;
		break;
	default:
		*trig = INTR_TRIGGER_CONFORM;
		*pol = INTR_POLARITY_CONFORM;
	}
	return (0);
}

fdt_pic_decode_t fdt_pic_table[] = {
	&fdt_pic_decode_iic,
	&fdt_pic_decode_openpic,
	NULL
};
