/*-
 * Copyright (c) 1999, 2000 Matthew R. Green
 * Copyright (c) 2001 - 2003 by Thomas Moestl <tmm@FreeBSD.org>
 * Copyright (c) 2005 - 2006 Marius Strobl <marius@FreeBSD.org>
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: NetBSD: psycho.c,v 1.39 2001/10/07 20:30:41 eeh Exp
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/7.0.0/sys/sparc64/pci/psycho.c 174272 2007-12-04 21:40:47Z marius $");

/*
 * Support for `Hummingbird' (UltraSPARC IIe), `Psycho' and `Psycho+'
 * (UltraSPARC II) and `Sabre' (UltraSPARC IIi) UPA to PCI bridges.
 */

#include "opt_ofw_pci.h"
#include "opt_psycho.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/reboot.h>
#include <sys/rman.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_pci.h>
#include <dev/ofw/openfirm.h>

#include <machine/bus.h>
#include <machine/bus_common.h>
#include <machine/bus_private.h>
#include <machine/iommureg.h>
#include <machine/iommuvar.h>
#include <machine/ofw_bus.h>
#include <machine/resource.h>
#include <machine/ver.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <sparc64/pci/ofw_pci.h>
#include <sparc64/pci/psychoreg.h>
#include <sparc64/pci/psychovar.h>

#include "pcib_if.h"

static const struct psycho_desc *psycho_find_desc(const struct psycho_desc *,
    const char *);
static const struct psycho_desc *psycho_get_desc(device_t);
static void psycho_set_intr(struct psycho_softc *, u_int, bus_addr_t,
    driver_filter_t, driver_intr_t);
static int psycho_find_intrmap(struct psycho_softc *, u_int, bus_addr_t *,
    bus_addr_t *, u_long *);
static driver_filter_t psycho_dmasync;
static void psycho_intr_enable(void *);
static void psycho_intr_disable(void *);
static void psycho_intr_eoi(void *);
static bus_space_tag_t psycho_alloc_bus_tag(struct psycho_softc *, int);

/* Interrupt handlers */
static driver_filter_t psycho_ue;
static driver_filter_t psycho_ce;
static driver_filter_t psycho_pci_bus;
static driver_filter_t psycho_powerfail;
static driver_intr_t psycho_overtemp;
#ifdef PSYCHO_MAP_WAKEUP
static driver_filter_t psycho_wakeup;
#endif

/* IOMMU support */
static void psycho_iommu_init(struct psycho_softc *, int, uint32_t);

/*
 * Methods
 */
static device_probe_t psycho_probe;
static device_attach_t psycho_attach;
static bus_read_ivar_t psycho_read_ivar;
static bus_setup_intr_t psycho_setup_intr;
static bus_teardown_intr_t psycho_teardown_intr;
static bus_alloc_resource_t psycho_alloc_resource;
static bus_activate_resource_t psycho_activate_resource;
static bus_deactivate_resource_t psycho_deactivate_resource;
static bus_release_resource_t psycho_release_resource;
static bus_get_dma_tag_t psycho_get_dma_tag;
static pcib_maxslots_t psycho_maxslots;
static pcib_read_config_t psycho_read_config;
static pcib_write_config_t psycho_write_config;
static pcib_route_interrupt_t psycho_route_interrupt;
static ofw_pci_intr_pending_t psycho_intr_pending;
static ofw_bus_get_node_t psycho_get_node;
static ofw_pci_alloc_busno_t psycho_alloc_busno;
static ofw_pci_adjust_busrange_t psycho_adjust_busrange;

static device_method_t psycho_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		psycho_probe),
	DEVMETHOD(device_attach,	psycho_attach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,	psycho_read_ivar),
	DEVMETHOD(bus_setup_intr,	psycho_setup_intr),
	DEVMETHOD(bus_teardown_intr,	psycho_teardown_intr),
	DEVMETHOD(bus_alloc_resource,	psycho_alloc_resource),
	DEVMETHOD(bus_activate_resource,	psycho_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	psycho_deactivate_resource),
	DEVMETHOD(bus_release_resource,	psycho_release_resource),
	DEVMETHOD(bus_get_dma_tag,	psycho_get_dma_tag),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots,	psycho_maxslots),
	DEVMETHOD(pcib_read_config,	psycho_read_config),
	DEVMETHOD(pcib_write_config,	psycho_write_config),
	DEVMETHOD(pcib_route_interrupt,	psycho_route_interrupt),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_node,	psycho_get_node),

	/* ofw_pci interface */
	DEVMETHOD(ofw_pci_intr_pending,	psycho_intr_pending),
	DEVMETHOD(ofw_pci_alloc_busno,	psycho_alloc_busno),
	DEVMETHOD(ofw_pci_adjust_busrange,	psycho_adjust_busrange),

	{ 0, 0 }
};

static devclass_t psycho_devclass;

DEFINE_CLASS_0(pcib, psycho_driver, psycho_methods,
    sizeof(struct psycho_softc));
DRIVER_MODULE(psycho, nexus, psycho_driver, psycho_devclass, 0, 0);

static SLIST_HEAD(, psycho_softc) psycho_softcs =
    SLIST_HEAD_INITIALIZER(psycho_softcs);

static uint8_t psycho_pci_bus_cnt;

static const struct intr_controller psycho_ic = {
	psycho_intr_enable,
	psycho_intr_disable,
	psycho_intr_eoi
};

struct psycho_icarg {
	struct psycho_softc	*pica_sc;
	bus_addr_t		pica_map;
	bus_addr_t		pica_clr;
};

struct psycho_dmasync {
	struct psycho_softc	*pds_sc;
	driver_filter_t		*pds_handler;	/* handler to call */
	void			*pds_arg;	/* argument for the handler */
	void			*pds_cookie;	/* parent bus int. cookie */
	device_t		pds_ppb;	/* farest PCI-PCI bridge */
	uint8_t			pds_bus;	/* bus of farest PCI device */
	uint8_t			pds_slot;	/* slot of farest PCI device */
	uint8_t			pds_func;	/* func. of farest PCI device */
};

#define	PSYCHO_READ8(sc, off) \
	bus_read_8((sc)->sc_mem_res, (off))
#define	PSYCHO_WRITE8(sc, off, v) \
	bus_write_8((sc)->sc_mem_res, (off), (v))
#define	PCICTL_READ8(sc, off) \
	PSYCHO_READ8((sc), (sc)->sc_pcictl + (off))
#define	PCICTL_WRITE8(sc, off, v) \
	PSYCHO_WRITE8((sc), (sc)->sc_pcictl + (off), (v))

/*
 * "Sabre" is the UltraSPARC IIi onboard UPA to PCI bridge.  It manages a
 * single PCI bus and does not have a streaming buffer.  It often has an APB
 * (advanced PCI bridge) connected to it, which was designed specifically for
 * the IIi.  The APB let's the IIi handle two independednt PCI buses, and
 * appears as two "Simba"'s underneath the Sabre.
 *
 * "Hummingbird" is the UltraSPARC IIe onboard UPA to PCI bridge. It's
 * basically the same as Sabre but without an APB underneath it.
 *
 * "Psycho" and "Psycho+" are dual UPA to PCI bridges.  They sit on the UPA bus
 * and manage two PCI buses.  "Psycho" has two 64-bit 33MHz buses, while
 * "Psycho+" controls both a 64-bit 33Mhz and a 64-bit 66Mhz PCI bus.  You
 * will usually find a "Psycho+" since I don't think the original "Psycho"
 * ever shipped, and if it did it would be in the U30.
 *
 * Each "Psycho" PCI bus appears as a separate OFW node, but since they are
 * both part of the same IC, they only have a single register space.  As such,
 * they need to be configured together, even though the autoconfiguration will
 * attach them separately.
 *
 * On UltraIIi machines, "Sabre" itself usually takes pci0, with "Simba" often
 * as pci1 and pci2, although they have been implemented with other PCI bus
 * numbers on some machines.
 *
 * On UltraII machines, there can be any number of "Psycho+" ICs, each
 * providing two PCI buses.
 */

#define	OFW_PCI_TYPE		"pci"

struct psycho_desc {
	const char	*pd_string;
	int		pd_mode;
	const char	*pd_name;
};

static const struct psycho_desc psycho_compats[] = {
	{ "pci108e,8000", PSYCHO_MODE_PSYCHO,	"Psycho compatible" },
	{ "pci108e,a000", PSYCHO_MODE_SABRE,	"Sabre compatible" },
	{ "pci108e,a001", PSYCHO_MODE_SABRE,	"Hummingbird compatible" },
	{ NULL,		  0,			NULL }
};

static const struct psycho_desc psycho_models[] = {
	{ "SUNW,psycho",  PSYCHO_MODE_PSYCHO,	"Psycho" },
	{ "SUNW,sabre",   PSYCHO_MODE_SABRE,	"Sabre" },
	{ NULL,		  0,			NULL }
};

static const struct psycho_desc *
psycho_find_desc(const struct psycho_desc *table, const char *string)
{
	const struct psycho_desc *desc;

	if (string == NULL)
		return (NULL);
	for (desc = table; desc->pd_string != NULL; desc++)
		if (strcmp(desc->pd_string, string) == 0)
			return (desc);
	return (NULL);
}

static const struct psycho_desc *
psycho_get_desc(device_t dev)
{
	const struct psycho_desc *rv;

	rv = psycho_find_desc(psycho_models, ofw_bus_get_model(dev));
	if (rv == NULL)
		rv = psycho_find_desc(psycho_compats, ofw_bus_get_compat(dev));
	return (rv);
}

static int
psycho_probe(device_t dev)
{
	const char *dtype;

	dtype = ofw_bus_get_type(dev);
	if (dtype != NULL && strcmp(dtype, OFW_PCI_TYPE) == 0 &&
	    psycho_get_desc(dev) != NULL) {
		device_set_desc(dev, "U2P UPA-PCI bridge");
		return (0);
	}

	return (ENXIO);
}

static int
psycho_attach(device_t dev)
{
	char name[sizeof("pci108e,1000")];
	struct psycho_icarg *pica;
	struct psycho_softc *asc, *sc, *osc;
	struct ofw_pci_ranges *range;
	const struct psycho_desc *desc;
	bus_addr_t intrclr, intrmap;
	uint64_t csr, dr;
	phandle_t child, node;
	uint32_t dvmabase, psycho_br[2];
	int32_t rev;
	u_int ver;
	int i, n, nrange, rid;

	node = ofw_bus_get_node(dev);
	sc = device_get_softc(dev);
	desc = psycho_get_desc(dev);

	sc->sc_node = node;
	sc->sc_dev = dev;
	sc->sc_mode = desc->pd_mode;

	/*
	 * The Psycho gets three register banks:
	 * (0) per-PBM configuration and status registers
	 * (1) per-PBM PCI configuration space, containing only the
	 *     PBM 256-byte PCI header
	 * (2) the shared Psycho configuration registers
	 */
	if (sc->sc_mode == PSYCHO_MODE_PSYCHO) {
		rid = 2;
		sc->sc_pcictl =
		    bus_get_resource_start(dev, SYS_RES_MEMORY, 0) -
		    bus_get_resource_start(dev, SYS_RES_MEMORY, 2);
		switch (sc->sc_pcictl) {
		case PSR_PCICTL0:
			sc->sc_half = 0;
			break;
		case PSR_PCICTL1:
			sc->sc_half = 1;
			break;
		default:
			panic("%s: bogus PCI control register location",
			    __func__);
		}
	} else {
		rid = 0;
		sc->sc_pcictl = PSR_PCICTL0;
		sc->sc_half = 0;
	}
	sc->sc_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    (sc->sc_mode == PSYCHO_MODE_PSYCHO ? RF_SHAREABLE : 0) |
	    RF_ACTIVE);
	if (sc->sc_mem_res == NULL)
		panic("%s: could not allocate registers", __func__);

	/*
	 * Match other Psycho's that are already configured against
	 * the base physical address. This will be the same for a
	 * pair of devices that share register space.
	 */
	osc = NULL;
	SLIST_FOREACH(asc, &psycho_softcs, sc_link) {
		if (rman_get_start(asc->sc_mem_res) ==
		    rman_get_start(sc->sc_mem_res)) {
			/* Found partner. */
			osc = asc;
			break;
		}
	}
	if (osc == NULL) {
		sc->sc_mtx = malloc(sizeof(*sc->sc_mtx), M_DEVBUF,
		    M_NOWAIT | M_ZERO);
		if (sc->sc_mtx == NULL)
			panic("%s: could not malloc mutex", __func__);
		mtx_init(sc->sc_mtx, "pcib_mtx", NULL, MTX_SPIN);
	} else {
		if (mtx_initialized(osc->sc_mtx) == 0)
			panic("%s: mutex not initialized", __func__);
		sc->sc_mtx = osc->sc_mtx;
	}

	/* Clear PCI AFSR. */
	PCICTL_WRITE8(sc, PCR_AFS, PCIAFSR_ERRMASK);

	csr = PSYCHO_READ8(sc, PSR_CS);
	ver = PSYCHO_GCSR_VERS(csr);
	sc->sc_ign = 0x1f; /* Hummingbird/Sabre IGN is always 0x1f. */
	if (sc->sc_mode == PSYCHO_MODE_PSYCHO)
		sc->sc_ign = PSYCHO_GCSR_IGN(csr);

	device_printf(dev, "%s, impl %d, version %d, IGN %#x, bus %c\n",
	    desc->pd_name, (u_int)PSYCHO_GCSR_IMPL(csr), ver, sc->sc_ign,
	    'A' + sc->sc_half);

	/* Set up the PCI control and PCI diagnostic registers. */

	/*
	 * Revision 0 EBus bridges have a bug which prevents them from
	 * working when bus parking is enabled.
	 */
	rev = -1;
	csr = PCICTL_READ8(sc, PCR_CS);
	csr &= ~PCICTL_ARB_PARK;
	for (child = OF_child(node); child != 0; child = OF_peer(child)) {
		if (OF_getprop(child, "name", name, sizeof(name)) == -1)
			continue;
		if ((strcmp(name, "ebus") == 0 ||
		    strcmp(name, "pci108e,1000") == 0) &&
		    OF_getprop(child, "revision-id", &rev, sizeof(rev)) > 0 &&
		    rev == 0)
			break;
	}
	if (rev != 0 && OF_getproplen(node, "no-bus-parking") < 0)
		csr |= PCICTL_ARB_PARK;

	/* Workarounds for version specific bugs. */
	dr = PCICTL_READ8(sc, PCR_DIAG);
	switch (ver) {
	case 0:
		dr |= DIAG_RTRY_DIS;
		dr &= ~DIAG_DWSYNC_DIS;
		/* XXX need to also disable rerun of the streaming buffers. */
		break;
	case 1:
		csr &= ~PCICTL_ARB_PARK;
		dr |= DIAG_RTRY_DIS | DIAG_DWSYNC_DIS;
		/* XXX need to also disable rerun of the streaming buffers. */
		break;
	default:
		dr |= DIAG_DWSYNC_DIS;
		dr &= ~DIAG_RTRY_DIS;
		break;
	}

	csr |= PCICTL_SERR | PCICTL_ERRINTEN | PCICTL_ARB_4;
	csr &= ~(PCICTL_SBHINTEN | PCICTL_WAKEUPEN);
#ifdef PSYCHO_DEBUG
	device_printf(dev, "PCI CSR 0x%016llx -> 0x%016llx\n",
	    (unsigned long long)PCICTL_READ8(sc, PCR_CS),
	    (unsigned long long)csr);
#endif
	PCICTL_WRITE8(sc, PCR_CS, csr);

	dr &= ~DIAG_ISYNC_DIS;
#ifdef PSYCHO_DEBUG
	device_printf(dev, "PCI DR 0x%016llx -> 0x%016llx\n",
	    (unsigned long long)PCICTL_READ8(sc, PCR_DIAG),
	    (unsigned long long)dr);
#endif
	PCICTL_WRITE8(sc, PCR_DIAG, dr);

	if (sc->sc_mode == PSYCHO_MODE_SABRE) {
		/* Use the PROM preset for now. */
		csr = PCICTL_READ8(sc, PCR_TAS);
		if (csr == 0)
			panic("%s: Hummingbird/Sabre TAS not initialized.",
			    __func__);
		dvmabase = (ffs(csr) - 1) << PCITAS_ADDR_SHIFT;
	} else
		dvmabase = -1;

	/* Initialize memory and I/O rmans. */
	sc->sc_pci_io_rman.rm_type = RMAN_ARRAY;
	sc->sc_pci_io_rman.rm_descr = "Psycho PCI I/O Ports";
	if (rman_init(&sc->sc_pci_io_rman) != 0 ||
	    rman_manage_region(&sc->sc_pci_io_rman, 0, PSYCHO_IO_SIZE) != 0)
		panic("%s: failed to set up I/O rman", __func__);
	sc->sc_pci_mem_rman.rm_type = RMAN_ARRAY;
	sc->sc_pci_mem_rman.rm_descr = "Psycho PCI Memory";
	if (rman_init(&sc->sc_pci_mem_rman) != 0 ||
	    rman_manage_region(&sc->sc_pci_mem_rman, 0, PSYCHO_MEM_SIZE) != 0)
		panic("%s: failed to set up memory rman", __func__);

	nrange = OF_getprop_alloc(node, "ranges", sizeof(*range),
	    (void **)&range);
	/*
	 * Make sure that the expected ranges are present. The OFW_PCI_CS_MEM64
	 * one is not currently used though.
	 */
	if (nrange != PSYCHO_NRANGE)
		panic("%s: unsupported number of ranges", __func__);
	/*
	 * Find the addresses of the various bus spaces.
	 * There should not be multiple ones of one kind.
	 * The physical start addresses of the ranges are the configuration,
	 * memory and I/O handles.
	 */
	for (n = 0; n < PSYCHO_NRANGE; n++) {
		i = OFW_PCI_RANGE_CS(&range[n]);
		if (sc->sc_pci_bh[i] != 0)
			panic("%s: duplicate range for space %d", __func__, i);
		sc->sc_pci_bh[i] = OFW_PCI_RANGE_PHYS(&range[n]);
	}
	free(range, M_OFWPROP);

	/* Register the softc, this is needed for paired Psychos. */
	SLIST_INSERT_HEAD(&psycho_softcs, sc, sc_link);

	/*
	 * If we're a Hummingbird/Sabre or the first of a pair of Psychos
	 * to arrive here, do the interrupt setup and start up the IOMMU.
	 */
	if (osc == NULL) {
		/*
		 * Hunt through all the interrupt mapping regs and register
		 * our interrupt controller for the corresponding interrupt
		 * vectors.
		 */
		for (n = 0; n <= PSYCHO_MAX_INO; n++) {
			if (psycho_find_intrmap(sc, n, &intrmap, &intrclr,
			    NULL) == 0)
				continue;
			pica = malloc(sizeof(*pica), M_DEVBUF, M_NOWAIT);
			if (pica == NULL)
				panic("%s: could not allocate interrupt "
				    "controller argument", __func__);
			pica->pica_sc = sc;
			pica->pica_map = intrmap;
			pica->pica_clr = intrclr;
#ifdef PSYCHO_DEBUG
			/*
			 * Enable all interrupts and clear all interrupt
			 * states. This aids the debugging of interrupt
			 * routing problems.
			 */
			device_printf(dev,
			    "intr map (INO %d, %s) %#lx: %#lx, clr: %#lx\n",
			    n, intrmap <= PSR_PCIB3_INT_MAP ? "PCI" : "OBIO",
			    (u_long)intrmap, (u_long)PSYCHO_READ8(sc, intrmap),
			    (u_long)intrclr);
			PSYCHO_WRITE8(sc, intrmap, INTMAP_VEC(sc->sc_ign, n));
			PSYCHO_WRITE8(sc, intrclr, 0);
			PSYCHO_WRITE8(sc, intrmap,
			    INTMAP_ENABLE(INTMAP_VEC(sc->sc_ign, n),
			    PCPU_GET(mid)));
#endif
			if (intr_controller_register(INTMAP_VEC(sc->sc_ign, n),
			    &psycho_ic, pica) != 0)
				panic("%s: could not register interrupt "
				    "controller for INO %d", __func__, n);
		}

		/*
		 * Establish handlers for interesting interrupts...
		 *
		 * XXX We need to remember these and remove this to support
		 * hotplug on the UPA/FHC bus.
		 *
		 * XXX Not all controllers have these, but installing them
		 * is better than trying to sort through this mess.
		 */
		psycho_set_intr(sc, 1, PSR_UE_INT_MAP, psycho_ue, NULL);
		psycho_set_intr(sc, 2, PSR_CE_INT_MAP, psycho_ce, NULL);
#ifdef DEBUGGER_ON_POWERFAIL
		psycho_set_intr(sc, 3, PSR_POWER_INT_MAP, psycho_powerfail,
		    NULL);
#else
		psycho_set_intr(sc, 3, PSR_POWER_INT_MAP, NULL,
		    (driver_intr_t *)psycho_powerfail);
#endif
		/* Psycho-specific initialization */
		if (sc->sc_mode == PSYCHO_MODE_PSYCHO) {
			/*
			 * Hummingbirds/Sabres do not have the following two
			 * interrupts.
			 */

			/*
			 * The spare hardware interrupt is used for the
			 * over-temperature interrupt.
			 */
			psycho_set_intr(sc, 4, PSR_SPARE_INT_MAP,
			    NULL, psycho_overtemp);
#ifdef PSYCHO_MAP_WAKEUP
			/*
			 * psycho_wakeup() doesn't do anything useful right
			 * now.
			 */
			psycho_set_intr(sc, 5, PSR_PWRMGT_INT_MAP,
			    psycho_wakeup, NULL);
#endif /* PSYCHO_MAP_WAKEUP */

			/* Initialize the counter-timer. */
			sparc64_counter_init(rman_get_bustag(sc->sc_mem_res),
			    rman_get_bushandle(sc->sc_mem_res), PSR_TC0);
		}

		/*
		 * Set up IOMMU and PCI configuration if we're the first
		 * of a pair of Psycho's to arrive here.
		 *
		 * We should calculate a TSB size based on amount of RAM
		 * and number of bus controllers and number and type of
		 * child devices.
		 *
		 * For the moment, 32KB should be more than enough.
		 */
		sc->sc_is = malloc(sizeof(struct iommu_state), M_DEVBUF,
		    M_NOWAIT | M_ZERO);
		if (sc->sc_is == NULL)
			panic("%s: malloc iommu_state failed", __func__);
		if (sc->sc_mode == PSYCHO_MODE_SABRE)
			sc->sc_is->is_pmaxaddr =
			    IOMMU_MAXADDR(SABRE_IOMMU_BITS);
		else
			sc->sc_is->is_pmaxaddr =
			    IOMMU_MAXADDR(PSYCHO_IOMMU_BITS);
		sc->sc_is->is_sb[0] = 0;
		sc->sc_is->is_sb[1] = 0;
		if (OF_getproplen(node, "no-streaming-cache") < 0)
			sc->sc_is->is_sb[0] = sc->sc_pcictl + PCR_STRBUF;
		psycho_iommu_init(sc, 3, dvmabase);
	} else {
		/* Just copy IOMMU state, config tag and address. */
		sc->sc_is = osc->sc_is;
		if (OF_getproplen(node, "no-streaming-cache") < 0)
			sc->sc_is->is_sb[1] = sc->sc_pcictl + PCR_STRBUF;
		iommu_reset(sc->sc_is);
	}

	/*
	 * Register a PCI bus error interrupt handler according to which
	 * half this is. Hummingbird/Sabre don't have a PCI bus B error
	 * interrupt but they are also only used for PCI bus A.
	 */
	psycho_set_intr(sc, 0, sc->sc_half == 0 ? PSR_PCIAERR_INT_MAP :
	    PSR_PCIBERR_INT_MAP, psycho_pci_bus, NULL);

	/* Allocate our tags. */
	sc->sc_pci_memt = psycho_alloc_bus_tag(sc, PCI_MEMORY_BUS_SPACE);
	sc->sc_pci_iot = psycho_alloc_bus_tag(sc, PCI_IO_BUS_SPACE);
	sc->sc_pci_cfgt = psycho_alloc_bus_tag(sc, PCI_CONFIG_BUS_SPACE);
	if (bus_dma_tag_create(bus_get_dma_tag(dev), 8, 0,
	    sc->sc_is->is_pmaxaddr, ~0, NULL, NULL, sc->sc_is->is_pmaxaddr,
	    0xff, 0xffffffff, 0, NULL, NULL, &sc->sc_pci_dmat) != 0)
		panic("%s: bus_dma_tag_create failed", __func__);
	/* Customize the tag. */
	sc->sc_pci_dmat->dt_cookie = sc->sc_is;
	sc->sc_pci_dmat->dt_mt = &iommu_dma_methods;

	/*
	 * Get the bus range from the firmware; it is used solely for obtaining
	 * the inital bus number, and cannot be trusted on all machines.
	 */
	n = OF_getprop(node, "bus-range", (void *)psycho_br, sizeof(psycho_br));
	if (n == -1)
		panic("%s: could not get bus-range", __func__);
	if (n != sizeof(psycho_br))
		panic("%s: broken bus-range (%d)", __func__, n);

	/* Clear PCI status error bits. */
	PCIB_WRITE_CONFIG(dev, psycho_br[0], PCS_DEVICE, PCS_FUNC,
	    PCIR_STATUS, PCIM_STATUS_PERR | PCIM_STATUS_RMABORT |
	    PCIM_STATUS_RTABORT | PCIM_STATUS_STABORT |
	    PCIM_STATUS_PERRREPORT, 2);

	/*
	 * Set the latency timer register as this isn't always done by the
	 * firmware.
	 */
	PCIB_WRITE_CONFIG(dev, psycho_br[0], PCS_DEVICE, PCS_FUNC,
	    PCIR_LATTIMER, 64, 1);

	sc->sc_pci_secbus = sc->sc_pci_subbus = psycho_alloc_busno(dev);
	/*
	 * Program the bus range registers.
	 * NOTE: for the Psycho, the second write changes the bus number the
	 * Psycho itself uses for it's configuration space, so these
	 * writes must be kept in this order!
	 * The Hummingbird/Sabre always uses bus 0, but there only can be one
	 * Hummingbird/Sabre per machine.
	 */
	PCIB_WRITE_CONFIG(dev, psycho_br[0], PCS_DEVICE, PCS_FUNC, PCSR_SUBBUS,
	    sc->sc_pci_subbus, 1);
	PCIB_WRITE_CONFIG(dev, psycho_br[0], PCS_DEVICE, PCS_FUNC, PCSR_SECBUS,
	    sc->sc_pci_secbus, 1);

	for (n = PCIR_VENDOR; n < PCIR_STATUS; n += sizeof(uint16_t))
		le16enc(&sc->sc_pci_hpbcfg[n], bus_space_read_2(
		    sc->sc_pci_cfgt, sc->sc_pci_bh[OFW_PCI_CS_CONFIG],
		    PSYCHO_CONF_OFF(sc->sc_pci_secbus, PCS_DEVICE,
		    PCS_FUNC, n)));
	for (n = PCIR_REVID; n <= PCIR_BIST; n += sizeof(uint8_t))
		sc->sc_pci_hpbcfg[n] = bus_space_read_1(sc->sc_pci_cfgt,
		    sc->sc_pci_bh[OFW_PCI_CS_CONFIG], PSYCHO_CONF_OFF(
		    sc->sc_pci_secbus, PCS_DEVICE, PCS_FUNC, n));

	ofw_bus_setup_iinfo(node, &sc->sc_pci_iinfo, sizeof(ofw_pci_intr_t));
	/*
	 * On E250 the interrupt map entry for the EBus bridge is wrong,
	 * causing incorrect interrupts to be assigned to some devices on
	 * the EBus. Work around it by changing our copy of the interrupt
	 * map mask to perform a full comparison of the INO. That way
	 * the interrupt map entry for the EBus bridge won't match at all
	 * and the INOs specified in the "interrupts" properties of the
	 * EBus devices will be used directly instead.
	 */
	if (strcmp(sparc64_model, "SUNW,Ultra-250") == 0 &&
	    sc->sc_pci_iinfo.opi_imapmsk != NULL)
		*(ofw_pci_intr_t *)(&sc->sc_pci_iinfo.opi_imapmsk[
		    sc->sc_pci_iinfo.opi_addrc]) = INTMAP_INO_MASK;

	device_add_child(dev, "pci", sc->sc_pci_secbus);
	return (bus_generic_attach(dev));
}

static void
psycho_set_intr(struct psycho_softc *sc, u_int index, bus_addr_t intrmap,
    driver_filter_t filt, driver_intr_t intr)
{
	u_long vec;
	int rid;

	rid = index;
	sc->sc_irq_res[index] = bus_alloc_resource_any(sc->sc_dev, SYS_RES_IRQ,
	    &rid, RF_ACTIVE);
	if (sc->sc_irq_res[index] == NULL ||
	    INTIGN(vec = rman_get_start(sc->sc_irq_res[index])) != sc->sc_ign ||
	    INTVEC(PSYCHO_READ8(sc, intrmap)) != vec ||
	    intr_vectors[vec].iv_ic != &psycho_ic ||
	    bus_setup_intr(sc->sc_dev, sc->sc_irq_res[index], INTR_TYPE_MISC,
	    filt, intr, sc, &sc->sc_ihand[index]) != 0)
		panic("%s: failed to set up interrupt %d", __func__, index);
}

static int
psycho_find_intrmap(struct psycho_softc *sc, u_int ino, bus_addr_t *intrmapptr,
    bus_addr_t *intrclrptr, bus_addr_t *intrdiagptr)
{
	bus_addr_t intrclr, intrmap;
	uint64_t diag;
	int found;

	/*
	 * XXX we only compare INOs rather than INRs since the firmware may
	 * not provide the IGN and the IGN is constant for all devices on
	 * that PCI controller.
	 * This could cause problems for the FFB/external interrupt which
	 * has a full vector that can be set arbitrarily.
	 */

	if (ino > PSYCHO_MAX_INO) {
		device_printf(sc->sc_dev, "out of range INO %d requested\n",
		    ino);
		return (0);
	}

	found = 0;
	/* Hunt through OBIO first. */
	diag = PSYCHO_READ8(sc, PSR_OBIO_INT_DIAG);
	for (intrmap = PSR_SCSI_INT_MAP, intrclr = PSR_SCSI_INT_CLR;
	    intrmap <= PSR_PWRMGT_INT_MAP; intrmap += 8, intrclr += 8,
	    diag >>= 2) {
		if (sc->sc_mode == PSYCHO_MODE_SABRE &&
		    (intrmap == PSR_TIMER0_INT_MAP ||
		    intrmap == PSR_TIMER1_INT_MAP ||
		    intrmap == PSR_PCIBERR_INT_MAP ||
		    intrmap == PSR_PWRMGT_INT_MAP))
			continue;
		if (INTINO(PSYCHO_READ8(sc, intrmap)) == ino) {
			diag &= 2;
			found = 1;
			break;
		}
	}

	if (!found) {
		diag = PSYCHO_READ8(sc, PSR_PCI_INT_DIAG);
		/* Now do PCI interrupts. */
		for (intrmap = PSR_PCIA0_INT_MAP, intrclr = PSR_PCIA0_INT_CLR;
		    intrmap <= PSR_PCIB3_INT_MAP; intrmap += 8, intrclr += 32,
		    diag >>= 8) {
			if (sc->sc_mode == PSYCHO_MODE_PSYCHO &&
			    (intrmap == PSR_PCIA2_INT_MAP ||
			    intrmap == PSR_PCIA3_INT_MAP))
				continue;
			if (((PSYCHO_READ8(sc, intrmap) ^ ino) & 0x3c) == 0) {
				intrclr += 8 * (ino & 3);
				diag = (diag >> ((ino & 3) * 2)) & 2;
				found = 1;
				break;
			}
		}
	}
	if (intrmapptr != NULL)
		*intrmapptr = intrmap;
	if (intrclrptr != NULL)
		*intrclrptr = intrclr;
	if (intrdiagptr != NULL)
		*intrdiagptr = diag;
	return (found);
}

/*
 * Interrupt handlers
 */
static int
psycho_ue(void *arg)
{
	struct psycho_softc *sc = arg;
	uint64_t afar, afsr;

	afar = PSYCHO_READ8(sc, PSR_UE_AFA);
	afsr = PSYCHO_READ8(sc, PSR_UE_AFS);
	/*
	 * On the UltraSPARC-IIi/IIe, IOMMU misses/protection faults cause
	 * the AFAR to be set to the physical address of the TTE entry that
	 * was invalid/write protected. Call into the iommu code to have
	 * them decoded to virtual I/O addresses.
	 */
	if ((afsr & UEAFSR_P_DTE) != 0)
		iommu_decode_fault(sc->sc_is, afar);
	panic("%s: uncorrectable DMA error AFAR %#lx AFSR %#lx",
	    device_get_name(sc->sc_dev), (u_long)afar, (u_long)afsr);
	return (FILTER_HANDLED);
}

static int
psycho_ce(void *arg)
{
	struct psycho_softc *sc = arg;
	uint64_t afar, afsr;

	mtx_lock_spin(sc->sc_mtx);
	afar = PSYCHO_READ8(sc, PSR_CE_AFA);
	afsr = PSYCHO_READ8(sc, PSR_CE_AFS);
	device_printf(sc->sc_dev, "correctable DMA error AFAR %#lx "
	    "AFSR %#lx\n", (u_long)afar, (u_long)afsr);
	/* Clear the error bits that we caught. */
	PSYCHO_WRITE8(sc, PSR_CE_AFS, afsr & CEAFSR_ERRMASK);
	mtx_unlock_spin(sc->sc_mtx);
	return (FILTER_HANDLED);
}

static int
psycho_pci_bus(void *arg)
{
	struct psycho_softc *sc = arg;
	uint64_t afar, afsr;

	afar = PCICTL_READ8(sc, PCR_AFA);
	afsr = PCICTL_READ8(sc, PCR_AFS);
	panic("%s: PCI bus %c error AFAR %#lx AFSR %#lx",
	    device_get_name(sc->sc_dev), 'A' + sc->sc_half, (u_long)afar,
	    (u_long)afsr);
	return (FILTER_HANDLED);
}

static int
psycho_powerfail(void *arg)
{
#ifdef DEBUGGER_ON_POWERFAIL
	struct psycho_softc *sc = arg;

	kdb_enter("powerfail");
#else
	static int shutdown;

	/* As the interrupt is cleared we may be called multiple times. */
	if (shutdown != 0)
		return (FILTER_HANDLED);
	shutdown++;
	printf("Power Failure Detected: Shutting down NOW.\n");
	shutdown_nice(0);
#endif
	return (FILTER_HANDLED);
}

static void
psycho_overtemp(void *arg)
{
	static int shutdown;

	/* As the interrupt is cleared we may be called multiple times. */
	if (shutdown != 0)
		return;
	shutdown++;
	printf("DANGER: OVER TEMPERATURE detected.\nShutting down NOW.\n");
	shutdown_nice(RB_POWEROFF);
}

#ifdef PSYCHO_MAP_WAKEUP
static int
psycho_wakeup(void *arg)
{
	struct psycho_softc *sc = arg;

	/* Gee, we don't really have a framework to deal with this properly. */
	device_printf(sc->sc_dev, "power management wakeup\n");
	return (FILTER_HANDLED);
}
#endif /* PSYCHO_MAP_WAKEUP */

static void
psycho_iommu_init(struct psycho_softc *sc, int tsbsize, uint32_t dvmabase)
{
	char *name;
	struct iommu_state *is = sc->sc_is;

	/* Punch in our copies. */
	is->is_bustag = rman_get_bustag(sc->sc_mem_res);
	is->is_bushandle = rman_get_bushandle(sc->sc_mem_res);
	is->is_iommu = PSR_IOMMU;
	is->is_dtag = PSR_IOMMU_TLB_TAG_DIAG;
	is->is_ddram = PSR_IOMMU_TLB_DATA_DIAG;
	is->is_dqueue = PSR_IOMMU_QUEUE_DIAG;
	is->is_dva = PSR_IOMMU_SVADIAG;
	is->is_dtcmp = PSR_IOMMU_TLB_CMP_DIAG;

	/* Give us a nice name... */
	name = malloc(32, M_DEVBUF, M_NOWAIT);
	if (name == NULL)
		panic("%s: could not malloc iommu name", __func__);
	snprintf(name, 32, "%s dvma", device_get_nameunit(sc->sc_dev));

	iommu_init(name, is, tsbsize, dvmabase, 0);
}

static int
psycho_maxslots(device_t dev)
{

	/* XXX: is this correct? */
	return (PCI_SLOTMAX);
}

static uint32_t
psycho_read_config(device_t dev, u_int bus, u_int slot, u_int func, u_int reg,
    int width)
{
	struct psycho_softc *sc;
	bus_space_handle_t bh;
	u_long offset = 0;
	uint8_t byte;
	uint16_t shrt;
	uint32_t r, wrd;
	int i;

	sc = device_get_softc(dev);
	bh = sc->sc_pci_bh[OFW_PCI_CS_CONFIG];

	/*
	 * The Hummingbird and Sabre bridges are picky in that they
	 * only allow their config space to be accessed using the
	 * "native" width of the respective register being accessed
	 * and return semi-random other content of their config space
	 * otherwise. Given that the PCI specs don't say anything
	 * about such a (unusual) limitation and lots of stuff expects
	 * to be able to access the contents of the config space at
	 * any width we allow just that. We do this by using a copy
	 * of the header of the bridge (the rest is all zero anyway)
	 * read during attach (expect for PCIR_STATUS) in order to
	 * simplify things.
	 * The Psycho bridges contain a dupe of their header at 0x80
	 * which we nullify that way also.
	 */
	if (bus == sc->sc_pci_secbus && slot == PCS_DEVICE &&
	    func == PCS_FUNC) {
		if (offset % width != 0)
			return (-1);

		if (reg >= sizeof(sc->sc_pci_hpbcfg))
			return (0);

		if ((reg < PCIR_STATUS && reg + width > PCIR_STATUS) ||
		    reg == PCIR_STATUS || reg == PCIR_STATUS + 1)
			le16enc(&sc->sc_pci_hpbcfg[PCIR_STATUS],
			    bus_space_read_2(sc->sc_pci_cfgt, bh,
			    PSYCHO_CONF_OFF(sc->sc_pci_secbus,
			    PCS_DEVICE, PCS_FUNC, PCIR_STATUS)));

		switch (width) {
		case 1:
			return (sc->sc_pci_hpbcfg[reg]);
		case 2:
			return (le16dec(&sc->sc_pci_hpbcfg[reg]));
		case 4:
			return (le32dec(&sc->sc_pci_hpbcfg[reg]));
		}
	}

	offset = PSYCHO_CONF_OFF(bus, slot, func, reg);
	switch (width) {
	case 1:
		i = bus_space_peek_1(sc->sc_pci_cfgt, bh, offset, &byte);
		r = byte;
		break;
	case 2:
		i = bus_space_peek_2(sc->sc_pci_cfgt, bh, offset, &shrt);
		r = shrt;
		break;
	case 4:
		i = bus_space_peek_4(sc->sc_pci_cfgt, bh, offset, &wrd);
		r = wrd;
		break;
	default:
		panic("%s: bad width", __func__);
	}

	if (i) {
#ifdef PSYCHO_DEBUG
		printf("%s: read data error reading: %d.%d.%d: 0x%x\n",
		    __func__, bus, slot, func, reg);
#endif
		r = -1;
	}
	return (r);
}

static void
psycho_write_config(device_t dev, u_int bus, u_int slot, u_int func, u_int reg,
    uint32_t val, int width)
{
	struct psycho_softc *sc;
	bus_space_handle_t bh;
	u_long offset = 0;

	sc = device_get_softc(dev);
	offset = PSYCHO_CONF_OFF(bus, slot, func, reg);
	bh = sc->sc_pci_bh[OFW_PCI_CS_CONFIG];
	switch (width) {
	case 1:
		bus_space_write_1(sc->sc_pci_cfgt, bh, offset, val);
		break;
	case 2:
		bus_space_write_2(sc->sc_pci_cfgt, bh, offset, val);
		break;
	case 4:
		bus_space_write_4(sc->sc_pci_cfgt, bh, offset, val);
		break;
	default:
		panic("%s: bad width", __func__);
	}
}

static int
psycho_route_interrupt(device_t bridge, device_t dev, int pin)
{
	struct psycho_softc *sc;
	struct ofw_pci_register reg;
	bus_addr_t intrmap;
	ofw_pci_intr_t pintr, mintr;
	uint8_t maskbuf[sizeof(reg) + sizeof(pintr)];

	sc = device_get_softc(bridge);
	pintr = pin;
	if (ofw_bus_lookup_imap(ofw_bus_get_node(dev), &sc->sc_pci_iinfo, &reg,
	    sizeof(reg), &pintr, sizeof(pintr), &mintr, sizeof(mintr), maskbuf))
		return (mintr);
	/*
	 * If this is outside of the range for an intpin, it's likely a full
	 * INO, and no mapping is required at all; this happens on the U30,
	 * where there's no interrupt map at the Psycho node. Fortunately,
	 * there seem to be no INOs in the intpin range on this boxen, so
	 * this easy heuristics will do.
	 */
	if (pin > 4)
		return (pin);
	/*
	 * Guess the INO; we always assume that this is a non-OBIO
	 * device, and that pin is a "real" intpin number. Determine
	 * the mapping register to be used by the slot number.
	 * We only need to do this on E450s, it seems; here, the slot numbers
	 * for bus A are one-based, while those for bus B seemingly have an
	 * offset of 2 (hence the factor of 3 below).
	 */
	intrmap = PSR_PCIA0_INT_MAP +
	    8 * (pci_get_slot(dev) - 1 + 3 * sc->sc_half);
	mintr = INTINO(PSYCHO_READ8(sc, intrmap)) + pin - 1;
	device_printf(bridge, "guessing interrupt %d for device %d.%d pin %d\n",
	    (int)mintr, pci_get_slot(dev), pci_get_function(dev), pin);
	return (mintr);
}

static int
psycho_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	struct psycho_softc *sc;

	sc = device_get_softc(dev);
	switch (which) {
	case PCIB_IVAR_DOMAIN:
		*result = 0;
		return (0);
	case PCIB_IVAR_BUS:
		*result = sc->sc_pci_secbus;
		return (0);
	}
	return (ENOENT);
}

static int
psycho_dmasync(void *arg)
{
	struct psycho_dmasync *pds = arg;

	(void)PCIB_READ_CONFIG(pds->pds_ppb, pds->pds_bus, pds->pds_slot,
	    pds->pds_func, PCIR_VENDOR, 2);
	(void)PSYCHO_READ8(pds->pds_sc, PSR_DMA_WRITE_SYNC);
	return (pds->pds_handler(pds->pds_arg));
}

static void
psycho_intr_enable(void *arg)
{
	struct intr_vector *iv = arg;
	struct psycho_icarg *pica = iv->iv_icarg;

	PSYCHO_WRITE8(pica->pica_sc, pica->pica_map,
	    INTMAP_ENABLE(iv->iv_vec, iv->iv_mid));
}

static void
psycho_intr_disable(void *arg)
{
	struct intr_vector *iv = arg;
	struct psycho_icarg *pica = iv->iv_icarg;

	PSYCHO_WRITE8(pica->pica_sc, pica->pica_map, iv->iv_vec);
}

static void
psycho_intr_eoi(void *arg)
{
	struct intr_vector *iv = arg;
	struct psycho_icarg *pica = iv->iv_icarg;

	PSYCHO_WRITE8(pica->pica_sc, pica->pica_clr, 0);
}

static int
psycho_setup_intr(device_t dev, device_t child, struct resource *ires,
    int flags, driver_filter_t *filt, driver_intr_t *intr, void *arg,
    void **cookiep)
{
	struct {
		int apb:1;
		int ppb:1;
	} found;
	devclass_t pci_devclass;
	device_t cdev, pdev, pcidev;
	struct psycho_softc *sc;
	struct psycho_dmasync *pds;
	u_long vec;
	int error;

	sc = device_get_softc(dev);
	/*
	 * Make sure the vector is fully specified and we registered
	 * our interrupt controller for it.
	 */
	vec = rman_get_start(ires);
	if (INTIGN(vec) != sc->sc_ign ||
	    intr_vectors[vec].iv_ic != &psycho_ic) {
		device_printf(dev, "invalid interrupt vector 0x%lx\n", vec);
		return (EINVAL);
	}

	/*
	 * The Sabre-APB-combination has a bug where it does not drain
	 * DMA write data for devices behind additional PCI-PCI bridges
	 * underneath the APB PCI-PCI bridge. The workaround is to do
	 * a read on the farest PCI-PCI bridge followed by a read of the
	 * PCI DMA write sync register of the Sabre.
	 * XXX installing the wrapper for an affected device and the
	 * actual workaround in psycho_dmasync() should be moved to
	 * psycho(4)-specific bus_dma_tag_create() and bus_dmamap_sync()
	 * methods, respectively, once DMA tag creation is newbus'ified,
	 * so the workaround isn't only applied for interrupt handlers
	 * but also for polling(4) callbacks.
	 */
	if (sc->sc_mode == PSYCHO_MODE_SABRE) {
		pds = malloc(sizeof(*pds), M_DEVBUF, M_NOWAIT | M_ZERO);
		if (pds == NULL)
			return (ENOMEM);
		pcidev = NULL;
		found.apb = found.ppb = 0;
		pci_devclass = devclass_find("pci");
		for (cdev = child; cdev != dev; cdev = pdev) {
			pdev = device_get_parent(cdev);
			if (pcidev == NULL) {
				if (device_get_devclass(pdev) != pci_devclass)
					continue;
				pcidev = cdev;
				continue;
			}
			/*
			 * NB: APB would also match as PCI-PCI bridges.
			 */
			if (pci_get_vendor(cdev) == 0x108e &&
			    pci_get_device(cdev) == 0x5000) {
				found.apb = 1;
				break;
			}
			if (pci_get_class(cdev) == PCIC_BRIDGE &&
			    pci_get_subclass(cdev) == PCIS_BRIDGE_PCI)
				found.ppb = 1;
		}
		if (found.apb && found.ppb && pcidev != NULL) {
			pds->pds_sc = sc;
			pds->pds_arg = arg;
			pds->pds_ppb =
			    device_get_parent(device_get_parent(pcidev));
			pds->pds_bus = pci_get_bus(pcidev);
			pds->pds_slot = pci_get_slot(pcidev);
			pds->pds_func = pci_get_function(pcidev);
			if (bootverbose)
				device_printf(dev, "installed DMA sync "
				    "workaround for device %d.%d on bus %d\n",
				    pds->pds_slot, pds->pds_func,
				    pds->pds_bus);
			if (intr == NULL) {
				pds->pds_handler = filt;
				error = bus_generic_setup_intr(dev, child,
				    ires, flags, psycho_dmasync, intr, pds,
				    cookiep);
			} else {
				pds->pds_handler = (driver_filter_t *)intr;
				error = bus_generic_setup_intr(dev, child,
				    ires, flags, filt,
				    (driver_intr_t *)psycho_dmasync, pds,
				    cookiep);
			}
		} else
			error = bus_generic_setup_intr(dev, child, ires,
			    flags, filt, intr, arg, cookiep);
		if (error != 0) {
			free(pds, M_DEVBUF);
			return (error);
		}
		pds->pds_cookie = *cookiep;
		*cookiep = pds;
		return (error);
	}
	return (bus_generic_setup_intr(dev, child, ires, flags, filt, intr,
	    arg, cookiep));
}

static int
psycho_teardown_intr(device_t dev, device_t child, struct resource *vec,
    void *cookie)
{
	struct psycho_softc *sc;
	struct psycho_dmasync *pds;
	int error;

	sc = device_get_softc(dev);
	if (sc->sc_mode == PSYCHO_MODE_SABRE) {
		pds = cookie;
		error = bus_generic_teardown_intr(dev, child, vec,
		    pds->pds_cookie);
		if (error == 0)
			free(pds, M_DEVBUF);
		return (error);
	}
	return (bus_generic_teardown_intr(dev, child, vec, cookie));
}

static struct resource *
psycho_alloc_resource(device_t bus, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct psycho_softc *sc;
	struct resource *rv;
	struct rman *rm;
	bus_space_tag_t bt;
	bus_space_handle_t bh;
	int needactivate = flags & RF_ACTIVE;

	flags &= ~RF_ACTIVE;

	sc = device_get_softc(bus);
	if (type == SYS_RES_IRQ) {
		/*
		 * XXX: Don't accept blank ranges for now, only single
		 * interrupts. The other case should not happen with the
		 * MI PCI code...
		 * XXX: This may return a resource that is out of the
		 * range that was specified. Is this correct...?
		 */
		if (start != end)
			panic("%s: XXX: interrupt range", __func__);
		start = end = INTMAP_VEC(sc->sc_ign, end);
		return (BUS_ALLOC_RESOURCE(device_get_parent(bus), child, type,
		    rid, start, end, count, flags));
	}
	switch (type) {
	case SYS_RES_MEMORY:
		rm = &sc->sc_pci_mem_rman;
		bt = sc->sc_pci_memt;
		bh = sc->sc_pci_bh[OFW_PCI_CS_MEM32];
		break;
	case SYS_RES_IOPORT:
		rm = &sc->sc_pci_io_rman;
		bt = sc->sc_pci_iot;
		bh = sc->sc_pci_bh[OFW_PCI_CS_IO];
		break;
	default:
		return (NULL);
	}

	rv = rman_reserve_resource(rm, start, end, count, flags, child);
	if (rv == NULL)
		return (NULL);
	rman_set_rid(rv, *rid);
	bh += rman_get_start(rv);
	rman_set_bustag(rv, bt);
	rman_set_bushandle(rv, bh);

	if (needactivate) {
		if (bus_activate_resource(child, type, *rid, rv)) {
			rman_release_resource(rv);
			return (NULL);
		}
	}

	return (rv);
}

static int
psycho_activate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{
	void *p;
	int error;

	if (type == SYS_RES_IRQ)
		return (BUS_ACTIVATE_RESOURCE(device_get_parent(bus), child,
		    type, rid, r));
	if (type == SYS_RES_MEMORY) {
		/*
		 * Need to memory-map the device space, as some drivers depend
		 * on the virtual address being set and useable.
		 */
		error = sparc64_bus_mem_map(rman_get_bustag(r),
		    rman_get_bushandle(r), rman_get_size(r), 0, 0, &p);
		if (error != 0)
			return (error);
		rman_set_virtual(r, p);
	}
	return (rman_activate_resource(r));
}

static int
psycho_deactivate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{

	if (type == SYS_RES_IRQ)
		return (BUS_DEACTIVATE_RESOURCE(device_get_parent(bus), child,
		    type, rid, r));
	if (type == SYS_RES_MEMORY) {
		sparc64_bus_mem_unmap(rman_get_virtual(r), rman_get_size(r));
		rman_set_virtual(r, NULL);
	}
	return (rman_deactivate_resource(r));
}

static int
psycho_release_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{
	int error;

	if (type == SYS_RES_IRQ)
		return (BUS_RELEASE_RESOURCE(device_get_parent(bus), child,
		    type, rid, r));
	if (rman_get_flags(r) & RF_ACTIVE) {
		error = bus_deactivate_resource(child, type, rid, r);
		if (error)
			return (error);
	}
	return (rman_release_resource(r));
}

static bus_dma_tag_t
psycho_get_dma_tag(device_t bus, device_t child)
{
	struct psycho_softc *sc;

	sc = device_get_softc(bus);
	return (sc->sc_pci_dmat);
}

static int
psycho_intr_pending(device_t dev, ofw_pci_intr_t intr)
{
	struct psycho_softc *sc;
	u_long diag;

	sc = device_get_softc(dev);
	if (psycho_find_intrmap(sc, intr, NULL, NULL, &diag) == 0) {
		device_printf(dev, "%s: mapping not found for %d\n", __func__,
		    intr);
		return (0);
	}
	return (diag != 0);
}

static phandle_t
psycho_get_node(device_t bus, device_t dev)
{
	struct psycho_softc *sc;

	sc = device_get_softc(bus);
	/* We only have one child, the PCI bus, which needs our own node. */
	return (sc->sc_node);
}

static int
psycho_alloc_busno(device_t dev)
{

	if (psycho_pci_bus_cnt == PCI_BUSMAX)
		panic("%s: out of PCI bus numbers", __func__);
	return (psycho_pci_bus_cnt++);
}

static void
psycho_adjust_busrange(device_t dev, u_int subbus)
{
	struct psycho_softc *sc;

	sc = device_get_softc(dev);
	/* If necessary, adjust the subordinate bus number register. */
	if (subbus > sc->sc_pci_subbus) {
#ifdef PSYCHO_DEBUG
		device_printf(dev,
		    "adjusting subordinate bus number from %d to %d\n",
		    sc->sc_pci_subbus, subbus);
#endif
		sc->sc_pci_subbus = subbus;
		PCIB_WRITE_CONFIG(dev, sc->sc_pci_secbus, PCS_DEVICE, PCS_FUNC,
		    PCSR_SUBBUS, subbus, 1);
	}
}

static bus_space_tag_t
psycho_alloc_bus_tag(struct psycho_softc *sc, int type)
{
	bus_space_tag_t bt;

	bt = malloc(sizeof(struct bus_space_tag), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (bt == NULL)
		panic("%s: out of memory", __func__);

	bt->bst_cookie = sc;
	bt->bst_parent = rman_get_bustag(sc->sc_mem_res);
	bt->bst_type = type;
	return (bt);
}
