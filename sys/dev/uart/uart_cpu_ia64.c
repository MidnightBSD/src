/*-
 * Copyright (c) 2003, 2004 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <machine/dig64.h>
#include <machine/md_var.h>
#include <machine/vmparam.h>

#include <dev/uart/uart.h>
#include <dev/uart/uart_cpu.h>

bus_space_tag_t uart_bus_space_io = IA64_BUS_SPACE_IO;
bus_space_tag_t uart_bus_space_mem = IA64_BUS_SPACE_MEM;

static int dig64_to_uart_parity[] = {
	UART_PARITY_NONE, UART_PARITY_NONE, UART_PARITY_EVEN,
	UART_PARITY_ODD, UART_PARITY_MARK, UART_PARITY_SPACE
};

int
uart_cpu_eqres(struct uart_bas *b1, struct uart_bas *b2)
{

	return ((b1->bsh == b2->bsh && b1->bst == b2->bst) ? 1 : 0);
}

int
uart_cpu_getdev(int devtype, struct uart_devinfo *di)
{
	struct dig64_hcdp_table *tbl;
	struct dig64_hcdp_entry *ent;
	struct uart_class *class;
	bus_addr_t addr;
	uint64_t hcdp;
	unsigned int i;

	class = &uart_ns8250_class;
	if (class == NULL)
		return (ENXIO);

	/*
	 * Use the DIG64 HCDP table if present.
	 */
	hcdp = ia64_get_hcdp();
	if (hcdp != 0) {
		tbl = (void*)IA64_PHYS_TO_RR7(hcdp);
		for (i = 0; i < tbl->entries; i++) {
			ent = tbl->entry + i;

			if (devtype == UART_DEV_CONSOLE &&
			    ent->type != DIG64_HCDP_CONSOLE)
				continue;

			if (devtype == UART_DEV_DBGPORT &&
			    ent->type != DIG64_HCDP_DBGPORT)
				continue;

			addr = ent->address.addr_high;
			addr = (addr << 32) + ent->address.addr_low;
			di->ops = uart_getops(class);
			di->bas.chan = 0;
			di->bas.bst = (ent->address.addr_space == 0)
			    ? uart_bus_space_mem : uart_bus_space_io;
			if (bus_space_map(di->bas.bst, addr,
			    uart_getrange(class), 0, &di->bas.bsh) != 0)
				continue;
			di->bas.regshft = 0;
			di->bas.rclk = ent->pclock << 4;
			/* We don't deal with 64-bit baud rates. */
			di->baudrate = ent->baud_low;
			di->databits = ent->databits;
			di->stopbits = ent->stopbits;
			di->parity = (ent->parity >= 6) ? UART_PARITY_NONE
			    : dig64_to_uart_parity[ent->parity];
			return (0);
		}

		/* FALLTHROUGH */
	}

	/* Check the environment. */
	return (uart_getenv(devtype, di, class));
}
