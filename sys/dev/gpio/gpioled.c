/* $MidnightBSD$ */
/*-
 * Copyright (c) 2009 Oleksandr Tymoshenko <gonzo@freebsd.org>
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
__FBSDID("$FreeBSD: stable/10/sys/dev/gpio/gpioled.c 278783 2015-02-14 20:50:38Z loos $");

#include "opt_platform.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/gpio.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>

#ifdef FDT
#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#endif

#include <dev/gpio/gpiobusvar.h>
#include <dev/led/led.h>

#include "gpiobus_if.h"

/*
 * Only one pin for led
 */
#define	GPIOLED_PIN	0

#define GPIOLED_LOCK(_sc)		mtx_lock(&(_sc)->sc_mtx)
#define	GPIOLED_UNLOCK(_sc)		mtx_unlock(&(_sc)->sc_mtx)
#define GPIOLED_LOCK_INIT(_sc) \
	mtx_init(&_sc->sc_mtx, device_get_nameunit(_sc->sc_dev), \
	    "gpioled", MTX_DEF)
#define GPIOLED_LOCK_DESTROY(_sc)	mtx_destroy(&_sc->sc_mtx);

struct gpioled_softc 
{
	device_t	sc_dev;
	device_t	sc_busdev;
	struct mtx	sc_mtx;
	struct cdev	*sc_leddev;
};

static void gpioled_control(void *, int);
static int gpioled_probe(device_t);
static int gpioled_attach(device_t);
static int gpioled_detach(device_t);

static void 
gpioled_control(void *priv, int onoff)
{
	int error;
	struct gpioled_softc *sc;

	sc = (struct gpioled_softc *)priv;
	GPIOLED_LOCK(sc);
	error = GPIOBUS_ACQUIRE_BUS(sc->sc_busdev, sc->sc_dev,
	    GPIOBUS_DONTWAIT);
	if (error != 0) {
		GPIOLED_UNLOCK(sc);
		return;
	}
	error = GPIOBUS_PIN_SETFLAGS(sc->sc_busdev, sc->sc_dev,
	    GPIOLED_PIN, GPIO_PIN_OUTPUT);
	if (error == 0)
		GPIOBUS_PIN_SET(sc->sc_busdev, sc->sc_dev, GPIOLED_PIN,
		    onoff ? GPIO_PIN_HIGH : GPIO_PIN_LOW);
	GPIOBUS_RELEASE_BUS(sc->sc_busdev, sc->sc_dev);
	GPIOLED_UNLOCK(sc);
}

#ifdef FDT
static void
gpioled_identify(driver_t *driver, device_t bus)
{
	phandle_t child, leds, root;

	root = OF_finddevice("/");
	if (root == 0)
		return;
	leds = fdt_find_compatible(root, "gpio-leds", 1);
	if (leds == 0)
		return;

	/* Traverse the 'gpio-leds' node and add its children. */
	for (child = OF_child(leds); child != 0; child = OF_peer(child))
		if (ofw_gpiobus_add_fdt_child(bus, child) == NULL)
			continue;
}
#endif

static int
gpioled_probe(device_t dev)
{
#ifdef FDT
	int match;
	phandle_t node;
	char *compat;

	/*
	 * We can match against our own node compatible string and also against
	 * our parent node compatible string.  The first is normally used to
	 * describe leds on a gpiobus and the later when there is a common node
	 * compatible with 'gpio-leds' which is used to concentrate all the
	 * leds nodes on the dts.
	 */
	match = 0;
	if (ofw_bus_is_compatible(dev, "gpioled"))
		match = 1;

	if (match == 0) {
		if ((node = ofw_bus_get_node(dev)) == -1)
			return (ENXIO);
		if ((node = OF_parent(node)) == -1)
			return (ENXIO);
		if (OF_getprop_alloc(node, "compatible", 1,
		    (void **)&compat) == -1)
			return (ENXIO);

		if (strcasecmp(compat, "gpio-leds") == 0)
			match = 1;

		free(compat, M_OFWPROP);
	}

	if (match == 0)
		return (ENXIO);
#endif
	device_set_desc(dev, "GPIO led");

	return (0);
}

static int
gpioled_attach(device_t dev)
{
	struct gpioled_softc *sc;
#ifdef FDT
	phandle_t node;
	char *name;
#else
	const char *name;
#endif

	sc = device_get_softc(dev);
	sc->sc_dev = dev;
	sc->sc_busdev = device_get_parent(dev);
	GPIOLED_LOCK_INIT(sc);
#ifdef FDT
	name = NULL;
	if ((node = ofw_bus_get_node(dev)) == -1)
		return (ENXIO);
	if (OF_getprop_alloc(node, "label", 1, (void **)&name) == -1)
		OF_getprop_alloc(node, "name", 1, (void **)&name);
#else
	if (resource_string_value(device_get_name(dev), 
	    device_get_unit(dev), "name", &name))
		name = NULL;
#endif

	sc->sc_leddev = led_create(gpioled_control, sc, name ? name :
	    device_get_nameunit(dev));
#ifdef FDT
	if (name != NULL)
		free(name, M_OFWPROP);
#endif

	return (0);
}

static int
gpioled_detach(device_t dev)
{
	struct gpioled_softc *sc;

	sc = device_get_softc(dev);
	if (sc->sc_leddev) {
		led_destroy(sc->sc_leddev);
		sc->sc_leddev = NULL;
	}
	GPIOLED_LOCK_DESTROY(sc);
	return (0);
}

static devclass_t gpioled_devclass;

static device_method_t gpioled_methods[] = {
	/* Device interface */
#ifdef FDT
	DEVMETHOD(device_identify,	gpioled_identify),
#endif
	DEVMETHOD(device_probe,		gpioled_probe),
	DEVMETHOD(device_attach,	gpioled_attach),
	DEVMETHOD(device_detach,	gpioled_detach),

	{ 0, 0 }
};

static driver_t gpioled_driver = {
	"gpioled",
	gpioled_methods,
	sizeof(struct gpioled_softc),
};

DRIVER_MODULE(gpioled, gpiobus, gpioled_driver, gpioled_devclass, 0, 0);
