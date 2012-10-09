/*-
 * Copyright (c) 2002 - 2008 S�ren Schmidt <sos@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
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

#include "opt_ata.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/ata.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/conf.h>
#include <sys/sema.h>
#include <sys/taskqueue.h>
#include <vm/uma.h>
#include <machine/resource.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <isa/isavar.h>
#include <dev/ata/ata-all.h>
#include <ata_if.h>

/* local vars */
struct ata_cbus_controller {
    struct resource *io;
    struct resource *ctlio;
    struct resource *bankio;
    struct resource *irq;
    void *ih;
    struct mtx bank_mtx;
    int locked_bank;
    int restart_bank;
    int hardware_bank;
    struct {
	void (*function)(void *);
	void *argument;
    } interrupt[2];
};

/* local prototypes */
static void ata_cbus_intr(void *);
static int ata_cbuschannel_banking(device_t dev, int flags);

static int
ata_cbus_probe(device_t dev)
{
    struct resource *io;
    int rid;
    u_long tmp;

    /* dont probe PnP devices */
    if (isa_get_vendorid(dev))
	return (ENXIO);

    /* allocate the ioport range */
    rid = ATA_IOADDR_RID;
    if (!(io = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0,
				  ATA_PC98_IOSIZE, RF_ACTIVE)))
	return ENOMEM;

    /* calculate & set the altport range */
    rid = ATA_PC98_CTLADDR_RID;
    if (bus_get_resource(dev, SYS_RES_IOPORT, rid, &tmp, &tmp)) {
	bus_set_resource(dev, SYS_RES_IOPORT, rid,
			 rman_get_start(io)+ATA_PC98_CTLOFFSET, ATA_CTLIOSIZE);
    }

    /* calculate & set the bank range */
    rid = ATA_PC98_BANKADDR_RID;
    if (bus_get_resource(dev, SYS_RES_IOPORT, rid, &tmp, &tmp)) {
	bus_set_resource(dev, SYS_RES_IOPORT, rid,
			 ATA_PC98_BANK, ATA_PC98_BANKIOSIZE);
    }

    bus_release_resource(dev, SYS_RES_IOPORT, ATA_IOADDR_RID, io);
    return 0;
}

static int
ata_cbus_attach(device_t dev)
{
    struct ata_cbus_controller *ctlr = device_get_softc(dev);
    device_t child;
    int rid, unit;

    /* allocate resources */
    rid = ATA_IOADDR_RID;
    if (!(ctlr->io = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0,
					ATA_PC98_IOSIZE, RF_ACTIVE)))
       return ENOMEM;

    rid = ATA_PC98_CTLADDR_RID;
    if (!(ctlr->ctlio = 
	  bus_alloc_resource(dev, SYS_RES_IOPORT, &rid,
			     rman_get_start(ctlr->io) + ATA_PC98_CTLOFFSET, ~0,
			     ATA_CTLIOSIZE, RF_ACTIVE))) {
	bus_release_resource(dev, SYS_RES_IOPORT, ATA_IOADDR_RID, ctlr->io);
	return ENOMEM;
    }

    rid = ATA_PC98_BANKADDR_RID;
    if (!(ctlr->bankio = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid,
					    ATA_PC98_BANK, ~0,
					    ATA_PC98_BANKIOSIZE, RF_ACTIVE))) {
	bus_release_resource(dev, SYS_RES_IOPORT, ATA_IOADDR_RID, ctlr->io);
	bus_release_resource(dev, SYS_RES_IOPORT, ATA_CTLADDR_RID, ctlr->ctlio);
	return ENOMEM;
    }

    rid = ATA_IRQ_RID;
    if (!(ctlr->irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
					     RF_ACTIVE | RF_SHAREABLE))) {
	device_printf(dev, "unable to alloc interrupt\n");
	bus_release_resource(dev, SYS_RES_IOPORT, ATA_IOADDR_RID, ctlr->io);
	bus_release_resource(dev, SYS_RES_IOPORT, ATA_CTLADDR_RID, ctlr->ctlio);
	bus_release_resource(dev, SYS_RES_IOPORT, 
			     ATA_PC98_BANKADDR_RID, ctlr->bankio);
	return ENXIO;
    }

    if ((bus_setup_intr(dev, ctlr->irq, ATA_INTR_FLAGS,
			NULL, ata_cbus_intr, ctlr, &ctlr->ih))) {
	device_printf(dev, "unable to setup interrupt\n");
	bus_release_resource(dev, SYS_RES_IOPORT, ATA_IOADDR_RID, ctlr->io);
	bus_release_resource(dev, SYS_RES_IOPORT, ATA_CTLADDR_RID, ctlr->ctlio);
	bus_release_resource(dev, SYS_RES_IOPORT, 
			     ATA_PC98_BANKADDR_RID, ctlr->bankio);
	bus_release_resource(dev, SYS_RES_IOPORT, ATA_IRQ_RID, ctlr->irq);
	return ENXIO;
    }

    mtx_init(&ctlr->bank_mtx, "ATA cbus bank lock", NULL, MTX_DEF);
    ctlr->hardware_bank = -1;
    ctlr->locked_bank = -1;
    ctlr->restart_bank = -1;

    for (unit = 0; unit < 2; unit++) {
	child = device_add_child(dev, "ata", unit);
	if (child == NULL)
	    device_printf(dev, "failed to add ata child device\n");
	else
	    device_set_ivars(child, (void *)(intptr_t)unit);
    }

    bus_generic_attach(dev);
    return (0);
}

static struct resource *
ata_cbus_alloc_resource(device_t dev, device_t child, int type, int *rid,
			u_long start, u_long end, u_long count, u_int flags)
{
    struct ata_cbus_controller *ctlr = device_get_softc(dev);

    if (type == SYS_RES_IOPORT) {
	switch (*rid) {
	case ATA_IOADDR_RID:
	    return ctlr->io;
	case ATA_CTLADDR_RID:
	    return ctlr->ctlio;
	}
    }
    if (type == SYS_RES_IRQ)
	return ctlr->irq;
    return 0;
}

static int
ata_cbus_setup_intr(device_t dev, device_t child, struct resource *irq,
	       int flags, driver_filter_t *filter, driver_intr_t *intr, 
	       void *arg, void **cookiep)
{
    struct ata_cbus_controller *controller = device_get_softc(dev);
    int unit = ((struct ata_channel *)device_get_softc(child))->unit;

    if (filter != NULL) {
	    printf("ata-cbus.c: we cannot use a filter here\n");
	    return (EINVAL);
    }
    controller->interrupt[unit].function = intr;
    controller->interrupt[unit].argument = arg;
    *cookiep = controller;

    return 0;
}

static int
ata_cbus_print_child(device_t dev, device_t child)
{
    struct ata_channel *ch = device_get_softc(child);
    int retval = 0;

    retval += bus_print_child_header(dev, child);
    retval += printf(" at bank %d", ch->unit);
    retval += bus_print_child_footer(dev, child);
    return retval;
}

static void
ata_cbus_intr(void *data)
{  
    struct ata_cbus_controller *ctlr = data;
    struct ata_channel *ch;
    int unit;

    for (unit = 0; unit < 2; unit++) {
	if (!(ch = ctlr->interrupt[unit].argument))
	    continue;
	if (ata_cbuschannel_banking(ch->dev, ATA_LF_WHICH) == unit)
	    ctlr->interrupt[unit].function(ch);
    }
}

static device_method_t ata_cbus_methods[] = {
    /* device interface */
    DEVMETHOD(device_probe,             ata_cbus_probe),
    DEVMETHOD(device_attach,            ata_cbus_attach),
//  DEVMETHOD(device_detach,            ata_cbus_detach),

    /* bus methods */
    DEVMETHOD(bus_alloc_resource,       ata_cbus_alloc_resource),
    DEVMETHOD(bus_setup_intr,           ata_cbus_setup_intr),
    DEVMETHOD(bus_print_child,          ata_cbus_print_child),

    DEVMETHOD_END
};

static driver_t ata_cbus_driver = {
    "atacbus",
    ata_cbus_methods,
    sizeof(struct ata_cbus_controller),
};

static devclass_t ata_cbus_devclass;

DRIVER_MODULE(atacbus, isa, ata_cbus_driver, ata_cbus_devclass, 0, 0);

static int
ata_cbuschannel_probe(device_t dev)
{
    char buffer[32];

    sprintf(buffer, "ATA channel %d", (int)(intptr_t)device_get_ivars(dev));
    device_set_desc_copy(dev, buffer);

    return ata_probe(dev);
}

static int
ata_cbuschannel_attach(device_t dev)
{
    struct ata_cbus_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    int i;

    if (ch->attached)
	return (0);
    ch->attached = 1;

    ch->unit = (intptr_t)device_get_ivars(dev);
    /* setup the resource vectors */
    for (i = ATA_DATA; i <= ATA_COMMAND; i ++) {
	ch->r_io[i].res = ctlr->io;
	ch->r_io[i].offset = i << 1;
    }
    ch->r_io[ATA_CONTROL].res = ctlr->ctlio;
    ch->r_io[ATA_CONTROL].offset = 0;
    ch->r_io[ATA_IDX_ADDR].res = ctlr->io;
    ata_default_registers(dev);

    /* initialize softc for this channel */
    ch->flags |= ATA_USE_16BIT;
    ata_generic_hw(dev);

    return ata_attach(dev);
}

static int
ata_cbuschannel_detach(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);

    if (!ch->attached)
	return (0);
    ch->attached = 0;

    return ata_detach(dev);
}

static int
ata_cbuschannel_suspend(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);

    if (!ch->attached)
	return (0);

    return ata_suspend(dev);
}

static int
ata_cbuschannel_resume(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);

    if (!ch->attached)
	return (0);

    return ata_resume(dev);
}

static int
ata_cbuschannel_banking(device_t dev, int flags)
{
    struct ata_cbus_controller *ctlr = device_get_softc(device_get_parent(dev));
#ifndef ATA_CAM
    struct ata_channel *ch = device_get_softc(dev);
#endif
    int res;

    mtx_lock(&ctlr->bank_mtx);
    switch (flags) {
#ifndef ATA_CAM
    case ATA_LF_LOCK:
	if (ctlr->locked_bank == -1)
	    ctlr->locked_bank = ch->unit;
	if (ctlr->locked_bank == ch->unit) {
	    ctlr->hardware_bank = ch->unit;
	    ATA_OUTB(ctlr->bankio, 0, ch->unit);
	}
	else
	    ctlr->restart_bank = ch->unit;
	break;

    case ATA_LF_UNLOCK:
	if (ctlr->locked_bank == ch->unit) {
	    ctlr->locked_bank = -1;
	    if (ctlr->restart_bank != -1) {
		if ((ch = ctlr->interrupt[ctlr->restart_bank].argument)) {
		    ctlr->restart_bank = -1;
		    mtx_unlock(&ctlr->bank_mtx);
		    ata_start(ch->dev);
		    return -1;
		}
	    }
	}
	break;
#endif

    case ATA_LF_WHICH:
	break;
    }
    res = ctlr->locked_bank;
    mtx_unlock(&ctlr->bank_mtx);
    return res;
}

static device_method_t ata_cbuschannel_methods[] = {
    /* device interface */
    DEVMETHOD(device_probe,     ata_cbuschannel_probe),
    DEVMETHOD(device_attach,    ata_cbuschannel_attach),
    DEVMETHOD(device_detach,    ata_cbuschannel_detach),
    DEVMETHOD(device_suspend,   ata_cbuschannel_suspend),
    DEVMETHOD(device_resume,    ata_cbuschannel_resume),

#ifndef ATA_CAM
    /* ATA methods */
    DEVMETHOD(ata_locking,      ata_cbuschannel_banking),
#endif
    DEVMETHOD_END
};

static driver_t ata_cbuschannel_driver = {
    "ata",
    ata_cbuschannel_methods,
    sizeof(struct ata_channel),
};

DRIVER_MODULE(ata, atacbus, ata_cbuschannel_driver, ata_devclass, NULL, NULL);
MODULE_DEPEND(ata, ata, 1, 1, 1);
