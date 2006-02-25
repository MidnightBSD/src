/*-
 * Copyright (c) 1994,1995 Stefan Esser, Wolfgang StanglMeier
 * Copyright (c) 2000 Michael Smith <msmith@freebsd.org>
 * Copyright (c) 2000 BSDi
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
 * $FreeBSD: src/sys/dev/pci/pcib_private.h,v 1.6 2004/01/11 06:52:31 imp Exp $
 */

#ifndef __PCIB_PRIVATE_H__
#define	__PCIB_PRIVATE_H__

/*
 * Export portions of generic PCI:PCI bridge support so that it can be
 * used by subclasses.
 */

/*
 * Bridge-specific data.
 */
struct pcib_softc 
{
    device_t	dev;
    uint32_t	flags;		/* flags */
#define PCIB_SUBTRACTIVE	0x1
    uint16_t	command;	/* command register */
    uint8_t	secbus;		/* secondary bus number */
    uint8_t	subbus;		/* subordinate bus number */
    pci_addr_t	pmembase;	/* base address of prefetchable memory */
    pci_addr_t	pmemlimit;	/* topmost address of prefetchable memory */
    pci_addr_t	membase;	/* base address of memory window */
    pci_addr_t	memlimit;	/* topmost address of memory window */
    uint32_t	iobase;		/* base address of port window */
    uint32_t	iolimit;	/* topmost address of port window */
    uint16_t	secstat;	/* secondary bus status register */
    uint16_t	bridgectl;	/* bridge control register */
    uint8_t	seclat;		/* secondary bus latency timer */
};

typedef uint32_t pci_read_config_fn(int b, int s, int f, int reg, int width);

int		host_pcib_get_busno(pci_read_config_fn read_config, int bus,
    int slot, int func, uint8_t *busnum);
int		pcib_attach(device_t dev);
void		pcib_attach_common(device_t dev);
int		pcib_read_ivar(device_t dev, device_t child, int which, uintptr_t *result);
int		pcib_write_ivar(device_t dev, device_t child, int which, uintptr_t value);
struct resource *pcib_alloc_resource(device_t dev, device_t child, int type, int *rid, 
					    u_long start, u_long end, u_long count, u_int flags);
int		pcib_maxslots(device_t dev);
uint32_t	pcib_read_config(device_t dev, int b, int s, int f, int reg, int width);
void		pcib_write_config(device_t dev, int b, int s, int f, int reg, uint32_t val, int width);
int		pcib_route_interrupt(device_t pcib, device_t dev, int pin);

extern devclass_t pcib_devclass;

#endif
