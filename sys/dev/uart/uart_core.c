/*-
 * Copyright (c) 2003 Marcel Moolenaar
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
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/fcntl.h>
#include <sys/interrupt.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/reboot.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>
#include <machine/stdarg.h>

#include <dev/uart/uart.h>
#include <dev/uart/uart_bus.h>
#include <dev/uart/uart_cpu.h>

#include "uart_if.h"

devclass_t uart_devclass;
char uart_driver_name[] = "uart";

SLIST_HEAD(uart_devinfo_list, uart_devinfo) uart_sysdevs =
    SLIST_HEAD_INITIALIZER(uart_sysdevs);

MALLOC_DEFINE(M_UART, "UART", "UART driver");

void
uart_add_sysdev(struct uart_devinfo *di)
{
	SLIST_INSERT_HEAD(&uart_sysdevs, di, next);
}

const char *
uart_getname(struct uart_class *uc)
{
	return ((uc != NULL) ? uc->name : NULL);
}

struct uart_ops *
uart_getops(struct uart_class *uc)
{
	return ((uc != NULL) ? uc->uc_ops : NULL);
}

int
uart_getrange(struct uart_class *uc)
{
	return ((uc != NULL) ? uc->uc_range : 0);
}

/*
 * Schedule a soft interrupt. We do this on the 0 to !0 transition
 * of the TTY pending interrupt status.
 */
void
uart_sched_softih(struct uart_softc *sc, uint32_t ipend)
{
	uint32_t new, old;

	do {
		old = sc->sc_ttypend;
		new = old | ipend;
	} while (!atomic_cmpset_32(&sc->sc_ttypend, old, new));

	if ((old & SER_INT_MASK) == 0)
		swi_sched(sc->sc_softih, 0);
}

/*
 * A break condition has been detected. We treat the break condition as
 * a special case that should not happen during normal operation. When
 * the break condition is to be passed to higher levels in the form of
 * a NUL character, we really want the break to be in the right place in
 * the input stream. The overhead to achieve that is not in relation to
 * the exceptional nature of the break condition, so we permit ourselves
 * to be sloppy.
 */
static __inline int
uart_intr_break(void *arg)
{
	struct uart_softc *sc = arg;

#if defined(KDB)
	if (sc->sc_sysdev != NULL && sc->sc_sysdev->type == UART_DEV_CONSOLE) {
		if (kdb_break())
			return (0);
	}
#endif
	if (sc->sc_opened)
		uart_sched_softih(sc, SER_INT_BREAK);
	return (0);
}

/*
 * Handle a receiver overrun situation. We lost at least 1 byte in the
 * input stream and it's our job to contain the situation. We grab as
 * much of the data we can, but otherwise flush the receiver FIFO to
 * create some breathing room. The net effect is that we avoid the
 * overrun condition to happen for the next X characters, where X is
 * related to the FIFO size at the cost of loosing data right away.
 * So, instead of having multiple overrun interrupts in close proximity
 * to each other and possibly pessimizing UART interrupt latency for
 * other UARTs in a multiport configuration, we create a longer segment
 * of missing characters by freeing up the FIFO.
 * Each overrun condition is marked in the input buffer by a token. The
 * token represents the loss of at least one, but possible more bytes in
 * the input stream.
 */
static __inline int
uart_intr_overrun(void *arg)
{
	struct uart_softc *sc = arg;

	if (sc->sc_opened) {
		UART_RECEIVE(sc);
		if (uart_rx_put(sc, UART_STAT_OVERRUN))
			sc->sc_rxbuf[sc->sc_rxput] = UART_STAT_OVERRUN;
		uart_sched_softih(sc, SER_INT_RXREADY);
	}
	UART_FLUSH(sc, UART_FLUSH_RECEIVER);
	return (0);
}

/*
 * Received data ready.
 */
static __inline int
uart_intr_rxready(void *arg)
{
	struct uart_softc *sc = arg;
	int rxp;

	rxp = sc->sc_rxput;
	UART_RECEIVE(sc);
#if defined(KDB)
	if (sc->sc_sysdev != NULL && sc->sc_sysdev->type == UART_DEV_CONSOLE) {
		while (rxp != sc->sc_rxput) {
			kdb_alt_break(sc->sc_rxbuf[rxp++], &sc->sc_altbrk);
			if (rxp == sc->sc_rxbufsz)
				rxp = 0;
		}
	}
#endif
	if (sc->sc_opened)
		uart_sched_softih(sc, SER_INT_RXREADY);
	else
		sc->sc_rxput = sc->sc_rxget;	/* Ignore received data. */
	return (1);
}

/*
 * Line or modem status change (OOB signalling).
 * We pass the signals to the software interrupt handler for further
 * processing. Note that we merge the delta bits, but set the state
 * bits. This is to avoid loosing state transitions due to having more
 * than 1 hardware interrupt between software interrupts.
 */
static __inline int
uart_intr_sigchg(void *arg)
{
	struct uart_softc *sc = arg;
	int new, old, sig;

	sig = UART_GETSIG(sc);

	if (sc->sc_pps.ppsparam.mode & PPS_CAPTUREBOTH) {
		if (sig & UART_SIG_DPPS) {
			pps_capture(&sc->sc_pps);
			pps_event(&sc->sc_pps, (sig & UART_SIG_PPS) ?
			    PPS_CAPTUREASSERT : PPS_CAPTURECLEAR);
		}
	}

	/*
	 * Keep track of signal changes, even when the device is not
	 * opened. This allows us to inform upper layers about a
	 * possible loss of DCD and thus the existence of a (possibly)
	 * different connection when we have DCD back, during the time
	 * that the device was closed.
	 */
	do {
		old = sc->sc_ttypend;
		new = old & ~SER_MASK_STATE;
		new |= sig & SER_INT_SIGMASK;
	} while (!atomic_cmpset_32(&sc->sc_ttypend, old, new));

	if (sc->sc_opened)
		uart_sched_softih(sc, SER_INT_SIGCHG);
	return (1);
}

/*
 * The transmitter can accept more data.
 */
static __inline int
uart_intr_txidle(void *arg)
{
	struct uart_softc *sc = arg;

	if (sc->sc_txbusy) {
		sc->sc_txbusy = 0;
		uart_sched_softih(sc, SER_INT_TXIDLE);
	}
	return (0);
}

static int
uart_intr(void *arg)
{
	struct uart_softc *sc = arg;
	int flag = 0, ipend;

	while (!sc->sc_leaving && (ipend = UART_IPEND(sc)) != 0) {
		flag = 1;
		if (ipend & SER_INT_OVERRUN)
			uart_intr_overrun(sc);
		if (ipend & SER_INT_BREAK)
			uart_intr_break(sc);
		if (ipend & SER_INT_RXREADY)
			uart_intr_rxready(sc);
		if (ipend & SER_INT_SIGCHG)
			uart_intr_sigchg(sc);
		if (ipend & SER_INT_TXIDLE)
			uart_intr_txidle(sc);		
	}
	return((flag)?FILTER_HANDLED:FILTER_STRAY);
}

serdev_intr_t *
uart_bus_ihand(device_t dev, int ipend)
{

	switch (ipend) {
	case SER_INT_BREAK:
		return (uart_intr_break);
	case SER_INT_OVERRUN:
		return (uart_intr_overrun);
	case SER_INT_RXREADY:
		return (uart_intr_rxready);
	case SER_INT_SIGCHG:
		return (uart_intr_sigchg);
	case SER_INT_TXIDLE:
		return (uart_intr_txidle);
	}
	return (NULL);
}

int
uart_bus_ipend(device_t dev)
{
	struct uart_softc *sc;

	sc = device_get_softc(dev);
	return (UART_IPEND(sc));
}

int
uart_bus_sysdev(device_t dev)
{
	struct uart_softc *sc;

	sc = device_get_softc(dev);
	return ((sc->sc_sysdev != NULL) ? 1 : 0);
}

int
uart_bus_probe(device_t dev, int regshft, int rclk, int rid, int chan)
{
	struct uart_softc *sc;
	struct uart_devinfo *sysdev;
	int error;

	sc = device_get_softc(dev);

	/*
	 * All uart_class references are weak. Check that the needed
	 * class has been compiled-in. Fail if not.
	 */
	if (sc->sc_class == NULL)
		return (ENXIO);

	/*
	 * Initialize the instance. Note that the instance (=softc) does
	 * not necessarily match the hardware specific softc. We can't do
	 * anything about it now, because we may not attach to the device.
	 * Hardware drivers cannot use any of the class specific fields
	 * while probing.
	 */
	kobj_init((kobj_t)sc, (kobj_class_t)sc->sc_class);
	sc->sc_dev = dev;
	if (device_get_desc(dev) == NULL)
		device_set_desc(dev, uart_getname(sc->sc_class));

	/*
	 * Allocate the register resource. We assume that all UARTs have
	 * a single register window in either I/O port space or memory
	 * mapped I/O space. Any UART that needs multiple windows will
	 * consequently not be supported by this driver as-is. We try I/O
	 * port space first because that's the common case.
	 */
	sc->sc_rrid = rid;
	sc->sc_rtype = SYS_RES_IOPORT;
	sc->sc_rres = bus_alloc_resource(dev, sc->sc_rtype, &sc->sc_rrid,
	    0, ~0, uart_getrange(sc->sc_class), RF_ACTIVE);
	if (sc->sc_rres == NULL) {
		sc->sc_rrid = rid;
		sc->sc_rtype = SYS_RES_MEMORY;
		sc->sc_rres = bus_alloc_resource(dev, sc->sc_rtype,
		    &sc->sc_rrid, 0, ~0, uart_getrange(sc->sc_class),
		    RF_ACTIVE);
		if (sc->sc_rres == NULL)
			return (ENXIO);
	}

	/*
	 * Fill in the bus access structure and compare this device with
	 * a possible console device and/or a debug port. We set the flags
	 * in the softc so that the hardware dependent probe can adjust
	 * accordingly. In general, you don't want to permanently disrupt
	 * console I/O.
	 */
	sc->sc_bas.bsh = rman_get_bushandle(sc->sc_rres);
	sc->sc_bas.bst = rman_get_bustag(sc->sc_rres);
	sc->sc_bas.chan = chan;
	sc->sc_bas.regshft = regshft;
	sc->sc_bas.rclk = (rclk == 0) ? sc->sc_class->uc_rclk : rclk;

	SLIST_FOREACH(sysdev, &uart_sysdevs, next) {
		if (chan == sysdev->bas.chan &&
		    uart_cpu_eqres(&sc->sc_bas, &sysdev->bas)) {
			/* XXX check if ops matches class. */
			sc->sc_sysdev = sysdev;
			sysdev->bas.rclk = sc->sc_bas.rclk;
		}
	}

	error = UART_PROBE(sc);
	bus_release_resource(dev, sc->sc_rtype, sc->sc_rrid, sc->sc_rres);
	return ((error) ? error : BUS_PROBE_DEFAULT);
}

int
uart_bus_attach(device_t dev)
{
	struct uart_softc *sc, *sc0;
	const char *sep;
	int error;

	/*
	 * The sc_class field defines the type of UART we're going to work
	 * with and thus the size of the softc. Replace the generic softc
	 * with one that matches the UART now that we're certain we handle
	 * the device.
	 */
	sc0 = device_get_softc(dev);
	if (sc0->sc_class->size > sizeof(*sc)) {
		sc = malloc(sc0->sc_class->size, M_UART, M_WAITOK|M_ZERO);
		bcopy(sc0, sc, sizeof(*sc));
		device_set_softc(dev, sc);
	} else
		sc = sc0;

	/*
	 * Protect ourselves against interrupts while we're not completely
	 * finished attaching and initializing. We don't expect interrupts
	 * until after UART_ATTACH() though.
	 */
	sc->sc_leaving = 1;

	mtx_init(&sc->sc_hwmtx_s, "uart_hwmtx", NULL, MTX_SPIN);
	if (sc->sc_hwmtx == NULL)
		sc->sc_hwmtx = &sc->sc_hwmtx_s;

	/*
	 * Re-allocate. We expect that the softc contains the information
	 * collected by uart_bus_probe() intact.
	 */
	sc->sc_rres = bus_alloc_resource(dev, sc->sc_rtype, &sc->sc_rrid,
	    0, ~0, uart_getrange(sc->sc_class), RF_ACTIVE);
	if (sc->sc_rres == NULL) {
		mtx_destroy(&sc->sc_hwmtx_s);
		return (ENXIO);
	}
	sc->sc_bas.bsh = rman_get_bushandle(sc->sc_rres);
	sc->sc_bas.bst = rman_get_bustag(sc->sc_rres);

	sc->sc_irid = 0;
	sc->sc_ires = bus_alloc_resource_any(dev, SYS_RES_IRQ, &sc->sc_irid,
	    RF_ACTIVE | RF_SHAREABLE);
	if (sc->sc_ires != NULL) {
		error = bus_setup_intr(dev,
		    sc->sc_ires, INTR_TYPE_TTY, 
		    uart_intr, NULL, sc, &sc->sc_icookie);		    
		if (error)
			error = bus_setup_intr(dev,
			    sc->sc_ires, INTR_TYPE_TTY | INTR_MPSAFE,
			    NULL, (driver_intr_t *)uart_intr, sc, &sc->sc_icookie);
		else
			sc->sc_fastintr = 1;

		if (error) {
			device_printf(dev, "could not activate interrupt\n");
			bus_release_resource(dev, SYS_RES_IRQ, sc->sc_irid,
			    sc->sc_ires);
			sc->sc_ires = NULL;
		}
	}
	if (sc->sc_ires == NULL) {
		/* XXX no interrupt resource. Force polled mode. */
		sc->sc_polled = 1;
	}

	sc->sc_rxbufsz = 384;
	sc->sc_rxbuf = malloc(sc->sc_rxbufsz * sizeof(*sc->sc_rxbuf),
	    M_UART, M_WAITOK);
	sc->sc_txbuf = malloc(sc->sc_txfifosz * sizeof(*sc->sc_txbuf),
	    M_UART, M_WAITOK);

	error = UART_ATTACH(sc);
	if (error)
		goto fail;

	if (sc->sc_hwiflow || sc->sc_hwoflow) {
		sep = "";
		device_print_prettyname(dev);
		if (sc->sc_hwiflow) {
			printf("%sRTS iflow", sep);
			sep = ", ";
		}
		if (sc->sc_hwoflow) {
			printf("%sCTS oflow", sep);
			sep = ", ";
		}
		printf("\n");
	}

	if (bootverbose && (sc->sc_fastintr || sc->sc_polled)) {
		sep = "";
		device_print_prettyname(dev);
		if (sc->sc_fastintr) {
			printf("%sfast interrupt", sep);
			sep = ", ";
		}
		if (sc->sc_polled) {
			printf("%spolled mode", sep);
			sep = ", ";
		}
		printf("\n");
	}

	if (sc->sc_sysdev != NULL) {
		if (sc->sc_sysdev->baudrate == 0) {
			if (UART_IOCTL(sc, UART_IOCTL_BAUD,
			    (intptr_t)&sc->sc_sysdev->baudrate) != 0)
				sc->sc_sysdev->baudrate = -1;
		}
		switch (sc->sc_sysdev->type) {
		case UART_DEV_CONSOLE:
			device_printf(dev, "console");
			break;
		case UART_DEV_DBGPORT:
			device_printf(dev, "debug port");
			break;
		case UART_DEV_KEYBOARD:
			device_printf(dev, "keyboard");
			break;
		default:
			device_printf(dev, "unknown system device");
			break;
		}
		printf(" (%d,%c,%d,%d)\n", sc->sc_sysdev->baudrate,
		    "noems"[sc->sc_sysdev->parity], sc->sc_sysdev->databits,
		    sc->sc_sysdev->stopbits);
	}

	sc->sc_pps.ppscap = PPS_CAPTUREBOTH;
	pps_init(&sc->sc_pps);

	error = (sc->sc_sysdev != NULL && sc->sc_sysdev->attach != NULL)
	    ? (*sc->sc_sysdev->attach)(sc) : uart_tty_attach(sc);
	if (error)
		goto fail;

	if (sc->sc_sysdev != NULL)
		sc->sc_sysdev->hwmtx = sc->sc_hwmtx;

	sc->sc_leaving = 0;
	uart_intr(sc);
	return (0);

 fail:
	free(sc->sc_txbuf, M_UART);
	free(sc->sc_rxbuf, M_UART);

	if (sc->sc_ires != NULL) {
		bus_teardown_intr(dev, sc->sc_ires, sc->sc_icookie);
		bus_release_resource(dev, SYS_RES_IRQ, sc->sc_irid,
		    sc->sc_ires);
	}
	bus_release_resource(dev, sc->sc_rtype, sc->sc_rrid, sc->sc_rres);

	mtx_destroy(&sc->sc_hwmtx_s);

	return (error);
}

int
uart_bus_detach(device_t dev)
{
	struct uart_softc *sc;

	sc = device_get_softc(dev);

	sc->sc_leaving = 1;

	if (sc->sc_sysdev != NULL)
		sc->sc_sysdev->hwmtx = NULL;

	UART_DETACH(sc);

	if (sc->sc_sysdev != NULL && sc->sc_sysdev->detach != NULL)
		(*sc->sc_sysdev->detach)(sc);
	else
		uart_tty_detach(sc);

	free(sc->sc_txbuf, M_UART);
	free(sc->sc_rxbuf, M_UART);

	if (sc->sc_ires != NULL) {
		bus_teardown_intr(dev, sc->sc_ires, sc->sc_icookie);
		bus_release_resource(dev, SYS_RES_IRQ, sc->sc_irid,
		    sc->sc_ires);
	}
	bus_release_resource(dev, sc->sc_rtype, sc->sc_rrid, sc->sc_rres);

	mtx_destroy(&sc->sc_hwmtx_s);

	if (sc->sc_class->size > sizeof(*sc)) {
		device_set_softc(dev, NULL);
		free(sc, M_UART);
	} else
		device_set_softc(dev, NULL);

	return (0);
}
