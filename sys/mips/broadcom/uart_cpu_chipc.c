/*-
 * Copyright (c) 2016 Michael Zhilin <mizhka@gmail.com>
 *
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
__FBSDID("$FreeBSD$");

#include "opt_uart.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/cons.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <machine/bus.h>

#include <dev/bhnd/cores/chipc/chipcreg.h>

#include <dev/uart/uart.h>
#include <dev/uart/uart_bus.h>
#include <dev/uart/uart_cpu.h>

#include "bcm_socinfo.h"

bus_space_tag_t uart_bus_space_io;
bus_space_tag_t uart_bus_space_mem;

static struct uart_class *chipc_uart_class = &uart_ns8250_class;

#define	CHIPC_UART_BAUDRATE	115200

int
uart_cpu_eqres(struct uart_bas *b1, struct uart_bas *b2)
{
	return ((b1->bsh == b2->bsh && b1->bst == b2->bst) ? 1 : 0);
}

static int
uart_cpu_init(struct uart_devinfo *di, int uart, int baudrate)
{
	struct bcm_socinfo	*socinfo;

	if (uart >= CHIPC_UART_MAX)
		return (EINVAL);

	socinfo = bcm_get_socinfo();
	di->ops = uart_getops(chipc_uart_class);
	di->bas.chan = 0;
	di->bas.bst = uart_bus_space_mem;
	di->bas.bsh = (bus_space_handle_t) BCM_SOCREG(CHIPC_UART(uart));
	di->bas.regshft = 0;
	di->bas.rclk = socinfo->uartrate;  /* in Hz */
	di->baudrate = baudrate;
	di->databits = 8;
	di->stopbits = 1;
	di->parity = UART_PARITY_NONE;

	return (0);
}

int
uart_cpu_getdev(int devtype, struct uart_devinfo *di)
{
	int			 ivar;

	uart_bus_space_io = NULL;
	uart_bus_space_mem = mips_bus_space_generic;

	/* Check the environment. */
	if (uart_getenv(devtype, di, chipc_uart_class) == 0)
		return (0);

	/* Scan the device hints for the first matching device */
	for (int i = 0; i < CHIPC_UART_MAX; i++) {
		if (resource_int_value("uart", i, "flags", &ivar))
			continue;

		/* Check usability */
		if (devtype == UART_DEV_CONSOLE && !UART_FLAGS_CONSOLE(ivar))
			continue;

		if (devtype == UART_DEV_DBGPORT && !UART_FLAGS_DBGPORT(ivar))
			continue;

		if (resource_int_value("uart", i, "disabled", &ivar) == 0 &&
		    ivar == 0)
			continue;

		/* Found */
		if (resource_int_value("uart", i, "baud", &ivar) != 0)
			ivar = CHIPC_UART_BAUDRATE;
		
		return (uart_cpu_init(di, i, ivar));
	}

	/* Default to uart0/115200 */
	return (uart_cpu_init(di, 0, CHIPC_UART_BAUDRATE));
}
