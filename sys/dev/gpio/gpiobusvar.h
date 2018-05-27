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
 *
 * $FreeBSD: stable/10/sys/dev/gpio/gpiobusvar.h 278786 2015-02-14 21:16:19Z loos $
 *
 */

#ifndef	__GPIOBUS_H__
#define	__GPIOBUS_H__

#include "opt_platform.h"

#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/rman.h>

#ifdef FDT
#include <dev/ofw/ofw_bus_subr.h>
#endif

#include "gpio_if.h"

#ifdef FDT
#define	GPIOBUS_IVAR(d) (struct gpiobus_ivar *)				\
	&((struct ofw_gpiobus_devinfo *)device_get_ivars(d))->opd_dinfo
#else
#define	GPIOBUS_IVAR(d) (struct gpiobus_ivar *) device_get_ivars(d)
#endif
#define	GPIOBUS_SOFTC(d) (struct gpiobus_softc *) device_get_softc(d)
#define	GPIOBUS_LOCK(_sc) mtx_lock(&(_sc)->sc_mtx)
#define	GPIOBUS_UNLOCK(_sc) mtx_unlock(&(_sc)->sc_mtx)
#define	GPIOBUS_LOCK_INIT(_sc) mtx_init(&_sc->sc_mtx,			\
	    device_get_nameunit(_sc->sc_dev), "gpiobus", MTX_DEF)
#define	GPIOBUS_LOCK_DESTROY(_sc) mtx_destroy(&_sc->sc_mtx)
#define	GPIOBUS_ASSERT_LOCKED(_sc) mtx_assert(&_sc->sc_mtx, MA_OWNED)
#define	GPIOBUS_ASSERT_UNLOCKED(_sc) mtx_assert(&_sc->sc_mtx, MA_NOTOWNED)

#define	GPIOBUS_WAIT		1
#define	GPIOBUS_DONTWAIT	2

struct gpiobus_softc
{
	struct mtx	sc_mtx;		/* bus mutex */
	struct rman	sc_intr_rman;	/* isr resources */
	device_t	sc_busdev;	/* bus device */
	device_t	sc_owner;	/* bus owner */
	device_t	sc_dev;		/* driver device */
	int		sc_npins;	/* total pins on bus */
	int		*sc_pins_mapped; /* mark mapped pins */
};

struct gpiobus_ivar
{
	struct resource_list	rl;	/* isr resource list */
	uint32_t	npins;	/* pins total */
	uint32_t	*flags;	/* pins flags */
	uint32_t	*pins;	/* pins map */
};

#ifdef FDT
struct ofw_gpiobus_devinfo {
	struct gpiobus_ivar	opd_dinfo;
	struct ofw_bus_devinfo	opd_obdinfo;
};

static __inline int
gpio_map_gpios(device_t bus, phandle_t dev, phandle_t gparent, int gcells,
    pcell_t *gpios, uint32_t *pin, uint32_t *flags)
{
	return (GPIO_MAP_GPIOS(bus, dev, gparent, gcells, gpios, pin, flags));
}

device_t ofw_gpiobus_add_fdt_child(device_t, phandle_t);
#endif
int gpio_check_flags(uint32_t, uint32_t);
int gpiobus_init_softc(device_t);

extern driver_t gpiobus_driver;

#endif	/* __GPIOBUS_H__ */
