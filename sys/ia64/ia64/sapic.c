/*-
 * Copyright (c) 2001 Doug Rabson
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
 *
 * $FreeBSD: src/sys/ia64/ia64/sapic.c,v 1.13 2003/09/10 22:49:38 marcel Exp $
 */

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/sysctl.h>

#include <machine/intr.h>
#include <machine/pal.h>
#include <machine/sapicreg.h>
#include <machine/sapicvar.h>

static MALLOC_DEFINE(M_SAPIC, "sapic", "I/O SAPIC devices");

static int sysctl_machdep_apic(SYSCTL_HANDLER_ARGS);

SYSCTL_OID(_machdep, OID_AUTO, apic, CTLTYPE_STRING|CTLFLAG_RD,
    NULL, 0, sysctl_machdep_apic, "A", "(x)APIC redirection table entries");

struct sapic *ia64_sapics[16]; /* XXX make this resizable */
int ia64_sapic_count;

u_int64_t ia64_lapic_address = PAL_PIB_DEFAULT_ADDR;

struct sapic_rte {
	u_int64_t	rte_vector		:8;
	u_int64_t	rte_delivery_mode	:3;
	u_int64_t	rte_destination_mode	:1;
	u_int64_t	rte_delivery_status	:1;
	u_int64_t	rte_polarity		:1;
	u_int64_t	rte_rirr		:1;
	u_int64_t	rte_trigger_mode	:1;
	u_int64_t	rte_mask		:1;
	u_int64_t	rte_flushen		:1;
	u_int64_t	rte_reserved		:30;
	u_int64_t	rte_destination_eid	:8;
	u_int64_t	rte_destination_id	:8;
};

static u_int32_t
sapic_read(struct sapic *sa, int which)
{
	vm_offset_t reg = sa->sa_registers;

	*(volatile u_int32_t *) (reg + SAPIC_IO_SELECT) = which;
	ia64_mf();
	return *(volatile u_int32_t *) (reg + SAPIC_IO_WINDOW);
}

static void
sapic_write(struct sapic *sa, int which, u_int32_t value)
{
	vm_offset_t reg = sa->sa_registers;

	*(volatile u_int32_t *) (reg + SAPIC_IO_SELECT) = which;
	ia64_mf();
	*(volatile u_int32_t *) (reg + SAPIC_IO_WINDOW) = value;
	ia64_mf();
}

static void
sapic_read_rte(struct sapic *sa, int which, struct sapic_rte *rte)
{
	u_int32_t *p = (u_int32_t *) rte;
	register_t c;

	c = intr_disable();
	p[0] = sapic_read(sa, SAPIC_RTE_BASE + 2*which);
	p[1] = sapic_read(sa, SAPIC_RTE_BASE + 2*which + 1);
	intr_restore(c);
}

static void
sapic_write_rte(struct sapic *sa, int which, struct sapic_rte *rte)
{
	u_int32_t *p = (u_int32_t *) rte;
	register_t c;

	c = intr_disable();
	sapic_write(sa, SAPIC_RTE_BASE + 2*which, p[0]);
	sapic_write(sa, SAPIC_RTE_BASE + 2*which + 1, p[1]);
	intr_restore(c);
}

int
sapic_config_intr(int irq, enum intr_trigger trig, enum intr_polarity pol)
{
	struct sapic_rte rte;
	struct sapic *sa;
	int i;

	for (i = 0; i < ia64_sapic_count; i++) {
		sa = ia64_sapics[i];
		if (irq < sa->sa_base || irq > sa->sa_limit)
			continue;

		sapic_read_rte(sa, irq - sa->sa_base, &rte);
		if (trig != INTR_TRIGGER_CONFORM)
			rte.rte_trigger_mode = (trig == INTR_TRIGGER_EDGE) ?
			    SAPIC_TRIGGER_EDGE : SAPIC_TRIGGER_LEVEL;
		else
			rte.rte_trigger_mode = (irq < 16) ?
			    SAPIC_TRIGGER_EDGE : SAPIC_TRIGGER_LEVEL;
		if (pol != INTR_POLARITY_CONFORM)
			rte.rte_polarity = (pol == INTR_POLARITY_HIGH) ?
			    SAPIC_POLARITY_HIGH : SAPIC_POLARITY_LOW;
		else
			rte.rte_polarity = (irq < 16) ? SAPIC_POLARITY_HIGH :
			    SAPIC_POLARITY_LOW;
		sapic_write_rte(sa, irq - sa->sa_base, &rte);
		return (0);
	}

	return (ENOENT);
}

struct sapic *
sapic_create(int id, int base, u_int64_t address)
{
	struct sapic_rte rte;
	struct sapic *sa;
	int i, max;

	sa = malloc(sizeof(struct sapic), M_SAPIC, M_NOWAIT);
	if (!sa)
		return 0;

	sa->sa_id = id;
	sa->sa_base = base;
	sa->sa_registers = IA64_PHYS_TO_RR6(address);

	max = (sapic_read(sa, SAPIC_VERSION) >> 16) & 0xff;
	sa->sa_limit = base + max;

	ia64_sapics[ia64_sapic_count++] = sa;

	/*
	 * Initialize all RTEs with a default trigger mode and polarity.
	 * This may be changed later by calling sapic_config_intr(). We
	 * mask all interrupts by default.
	 */
	bzero(&rte, sizeof(rte));
	rte.rte_mask = 1;
	for (i = base; i <= sa->sa_limit; i++) {
		rte.rte_trigger_mode = (i < 16) ? SAPIC_TRIGGER_EDGE :
		    SAPIC_TRIGGER_LEVEL;
		rte.rte_polarity = (i < 16) ? SAPIC_POLARITY_HIGH :
		    SAPIC_POLARITY_LOW;
		sapic_write_rte(sa, i - base, &rte);
	}

	return (sa);
}

int
sapic_enable(int irq, int vector)
{
	struct sapic_rte rte;
	struct sapic *sa;
	uint64_t lid = ia64_get_lid();
	int i;

	for (i = 0; i < ia64_sapic_count; i++) {
		sa = ia64_sapics[i];
		if (irq < sa->sa_base || irq > sa->sa_limit)
			continue;

		sapic_read_rte(sa, irq - sa->sa_base, &rte);
		rte.rte_destination_id = (lid >> 24) & 255;
		rte.rte_destination_eid = (lid >> 16) & 255;
		rte.rte_delivery_mode = SAPIC_DELMODE_LOWPRI;
		rte.rte_vector = vector;
		rte.rte_mask = 0;
		sapic_write_rte(sa, irq - sa->sa_base, &rte);
		return (0);
	}
	return (ENOENT);
}

void
sapic_eoi(struct sapic *sa, int vector)
{
	vm_offset_t reg = sa->sa_registers;

	*(volatile u_int32_t *) (reg + SAPIC_APIC_EOI) = vector;
	ia64_mf();
}

static int
sysctl_machdep_apic(SYSCTL_HANDLER_ARGS)
{
	char buf[80];
	struct sapic_rte rte;
	struct sapic *sa;
	int apic, count, error, index, len;

	len = sprintf(buf, "\n    APIC Idx: Id,EId : RTE\n");
	error = SYSCTL_OUT(req, buf, len);
	if (error)
		return (error);

	for (apic = 0; apic < ia64_sapic_count; apic++) {
		sa = ia64_sapics[apic];
		count = sa->sa_limit - sa->sa_base + 1;
		for (index = 0; index < count; index++) {
			sapic_read_rte(sa, index, &rte);
			if (rte.rte_mask != 0)
				continue;
			len = sprintf(buf,
    "    0x%02x %3d: (%02x,%02x): %3d %d %d %s %s %s %s %s\n",
			    sa->sa_id, index,
			    rte.rte_destination_id, rte.rte_destination_eid,
			    rte.rte_vector, rte.rte_delivery_mode,
			    rte.rte_destination_mode,
			    rte.rte_delivery_status ? "DS" : "  ",
			    rte.rte_polarity ? "low-active " : "high-active",
			    rte.rte_rirr ? "RIRR" : "    ",
			    rte.rte_trigger_mode ? "level" : "edge ",
			    rte.rte_flushen ? "F" : " ");
			error = SYSCTL_OUT(req, buf, len);
			if (error)
				return (error);
		}
	}

	return (0);
}

#ifdef DDB

#include <ddb/ddb.h>

void
sapic_print(struct sapic *sa, int input)
{
	struct sapic_rte rte;

	sapic_read_rte(sa, input, &rte);
	if (rte.rte_mask == 0) {
		db_printf("%3d %d %d %s %s %s %s %s ID=%x EID=%x\n",
			  rte.rte_vector,
			  rte.rte_delivery_mode,
			  rte.rte_destination_mode,
			  rte.rte_delivery_status ? "DS" : "  ",
			  rte.rte_polarity ? "low-active " : "high-active",
			  rte.rte_rirr ? "RIRR" : "    ",
			  rte.rte_trigger_mode ? "level" : "edge ",
			  rte.rte_flushen ? "F" : " ",
			  rte.rte_destination_id,
			  rte.rte_destination_eid);
	}
}

#endif
