/*	$NetBSD: pucvar.h,v 1.2 1999/02/06 06:29:54 cgd Exp $	*/
/*	$FreeBSD: src/sys/dev/puc/pucvar.h,v 1.15 2005/01/11 06:24:40 imp Exp $ */

/*-
 * Copyright (c) 2002 JF Hay.  All rights reserved.
 * Copyright (c) 2000 M. Warner Losh.  All rights reserved.
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

/*-
 * Copyright (c) 1998, 1999 Christopher G. Demetriou.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

/*
 * Exported (or conveniently located) PCI "universal" communications card
 * software structures.
 *
 * Author: Christopher G. Demetriou, May 14, 1998.
 */

#define	PUC_MAX_PORTS		16

struct puc_softc;
typedef int puc_init_t(struct puc_softc *sc);
struct puc_device_description {
	const char	*name;
	uint32_t	rval[4];
	uint32_t	rmask[4];
	struct {
		int	type;
		int	bar;
		int	offset;
		u_int	serialfreq;
		u_int	flags;
		int	regshft;
	} ports[PUC_MAX_PORTS];
	uint32_t	ilr_type;
	uint32_t	ilr_offset[2];
	puc_init_t	*init;
};

#define	PUC_REG_VEND		0
#define	PUC_REG_PROD		1
#define	PUC_REG_SVEND		2
#define	PUC_REG_SPROD		3

#define	PUC_PORT_TYPE_NONE	0
#define	PUC_PORT_TYPE_COM	1
#define	PUC_PORT_TYPE_LPT	2
#define	PUC_PORT_TYPE_UART	3

/* UART subtypes. */
#define	PUC_PORT_SUBTYPE_MASK	(~0xff)
#define	PUC_PORT_UART_NS8250	(0<<8)
#define	PUC_PORT_UART_SAB82532	(1<<8)
#define	PUC_PORT_UART_Z8530	(2<<8)

/* Interrupt Latch Register (ILR) types */
#define	PUC_ILR_TYPE_NONE	0
#define	PUC_ILR_TYPE_DIGI	1

#define	PUC_FLAGS_MEMORY	0x0001		/* Use memory mapped I/O. */
#define	PUC_FLAGS_ALTRES	0x0002		/* Use alternate I/O type. */

#define	PUC_PORT_VALID(desc, port) \
  ((port) < PUC_MAX_PORTS && (desc).ports[(port)].type != PUC_PORT_TYPE_NONE)

#define PUC_MAX_BAR		6

enum puc_device_ivars {
	PUC_IVAR_FREQ,
	PUC_IVAR_SUBTYPE,
	PUC_IVAR_REGSHFT,
	PUC_IVAR_PORT
};

#ifdef PUC_ENTRAILS
int puc_attach(device_t dev, const struct puc_device_description *desc);
extern devclass_t puc_devclass;
struct resource *puc_alloc_resource(device_t, device_t, int, int *,
    u_long, u_long, u_long, u_int);
int puc_release_resource(device_t, device_t, int, int, struct resource *);
int puc_get_resource(device_t, device_t, int, int, u_long *, u_long *);
int puc_read_ivar(device_t, device_t, int, uintptr_t *);
int puc_setup_intr(device_t, device_t, struct resource *, int,
    void (*)(void *), void *, void **);
int puc_teardown_intr(device_t, device_t, struct resource *,
    void *);

struct puc_softc {
	struct puc_device_description sc_desc;

	/* card-global dynamic data */
	int			fastintr;
	int			barmuxed;
	int			irqrid;
	struct resource		*irqres;
	void			*intr_cookie;
	int			ilr_enabled;
	bus_space_tag_t		ilr_st;
	bus_space_handle_t	ilr_sh;

	struct {
		int		used;
		int 		bar;
		int		type;	/* SYS_RES_IOPORT or SYS_RES_MEMORY. */
		struct resource	*res;
	} sc_bar_mappings[PUC_MAX_BAR];

	/* per-port dynamic data */
        struct {
		struct device	*dev;
		/* filled in by bus_setup_intr() */
		void		(*ihand)(void *);
		void		*ihandarg;
        } sc_ports[PUC_MAX_PORTS];
};

#endif /* PUC_ENTRAILS */
