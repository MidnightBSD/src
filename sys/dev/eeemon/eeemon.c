/*-
 * Copyright (c) 2008 Rui Paulo <rpaulo@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__MBSDID("$MidnightBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/taskqueue.h>
#include <sys/rman.h>

#include <isa/isavar.h>
#include <machine/cpufunc.h>
#include <machine/bus.h>

#include "eeemonreg.h"

enum {
	EEEMON_TEMP,
	EEEMON_FAN,
	EEEMON_VOLT,
	EEEMON_FANMANUAL,
	EEEMON_FANSPEED
};

/*
 * Device interface.
 */
static void	eeemon_identify(driver_t *driver, device_t parent);
static int 	eeemon_probe(device_t dev);
static int 	eeemon_attach(device_t dev);
static int 	eeemon_detach(device_t dev);
static void	eeemon_intrhook(void *arg);
static int	eeemon_match(void);
static int	eeemon_readbyte(uint16_t addr);
static void	eeemon_writebyte(uint16_t addr, uint8_t byte);
static int	eeemon_sysctl(SYSCTL_HANDLER_ARGS);

struct eeemon_softc {
	device_t		sc_dev;
        struct sysctl_oid	*sc_sctl_cputemp;
	struct sysctl_oid	*sc_sctl_cpufan;
	struct intr_config_hook sc_ich;
};

/*
 * Driver methods.
 */
static device_method_t eeemon_methods[] = {
	DEVMETHOD(device_identify,	eeemon_identify),
	DEVMETHOD(device_probe,		eeemon_probe),
	DEVMETHOD(device_attach,	eeemon_attach),
	DEVMETHOD(device_detach,	eeemon_detach),

	{ 0, 0 }
};

static driver_t	eeemon_driver = {
	"eeemon",
	eeemon_methods,
	sizeof(struct eeemon_softc)
};

static devclass_t eeemon_devclass;

DRIVER_MODULE(eeemon, isa, eeemon_driver, eeemon_devclass, 0, 0);

static int
eeemon_match(void)
{
	int i;
	char *model;

	model = getenv("smbios.system.serial");
	i = 0;
	if (strncmp(model, "EeePC", 5) == 0)
		i = 1;
	freeenv(model);

	return (i);
}

static void
eeemon_identify(driver_t *driver, device_t parent)
{
	device_t child;

        if (device_find_child(parent, "eeemon", -1) != NULL)
		return;

	if (eeemon_match() == 0)
		return;

	child = BUS_ADD_CHILD(parent, ISA_ORDER_SPECULATIVE, "eeemon", -1);
	if (child == NULL)
		device_printf(parent, "add eeemon child failed\n");

}

static int
eeemon_probe(device_t dev)
{
	if (resource_disabled("eeemon", 0))
		return (ENXIO);
	
	if (eeemon_match() == 0) {
		device_printf(dev, "model not recognized\n");
		return (ENXIO);
	}
	device_set_desc(dev, "Asus Eee PC Hardware Monitor");

	return (BUS_PROBE_DEFAULT);
}

static int
eeemon_attach(device_t dev)
{
	struct eeemon_softc *sc;

	sc = device_get_softc(dev);

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "voltage", CTLTYPE_INT | CTLFLAG_RW,
	    sc, EEEMON_VOLT, eeemon_sysctl, "I",
	    "Voltage (V)");

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "fan_manual", CTLTYPE_INT | CTLFLAG_RW,
	    sc, EEEMON_FANMANUAL, eeemon_sysctl, "I",
	    "Fan Manual");

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "fan_speed", CTLTYPE_INT | CTLFLAG_RW,
	    sc, EEEMON_FANSPEED, eeemon_sysctl, "I",
	    "Fan Speed (%)");

	sc->sc_ich.ich_func = eeemon_intrhook;
	sc->sc_ich.ich_arg = sc;
	config_intrhook_establish(&sc->sc_ich);

	return (0);
}

static int
eeemon_detach(device_t dev)
{
	struct eeemon_softc *sc;

	sc = device_get_softc(dev);
	sysctl_remove_oid(sc->sc_sctl_cputemp, 1, 0);
	sysctl_remove_oid(sc->sc_sctl_cpufan, 1, 0);

	return (0);
}

static void
eeemon_intrhook(void *arg)
{
	device_t nexus, acpi, cpu;
        struct sysctl_ctx_list *sysctlctx;
	struct eeemon_softc *sc;

	sc = arg;

        /*
	 * dev.cpu.0.temperature and dev.cpu.0.fan
	 */
	nexus = device_find_child(root_bus, "nexus", 0);
	acpi = device_find_child(nexus, "acpi", 0);
	cpu = device_find_child(acpi, "cpu", 0);
	if (cpu) {
		sysctlctx = device_get_sysctl_ctx(cpu);
		sc->sc_sctl_cputemp = SYSCTL_ADD_PROC(sysctlctx,
		    SYSCTL_CHILDREN(device_get_sysctl_tree(cpu)),
		    OID_AUTO, "temperature", CTLTYPE_INT | CTLFLAG_RD,
		    sc, EEEMON_TEMP, eeemon_sysctl, "I",
		    "CPU temperature (degC)");
		sc->sc_sctl_cpufan = SYSCTL_ADD_PROC(sysctlctx,
		    SYSCTL_CHILDREN(device_get_sysctl_tree(cpu)),
		    OID_AUTO, "fan", CTLTYPE_INT | CTLFLAG_RD,
		    sc, EEEMON_FAN, eeemon_sysctl, "I",
		    "CPU Fan speed (RPM)");
	}
	config_intrhook_disestablish(&sc->sc_ich);
}

static int
eeemon_readbyte(uint16_t addr)
{
	int d;
	
	outb(EEEMON_ADDRH, (addr & 0xff00) >> 8);
	outb(EEEMON_ADDRL, addr & 0x00ff);
	d = inb(EEEMON_DATA);

	return (d);
}

static void
eeemon_writebyte(uint16_t addr, uint8_t byte)
{
	outb(EEEMON_ADDRH, (addr & 0xff00) >> 8);
	outb(EEEMON_ADDRL, addr & 0x00ff);
	outb(EEEMON_DATA, byte);
}

static int
eeemon_sysctl(SYSCTL_HANDLER_ARGS)
{
	int value;
	unsigned short port;
	unsigned char mask;
	int pin;
	int error;

	switch (arg2) {
	case EEEMON_TEMP:
		value = eeemon_readbyte(EEEMON_TEMPVAL);
		break;
	case EEEMON_FAN:
		value = (eeemon_readbyte(EEEMON_FANHVAL) << 8)
		    | eeemon_readbyte(EEEMON_FANLVAL);
		break;
	case EEEMON_FANMANUAL:
		value = (eeemon_readbyte(EEEMON_FANMANUALVAL) & 0x02) ? 1 : 0;
		break;
	case EEEMON_FANSPEED:
		value = eeemon_readbyte(EEEMON_FANSPEEDVAL);
		break;
	case EEEMON_VOLT:
		pin = 0x66;
		port = 0xfc20 + ((pin >> 3) & 0x1f);
		mask = 1 << (pin & 0x07);
		value = (eeemon_readbyte(port) & mask) ? 1 : 0;
	}


	error = sysctl_handle_int(oidp, &value, 0, req);
	if (error || !req->newptr)
		return (error);

	switch (arg2) {
	case EEEMON_FANSPEED:
		if (value > 100 || value < 0)
			return (EINVAL);
		eeemon_writebyte(EEEMON_FANSPEEDVAL, value);
		break;
	case EEEMON_FANMANUAL:
		if (value > 1 || value < 0)
			return (EINVAL);
		if (value) {
			// SF25=1: Prevent the EC from controlling the fan.
			eeemon_writebyte(EEEMON_FANMANUALVAL, eeemon_readbyte(EEEMON_FANMANUALVAL) | 0x02);
		} else
			// SF25=0: Allow the EC to control the fan.
			eeemon_writebyte(EEEMON_FANMANUALVAL, eeemon_readbyte(EEEMON_FANMANUALVAL) & ~0x02);
		break;
	case EEEMON_VOLT:
		if (value > 1 || value < 0)
			return (EINVAL);
		pin = 0x66;
		port = 0xfc20 + ((pin >> 3) & 0x1f);
		mask = 1 << (pin & 0x07);
		if (value) {
			eeemon_writebyte(port, eeemon_readbyte(port) | mask);
		} else
			eeemon_writebyte(port, eeemon_readbyte(port) & ~mask);
	}

	return (0);
}
