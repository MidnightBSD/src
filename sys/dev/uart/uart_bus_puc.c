/*-
 * Copyright (c) 2002 JF Hay.  All rights reserved.
 * Copyright (c) 2001 M. Warner Losh.  All rights reserved.
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
__FBSDID("$FreeBSD: src/sys/dev/uart/uart_bus_puc.c,v 1.2 2003/09/26 05:14:56 marcel Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/pci/pcivar.h>
#include <dev/puc/pucvar.h>

#include <dev/uart/uart.h>
#include <dev/uart/uart_bus.h>

static int uart_puc_probe(device_t dev);

static device_method_t uart_puc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		uart_puc_probe),
	DEVMETHOD(device_attach,	uart_bus_attach),
	DEVMETHOD(device_detach,	uart_bus_detach),
	{ 0, 0 }
};

static driver_t uart_puc_driver = {
	uart_driver_name,
	uart_puc_methods,
	sizeof(struct uart_softc),
};

static int
uart_puc_probe(device_t dev)
{
	device_t parent;
	struct uart_softc *sc;
	uintptr_t port, rclk, regshft, type;

	parent = device_get_parent(dev);
	sc = device_get_softc(dev);

	if (BUS_READ_IVAR(parent, dev, PUC_IVAR_SUBTYPE, &type))
		return (ENXIO);
	switch (type) {
	case PUC_PORT_UART_NS8250:
		sc->sc_class = &uart_ns8250_class;
		port = 0;
		break;
	case PUC_PORT_UART_SAB82532:
		sc->sc_class = &uart_sab82532_class;
		if (BUS_READ_IVAR(parent, dev, PUC_IVAR_PORT, &port))
			port = 0;
		break;
	case PUC_PORT_UART_Z8530:
		sc->sc_class = &uart_z8530_class;
		if (BUS_READ_IVAR(parent, dev, PUC_IVAR_PORT, &port))
			port = 0;
		break;
	default:
		return (ENXIO);
	}

	if (BUS_READ_IVAR(parent, dev, PUC_IVAR_FREQ, &rclk))
		rclk = 0;
	if (BUS_READ_IVAR(parent, dev, PUC_IVAR_REGSHFT, &regshft))
		regshft = 0;
	return (uart_bus_probe(dev, regshft, rclk, 0, port));
}

DRIVER_MODULE(uart, puc, uart_puc_driver, uart_devclass, 0, 0);
