/*-
 * Copyright 1998 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/7.0.0/sys/i386/i386/nexus.c 169391 2007-05-08 21:29:14Z jhb $");

/*
 * This code implements a `root nexus' for Intel Architecture
 * machines.  The function of the root nexus is to serve as an
 * attachment point for both processors and buses, and to manage
 * resources which are common to all of them.  In particular,
 * this code implements the core resource managers for interrupt
 * requests, DMA requests (which rightfully should be a part of the
 * ISA code but it's easier to do it here for now), I/O port addresses,
 * and I/O memory address space.
 */

#include "opt_apic.h"
#include "opt_isa.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <machine/bus.h>
#include <machine/intr_machdep.h>
#include <sys/rman.h>
#include <sys/interrupt.h>

#include <machine/vmparam.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/pmap.h>

#include <machine/resource.h>

#ifdef DEV_APIC
#include "pcib_if.h"
#endif

#ifdef DEV_ISA
#include <isa/isavar.h>
#ifdef PC98
#include <pc98/cbus/cbus.h>
#else
#include <i386/isa/isa.h>
#endif
#endif
#include <sys/rtprio.h>

static MALLOC_DEFINE(M_NEXUSDEV, "nexusdev", "Nexus device");
struct nexus_device {
	struct resource_list	nx_resources;
};

#define DEVTONX(dev)	((struct nexus_device *)device_get_ivars(dev))

static struct rman irq_rman, drq_rman, port_rman, mem_rman;

static	int nexus_probe(device_t);
static	int nexus_attach(device_t);
static	int nexus_print_all_resources(device_t dev);
static	int nexus_print_child(device_t, device_t);
static device_t nexus_add_child(device_t bus, int order, const char *name,
				int unit);
static	struct resource *nexus_alloc_resource(device_t, device_t, int, int *,
					      u_long, u_long, u_long, u_int);
static	int nexus_config_intr(device_t, int, enum intr_trigger,
			      enum intr_polarity);
static	int nexus_activate_resource(device_t, device_t, int, int,
				    struct resource *);
static	int nexus_deactivate_resource(device_t, device_t, int, int,
				      struct resource *);
static	int nexus_release_resource(device_t, device_t, int, int,
				   struct resource *);
static	int nexus_setup_intr(device_t, device_t, struct resource *, int flags,
			     driver_filter_t filter, void (*)(void *), void *,
			      void **);
static	int nexus_teardown_intr(device_t, device_t, struct resource *,
				void *);
static struct resource_list *nexus_get_reslist(device_t dev, device_t child);
static	int nexus_set_resource(device_t, device_t, int, int, u_long, u_long);
static	int nexus_get_resource(device_t, device_t, int, int, u_long *, u_long *);
static void nexus_delete_resource(device_t, device_t, int, int);
#ifdef DEV_APIC
static	int nexus_alloc_msi(device_t pcib, device_t dev, int count, int maxcount, int *irqs);
static	int nexus_release_msi(device_t pcib, device_t dev, int count, int *irqs);
static	int nexus_alloc_msix(device_t pcib, device_t dev, int *irq);
static	int nexus_release_msix(device_t pcib, device_t dev, int irq);
static	int nexus_map_msi(device_t pcib, device_t dev, int irq, uint64_t *addr, uint32_t *data);
#endif

static device_method_t nexus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		nexus_probe),
	DEVMETHOD(device_attach,	nexus_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	nexus_print_child),
	DEVMETHOD(bus_add_child,	nexus_add_child),
	DEVMETHOD(bus_alloc_resource,	nexus_alloc_resource),
	DEVMETHOD(bus_release_resource,	nexus_release_resource),
	DEVMETHOD(bus_activate_resource, nexus_activate_resource),
	DEVMETHOD(bus_deactivate_resource, nexus_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	nexus_setup_intr),
	DEVMETHOD(bus_teardown_intr,	nexus_teardown_intr),
	DEVMETHOD(bus_config_intr,	nexus_config_intr),
	DEVMETHOD(bus_get_resource_list, nexus_get_reslist),
	DEVMETHOD(bus_set_resource,	nexus_set_resource),
	DEVMETHOD(bus_get_resource,	nexus_get_resource),
	DEVMETHOD(bus_delete_resource,	nexus_delete_resource),

	/* pcib interface */
#ifdef DEV_APIC
	DEVMETHOD(pcib_alloc_msi,	nexus_alloc_msi),
	DEVMETHOD(pcib_release_msi,	nexus_release_msi),
	DEVMETHOD(pcib_alloc_msix,	nexus_alloc_msix),
	DEVMETHOD(pcib_release_msix,	nexus_release_msix),
	DEVMETHOD(pcib_map_msi,		nexus_map_msi),
#endif

	{ 0, 0 }
};

static driver_t nexus_driver = {
	"nexus",
	nexus_methods,
	1,			/* no softc */
};
static devclass_t nexus_devclass;

DRIVER_MODULE(nexus, root, nexus_driver, nexus_devclass, 0, 0);

static int
nexus_probe(device_t dev)
{
	int irq;

	device_quiet(dev);	/* suppress attach message for neatness */

	/*
	 * XXX working notes:
	 *
	 * - IRQ resource creation should be moved to the PIC/APIC driver.
	 * - DRQ resource creation should be moved to the DMAC driver.
	 * - The above should be sorted to probe earlier than any child busses.
	 *
	 * - Leave I/O and memory creation here, as child probes may need them.
	 *   (especially eg. ACPI)
	 */

	/*
	 * IRQ's are on the mainboard on old systems, but on the ISA part
	 * of PCI->ISA bridges.  There would be multiple sets of IRQs on
	 * multi-ISA-bus systems.  PCI interrupts are routed to the ISA
	 * component, so in a way, PCI can be a partial child of an ISA bus(!).
	 * APIC interrupts are global though.
	 */
	irq_rman.rm_start = 0;
	irq_rman.rm_type = RMAN_ARRAY;
	irq_rman.rm_descr = "Interrupt request lines";
	irq_rman.rm_end = NUM_IO_INTS - 1;
	if (rman_init(&irq_rman))
		panic("nexus_probe irq_rman");

	/*
	 * We search for regions of existing IRQs and add those to the IRQ
	 * resource manager.
	 */
	for (irq = 0; irq < NUM_IO_INTS; irq++)
		if (intr_lookup_source(irq) != NULL)
			if (rman_manage_region(&irq_rman, irq, irq) != 0)
				panic("nexus_probe irq_rman add");

	/*
	 * ISA DMA on PCI systems is implemented in the ISA part of each
	 * PCI->ISA bridge and the channels can be duplicated if there are
	 * multiple bridges.  (eg: laptops with docking stations)
	 */
	drq_rman.rm_start = 0;
#ifdef PC98
	drq_rman.rm_end = 3;
#else
	drq_rman.rm_end = 7;
#endif
	drq_rman.rm_type = RMAN_ARRAY;
	drq_rman.rm_descr = "DMA request lines";
	/* XXX drq 0 not available on some machines */
	if (rman_init(&drq_rman)
	    || rman_manage_region(&drq_rman,
				  drq_rman.rm_start, drq_rman.rm_end))
		panic("nexus_probe drq_rman");

	/*
	 * However, IO ports and Memory truely are global at this level,
	 * as are APIC interrupts (however many IO APICS there turn out
	 * to be on large systems..)
	 */
	port_rman.rm_start = 0;
	port_rman.rm_end = 0xffff;
	port_rman.rm_type = RMAN_ARRAY;
	port_rman.rm_descr = "I/O ports";
	if (rman_init(&port_rman)
	    || rman_manage_region(&port_rman, 0, 0xffff))
		panic("nexus_probe port_rman");

	mem_rman.rm_start = 0;
	mem_rman.rm_end = ~0u;
	mem_rman.rm_type = RMAN_ARRAY;
	mem_rman.rm_descr = "I/O memory addresses";
	if (rman_init(&mem_rman)
	    || rman_manage_region(&mem_rman, 0, ~0))
		panic("nexus_probe mem_rman");

	return 0;
}

static int
nexus_attach(device_t dev)
{

	bus_generic_probe(dev);
	bus_generic_attach(dev);
	return 0;
}

static int
nexus_print_all_resources(device_t dev)
{
	struct	nexus_device *ndev = DEVTONX(dev);
	struct resource_list *rl = &ndev->nx_resources;
	int retval = 0;

	if (STAILQ_FIRST(rl))
		retval += printf(" at");

	retval += resource_list_print_type(rl, "port", SYS_RES_IOPORT, "%#lx");
	retval += resource_list_print_type(rl, "iomem", SYS_RES_MEMORY, "%#lx");
	retval += resource_list_print_type(rl, "irq", SYS_RES_IRQ, "%ld");

	return retval;
}

static int
nexus_print_child(device_t bus, device_t child)
{
	int retval = 0;

	retval += bus_print_child_header(bus, child);
	retval += nexus_print_all_resources(child);
	if (device_get_flags(child))
		retval += printf(" flags %#x", device_get_flags(child));
	retval += printf(" on motherboard\n");	/* XXX "motherboard", ick */

	return (retval);
}

static device_t
nexus_add_child(device_t bus, int order, const char *name, int unit)
{
	device_t		child;
	struct nexus_device	*ndev;

	ndev = malloc(sizeof(struct nexus_device), M_NEXUSDEV, M_NOWAIT|M_ZERO);
	if (!ndev)
		return(0);
	resource_list_init(&ndev->nx_resources);

	child = device_add_child_ordered(bus, order, name, unit);

	/* should we free this in nexus_child_detached? */
	device_set_ivars(child, ndev);

	return(child);
}

/*
 * Allocate a resource on behalf of child.  NB: child is usually going to be a
 * child of one of our descendants, not a direct child of nexus0.
 * (Exceptions include npx.)
 */
static struct resource *
nexus_alloc_resource(device_t bus, device_t child, int type, int *rid,
		     u_long start, u_long end, u_long count, u_int flags)
{
	struct nexus_device *ndev = DEVTONX(child);
	struct	resource *rv;
	struct resource_list_entry *rle;
	struct	rman *rm;
	int needactivate = flags & RF_ACTIVE;

	/*
	 * If this is an allocation of the "default" range for a given RID, and
	 * we know what the resources for this device are (ie. they aren't maintained
	 * by a child bus), then work out the start/end values.
	 */
	if ((start == 0UL) && (end == ~0UL) && (count == 1)) {
		if (ndev == NULL)
			return(NULL);
		rle = resource_list_find(&ndev->nx_resources, type, *rid);
		if (rle == NULL)
			return(NULL);
		start = rle->start;
		end = rle->end;
		count = rle->count;
	}

	flags &= ~RF_ACTIVE;

	switch (type) {
	case SYS_RES_IRQ:
		rm = &irq_rman;
		break;

	case SYS_RES_DRQ:
		rm = &drq_rman;
		break;

	case SYS_RES_IOPORT:
		rm = &port_rman;
		break;

	case SYS_RES_MEMORY:
		rm = &mem_rman;
		break;

	default:
		return 0;
	}

	rv = rman_reserve_resource(rm, start, end, count, flags, child);
	if (rv == 0)
		return 0;
	rman_set_rid(rv, *rid);

	if (needactivate) {
		if (bus_activate_resource(child, type, *rid, rv)) {
			rman_release_resource(rv);
			return 0;
		}
	}

	return rv;
}

static int
nexus_activate_resource(device_t bus, device_t child, int type, int rid,
			struct resource *r)
{
#ifdef PC98
	bus_space_handle_t bh;
	int error;
#endif
	void *vaddr;

	/*
	 * If this is a memory resource, map it into the kernel.
	 */
	switch (type) {
	case SYS_RES_IOPORT:
#ifdef PC98
		error = i386_bus_space_handle_alloc(I386_BUS_SPACE_IO,
		    rman_get_start(r), rman_get_size(r), &bh);
		if (error)
			return (error);
		rman_set_bushandle(r, bh);
#else
		rman_set_bushandle(r, rman_get_start(r));
#endif
		rman_set_bustag(r, I386_BUS_SPACE_IO);
		break;
	case SYS_RES_MEMORY:
#ifdef PC98
		error = i386_bus_space_handle_alloc(I386_BUS_SPACE_MEM,
		    rman_get_start(r), rman_get_size(r), &bh);
		if (error)
			return (error);
#endif
		vaddr = pmap_mapdev(rman_get_start(r), rman_get_size(r));
		rman_set_virtual(r, vaddr);
		rman_set_bustag(r, I386_BUS_SPACE_MEM);
#ifdef PC98
		/* PC-98: the type of bus_space_handle_t is the structure. */
		bh->bsh_base = (bus_addr_t) vaddr;
		rman_set_bushandle(r, bh);
#else
		/* IBM-PC: the type of bus_space_handle_t is u_int */
		rman_set_bushandle(r, (bus_space_handle_t) vaddr);
#endif
	}
	return (rman_activate_resource(r));
}

static int
nexus_deactivate_resource(device_t bus, device_t child, int type, int rid,
			  struct resource *r)
{

	/*
	 * If this is a memory resource, unmap it.
	 */
	if (type == SYS_RES_MEMORY) {
		pmap_unmapdev((vm_offset_t)rman_get_virtual(r),
		    rman_get_size(r));
	}
#ifdef PC98
	if (type == SYS_RES_MEMORY || type == SYS_RES_IOPORT) {
		bus_space_handle_t bh;

		bh = rman_get_bushandle(r);
		i386_bus_space_handle_free(rman_get_bustag(r), bh, bh->bsh_sz);
	}
#endif
	return (rman_deactivate_resource(r));
}

static int
nexus_release_resource(device_t bus, device_t child, int type, int rid,
		       struct resource *r)
{
	if (rman_get_flags(r) & RF_ACTIVE) {
		int error = bus_deactivate_resource(child, type, rid, r);
		if (error)
			return error;
	}
	return (rman_release_resource(r));
}

/*
 * Currently this uses the really grody interface from kern/kern_intr.c
 * (which really doesn't belong in kern/anything.c).  Eventually, all of
 * the code in kern_intr.c and machdep_intr.c should get moved here, since
 * this is going to be the official interface.
 */
static int
nexus_setup_intr(device_t bus, device_t child, struct resource *irq,
		 int flags, driver_filter_t filter, void (*ihand)(void *),
		 void *arg, void **cookiep)
{
	int		error;

	/* somebody tried to setup an irq that failed to allocate! */
	if (irq == NULL)
		panic("nexus_setup_intr: NULL irq resource!");

	*cookiep = 0;
	if ((rman_get_flags(irq) & RF_SHAREABLE) == 0)
		flags |= INTR_EXCL;

	/*
	 * We depend here on rman_activate_resource() being idempotent.
	 */
	error = rman_activate_resource(irq);
	if (error)
		return (error);

	error = intr_add_handler(device_get_nameunit(child),
	    rman_get_start(irq), filter, ihand, arg, flags, cookiep);

	return (error);
}

static int
nexus_teardown_intr(device_t dev, device_t child, struct resource *r, void *ih)
{
	return (intr_remove_handler(ih));
}

static int
nexus_config_intr(device_t dev, int irq, enum intr_trigger trig,
    enum intr_polarity pol)
{
	return (intr_config_intr(irq, trig, pol));
}

static struct resource_list *
nexus_get_reslist(device_t dev, device_t child)
{
	struct nexus_device *ndev = DEVTONX(child);

	return (&ndev->nx_resources);
}

static int
nexus_set_resource(device_t dev, device_t child, int type, int rid, u_long start, u_long count)
{
	struct nexus_device	*ndev = DEVTONX(child);
	struct resource_list	*rl = &ndev->nx_resources;

	/* XXX this should return a success/failure indicator */
	resource_list_add(rl, type, rid, start, start + count - 1, count);
	return(0);
}

static int
nexus_get_resource(device_t dev, device_t child, int type, int rid, u_long *startp, u_long *countp)
{
	struct nexus_device	*ndev = DEVTONX(child);
	struct resource_list	*rl = &ndev->nx_resources;
	struct resource_list_entry *rle;

	rle = resource_list_find(rl, type, rid);
	if (!rle)
		return(ENOENT);
	if (startp)
		*startp = rle->start;
	if (countp)
		*countp = rle->count;
	return(0);
}

static void
nexus_delete_resource(device_t dev, device_t child, int type, int rid)
{
	struct nexus_device	*ndev = DEVTONX(child);
	struct resource_list	*rl = &ndev->nx_resources;

	resource_list_delete(rl, type, rid);
}

/* Called from the MSI code to add new IRQs to the IRQ rman. */
void
nexus_add_irq(u_long irq)
{

	if (rman_manage_region(&irq_rman, irq, irq) != 0)
		panic("%s: failed", __func__);
}

#ifdef DEV_APIC
static int
nexus_alloc_msix(device_t pcib, device_t dev, int *irq)
{

	return (msix_alloc(dev, irq));
}

static int
nexus_release_msix(device_t pcib, device_t dev, int irq)
{

	return (msix_release(irq));
}

static int
nexus_alloc_msi(device_t pcib, device_t dev, int count, int maxcount, int *irqs)
{

	return (msi_alloc(dev, count, maxcount, irqs));
}

static int
nexus_release_msi(device_t pcib, device_t dev, int count, int *irqs)
{

	return (msi_release(irqs, count));
}

static int
nexus_map_msi(device_t pcib, device_t dev, int irq, uint64_t *addr, uint32_t *data)
{

	return (msi_map(irq, addr, data));
}
#endif

/* Placeholder for system RAM. */
static void
ram_identify(driver_t *driver, device_t parent)
{

	if (resource_disabled("ram", 0))
		return;	
	if (BUS_ADD_CHILD(parent, 0, "ram", 0) == NULL)
		panic("ram_identify");
}

static int
ram_probe(device_t dev)
{

	device_quiet(dev);
	device_set_desc(dev, "System RAM");
	return (0);
}

static int
ram_attach(device_t dev)
{
	struct resource *res;
	vm_paddr_t *p;
	int error, i, rid;

	/*
	 * We use the dump_avail[] array rather than phys_avail[] for
	 * the memory map as phys_avail[] contains holes for kernel
	 * memory, page 0, the message buffer, and the dcons buffer.
	 * We test the end address in the loop instead of the start
	 * since the start address for the first segment is 0.
	 *
	 * XXX: It would be preferable to use the SMAP if it exists
	 * instead since if the SMAP is very fragmented we may not
	 * include some memory regions in dump_avail[] and phys_avail[].
	 */
	for (i = 0, p = dump_avail; p[1] != 0; i++, p += 2) {
		rid = i;
#ifdef PAE
		/*
		 * Resources use long's to track resources, so we can't
		 * include memory regions above 4GB.
		 */
		if (p[0] >= ~0ul)
			break;
#endif
		error = bus_set_resource(dev, SYS_RES_MEMORY, rid, p[0],
		    p[1] - p[0]);
		if (error)
			panic("ram_attach: resource %d failed set with %d", i,
			    error);
		res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, 0);
		if (res == NULL)
			panic("ram_attach: resource %d failed to attach", i);
	}
	return (0);
}

static device_method_t ram_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	ram_identify),
	DEVMETHOD(device_probe,		ram_probe),
	DEVMETHOD(device_attach,	ram_attach),
	{ 0, 0 }
};

static driver_t ram_driver = {
	"ram",
	ram_methods,
	1,		/* no softc */
};

static devclass_t ram_devclass;

DRIVER_MODULE(ram, nexus, ram_driver, ram_devclass, 0, 0);

#ifdef DEV_ISA
/*
 * Placeholder which claims PnP 'devices' which describe system
 * resources.
 */
static struct isa_pnp_id sysresource_ids[] = {
	{ 0x010cd041 /* PNP0c01 */, "System Memory" },
	{ 0x020cd041 /* PNP0c02 */, "System Resource" },
	{ 0 }
};

static int
sysresource_probe(device_t dev)
{
	int	result;

	if ((result = ISA_PNP_PROBE(device_get_parent(dev), dev, sysresource_ids)) <= 0) {
		device_quiet(dev);
	}
	return(result);
}

static int
sysresource_attach(device_t dev)
{
	return(0);
}

static device_method_t sysresource_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		sysresource_probe),
	DEVMETHOD(device_attach,	sysresource_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),
	{ 0, 0 }
};

static driver_t sysresource_driver = {
	"sysresource",
	sysresource_methods,
	1,		/* no softc */
};

static devclass_t sysresource_devclass;

DRIVER_MODULE(sysresource, isa, sysresource_driver, sysresource_devclass, 0, 0);
#endif /* DEV_ISA */
