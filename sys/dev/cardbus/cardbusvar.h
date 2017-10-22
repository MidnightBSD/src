/*-
 * Copyright (c) 2000,2001 Jonathan Chen.
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
 * $FreeBSD: release/7.0.0/sys/dev/cardbus/cardbusvar.h 153862 2005-12-29 23:41:29Z imp $
 */

/*
 * Structure definitions for the Cardbus Bus driver
 */
struct cardbus_devinfo
{
	struct pci_devinfo pci;
	uint8_t        mprefetchable; /* bit mask of prefetchable BARs */
	uint8_t        mbelow1mb; /* bit mask of BARs which require below 1Mb */
	uint8_t        ibelow1mb; /* bit mask of BARs which require below 1Mb */
	uint16_t	mfrid;		/* manufacturer id */
	uint16_t	prodid;		/* product id */
	u_int		funcid;		/* function id */
	union {
		struct {
			uint8_t	nid[6];		/* MAC address */
		} lan;
	} funce;
	uint32_t	fepresent;	/* bit mask of funce values present */
};

struct cis_buffer
{
	size_t	len;			/* Actual length of the CIS */
	uint8_t buffer[2040];		/* small enough to be 2k */
};

struct cardbus_softc 
{
	/* XXX need mutex XXX */
	device_t	sc_dev;
	struct cdev 	*sc_cisdev;
	struct cis_buffer *sc_cis;
	int		sc_cis_open;
};

struct tuple_callbacks;

typedef int (tuple_cb) (device_t cbdev, device_t child, int id, int len,
		 uint8_t *tupledata, uint32_t start, uint32_t *off,
		 struct tuple_callbacks *info, void *);

struct tuple_callbacks {
	int	id;
	char	*name;
	tuple_cb *func;
};

int	cardbus_device_create(struct cardbus_softc *);
int	cardbus_device_destroy(struct cardbus_softc *);
int	cardbus_parse_cis(device_t cbdev, device_t child,
	    struct tuple_callbacks *callbacks, void *);
