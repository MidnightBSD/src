/*-
 * Copyright (c) 2000 Matthew Jacob
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
__FBSDID("$FreeBSD: src/sys/alpha/mcbus/mcpcia.c,v 1.30 2005/01/05 20:05:51 imp Exp $");

#define __RMAN_RESOURCE_VISIBLE
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/bus.h>
#include <sys/cons.h>
#include <machine/bus.h>
#include <machine/md_var.h>
#include <sys/proc.h>
#include <sys/rman.h>
#include <sys/interrupt.h>

#include <machine/swiz.h>
#include <machine/intr.h>
#include <machine/intrcnt.h>
#include <machine/resource.h>
#include <machine/sgmap.h>
#include <machine/prom.h>
#include <vm/vm.h>
#include <vm/vm_page.h>
 

#include <alpha/mcbus/mcbusreg.h>
#include <alpha/mcbus/mcbusvar.h>

#include <alpha/mcbus/mcpciareg.h>
#include <alpha/mcbus/mcpciavar.h>
#include <alpha/pci/pcibus.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include "alphapci_if.h"
#include "pcib_if.h"

static devclass_t	mcpcia_devclass;

/* We're only allowing for one MCBUS right now */
static device_t		mcpcias[MCPCIA_PER_MCBUS];

#define KV(pa)		((void *)ALPHA_PHYS_TO_K0SEG(pa))

struct mcpcia_softc {
	struct mcpcia_softc *next;
	device_t	dev;		/* backpointer */
	u_int64_t	sysbase;	/* shorthand */
	vm_offset_t	dmem_base;	/* dense memory */
	vm_offset_t	smem_base;	/* sparse memory */
	vm_offset_t	io_base;	/* sparse i/o */
	int		mcpcia_inst;	/* our mcpcia instance # */
	struct swiz_space io_space;	/* accessor for ports */
	struct swiz_space mem_space;	/* accessor for memory */
	struct rman	io_rman;	/* resource manager for ports */
	struct rman	mem_rman;	/* resource manager for memory */
};
static struct mcpcia_softc *mcpcia_eisa = NULL;
extern void dec_kn300_cons_init(void);

static driver_intr_t mcpcia_intr;
static void mcpcia_enable_intr(struct mcpcia_softc *, int);
static void mcpcia_disable_intr(struct mcpcia_softc *, int);

/*
 * SGMAP window for ISA: 8M at 8M
 */
#define	MCPCIA_ISA_SG_MAPPED_BASE	(8*1024*1024)
#define	MCPCIA_ISA_SG_MAPPED_SIZE	(8*1024*1024)

/*
 * Direct-mapped window: 2G at 2G
 */
#define	MCPCIA_DIRECT_MAPPED_BASE	(2UL*1024UL*1024UL*1024UL)
#define	MCPCIA_DIRECT_MAPPED_SIZE	(2UL*1024UL*1024UL*1024UL)

/*
 * SGMAP window for PCI: 1G at 1G
 */
#define	MCPCIA_PCI_SG_MAPPED_BASE	(1UL*1024UL*1024UL*1024UL)
#define	MCPCIA_PCI_SG_MAPPED_SIZE	(1UL*1024UL*1024UL*1024UL)

#define	MCPCIA_SGTLB_INVALIDATE(sc)					\
do {									\
	alpha_mb();							\
	REGVAL(MCPCIA_SG_TBIA(sc)) = 0xdeadbeef;			\
	alpha_mb();							\
} while (0)

static void mcpcia_dma_init(struct mcpcia_softc *);
static void mcpcia_sgmap_map(void *, bus_addr_t, vm_offset_t);

#define MCPCIA_SOFTC(dev)	(struct mcpcia_softc *) device_get_softc(dev)

static struct mcpcia_softc *mcpcia_root;

/*
 * Early console support requires us to partially probe the bus to
 * find the ISA bus resources.
 */
void
mcpcia_init(int gid, int mid)
{
	static struct swiz_space io_space;
	static struct swiz_space mem_space;
	u_int64_t sysbase;
	vm_offset_t regs, io_base, smem_base;

	sysbase = MCBUS_IOSPACE |
		(((u_int64_t) gid) << MCBUS_GID_SHIFT) |
		(((u_int64_t) mid) << MCBUS_MID_SHIFT);

	if (EISA_PRESENT(REGVAL(sysbase
				| MCPCIA_PCI_BRIDGE
				| _MCPCIA_PCI_REV))) {
		/*
		 * Define temporary spaces for bootstrap i/o.
		 */
		regs	  = (vm_offset_t) KV(sysbase);
		io_base	  = regs + MCPCIA_PCI_IOSPACE;
		smem_base = regs + MCPCIA_PCI_SPARSE;

		swiz_init_space(&io_space, io_base);
		swiz_init_space(&mem_space, smem_base);

		busspace_isa_io = (struct alpha_busspace *) &io_space;
		busspace_isa_mem = (struct alpha_busspace *) &mem_space;
	}
}

static int
mcpcia_probe(device_t dev)
{
	device_t child;
	int unit;
	struct mcpcia_softc *xc, *sc = MCPCIA_SOFTC(dev);

	unit = device_get_unit(dev);
	if (mcpcias[unit]) {
		printf("%s: already attached\n", device_get_nameunit(dev));
		return EEXIST;
	}
	sc->mcpcia_inst = unit;
	if ((xc = mcpcia_root) == NULL) {
		mcpcia_root = sc;
	} else {
		while (xc->next)
			xc = xc->next;
		xc->next = sc;
	}
	sc->dev = mcpcias[unit] = dev;
	/* PROBE ? */
	device_set_desc(dev, "MCPCIA PCI Adapter");
	if (unit == 0) {
		pci_init_resources();
	}
	child = device_add_child(dev, "pci", -1);
	device_set_ivars(child, &sc->mcpcia_inst);
	return (0);
}

static int
mcpcia_attach(device_t dev)
{
	struct mcpcia_softc *sc = MCPCIA_SOFTC(dev);
	device_t p = device_get_parent(dev);
	vm_offset_t regs;
	u_int32_t ctl;
	int mid, gid, rval;
	void *intr;

	mid = mcbus_get_mid(dev);
	gid = mcbus_get_gid(dev);

	sc->sysbase = MCBUS_IOSPACE |
	    (((u_int64_t) gid) << MCBUS_GID_SHIFT) | \
	    (((u_int64_t) mid) << MCBUS_MID_SHIFT);
	regs		= (vm_offset_t) KV(sc->sysbase);
	sc->dmem_base	= regs + MCPCIA_PCI_DENSE;
	sc->smem_base	= regs + MCPCIA_PCI_SPARSE;
	sc->io_base	= regs + MCPCIA_PCI_IOSPACE;

	swiz_init_space(&sc->io_space, sc->io_base);
	swiz_init_space(&sc->mem_space, sc->smem_base);

	sc->io_rman.rm_start = 0;
	sc->io_rman.rm_end = ~0u;
	sc->io_rman.rm_type = RMAN_ARRAY;
	sc->io_rman.rm_descr = "I/O ports";
	if (rman_init(&sc->io_rman)
	    || rman_manage_region(&sc->io_rman, 0x0, (1L << 32)))
		panic("mcpcia_attach: io_rman");

	sc->mem_rman.rm_start = 0;
	sc->mem_rman.rm_end = ~0u;
	sc->mem_rman.rm_type = RMAN_ARRAY;
	sc->mem_rman.rm_descr = "I/O memory";
	if (rman_init(&sc->mem_rman)
	    || rman_manage_region(&sc->mem_rman, 0x0, (1L << 32)))
		panic("mcpcia_attach: mem_rman");

	/*
 	 * Disable interrupts and clear errors prior to probing
	 */
	REGVAL(MCPCIA_INT_MASK0(sc)) = 0;
	REGVAL(MCPCIA_INT_MASK1(sc)) = 0;
	REGVAL(MCPCIA_CAP_ERR(sc)) = 0xFFFFFFFF;
	alpha_mb();

	/*
	 * Say who we are
	 */
	ctl = REGVAL(MCPCIA_PCI_REV(sc));
	printf("%s: Horse Revision %d, %s Handed Saddle Revision %d,"
	    " CAP Revision %d\n", device_get_nameunit(dev), HORSE_REV(ctl),
	    (SADDLE_TYPE(ctl) & 1)? "Right": "Left", SADDLE_REV(ctl),
	    CAP_REV(ctl));

	/*
	 * See if we're the fella with the EISA bus...
	 */

	if (EISA_PRESENT(REGVAL(MCPCIA_PCI_REV(sc)))) {
		mcpcia_eisa = sc;
	}

	/*
	 * Set up DMA stuff here.
	 */

	mcpcia_dma_init(sc);

	/*
	 * Register our interrupt service requirements with our parent.
	 */
	rval =
	    BUS_SETUP_INTR(p, dev, NULL, INTR_TYPE_MISC, mcpcia_intr, 0, &intr);
	if (rval == 0) {
		if (sc == mcpcia_eisa) {
			busspace_isa_io = (struct alpha_busspace *)
				&sc->io_space;
			busspace_isa_mem = (struct alpha_busspace *)
				&sc->mem_space;
			/*
			 * Enable EISA interrupts.
			 */
			mcpcia_enable_intr(sc, 16);
		}
		bus_generic_attach(dev);
	}
	return (rval);
}

static void
mcpcia_enable_intr(struct mcpcia_softc *sc, int irq)
{

	REGVAL(MCPCIA_INT_MASK0(sc)) |= (1 << irq);
	alpha_mb();
}

static void
mcpcia_disable_intr(struct mcpcia_softc *sc, int irq)
{

	/*
	 * We need to write to INT_REQ as well as INT_MASK0 in case we
	 * are trying to mask an interrupt which is already
	 * asserted. Writing a 1 bit to INT_REQ clears the
	 * corresponding bit in the register.
	 */
 	REGVAL(MCPCIA_INT_MASK0(sc)) &= ~(1 << irq);
	REGVAL(MCPCIA_INT_REQ(sc)) = (1 << irq);
	alpha_mb();
}

static void
mcpcia_disable_intr_vec(uintptr_t vector)
{
	int mid, irq;
	struct mcpcia_softc *sc = mcpcia_root;

	if (vector < MCPCIA_VEC_PCI) {
		printf("EISA disable (0x%lx)\n", vector);
		return;
	}

	if (vector == MCPCIA_VEC_NCR) {
		mid = 5;
		irq = 16;
	} else {
		int tmp, slot;
                tmp = vector - MCPCIA_VEC_PCI;
                mid = (tmp / MCPCIA_VECWIDTH_PER_MCPCIA) + 4;
		tmp &= (MCPCIA_VECWIDTH_PER_MCPCIA - 1);
		slot = tmp / MCPCIA_VECWIDTH_PER_SLOT;
		if (slot < 2 || slot > 5) {
			printf("Bad slot (%d) for vector %lx\n", slot, vector);
			return;
		}
		tmp -= (2 * MCPCIA_VECWIDTH_PER_SLOT);
		irq = (tmp >> MCPCIA_VECWIDTH_PER_INTPIN) & 0xf;
	}
/*	printf("D<%03x>=%d,%d\n", vector, mid, irq); */
	while (sc) {
		if (mcbus_get_mid(sc->dev) == mid) {
			break;
		}
		sc = sc->next;
	}
	if (sc == NULL) {
		panic("couldn't find MCPCIA softc for vector 0x%lx", vector);
	}
	mtx_lock_spin(&icu_lock);
	mcpcia_disable_intr(sc, irq);
	mtx_unlock_spin(&icu_lock);
}

static void
mcpcia_enable_intr_vec(uintptr_t vector)
{
	int mid, irq;
	struct mcpcia_softc *sc = mcpcia_root;

	if (vector < MCPCIA_VEC_PCI) {
		printf("EISA ensable (0x%lx)\n", vector);
		return;
	}

	if (vector == MCPCIA_VEC_NCR) {
		mid = 5;
		irq = 16;
	} else {
		int tmp, slot;
                tmp = vector - MCPCIA_VEC_PCI;
                mid = (tmp / MCPCIA_VECWIDTH_PER_MCPCIA) + 4;
		tmp &= (MCPCIA_VECWIDTH_PER_MCPCIA - 1);
		slot = tmp / MCPCIA_VECWIDTH_PER_SLOT;
		if (slot < 2 || slot > 5) {
			printf("Bad slot (%d) for vector %lx\n", slot, vector);
			return;
		}
		tmp -= (2 * MCPCIA_VECWIDTH_PER_SLOT);
		irq = (tmp >> MCPCIA_VECWIDTH_PER_INTPIN) & 0xf;
	}
/*	printf("E<%03x>=%d,%d\n", vector, mid, irq); */
	while (sc) {
		if (mcbus_get_mid(sc->dev) == mid) {
			break;
		}
		sc = sc->next;
	}
	if (sc == NULL) {
		panic("couldn't find MCPCIA softc for vector 0x%lx", vector);
	}
	mtx_lock_spin(&icu_lock);
	mcpcia_enable_intr(sc, irq);
	mtx_unlock_spin(&icu_lock);
}

static int
mcpcia_pci_route_interrupt(device_t bus, device_t dev, int pin)
{
	int irq, slot, mid;

	/*
	 * Validate requested pin number.
	 */
	if ((pin < 1) || (pin > 4)) {
		printf("mcpcia_pci_route_interrupt: bad interrupt pin %d\n",
		    pin);
		return(255);
	}

	slot = pci_get_slot(dev);
	mid = mcbus_get_mid(bus);

#if 0
	printf("mcpcia_pci_route_interrupt: called for slot=%d, pin=%d, mid=%d\n", slot, pin, mid);
#endif

	if (mid == 5 && slot == 1) {
		irq = 16;	/* MID 5, slot 1, is the internal NCR 53c810 */
	} else if (slot >= 2 && slot <= 5) {
		irq = ((slot - 2) * 4) + (pin - 1);
	} else {
		printf("mcpcia_pci_route_interrupt: weird device number %d\n",
		    slot);
		return (255);
	}

	return(irq);
}

static int
mcpcia_setup_intr(device_t dev, device_t child, struct resource *ir, int flags,
       driver_intr_t *intr, void *arg, void **cp)
{
	struct mcpcia_softc *sc = MCPCIA_SOFTC(dev);
	int mid, birq, irq, error, h;
	
	irq = ir->r_start;
	mid = mcbus_get_mid(dev);

	error = rman_activate_resource(ir);
	if (error)
		return error;

	/*
	 * We now construct a vector as the hardware would, unless
	 * this is the internal NCR 53c810 interrupt.
	 */
	if (irq == 16) {
		h = MCPCIA_VEC_NCR;
	} else {
		h = MCPCIA_VEC_PCI +
		    ((mid - MCPCIA_PCI_MIDMIN) * MCPCIA_VECWIDTH_PER_MCPCIA) +
		    irq * MCPCIA_VECWIDTH_PER_INTPIN +
		    2 * MCPCIA_VECWIDTH_PER_SLOT;
	}
	birq = irq + INTRCNT_KN300_IRQ;
	error = alpha_setup_intr(device_get_nameunit(child), h,
	    intr, arg, flags, cp, &intrcnt[birq],
	    mcpcia_disable_intr_vec, mcpcia_enable_intr_vec);
	if (error)
		return error;
	mtx_lock_spin(&icu_lock);
	mcpcia_enable_intr(sc, irq);
	mtx_unlock_spin(&icu_lock);
	device_printf(child, "interrupting at IRQ 0x%x (vec 0x%x)\n",
	    irq , h);
	return (0);
}

static int
mcpcia_teardown_intr(device_t dev, device_t child, struct resource *i, void *c)
{
	struct mcpcia_softc *sc = MCPCIA_SOFTC(dev);

	mtx_lock_spin(&icu_lock);
	mcpcia_disable_intr(sc, i->r_start);
	mtx_unlock_spin(&icu_lock);
	alpha_teardown_intr(c);
	return (rman_deactivate_resource(i));
}

static int
mcpcia_read_ivar(device_t dev, device_t child, int which, u_long *result)
{
	switch (which) {
	case  PCIB_IVAR_BUS:
		*result = 0;
		return 0;
	}
	return ENOENT;
}

static void *
mcpcia_cvt_dense(device_t dev, vm_offset_t addr)
{
	struct mcpcia_softc *sc = MCPCIA_SOFTC(dev);

	addr &= 0xffffffffUL;
	return (void *) KV(addr | sc->dmem_base);
	
}

static struct alpha_busspace *
mcpcia_get_bustag(device_t dev, int type)
{
	struct mcpcia_softc *sc = MCPCIA_SOFTC(dev);

	switch (type) {
	case SYS_RES_IOPORT:
		return (struct alpha_busspace *) &sc->io_space;

	case SYS_RES_MEMORY:
		return (struct alpha_busspace *) &sc->mem_space;
	}

	return 0;
}

static struct rman *
mcpcia_get_rman(device_t dev, int type)
{
	struct mcpcia_softc *sc = MCPCIA_SOFTC(dev);

	switch (type) {
	case SYS_RES_IOPORT:
		return &sc->io_rman;

	case SYS_RES_MEMORY:
		return &sc->mem_rman;
	}

	return 0;
}

static int
mcpcia_maxslots(device_t dev)
{
	return (MCPCIA_MAXDEV);
}

static u_int32_t
mcpcia_read_config(device_t dev, int bus, int slot, int func,
		   int off, int sz)
{
	struct mcpcia_softc *sc = MCPCIA_SOFTC(dev);
	u_int32_t *dp, data, rvp;
	u_int64_t paddr;

	if ((off == PCIR_INTLINE) && (sz == 1)) {
		/* SRM left bad value; let intr_route fill them in later */
		return ~0;
	}

	rvp = data = ~0;

	/*
	 * There's nothing in slot 0 on a primary bus.
	 */
	if (bus == 0 && (slot < 1 || slot >= MCPCIA_MAXDEV))
		return (data);

	paddr = bus << 21;
	paddr |= slot << 16;
	paddr |= func << 13;
	paddr |= ((sz - 1) << 3);
	paddr |= ((unsigned long) ((off >> 2) << 7));
	paddr |= MCPCIA_PCI_CONF;
	paddr |= sc->sysbase;
	dp = (u_int32_t *)KV(paddr);

#if	0
printf("CFGREAD MID %d %d.%d.%d sz %d off %d -> paddr 0x%x",
mcbus_get_mid(dev), bus , slot, func, sz, off, paddr);
#endif
	if (badaddr(dp, sizeof (*dp)) == 0) {
		data = *dp;
	}
	if (data != ~0) {
		if (sz == 1) {
			rvp = SPARSE_BYTE_EXTRACT(off, data);
		} else if (sz == 2) {
			rvp = SPARSE_WORD_EXTRACT(off, data);
		} else {
			rvp = data;
		}
	} else {
		rvp = data;
	}

#if	0
printf(" data 0x%x -> 0x%x\n", data, rvp);
#endif
	return (rvp);
}

static void
mcpcia_write_config(device_t dev, int bus, int slot, int func,
		    int off, u_int32_t data, int sz)
{
	struct mcpcia_softc *sc = MCPCIA_SOFTC(dev);
	u_int32_t *dp;
	u_int64_t paddr;

	/*
	 * There's nothing in slot 0 on a primary bus.
	 */
	if (bus != 0 && (slot < 1 || slot >= MCPCIA_MAXDEV))
		return;

	paddr = bus << 21;
	paddr |= slot << 16;
	paddr |= func << 13;
	paddr |= ((sz - 1) << 3);
	paddr |= ((unsigned long) ((off >> 2) << 7));
	paddr |= MCPCIA_PCI_CONF;
	paddr |= sc->sysbase;
	dp = (u_int32_t *)KV(paddr);

	if (badaddr(dp, sizeof (*dp)) == 0) {
		u_int32_t new_data;
		if (sz == 1) {
			new_data = SPARSE_BYTE_INSERT(off, data);
		} else if (sz == 2) {
			new_data = SPARSE_WORD_INSERT(off, data);
		} else  {
			new_data = data;
		}

#if	0
printf("CFGWRITE MID%d %d.%d.%d sz %d off %d paddr %lx, data %x new_data %x\n",
mcbus_get_mid(dev), bus , slot, func, sz, off, paddr, data, new_data);
#endif

		*dp = new_data;
	}
}

static void
mcpcia_sgmap_map(void *arg, bus_addr_t ba, vm_offset_t pa)
{
	u_int64_t *sgtable = arg;
	int index = alpha_btop(ba - MCPCIA_ISA_SG_MAPPED_BASE);

	if (pa) {
		if (pa > (1L<<32))
			panic("mcpcia_sgmap_map: can't map address 0x%lx", pa);
		sgtable[index] = ((pa >> 13) << 1) | 1;
	} else {
		sgtable[index] = 0;
	}
	alpha_mb();
	MCPCIA_SGTLB_INVALIDATE(mcpcia_eisa);
}

static void
mcpcia_dma_init(struct mcpcia_softc *sc)
{

	/*
	 * Disable all windows first.
	 */

	REGVAL(MCPCIA_W0_BASE(sc)) = 0;
	REGVAL(MCPCIA_W1_BASE(sc)) = 0;
	REGVAL(MCPCIA_W2_BASE(sc)) = 0;
	REGVAL(MCPCIA_W3_BASE(sc)) = 0;
	REGVAL(MCPCIA_T0_BASE(sc)) = 0;
	REGVAL(MCPCIA_T1_BASE(sc)) = 0;
	REGVAL(MCPCIA_T2_BASE(sc)) = 0;
	REGVAL(MCPCIA_T3_BASE(sc)) = 0;
	alpha_mb();

	/*
	 * Set up window 0 as an 8MB SGMAP-mapped window starting at 8MB.
	 * Do this only for the EISA carrying MCPCIA. Partly because
	 * there's only one chipset sgmap thingie.
	 */

	if (sc == mcpcia_eisa) {
		void *sgtable;
		REGVAL(MCPCIA_W0_MASK(sc)) = MCPCIA_WMASK_8M;

		sgtable = contigmalloc(8192, M_DEVBUF,
		    M_NOWAIT, 0, 1L<<34, 32<<10, 1L<<34);

		if (sgtable == NULL) {
			panic("mcpcia_dma_init: cannot allocate sgmap");
			/* NOTREACHED */
		}
		REGVAL(MCPCIA_T0_BASE(sc)) =
		    pmap_kextract((vm_offset_t)sgtable) >> MCPCIA_TBASEX_SHIFT;

		alpha_mb();
		REGVAL(MCPCIA_W0_BASE(sc)) = MCPCIA_WBASE_EN |
		    MCPCIA_WBASE_SG | MCPCIA_ISA_SG_MAPPED_BASE;
		alpha_mb();
		MCPCIA_SGTLB_INVALIDATE(sc);
		chipset.sgmap = sgmap_map_create(MCPCIA_ISA_SG_MAPPED_BASE,
		    MCPCIA_ISA_SG_MAPPED_BASE + MCPCIA_ISA_SG_MAPPED_SIZE - 1,
		    mcpcia_sgmap_map, sgtable);
	}

	/*
	 * Set up window 1 as a 2 GB Direct-mapped window starting at 2GB.
	 */

	REGVAL(MCPCIA_W1_MASK(sc)) = MCPCIA_WMASK_2G;
	REGVAL(MCPCIA_T1_BASE(sc)) = 0;
	alpha_mb();
	REGVAL(MCPCIA_W1_BASE(sc)) =
		MCPCIA_DIRECT_MAPPED_BASE | MCPCIA_WBASE_EN;
	alpha_mb();

	/*
	 * When we get around to redoing the 'chipset' stuff to have more
	 * than one sgmap handler...
	 */

#if	0
	/*
	 * Set up window 2 as a 1G SGMAP-mapped window starting at 1G.
	 */

	REGVAL(MCPCIA_W2_MASK(sc)) = MCPCIA_WMASK_1G;
	REGVAL(MCPCIA_T2_BASE(sc)) =
		ccp->cc_pci_sgmap.aps_ptpa >> MCPCIA_TBASEX_SHIFT;
	alpha_mb();
	REGVAL(MCPCIA_W2_BASE(sc)) =
		MCPCIA_WBASE_EN | MCPCIA_WBASE_SG | MCPCIA_PCI_SG_MAPPED_BASE;
	alpha_mb();
#endif

	/* XXX XXX BEGIN XXX XXX */
	{							/* XXX */
		alpha_XXX_dmamap_or = MCPCIA_DIRECT_MAPPED_BASE;/* XXX */
	}							/* XXX */
	/* XXX XXX END XXX XXX */
}

/*
 */

static void
mcpcia_intr(void *arg)
{
	unsigned long vec = (unsigned long) arg;

	/*
	 * Check for I2C interrupts.  These are technically within
	 * the PCI vector range, but no PCI device should ever map
	 * to them.
	 */
	if (vec == MCPCIA_I2C_CVEC) {
		printf("i2c: controller interrupt\n");
		return;
	}
	if (vec == MCPCIA_I2C_BVEC) {
		printf("i2c: bus interrupt\n");
		return;
	}

	alpha_dispatch_intr(NULL, vec);
}

static device_method_t mcpcia_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			mcpcia_probe),
	DEVMETHOD(device_attach,		mcpcia_attach),

	/* Bus interface */
	DEVMETHOD(bus_print_child,		bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,		mcpcia_read_ivar),
	DEVMETHOD(bus_setup_intr,		mcpcia_setup_intr),
	DEVMETHOD(bus_teardown_intr,		mcpcia_teardown_intr),
	DEVMETHOD(bus_alloc_resource,		alpha_pci_alloc_resource),
	DEVMETHOD(bus_release_resource,		pci_release_resource),
	DEVMETHOD(bus_activate_resource,	pci_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	pci_deactivate_resource),

	/* alphapci interface */
	DEVMETHOD(alphapci_cvt_dense,		mcpcia_cvt_dense),
	DEVMETHOD(alphapci_get_bustag,		mcpcia_get_bustag),
	DEVMETHOD(alphapci_get_rman,		mcpcia_get_rman),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots,		mcpcia_maxslots),
	DEVMETHOD(pcib_read_config,		mcpcia_read_config),
	DEVMETHOD(pcib_write_config,		mcpcia_write_config),
	DEVMETHOD(pcib_route_interrupt,		mcpcia_pci_route_interrupt),

	{ 0, 0 }
};

static driver_t mcpcia_driver = {
	"pcib", mcpcia_methods, sizeof (struct mcpcia_softc)
};

DRIVER_MODULE(pcib, mcbus, mcpcia_driver, mcpcia_devclass, 0, 0);
