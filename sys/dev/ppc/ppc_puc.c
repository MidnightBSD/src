/*-
 * Copyright (c) 1997-2000 Nicolas Souchu
 * Copyright (c) 2001 Alcove - Nicolas Souchu
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
 * $FreeBSD: src/sys/dev/ppc/ppc_puc.c,v 1.2 2004/05/30 20:08:37 phk Exp $
 *
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
  
#include <machine/bus.h>

#include <dev/ppbus/ppbconf.h>
#include <dev/ppbus/ppb_msq.h>
#include <dev/ppc/ppcvar.h>
#include <dev/ppc/ppcreg.h>

#include "ppbus_if.h"

static int ppc_puc_probe(device_t dev);

static device_method_t ppc_puc_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,         ppc_puc_probe),
	DEVMETHOD(device_attach,        ppc_attach),

	/* bus interface */
	DEVMETHOD(bus_read_ivar,	ppc_read_ivar),
	DEVMETHOD(bus_setup_intr,	ppc_setup_intr),
	DEVMETHOD(bus_teardown_intr,	ppc_teardown_intr),
	DEVMETHOD(bus_alloc_resource,   bus_generic_alloc_resource),

	/* ppbus interface */
	DEVMETHOD(ppbus_io,		ppc_io),
	DEVMETHOD(ppbus_exec_microseq,	ppc_exec_microseq),
	DEVMETHOD(ppbus_reset_epp,	ppc_reset_epp),
	DEVMETHOD(ppbus_setmode,	ppc_setmode),
	DEVMETHOD(ppbus_ecp_sync,	ppc_ecp_sync),
	DEVMETHOD(ppbus_read,		ppc_read),
	DEVMETHOD(ppbus_write,		ppc_write),

        { 0, 0 }
  };
  
static driver_t ppc_puc_driver = {
	"ppc",
	ppc_puc_methods,
	sizeof(struct ppc_data),
};

static int
ppc_puc_probe(dev)
	device_t	dev;
{
	device_set_desc(dev, "Parallel port");
	return (ppc_probe(dev));
}

DRIVER_MODULE(ppc, puc, ppc_puc_driver, ppc_devclass, 0, 0);
