/*-
 * Copyright 2003 by Peter Grehan. All rights reserved.
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
 * $FreeBSD: release/7.0.0/sys/powerpc/powermac/gracklevar.h 139825 2005-01-07 02:29:27Z imp $
 */

#ifndef	_POWERPC_POWERMAC_GRACKLEVAR_H_
#define	_POWERPC_POWERMAC_GRACKLEVAR_H_

struct grackle_range {
	u_int32_t	pci_hi;
	u_int32_t	pci_mid;
	u_int32_t	pci_lo;
	u_int32_t	pci_iospace;
	u_int32_t	size_hi;
	u_int32_t	size_lo;
};

struct grackle_softc {
	device_t		sc_dev;
	phandle_t		sc_node;
	vm_offset_t		sc_addr;
	vm_offset_t		sc_data;
	int			sc_bus;
	struct			grackle_range sc_range[6];
	int			sc_nrange;
	int			sc_iostart;
	struct			rman sc_io_rman;
	struct			rman sc_mem_rman;
	bus_space_tag_t		sc_memt;
	bus_dma_tag_t		sc_dmat;
};

/*
 * Apple uses address map B for the MPC106
 */
#define GRACKLE_ADDR	0xFEC00000
#define GRACKLE_DATA	0xFEE00000

/*
 * The high bit of the config word is 'Enable'. This should always be
 * set.
 */
#define GRACKLE_CFG_ENABLE	0x80000000

#endif  /* _POWERPC_POWERMAC_GRACKLEVAR_H_ */
