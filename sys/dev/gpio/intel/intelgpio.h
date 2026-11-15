/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Beckhoff Automation GmbH & Co. KG
 */

#ifndef _DEV_GPIO_INTEL_INTELGPIO_H_
#define _DEV_GPIO_INTEL_INTELGPIO_H_

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/gpio.h>
#include <sys/kernel.h>
#include <sys/rman.h>

#include <contrib/dev/acpica/include/acpi.h>

#define INTELGPIO_PADBAR_REG	      0x00c
#define INTELGPIO_PAD_SIZE	      16

#define INTELGPIO_PADCFG0_GPIOTXSTATE (1 << 0)
#define INTELGPIO_PADCFG0_GPIORXSTATE (1 << 1)
#define INTELGPIO_PADCFG0_GPIOTXDIS   (1 << 8)
#define INTELGPIO_PADCFG0_GPIORXDIS   (1 << 9)
#define INTELGPIO_PADCFG0_PMODE_MASK  (0x7 << 10)
#define INTELGPIO_PADCFG0_PMODE_GPIO  0
#define INTELGPIO_PADCFG0_GPIROUTIOXAPIC (1 << 20)
#define INTELGPIO_PADCFG0_RXINV	      (1 << 23)
#define INTELGPIO_PADCFG0_RXEVCFG_MASK (0x3 << 25)
#define INTELGPIO_PADCFG0_RXEVCFG_LEVEL (0x0 << 25)
#define INTELGPIO_PADCFG0_RXEVCFG_EDGE (0x1 << 25)

#define INTELGPIO_GPIO_NOMAP	      (-1)
#define INTELGPIO_MAX_COMMUNITIES     8

struct intelgpio_padgroup {
	int first_pad;
	int npads;
	int gpio_base;
	const char *name;
};

struct intelgpio_community {
	int ngroups;
	const struct intelgpio_padgroup *groups;
};

/*
 * The per-community register offsets below describe the pad group register
 * banks.  Bank N of a community belongs to the Nth entry of its groups
 * array, so that array has to list every pad group of the community in
 * hardware order, unmapped ones included.  A platform that leaves gpi_ie
 * zero does not support pin interrupts.
 */
struct intelgpio_platform {
	const struct intelgpio_community *communities;
	int ncommunities;
	char **hids;
	const char *desc;
	bus_size_t hostsw_own;	/* host software pad ownership */
	bus_size_t gpi_is;	/* GPI interrupt status */
	bus_size_t gpi_ie;	/* GPI interrupt enable */
};

/* Interrupt state of a single pin. */
struct intelgpio_pin_intr {
	driver_filter_t *filter;
	driver_intr_t *handler;
	void *arg;
	enum intr_trigger trig;
	enum intr_polarity pol;
	uint32_t saved_padcfg0;
	bool established;
	bool running;
};

struct intelgpio_softc {
	device_t sc_dev;
	device_t sc_busdev;
	struct mtx sc_mtx;
	const struct intelgpio_platform *sc_plat;

	struct resource *sc_mem_res[INTELGPIO_MAX_COMMUNITIES];
	uint32_t sc_padbar[INTELGPIO_MAX_COMMUNITIES];

	/* Pin interrupts, see intelgpio.c */
	struct mtx sc_intr_mtx;
	struct resource *sc_irq_res;
	int sc_irq_rid;
	void *sc_irq_cookie;
	struct rman sc_pin_rman;
	bool sc_pin_rman_valid;
	int sc_npins;
	int sc_nintrs;
	struct intelgpio_pin_intr *sc_pin_intr;
};

DECLARE_CLASS(intelgpio_driver);

int intelgpio_probe(device_t dev, const struct intelgpio_platform *plat);
int intelgpio_attach(device_t dev, const struct intelgpio_platform *plat);

#endif /* _DEV_GPIO_INTEL_INTELGPIO_H_ */
