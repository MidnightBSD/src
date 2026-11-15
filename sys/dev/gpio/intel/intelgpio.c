/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Beckhoff Automation GmbH & Co. KG
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/gpio.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>

#include <dev/acpica/acpivar.h>
#include <dev/gpio/gpiobusvar.h>

#include "intelgpio.h"

#include "gpio_if.h"
#include "opt_acpi.h"

#define INTELGPIO_LOCK(_sc)   mtx_lock_spin(&(_sc)->sc_mtx)
#define INTELGPIO_UNLOCK(_sc) mtx_unlock_spin(&(_sc)->sc_mtx)
#define INTELGPIO_LOCK_INIT(_sc)                                     \
	mtx_init(&(_sc)->sc_mtx, device_get_nameunit((_sc)->sc_dev), \
	    "intelgpio", MTX_SPIN)
#define INTELGPIO_LOCK_DESTROY(_sc) mtx_destroy(&(_sc)->sc_mtx)

static int
intelgpio_gpio_to_pad(struct intelgpio_softc *sc, uint32_t pin,
    struct resource **res, bus_size_t *pad_off,
    const struct intelgpio_padgroup **pgp)
{
	const struct intelgpio_platform *plat = sc->sc_plat;
	const struct intelgpio_community *com;
	const struct intelgpio_padgroup *pg;
	int i, j, offset, local_pad;

	for (i = 0; i < plat->ncommunities; i++) {
		com = &plat->communities[i];
		for (j = 0; j < com->ngroups; j++) {
			pg = &com->groups[j];
			if (pg->gpio_base == INTELGPIO_GPIO_NOMAP)
				continue;
			if (pin >= pg->gpio_base &&
			    pin < pg->gpio_base + pg->npads) {
				if (res != NULL)
					*res = sc->sc_mem_res[i];
				if (pad_off != NULL) {
					offset = pin - pg->gpio_base;
					local_pad = (pg->first_pad + offset) -
					    com->groups[0].first_pad;
					*pad_off = sc->sc_padbar[i] +
					    (local_pad * INTELGPIO_PAD_SIZE);
				}
				if (pgp != NULL)
					*pgp = pg;
				return (0);
			}
		}
	}
	return (-1);
}

static device_t
intelgpio_get_bus(device_t dev)
{
	struct intelgpio_softc *sc;

	sc = device_get_softc(dev);
	return (sc->sc_busdev);
}

static int
intelgpio_pin_max(device_t dev, int *maxpin)
{
	struct intelgpio_softc *sc;
	const struct intelgpio_platform *plat;
	const struct intelgpio_community *com;
	const struct intelgpio_padgroup *pg;
	int i, j;

	if (maxpin == NULL)
		return (EINVAL);

	sc = device_get_softc(dev);
	plat = sc->sc_plat;

	/* Find the last group with a valid gpio_base, skipping NOMAP */
	*maxpin = 0;
	for (i = 0; i < plat->ncommunities; i++) {
		com = &plat->communities[i];
		for (j = 0; j < com->ngroups; j++) {
			pg = &com->groups[j];
			if (pg->gpio_base == INTELGPIO_GPIO_NOMAP)
				continue;
			*maxpin = pg->gpio_base + (pg->npads - 1);
		}
	}
	return (0);
}

static int
intelgpio_pin_getname(device_t dev, uint32_t pin, char *name)
{
	struct intelgpio_softc *sc;
	const struct intelgpio_padgroup *pg;
	int offset;

	sc = device_get_softc(dev);
	if (intelgpio_gpio_to_pad(sc, pin, NULL, NULL, &pg) != 0)
		return (EINVAL);

	offset = pin - pg->gpio_base;
	snprintf(name, GPIOMAXNAME, "%s%d", pg->name, offset);
	return (0);
}

static int
intelgpio_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps)
{
	struct intelgpio_softc *sc;

	sc = device_get_softc(dev);

	/*
	 * The getcaps function is expected to return the capabilities
	 * of the pin. Validate that the pin number maps to an existing
	 * pad first.
	 */
	if (intelgpio_gpio_to_pad(sc, pin, NULL, NULL, NULL) != 0)
		return (EINVAL);

	*caps = GPIO_PIN_INPUT | GPIO_PIN_OUTPUT;
	return (0);
}

static int
intelgpio_pin_getflags(device_t dev, uint32_t pin, uint32_t *flags)
{
	struct intelgpio_softc *sc;
	struct resource *res;
	bus_size_t pad_off;
	uint32_t val;

	sc = device_get_softc(dev);
	if (intelgpio_gpio_to_pad(sc, pin, &res, &pad_off, NULL) != 0)
		return (EINVAL);
	if (res == NULL)
		return (EINVAL);

	*flags = 0;

	INTELGPIO_LOCK(sc);
	val = bus_read_4(res, pad_off);
	INTELGPIO_UNLOCK(sc);

	/*
	 * If the pad is not in GPIO mode (i.e. configured for a native
	 * function like PCIe, UART, I2C, etc.), report no flags. The pin
	 * cannot be used as GPIO without first switching it via setflags.
	 */
	if ((val & INTELGPIO_PADCFG0_PMODE_MASK) !=
	    INTELGPIO_PADCFG0_PMODE_GPIO)
		return (0);

	/*
	 * Report OUTPUT only if TX is explicitly enabled. Default to INPUT
	 * otherwise: a non-driving pin is considered an input from the user's
	 * perspective.
	 */
	if (!(val & INTELGPIO_PADCFG0_GPIOTXDIS))
		*flags = GPIO_PIN_OUTPUT;
	else
		*flags = GPIO_PIN_INPUT;

	return (0);
}

static int
intelgpio_pin_setflags(device_t dev, uint32_t pin, uint32_t flags)
{
	struct intelgpio_softc *sc;
	struct resource *res;
	bus_size_t pad_off;
	uint32_t val;

	sc = device_get_softc(dev);
	if (intelgpio_gpio_to_pad(sc, pin, &res, &pad_off, NULL) != 0)
		return (EINVAL);
	if (res == NULL)
		return (EINVAL);

	/*
	 * We currently only support input and output, not both simultaneously
	 */
	if (flags & ~(GPIO_PIN_INPUT | GPIO_PIN_OUTPUT))
		return (EINVAL);
	if ((flags & GPIO_PIN_INPUT) && (flags & GPIO_PIN_OUTPUT))
		return (EINVAL);

	INTELGPIO_LOCK(sc);
	val = bus_read_4(res, pad_off);

	/*
	 * Switch the pad to GPIO mode. Intel pads can operate in different
	 * modes (native function 1-15 or GPIO). Since setflags is only called
	 * when userspace or a consumer explicitly wants to use the pin as a
	 * GPIO, we force the pad into GPIO mode here so the direction bits
	 * (TXDIS/RXDIS) below take effect.
	 */
	val &= ~INTELGPIO_PADCFG0_PMODE_MASK;
	val |= INTELGPIO_PADCFG0_PMODE_GPIO;

	if (flags & GPIO_PIN_INPUT)
		val &= ~INTELGPIO_PADCFG0_GPIORXDIS;
	else
		val |= INTELGPIO_PADCFG0_GPIORXDIS;

	if (flags & GPIO_PIN_OUTPUT)
		val &= ~INTELGPIO_PADCFG0_GPIOTXDIS;
	else
		val |= INTELGPIO_PADCFG0_GPIOTXDIS;

	bus_write_4(res, pad_off, val);
	INTELGPIO_UNLOCK(sc);

	return (0);
}

static int
intelgpio_pin_get(device_t dev, uint32_t pin, unsigned int *value)
{
	struct intelgpio_softc *sc;
	struct resource *res;
	bus_size_t pad_off;
	uint32_t val;

	sc = device_get_softc(dev);
	if (intelgpio_gpio_to_pad(sc, pin, &res, &pad_off, NULL) != 0)
		return (EINVAL);
	if (res == NULL)
		return (EINVAL);

	INTELGPIO_LOCK(sc);
	val = bus_read_4(res, pad_off);
	INTELGPIO_UNLOCK(sc);

	*value = (val & INTELGPIO_PADCFG0_GPIORXSTATE) ?
	    GPIO_PIN_HIGH : GPIO_PIN_LOW;

	return (0);
}

static int
intelgpio_pin_set(device_t dev, uint32_t pin, unsigned int value)
{
	struct intelgpio_softc *sc;
	struct resource *res;
	bus_size_t pad_off;
	uint32_t val;

	sc = device_get_softc(dev);
	if (intelgpio_gpio_to_pad(sc, pin, &res, &pad_off, NULL) != 0)
		return (EINVAL);
	if (res == NULL)
		return (EINVAL);

	INTELGPIO_LOCK(sc);
	val = bus_read_4(res, pad_off);
	if (value == GPIO_PIN_LOW)
		val &= ~INTELGPIO_PADCFG0_GPIOTXSTATE;
	else
		val |= INTELGPIO_PADCFG0_GPIOTXSTATE;
	bus_write_4(res, pad_off, val);
	INTELGPIO_UNLOCK(sc);

	return (0);
}

static int
intelgpio_pin_toggle(device_t dev, uint32_t pin)
{
	struct intelgpio_softc *sc;
	struct resource *res;
	bus_size_t pad_off;
	uint32_t val;

	sc = device_get_softc(dev);
	if (intelgpio_gpio_to_pad(sc, pin, &res, &pad_off, NULL) != 0)
		return (EINVAL);
	if (res == NULL)
		return (EINVAL);

	INTELGPIO_LOCK(sc);
	val = bus_read_4(res, pad_off);
	val ^= INTELGPIO_PADCFG0_GPIOTXSTATE;
	bus_write_4(res, pad_off, val);
	INTELGPIO_UNLOCK(sc);

	return (0);
}

/*
 * Pin interrupts.
 *
 * x86 has no INTRNG, so a pin cannot be handed out as an interrupt of the
 * platform interrupt controller.  The controller acts as the bus for its
 * pin interrupts instead: the bus of a device which has a GpioInt()
 * resource (acpi_iicbus(4) for now) forwards the SYS_RES_IRQ allocation and
 * the interrupt setup of its child to the controller, using the pin number
 * as the start of the resource:
 *
 *	BUS_ALLOC_RESOURCE(gpio, child, SYS_RES_IRQ, &rid, pin, pin, 1, flags)
 *	BUS_SETUP_INTR(gpio, child, res, flags, filter, handler, arg, &cookie)
 *	BUS_TEARDOWN_INTR(gpio, child, res, cookie)
 *	BUS_RELEASE_RESOURCE(gpio, child, SYS_RES_IRQ, rid, res)
 *
 * All pins share the interrupt of the controller.  Its ithread calls the
 * handlers of the pending pins one by one, so they run in the same context
 * an interrupt handler of their own would have provided.  A filter is called
 * from the ithread too.  Trigger and polarity of a pin are taken from the
 * GpioInt() resource of the child.
 */

static int
intelgpio_pin_lookup(struct intelgpio_softc *sc, rman_res_t pin, int *comp,
    int *bankp, int *bitp)
{
	const struct intelgpio_platform *plat = sc->sc_plat;
	const struct intelgpio_padgroup *pg;
	int i, j;

	for (i = 0; i < plat->ncommunities; i++) {
		for (j = 0; j < plat->communities[i].ngroups; j++) {
			pg = &plat->communities[i].groups[j];
			if (pg->gpio_base == INTELGPIO_GPIO_NOMAP)
				continue;
			if (pin >= pg->gpio_base &&
			    pin < pg->gpio_base + pg->npads) {
				*comp = i;
				*bankp = j;
				*bitp = pin - pg->gpio_base;
				return (0);
			}
		}
	}
	return (-1);
}

static void
intelgpio_intr_pin(struct intelgpio_softc *sc, struct resource *res, int bank,
    int bit, int pin)
{
	const struct intelgpio_platform *plat = sc->sc_plat;
	struct intelgpio_pin_intr *pi;
	driver_filter_t *filter;
	driver_intr_t *handler;
	void *arg;
	uint32_t mask, val;
	bool level;
	int rv;

	pi = &sc->sc_pin_intr[pin];
	mask = 1U << bit;

	mtx_lock(&sc->sc_intr_mtx);
	if (!pi->established) {
		/* Nobody listens to this pin, so keep it quiet. */
		INTELGPIO_LOCK(sc);
		val = bus_read_4(res, plat->gpi_ie + bank * 4);
		bus_write_4(res, plat->gpi_ie + bank * 4, val & ~mask);
		bus_write_4(res, plat->gpi_is + bank * 4, mask);
		INTELGPIO_UNLOCK(sc);
		mtx_unlock(&sc->sc_intr_mtx);
		return;
	}
	filter = pi->filter;
	handler = pi->handler;
	arg = pi->arg;
	level = pi->trig == INTR_TRIGGER_LEVEL;
	pi->running = true;
	mtx_unlock(&sc->sc_intr_mtx);

	/*
	 * The status bit is latched.  An edge has to be acknowledged before
	 * the handler runs, or an edge arriving in the meantime is lost.  A
	 * level is acknowledged afterwards, as the status bit is set again
	 * for as long as the device keeps the line asserted.
	 */
	if (!level)
		bus_write_4(res, plat->gpi_is + bank * 4, mask);
	rv = FILTER_SCHEDULE_THREAD;
	if (filter != NULL)
		rv = filter(arg);
	if (handler != NULL && (rv & FILTER_SCHEDULE_THREAD) != 0)
		handler(arg);
	if (level)
		bus_write_4(res, plat->gpi_is + bank * 4, mask);

	mtx_lock(&sc->sc_intr_mtx);
	pi->running = false;
	wakeup(pi);
	mtx_unlock(&sc->sc_intr_mtx);
}

static void
intelgpio_intr(void *arg)
{
	struct intelgpio_softc *sc = arg;
	const struct intelgpio_platform *plat = sc->sc_plat;
	const struct intelgpio_padgroup *pg;
	struct resource *res;
	uint32_t pending;
	int bit, i, j;

	for (i = 0; i < plat->ncommunities; i++) {
		res = sc->sc_mem_res[i];
		if (res == NULL)
			continue;
		for (j = 0; j < plat->communities[i].ngroups; j++) {
			pg = &plat->communities[i].groups[j];
			if (pg->gpio_base == INTELGPIO_GPIO_NOMAP)
				continue;
			INTELGPIO_LOCK(sc);
			pending = bus_read_4(res, plat->gpi_is + j * 4) &
			    bus_read_4(res, plat->gpi_ie + j * 4);
			INTELGPIO_UNLOCK(sc);
			while (pending != 0) {
				bit = ffs(pending) - 1;
				pending &= ~(1U << bit);
				if (bit >= pg->npads)
					continue;
				intelgpio_intr_pin(sc, res, j, bit,
				    pg->gpio_base + bit);
			}
		}
	}
}

static struct resource *
intelgpio_alloc_resource(device_t dev, device_t child, int type, int *rid,
    rman_res_t start, rman_res_t end, rman_res_t count, u_int flags)
{
	struct intelgpio_softc *sc;
	struct resource *r;
	int bank, bit, com;

	sc = device_get_softc(dev);
	if (type != SYS_RES_IRQ || !sc->sc_pin_rman_valid)
		return (NULL);
	if (start != end || count != 1 ||
	    intelgpio_pin_lookup(sc, start, &com, &bank, &bit) != 0 ||
	    sc->sc_mem_res[com] == NULL)
		return (NULL);

	r = rman_reserve_resource(&sc->sc_pin_rman, start, end, count,
	    flags & ~RF_ACTIVE, child);
	if (r == NULL)
		return (NULL);
	rman_set_rid(r, *rid);
	if ((flags & RF_ACTIVE) != 0 && rman_activate_resource(r) != 0) {
		rman_release_resource(r);
		return (NULL);
	}
	return (r);
}

static int
intelgpio_release_resource(device_t dev, device_t child, int type, int rid,
    struct resource *r)
{
	struct intelgpio_softc *sc;
	bool busy;

	sc = device_get_softc(dev);
	if (type != SYS_RES_IRQ || !sc->sc_pin_rman_valid ||
	    !rman_is_region_manager(r, &sc->sc_pin_rman))
		return (EINVAL);

	mtx_lock(&sc->sc_intr_mtx);
	busy = sc->sc_pin_intr[rman_get_start(r)].established;
	mtx_unlock(&sc->sc_intr_mtx);
	if (busy)
		return (EBUSY);

	if ((rman_get_flags(r) & RF_ACTIVE) != 0)
		rman_deactivate_resource(r);
	return (rman_release_resource(r));
}

struct intelgpio_gpioint {
	ACPI_HANDLE handle;
	u_int pin;
	bool found;
	UINT8 triggering;
	UINT8 polarity;
};

static ACPI_STATUS
intelgpio_gpioint_cb(ACPI_RESOURCE *res, void *context)
{
	struct intelgpio_gpioint *gi = context;
	ACPI_RESOURCE_GPIO *gpio;
	ACPI_HANDLE handle;

	if (res->Type != ACPI_RESOURCE_TYPE_GPIO)
		return (AE_OK);
	gpio = &res->Data.Gpio;
	if (gpio->ConnectionType != ACPI_RESOURCE_GPIO_TYPE_INT ||
	    gpio->PinTableLength == 0 || gpio->PinTable[0] != gi->pin ||
	    gpio->ResourceSource.StringLength == 0 ||
	    gpio->ResourceSource.StringPtr == NULL ||
	    ACPI_FAILURE(AcpiGetHandle(ACPI_ROOT_OBJECT,
	    gpio->ResourceSource.StringPtr, &handle)) || handle != gi->handle)
		return (AE_OK);

	gi->found = true;
	gi->triggering = gpio->Triggering;
	gi->polarity = gpio->Polarity;
	return (AE_CTRL_TERMINATE);
}

/*
 * Find out how the device wants its pin interrupt to trigger.  This is
 * taken from the GpioInt() resource of the device which refers to the pin.
 * bus_config_intr() is of no use here: acpi(4) calls that method of ours to
 * configure the interrupt of the controller itself.
 */
static int
intelgpio_gpioint_config(device_t dev, device_t child, int pin,
    enum intr_trigger *trig, enum intr_polarity *pol)
{
	struct intelgpio_gpioint gi;
	ACPI_HANDLE handle;

	*trig = INTR_TRIGGER_CONFORM;
	*pol = INTR_POLARITY_CONFORM;

	handle = acpi_get_handle(child);
	if (handle == NULL)
		return (0);
	memset(&gi, 0, sizeof(gi));
	gi.handle = acpi_get_handle(dev);
	gi.pin = pin;
	(void)AcpiWalkResources(handle, "_CRS", intelgpio_gpioint_cb, &gi);
	if (!gi.found)
		return (0);
	if (gi.polarity == ACPI_ACTIVE_BOTH)
		return (ENXIO);

	*trig = gi.triggering == ACPI_EDGE_SENSITIVE ?
	    INTR_TRIGGER_EDGE : INTR_TRIGGER_LEVEL;
	*pol = gi.polarity == ACPI_ACTIVE_LOW ?
	    INTR_POLARITY_LOW : INTR_POLARITY_HIGH;
	return (0);
}

/*
 * Program the trigger of an established pin interrupt and enable it.
 * Called with both locks held.
 */
static void
intelgpio_intr_enable(struct intelgpio_softc *sc, struct resource *res,
    bus_size_t pad_off, int bank, int bit, struct intelgpio_pin_intr *pi)
{
	const struct intelgpio_platform *plat = sc->sc_plat;
	uint32_t val;

	val = bus_read_4(res, pad_off);
	val &= ~(INTELGPIO_PADCFG0_RXEVCFG_MASK | INTELGPIO_PADCFG0_RXINV);
	if (pi->trig == INTR_TRIGGER_EDGE)
		val |= INTELGPIO_PADCFG0_RXEVCFG_EDGE;
	else
		val |= INTELGPIO_PADCFG0_RXEVCFG_LEVEL;
	if (pi->pol == INTR_POLARITY_LOW)
		val |= INTELGPIO_PADCFG0_RXINV;
	/*
	 * Firmware may route the pad to an IOAPIC pin as well.  Nothing
	 * handles the pad there, so it would only fire the interrupt handlers
	 * of whatever device shares that IOAPIC pin until we get to service
	 * the pad.
	 */
	val &= ~INTELGPIO_PADCFG0_GPIROUTIOXAPIC;
	bus_write_4(res, pad_off, val);

	/* Drop whatever got latched before. */
	bus_write_4(res, plat->gpi_is + bank * 4, 1U << bit);
	val = bus_read_4(res, plat->gpi_ie + bank * 4);
	bus_write_4(res, plat->gpi_ie + bank * 4, val | (1U << bit));
}

static int
intelgpio_setup_intr(device_t dev, device_t child, struct resource *r,
    int flags, driver_filter_t *filter, driver_intr_t *handler, void *arg,
    void **cookiep)
{
	struct intelgpio_softc *sc;
	const struct intelgpio_platform *plat;
	struct intelgpio_pin_intr *pi;
	struct resource *res;
	bus_size_t pad_off;
	enum intr_trigger trig;
	enum intr_polarity pol;
	uint32_t val;
	int bank, bit, com, error, pin;

	sc = device_get_softc(dev);
	plat = sc->sc_plat;
	if (!sc->sc_pin_rman_valid ||
	    !rman_is_region_manager(r, &sc->sc_pin_rman))
		return (EINVAL);
	if (filter == NULL && handler == NULL)
		return (EINVAL);
	pin = rman_get_start(r);
	if (intelgpio_pin_lookup(sc, pin, &com, &bank, &bit) != 0 ||
	    intelgpio_gpio_to_pad(sc, pin, &res, &pad_off, NULL) != 0 ||
	    res == NULL)
		return (EINVAL);
	error = intelgpio_gpioint_config(dev, child, pin, &trig, &pol);
	if (error != 0)
		return (error);

	pi = &sc->sc_pin_intr[pin];
	mtx_lock(&sc->sc_intr_mtx);
	if (pi->established) {
		mtx_unlock(&sc->sc_intr_mtx);
		return (EBUSY);
	}

	INTELGPIO_LOCK(sc);
	val = bus_read_4(res, pad_off);
	if ((bus_read_4(res, plat->hostsw_own + bank * 4) & (1U << bit)) == 0) {
		/*
		 * The pad is in ACPI mode: it raises a GPE and never sets
		 * its interrupt status bit.
		 */
		error = ENXIO;
	} else if ((val & INTELGPIO_PADCFG0_PMODE_MASK) !=
	    INTELGPIO_PADCFG0_PMODE_GPIO ||
	    (val & INTELGPIO_PADCFG0_GPIORXDIS) != 0) {
		/* Not an input pin. */
		error = ENXIO;
	} else {
		pi->saved_padcfg0 = val;
		pi->trig = trig;
		pi->pol = pol;
		if (pi->trig == INTR_TRIGGER_CONFORM) {
			/* Not described, stick to the firmware setup. */
			pi->trig = (val & INTELGPIO_PADCFG0_RXEVCFG_MASK) ==
			    INTELGPIO_PADCFG0_RXEVCFG_LEVEL ?
			    INTR_TRIGGER_LEVEL : INTR_TRIGGER_EDGE;
			pi->pol = (val & INTELGPIO_PADCFG0_RXINV) != 0 ?
			    INTR_POLARITY_LOW : INTR_POLARITY_HIGH;
		}
		pi->filter = filter;
		pi->handler = handler;
		pi->arg = arg;
		pi->established = true;
		sc->sc_nintrs++;
		intelgpio_intr_enable(sc, res, pad_off, bank, bit, pi);
	}
	INTELGPIO_UNLOCK(sc);
	mtx_unlock(&sc->sc_intr_mtx);

	if (error == 0)
		*cookiep = pi;
	else
		device_printf(dev, "pin %d cannot be used as an interrupt\n",
		    pin);
	return (error);
}

static int
intelgpio_teardown_intr(device_t dev, device_t child, struct resource *r,
    void *cookie)
{
	const uint32_t restore = INTELGPIO_PADCFG0_RXEVCFG_MASK |
	    INTELGPIO_PADCFG0_RXINV | INTELGPIO_PADCFG0_GPIROUTIOXAPIC;
	struct intelgpio_softc *sc;
	const struct intelgpio_platform *plat;
	struct intelgpio_pin_intr *pi;
	struct resource *res;
	bus_size_t pad_off;
	uint32_t val;
	int bank, bit, com, pin;

	sc = device_get_softc(dev);
	plat = sc->sc_plat;
	if (!sc->sc_pin_rman_valid ||
	    !rman_is_region_manager(r, &sc->sc_pin_rman))
		return (EINVAL);
	pin = rman_get_start(r);
	pi = &sc->sc_pin_intr[pin];
	if (cookie != pi ||
	    intelgpio_pin_lookup(sc, pin, &com, &bank, &bit) != 0 ||
	    intelgpio_gpio_to_pad(sc, pin, &res, &pad_off, NULL) != 0 ||
	    res == NULL)
		return (EINVAL);

	mtx_lock(&sc->sc_intr_mtx);
	if (!pi->established) {
		mtx_unlock(&sc->sc_intr_mtx);
		return (EINVAL);
	}

	INTELGPIO_LOCK(sc);
	val = bus_read_4(res, plat->gpi_ie + bank * 4);
	bus_write_4(res, plat->gpi_ie + bank * 4, val & ~(1U << bit));
	INTELGPIO_UNLOCK(sc);

	/* Let a handler which is running right now finish. */
	pi->established = false;
	while (pi->running)
		msleep(pi, &sc->sc_intr_mtx, 0, "igpint", 0);

	/* Hand the pad back the way firmware configured it. */
	INTELGPIO_LOCK(sc);
	val = bus_read_4(res, pad_off);
	val &= ~restore;
	val |= pi->saved_padcfg0 & restore;
	bus_write_4(res, pad_off, val);
	bus_write_4(res, plat->gpi_is + bank * 4, 1U << bit);
	INTELGPIO_UNLOCK(sc);

	pi->filter = NULL;
	pi->handler = NULL;
	pi->arg = NULL;
	pi->trig = INTR_TRIGGER_CONFORM;
	pi->pol = INTR_POLARITY_CONFORM;
	sc->sc_nintrs--;
	mtx_unlock(&sc->sc_intr_mtx);

	return (0);
}

/*
 * Pad configuration and interrupt enables do not survive a suspend.
 */
static int
intelgpio_resume(device_t dev)
{
	struct intelgpio_softc *sc;
	struct intelgpio_pin_intr *pi;
	struct resource *res;
	bus_size_t pad_off;
	int bank, bit, com, pin;

	sc = device_get_softc(dev);
	if (!sc->sc_pin_rman_valid)
		return (0);

	mtx_lock(&sc->sc_intr_mtx);
	for (pin = 0; pin < sc->sc_npins; pin++) {
		pi = &sc->sc_pin_intr[pin];
		if (!pi->established ||
		    intelgpio_pin_lookup(sc, pin, &com, &bank, &bit) != 0 ||
		    intelgpio_gpio_to_pad(sc, pin, &res, &pad_off, NULL) != 0 ||
		    res == NULL)
			continue;
		INTELGPIO_LOCK(sc);
		intelgpio_intr_enable(sc, res, pad_off, bank, bit, pi);
		INTELGPIO_UNLOCK(sc);
	}
	mtx_unlock(&sc->sc_intr_mtx);

	return (0);
}

static void
intelgpio_intr_detach(struct intelgpio_softc *sc)
{

	if (sc->sc_irq_cookie != NULL)
		bus_teardown_intr(sc->sc_dev, sc->sc_irq_res,
		    sc->sc_irq_cookie);
	sc->sc_irq_cookie = NULL;
	if (sc->sc_irq_res != NULL)
		bus_release_resource(sc->sc_dev, SYS_RES_IRQ, sc->sc_irq_rid,
		    sc->sc_irq_res);
	sc->sc_irq_res = NULL;
	if (sc->sc_pin_intr != NULL) {
		free(sc->sc_pin_intr, M_DEVBUF);
		sc->sc_pin_intr = NULL;
		mtx_destroy(&sc->sc_intr_mtx);
	}
}

/*
 * Pin interrupts are optional: the controller still works as a plain GPIO
 * provider if they cannot be set up.
 */
static void
intelgpio_intr_attach(struct intelgpio_softc *sc)
{
	const struct intelgpio_platform *plat = sc->sc_plat;
	const struct intelgpio_padgroup *pg;
	int error, i, j, maxpin;

	if (plat->gpi_ie == 0 || intelgpio_pin_max(sc->sc_dev, &maxpin) != 0)
		return;

	sc->sc_npins = maxpin + 1;
	sc->sc_pin_intr = malloc(sc->sc_npins * sizeof(*sc->sc_pin_intr),
	    M_DEVBUF, M_WAITOK | M_ZERO);
	mtx_init(&sc->sc_intr_mtx, device_get_nameunit(sc->sc_dev),
	    "intelgpio intr", MTX_DEF);

	sc->sc_irq_rid = 0;
	sc->sc_irq_res = bus_alloc_resource_any(sc->sc_dev, SYS_RES_IRQ,
	    &sc->sc_irq_rid, RF_ACTIVE | RF_SHAREABLE);
	if (sc->sc_irq_res == NULL) {
		device_printf(sc->sc_dev,
		    "no interrupt, pin interrupts are not available\n");
		intelgpio_intr_detach(sc);
		return;
	}

	/* No pin has a handler yet. */
	for (i = 0; i < plat->ncommunities; i++) {
		if (sc->sc_mem_res[i] == NULL)
			continue;
		for (j = 0; j < plat->communities[i].ngroups; j++) {
			pg = &plat->communities[i].groups[j];
			if (pg->gpio_base == INTELGPIO_GPIO_NOMAP)
				continue;
			bus_write_4(sc->sc_mem_res[i], plat->gpi_ie + j * 4, 0);
		}
	}

	sc->sc_pin_rman.rm_type = RMAN_ARRAY;
	sc->sc_pin_rman.rm_descr = "Intel GPIO pin interrupts";
	sc->sc_pin_rman.rm_start = 0;
	sc->sc_pin_rman.rm_end = maxpin;
	error = rman_init(&sc->sc_pin_rman);
	if (error == 0) {
		error = rman_manage_region(&sc->sc_pin_rman, 0, maxpin);
		if (error != 0)
			rman_fini(&sc->sc_pin_rman);
	}
	if (error == 0) {
		error = bus_setup_intr(sc->sc_dev, sc->sc_irq_res,
		    INTR_TYPE_MISC | INTR_MPSAFE, NULL, intelgpio_intr, sc,
		    &sc->sc_irq_cookie);
		if (error != 0)
			rman_fini(&sc->sc_pin_rman);
	}
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "cannot set up interrupt (%d), "
		    "pin interrupts are not available\n", error);
		intelgpio_intr_detach(sc);
		return;
	}
	sc->sc_pin_rman_valid = true;
}

int
intelgpio_probe(device_t dev, const struct intelgpio_platform *plat)
{
	int rv;

	if (acpi_disabled("intelgpio"))
		return (ENXIO);

	rv = ACPI_ID_PROBE(device_get_parent(dev), dev, plat->hids, NULL);
	if (rv <= 0)
		device_set_desc(dev, plat->desc);
	return (rv);
}

static int
intelgpio_detach(device_t dev)
{
	struct intelgpio_softc *sc;
	int error, i;

	sc = device_get_softc(dev);

	if (sc->sc_pin_rman_valid) {
		/* Pin interrupts are still in use by other devices. */
		if (sc->sc_nintrs != 0)
			return (EBUSY);
		error = rman_fini(&sc->sc_pin_rman);
		if (error != 0)
			return (error);
		sc->sc_pin_rman_valid = false;
	}
	intelgpio_intr_detach(sc);

	if (sc->sc_busdev != NULL)
		gpiobus_detach_bus(dev);

	for (i = 0; i < sc->sc_plat->ncommunities; i++) {
		if (sc->sc_mem_res[i] != NULL)
			bus_release_resource(dev, SYS_RES_MEMORY, i,
			    sc->sc_mem_res[i]);
	}

	INTELGPIO_LOCK_DESTROY(sc);
	return (0);
}

int
intelgpio_attach(device_t dev, const struct intelgpio_platform *plat)
{
	struct intelgpio_softc *sc;
	bool have_res;
	int i;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;
	sc->sc_plat = plat;

	KASSERT(plat->ncommunities <= INTELGPIO_MAX_COMMUNITIES,
	    ("too many communities: %d > %d", plat->ncommunities,
		INTELGPIO_MAX_COMMUNITIES));

	INTELGPIO_LOCK_INIT(sc);

	have_res = false;
	for (i = 0; i < plat->ncommunities; i++) {
		sc->sc_mem_res[i] = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
		    &i, RF_ACTIVE);
		if (sc->sc_mem_res[i] == NULL) {
			device_printf(dev,
			    "failed to allocate memory resource "
			    "for community %d\n",
			    i);
			continue;
		}
		sc->sc_padbar[i] = bus_read_4(sc->sc_mem_res[i],
		    INTELGPIO_PADBAR_REG);
		have_res = true;
	}

	if (!have_res) {
		device_printf(dev, "no memory resources found\n");
		INTELGPIO_LOCK_DESTROY(sc);
		return (ENXIO);
	}

	intelgpio_intr_attach(sc);

	sc->sc_busdev = gpiobus_attach_bus(dev);
	if (sc->sc_busdev == NULL) {
		device_printf(dev, "failed to add gpiobus\n");
		intelgpio_detach(dev);
		return (ENXIO);
	}

	return (0);
}


static device_method_t intelgpio_methods[] = {
	DEVMETHOD(device_detach, intelgpio_detach),
	DEVMETHOD(device_resume, intelgpio_resume),

	DEVMETHOD(bus_alloc_resource, intelgpio_alloc_resource),
	DEVMETHOD(bus_release_resource, intelgpio_release_resource),
	DEVMETHOD(bus_setup_intr, intelgpio_setup_intr),
	DEVMETHOD(bus_teardown_intr, intelgpio_teardown_intr),

	DEVMETHOD(gpio_get_bus, intelgpio_get_bus),
	DEVMETHOD(gpio_pin_max, intelgpio_pin_max),
	DEVMETHOD(gpio_pin_getname, intelgpio_pin_getname),
	DEVMETHOD(gpio_pin_getcaps, intelgpio_pin_getcaps),
	DEVMETHOD(gpio_pin_getflags, intelgpio_pin_getflags),
	DEVMETHOD(gpio_pin_setflags, intelgpio_pin_setflags),
	DEVMETHOD(gpio_pin_get, intelgpio_pin_get),
	DEVMETHOD(gpio_pin_set, intelgpio_pin_set),
	DEVMETHOD(gpio_pin_toggle, intelgpio_pin_toggle),

	DEVMETHOD_END
};

DEFINE_CLASS_0(intelgpio, intelgpio_driver, intelgpio_methods,
    sizeof(struct intelgpio_softc));
