/*	$NetBSD: ohci.c,v 1.138 2003/02/08 03:32:50 ichiro Exp $	*/

/* Also, already ported:
 *	$NetBSD: ohci.c,v 1.140 2003/05/13 04:42:00 gson Exp $
 *	$NetBSD: ohci.c,v 1.141 2003/09/10 20:08:29 mycroft Exp $
 *	$NetBSD: ohci.c,v 1.142 2003/10/11 03:04:26 toshii Exp $
 *	$NetBSD: ohci.c,v 1.143 2003/10/18 04:50:35 simonb Exp $
 *	$NetBSD: ohci.c,v 1.144 2003/11/23 19:18:06 augustss Exp $
 *	$NetBSD: ohci.c,v 1.145 2003/11/23 19:20:25 augustss Exp $
 *	$NetBSD: ohci.c,v 1.146 2003/12/29 08:17:10 toshii Exp $
 *	$NetBSD: ohci.c,v 1.147 2004/06/22 07:20:35 mycroft Exp $
 *	$NetBSD: ohci.c,v 1.148 2004/06/22 18:27:46 mycroft Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/7.0.0/sys/dev/usb/ohci.c 170960 2007-06-20 05:11:37Z imp $");

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * USB Open Host Controller driver.
 *
 * OHCI spec: http://www.compaq.com/productinfo/development/openhci.html
 * USB spec: http://www.usb.org/developers/docs/usbspec.zip
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/endian.h>
#include <sys/module.h>
#include <sys/bus.h>
#if defined(DIAGNOSTIC) && defined(__i386__) && defined(__FreeBSD__)
#include <machine/cpu.h>
#endif
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/sysctl.h>

#include <machine/bus.h>
#include <machine/endian.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>
#include <dev/usb/usb_quirks.h>

#include <dev/usb/ohcireg.h>
#include <dev/usb/ohcivar.h>

#define delay(d)                DELAY(d)

#ifdef USB_DEBUG
#define DPRINTF(x)	if (ohcidebug) printf x
#define DPRINTFN(n,x)	if (ohcidebug>(n)) printf x
int ohcidebug = 0;
SYSCTL_NODE(_hw_usb, OID_AUTO, ohci, CTLFLAG_RW, 0, "USB ohci");
SYSCTL_INT(_hw_usb_ohci, OID_AUTO, debug, CTLFLAG_RW,
	   &ohcidebug, 0, "ohci debug level");
#define bitmask_snprintf(q,f,b,l) snprintf((b), (l), "%b", (q), (f))
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

struct ohci_pipe;

static ohci_soft_ed_t  *ohci_alloc_sed(ohci_softc_t *);
static void		ohci_free_sed(ohci_softc_t *, ohci_soft_ed_t *);

static ohci_soft_td_t  *ohci_alloc_std(ohci_softc_t *);
static void		ohci_free_std(ohci_softc_t *, ohci_soft_td_t *);

static ohci_soft_itd_t *ohci_alloc_sitd(ohci_softc_t *);
static void		ohci_free_sitd(ohci_softc_t *,ohci_soft_itd_t *);

#if 0
static void		ohci_free_std_chain(ohci_softc_t *, ohci_soft_td_t *,
					    ohci_soft_td_t *);
#endif
static usbd_status	ohci_alloc_std_chain(struct ohci_pipe *,
			    ohci_softc_t *, int, int, usbd_xfer_handle,
			    ohci_soft_td_t *, ohci_soft_td_t **);

#if defined(__NetBSD__) || defined(__OpenBSD__)
static void		ohci_shutdown(void *v);
static void		ohci_power(int, void *);
#endif
static usbd_status	ohci_open(usbd_pipe_handle);
static void		ohci_poll(struct usbd_bus *);
static void		ohci_softintr(void *);
static void		ohci_waitintr(ohci_softc_t *, usbd_xfer_handle);
static void		ohci_add_done(ohci_softc_t *, ohci_physaddr_t);
static void		ohci_rhsc(ohci_softc_t *, usbd_xfer_handle);

static usbd_status	ohci_device_request(usbd_xfer_handle xfer);
static void		ohci_add_ed(ohci_soft_ed_t *, ohci_soft_ed_t *);
static void		ohci_rem_ed(ohci_soft_ed_t *, ohci_soft_ed_t *);
static void		ohci_hash_add_td(ohci_softc_t *, ohci_soft_td_t *);
static void		ohci_hash_rem_td(ohci_softc_t *, ohci_soft_td_t *);
static ohci_soft_td_t  *ohci_hash_find_td(ohci_softc_t *, ohci_physaddr_t);
static void		ohci_hash_add_itd(ohci_softc_t *, ohci_soft_itd_t *);
static void		ohci_hash_rem_itd(ohci_softc_t *, ohci_soft_itd_t *);
static ohci_soft_itd_t *ohci_hash_find_itd(ohci_softc_t *, ohci_physaddr_t);

static usbd_status	ohci_setup_isoc(usbd_pipe_handle pipe);
static void		ohci_device_isoc_enter(usbd_xfer_handle);

static usbd_status	ohci_allocm(struct usbd_bus *, usb_dma_t *, u_int32_t);
static void		ohci_freem(struct usbd_bus *, usb_dma_t *);

static usbd_xfer_handle	ohci_allocx(struct usbd_bus *);
static void		ohci_freex(struct usbd_bus *, usbd_xfer_handle);

static usbd_status	ohci_root_ctrl_transfer(usbd_xfer_handle);
static usbd_status	ohci_root_ctrl_start(usbd_xfer_handle);
static void		ohci_root_ctrl_abort(usbd_xfer_handle);
static void		ohci_root_ctrl_close(usbd_pipe_handle);
static void		ohci_root_ctrl_done(usbd_xfer_handle);

static usbd_status	ohci_root_intr_transfer(usbd_xfer_handle);
static usbd_status	ohci_root_intr_start(usbd_xfer_handle);
static void		ohci_root_intr_abort(usbd_xfer_handle);
static void		ohci_root_intr_close(usbd_pipe_handle);
static void		ohci_root_intr_done(usbd_xfer_handle);

static usbd_status	ohci_device_ctrl_transfer(usbd_xfer_handle);
static usbd_status	ohci_device_ctrl_start(usbd_xfer_handle);
static void		ohci_device_ctrl_abort(usbd_xfer_handle);
static void		ohci_device_ctrl_close(usbd_pipe_handle);
static void		ohci_device_ctrl_done(usbd_xfer_handle);

static usbd_status	ohci_device_bulk_transfer(usbd_xfer_handle);
static usbd_status	ohci_device_bulk_start(usbd_xfer_handle);
static void		ohci_device_bulk_abort(usbd_xfer_handle);
static void		ohci_device_bulk_close(usbd_pipe_handle);
static void		ohci_device_bulk_done(usbd_xfer_handle);

static usbd_status	ohci_device_intr_transfer(usbd_xfer_handle);
static usbd_status	ohci_device_intr_start(usbd_xfer_handle);
static void		ohci_device_intr_abort(usbd_xfer_handle);
static void		ohci_device_intr_close(usbd_pipe_handle);
static void		ohci_device_intr_done(usbd_xfer_handle);

static usbd_status	ohci_device_isoc_transfer(usbd_xfer_handle);
static usbd_status	ohci_device_isoc_start(usbd_xfer_handle);
static void		ohci_device_isoc_abort(usbd_xfer_handle);
static void		ohci_device_isoc_close(usbd_pipe_handle);
static void		ohci_device_isoc_done(usbd_xfer_handle);

static usbd_status	ohci_device_setintr(ohci_softc_t *sc,
			    struct ohci_pipe *pipe, int ival);
static usbd_status	ohci_device_intr_insert(ohci_softc_t *sc,
			    usbd_xfer_handle xfer);

static int		ohci_str(usb_string_descriptor_t *, int, const char *);

static void		ohci_timeout(void *);
static void		ohci_timeout_task(void *);
static void		ohci_rhsc_able(ohci_softc_t *, int);
static void		ohci_rhsc_enable(void *);

static void		ohci_close_pipe(usbd_pipe_handle, ohci_soft_ed_t *);
static void		ohci_abort_xfer(usbd_xfer_handle, usbd_status);

static void		ohci_device_clear_toggle(usbd_pipe_handle pipe);
static void		ohci_noop(usbd_pipe_handle pipe);

static usbd_status ohci_controller_init(ohci_softc_t *sc);

#ifdef USB_DEBUG
static void		ohci_dumpregs(ohci_softc_t *);
static void		ohci_dump_tds(ohci_soft_td_t *);
static void		ohci_dump_td(ohci_soft_td_t *);
static void		ohci_dump_ed(ohci_soft_ed_t *);
static void		ohci_dump_itd(ohci_soft_itd_t *);
static void		ohci_dump_itds(ohci_soft_itd_t *);
#endif

#define OBARR(sc) bus_space_barrier((sc)->iot, (sc)->ioh, 0, (sc)->sc_size, \
			BUS_SPACE_BARRIER_READ|BUS_SPACE_BARRIER_WRITE)
#define OWRITE1(sc, r, x) \
 do { OBARR(sc); bus_space_write_1((sc)->iot, (sc)->ioh, (r), (x)); } while (0)
#define OWRITE2(sc, r, x) \
 do { OBARR(sc); bus_space_write_2((sc)->iot, (sc)->ioh, (r), (x)); } while (0)
#define OWRITE4(sc, r, x) \
 do { OBARR(sc); bus_space_write_4((sc)->iot, (sc)->ioh, (r), (x)); } while (0)
#define OREAD1(sc, r) (OBARR(sc), bus_space_read_1((sc)->iot, (sc)->ioh, (r)))
#define OREAD2(sc, r) (OBARR(sc), bus_space_read_2((sc)->iot, (sc)->ioh, (r)))
#define OREAD4(sc, r) (OBARR(sc), bus_space_read_4((sc)->iot, (sc)->ioh, (r)))

/* Reverse the bits in a value 0 .. 31 */
static u_int8_t revbits[OHCI_NO_INTRS] =
  { 0x00, 0x10, 0x08, 0x18, 0x04, 0x14, 0x0c, 0x1c,
    0x02, 0x12, 0x0a, 0x1a, 0x06, 0x16, 0x0e, 0x1e,
    0x01, 0x11, 0x09, 0x19, 0x05, 0x15, 0x0d, 0x1d,
    0x03, 0x13, 0x0b, 0x1b, 0x07, 0x17, 0x0f, 0x1f };

struct ohci_pipe {
	struct usbd_pipe pipe;
	ohci_soft_ed_t *sed;
	u_int32_t aborting;
	union {
		ohci_soft_td_t *td;
		ohci_soft_itd_t *itd;
	} tail;
	/* Info needed for different pipe kinds. */
	union {
		/* Control pipe */
		struct {
			usb_dma_t reqdma;
			u_int length;
			ohci_soft_td_t *setup, *data, *stat;
		} ctl;
		/* Interrupt pipe */
		struct {
			int nslots;
			int pos;
		} intr;
		/* Bulk pipe */
		struct {
			u_int length;
			int isread;
		} bulk;
		/* Iso pipe */
		struct iso {
			int next, inuse;
		} iso;
	} u;
};

#define OHCI_INTR_ENDPT 1

static struct usbd_bus_methods ohci_bus_methods = {
	ohci_open,
	ohci_softintr,
	ohci_poll,
	ohci_allocm,
	ohci_freem,
	ohci_allocx,
	ohci_freex,
};

static struct usbd_pipe_methods ohci_root_ctrl_methods = {
	ohci_root_ctrl_transfer,
	ohci_root_ctrl_start,
	ohci_root_ctrl_abort,
	ohci_root_ctrl_close,
	ohci_noop,
	ohci_root_ctrl_done,
};

static struct usbd_pipe_methods ohci_root_intr_methods = {
	ohci_root_intr_transfer,
	ohci_root_intr_start,
	ohci_root_intr_abort,
	ohci_root_intr_close,
	ohci_noop,
	ohci_root_intr_done,
};

static struct usbd_pipe_methods ohci_device_ctrl_methods = {
	ohci_device_ctrl_transfer,
	ohci_device_ctrl_start,
	ohci_device_ctrl_abort,
	ohci_device_ctrl_close,
	ohci_noop,
	ohci_device_ctrl_done,
};

static struct usbd_pipe_methods ohci_device_intr_methods = {
	ohci_device_intr_transfer,
	ohci_device_intr_start,
	ohci_device_intr_abort,
	ohci_device_intr_close,
	ohci_device_clear_toggle,
	ohci_device_intr_done,
};

static struct usbd_pipe_methods ohci_device_bulk_methods = {
	ohci_device_bulk_transfer,
	ohci_device_bulk_start,
	ohci_device_bulk_abort,
	ohci_device_bulk_close,
	ohci_device_clear_toggle,
	ohci_device_bulk_done,
};

static struct usbd_pipe_methods ohci_device_isoc_methods = {
	ohci_device_isoc_transfer,
	ohci_device_isoc_start,
	ohci_device_isoc_abort,
	ohci_device_isoc_close,
	ohci_noop,
	ohci_device_isoc_done,
};

int
ohci_detach(struct ohci_softc *sc, int flags)
{
	int i, rv = 0;

	sc->sc_dying = 1;
	callout_stop(&sc->sc_tmo_rhsc);

#if defined(__NetBSD__) || defined(__OpenBSD__)
	powerhook_disestablish(sc->sc_powerhook);
	shutdownhook_disestablish(sc->sc_shutdownhook);
#endif

	OWRITE4(sc, OHCI_INTERRUPT_DISABLE, OHCI_ALL_INTRS);
	OWRITE4(sc, OHCI_CONTROL, OHCI_HCFS_RESET);

	usb_delay_ms(&sc->sc_bus, 300); /* XXX let stray task complete */

	for (i = 0; i < OHCI_NO_EDS; i++)
		ohci_free_sed(sc, sc->sc_eds[i]);
	ohci_free_sed(sc, sc->sc_isoc_head);
	ohci_free_sed(sc, sc->sc_bulk_head);
	ohci_free_sed(sc, sc->sc_ctrl_head);
	usb_freemem(&sc->sc_bus, &sc->sc_hccadma);

	return (rv);
}

ohci_soft_ed_t *
ohci_alloc_sed(ohci_softc_t *sc)
{
	ohci_soft_ed_t *sed;
	usbd_status err;
	int i, offs;
	usb_dma_t dma;

	if (sc->sc_freeeds == NULL) {
		DPRINTFN(2, ("ohci_alloc_sed: allocating chunk\n"));
		err = usb_allocmem(&sc->sc_bus, OHCI_SED_SIZE * OHCI_SED_CHUNK,
			  OHCI_ED_ALIGN, &dma);
		if (err)
			return (NULL);
		for(i = 0; i < OHCI_SED_CHUNK; i++) {
			offs = i * OHCI_SED_SIZE;
			sed = KERNADDR(&dma, offs);
			sed->physaddr = DMAADDR(&dma, offs);
			sed->next = sc->sc_freeeds;
			sc->sc_freeeds = sed;
		}
	}
	sed = sc->sc_freeeds;
	sc->sc_freeeds = sed->next;
	memset(&sed->ed, 0, sizeof(ohci_ed_t));
	sed->next = 0;
	return (sed);
}

void
ohci_free_sed(ohci_softc_t *sc, ohci_soft_ed_t *sed)
{
	sed->next = sc->sc_freeeds;
	sc->sc_freeeds = sed;
}

ohci_soft_td_t *
ohci_alloc_std(ohci_softc_t *sc)
{
	ohci_soft_td_t *std;
	usbd_status err;
	int i, offs;
	usb_dma_t dma;
	int s;

	if (sc->sc_freetds == NULL) {
		DPRINTFN(2, ("ohci_alloc_std: allocating chunk\n"));
		err = usb_allocmem(&sc->sc_bus, OHCI_STD_SIZE * OHCI_STD_CHUNK,
			  OHCI_TD_ALIGN, &dma);
		if (err)
			return (NULL);
		s = splusb();
		for(i = 0; i < OHCI_STD_CHUNK; i++) {
			offs = i * OHCI_STD_SIZE;
			std = KERNADDR(&dma, offs);
			std->physaddr = DMAADDR(&dma, offs);
			std->nexttd = sc->sc_freetds;
			sc->sc_freetds = std;
		}
		splx(s);
	}

	s = splusb();
	std = sc->sc_freetds;
	sc->sc_freetds = std->nexttd;
	memset(&std->td, 0, sizeof(ohci_td_t));
	std->nexttd = NULL;
	std->xfer = NULL;
	ohci_hash_add_td(sc, std);
	splx(s);

	return (std);
}

void
ohci_free_std(ohci_softc_t *sc, ohci_soft_td_t *std)
{
	int s;

	s = splusb();
	ohci_hash_rem_td(sc, std);
	std->nexttd = sc->sc_freetds;
	sc->sc_freetds = std;
	splx(s);
}

usbd_status
ohci_alloc_std_chain(struct ohci_pipe *opipe, ohci_softc_t *sc,
		     int alen, int rd, usbd_xfer_handle xfer,
		     ohci_soft_td_t *sp, ohci_soft_td_t **ep)
{
	ohci_soft_td_t *next, *cur, *end;
	ohci_physaddr_t dataphys, physend;
	u_int32_t tdflags;
	int offset = 0;
	int len, maxp, curlen, curlen2, seg, segoff;
	struct usb_dma_mapping *dma = &xfer->dmamap;
	u_int16_t flags = xfer->flags;

	DPRINTFN(alen < 4096,("ohci_alloc_std_chain: start len=%d\n", alen));

	len = alen;
	cur = sp;
	end = NULL;

	maxp = UGETW(opipe->pipe.endpoint->edesc->wMaxPacketSize);
	tdflags = htole32(
	    (rd ? OHCI_TD_IN : OHCI_TD_OUT) |
	    (flags & USBD_SHORT_XFER_OK ? OHCI_TD_R : 0) |
	    OHCI_TD_NOCC | OHCI_TD_TOGGLE_CARRY | OHCI_TD_SET_DI(6));

	seg = 0;
	segoff = 0;
	while (len > 0) {
		next = ohci_alloc_std(sc);
		if (next == NULL)
			goto nomem;

		/*
		 * The OHCI hardware can handle at most one 4k crossing.
		 * The OHCI spec says: If during the data transfer the buffer
		 * address contained in the HC's working copy of
		 * CurrentBufferPointer crosses a 4K boundary, the upper 20
		 * bits of Buffer End are copied to the working value of
		 * CurrentBufferPointer causing the next buffer address to
		 * be the 0th byte in the same 4K page that contains the
		 * last byte of the buffer (the 4K boundary crossing may
		 * occur within a data packet transfer.)
		 */
		KASSERT(seg < dma->nsegs, ("ohci_alloc_std_chain: overrun"));
		dataphys = dma->segs[seg].ds_addr + segoff;
		curlen = dma->segs[seg].ds_len - segoff;
		if (curlen > len)
			curlen = len;
		physend = dataphys + curlen - 1;
		if (OHCI_PAGE(dataphys) != OHCI_PAGE(physend)) {
			/* Truncate to two OHCI pages if there are more. */
			if (curlen > 2 * OHCI_PAGE_SIZE -
			    OHCI_PAGE_OFFSET(dataphys))
				curlen = 2 * OHCI_PAGE_SIZE -
				    OHCI_PAGE_OFFSET(dataphys);
			if (curlen < len)
				curlen -= curlen % maxp;
			physend = dataphys + curlen - 1;
		} else if (OHCI_PAGE_OFFSET(physend + 1) == 0 && curlen < len &&
		    curlen + segoff == dma->segs[seg].ds_len) {
			/* We can possibly include another segment. */
			KASSERT(seg + 1 < dma->nsegs,
			    ("ohci_alloc_std_chain: overrun2"));
			seg++;

			/* Determine how much of the second segment to use. */
			curlen2 = dma->segs[seg].ds_len;
			if (curlen + curlen2 > len)
				curlen2 = len - curlen;
			if (OHCI_PAGE(dma->segs[seg].ds_addr) !=
			    OHCI_PAGE(dma->segs[seg].ds_addr + curlen2 - 1))
				curlen2 = OHCI_PAGE_SIZE -
				    OHCI_PAGE_OFFSET(dma->segs[seg].ds_addr);
			if (curlen + curlen2 < len)
				curlen2 -= (curlen + curlen2) % maxp;

			if (curlen2 > 0) {
				/* We can include a second segment */
				segoff = curlen2;
				physend = dma->segs[seg].ds_addr + curlen2 - 1;
				curlen += curlen2;
			} else {
				/* Second segment not usable now. */
				seg--;
				segoff += curlen;
			}
		} else {
			/* Simple case where there is just one OHCI page. */
			segoff += curlen;
		}
		if (curlen == 0 && len != 0) {
			/*
			 * A maxp length packet would need to be split.
			 * This shouldn't be possible if PAGE_SIZE >= 4k
			 * and the buffer is contiguous in virtual memory.
			 */
			panic("ohci_alloc_std_chain: XXX need to copy");
		}
		if (segoff >= dma->segs[seg].ds_len) {
			KASSERT(segoff == dma->segs[seg].ds_len,
			    ("ohci_alloc_std_chain: overlap"));
			seg++;
			segoff = 0;
		}
		DPRINTFN(4,("ohci_alloc_std_chain: dataphys=0x%08x "
			    "len=%d curlen=%d\n",
			    dataphys, len, curlen));
		len -= curlen;

		cur->td.td_flags = tdflags;
		cur->td.td_cbp = htole32(dataphys);
		cur->nexttd = next;
		cur->td.td_nexttd = htole32(next->physaddr);
		cur->td.td_be = htole32(physend);
		cur->len = curlen;
		cur->flags = OHCI_ADD_LEN;
		cur->xfer = xfer;
		DPRINTFN(10,("ohci_alloc_std_chain: cbp=0x%08x be=0x%08x\n",
			    dataphys, dataphys + curlen - 1));
		if (len < 0)
			panic("Length went negative: %d curlen %d dma %p offset %08x", len, curlen, dma, (int)0);

		DPRINTFN(10,("ohci_alloc_std_chain: extend chain\n"));
		offset += curlen;
		end = cur;
		cur = next;
	}
	if (((flags & USBD_FORCE_SHORT_XFER) || alen == 0) &&
	    alen % UGETW(opipe->pipe.endpoint->edesc->wMaxPacketSize) == 0) {
		/* Force a 0 length transfer at the end. */
		next = ohci_alloc_std(sc);
		if (next == NULL)
			goto nomem;

		cur->td.td_flags = tdflags;
		cur->td.td_cbp = 0; /* indicate 0 length packet */
		cur->nexttd = next;
		cur->td.td_nexttd = htole32(next->physaddr);
		cur->td.td_be = ~0;
		cur->len = 0;
		cur->flags = 0;
		cur->xfer = xfer;
		DPRINTFN(2,("ohci_alloc_std_chain: add 0 xfer\n"));
		end = cur;
	}
	*ep = end;

	return (USBD_NORMAL_COMPLETION);

 nomem:
	/* XXX free chain */
	return (USBD_NOMEM);
}

#if 0
static void
ohci_free_std_chain(ohci_softc_t *sc, ohci_soft_td_t *std,
		    ohci_soft_td_t *stdend)
{
	ohci_soft_td_t *p;

	for (; std != stdend; std = p) {
		p = std->nexttd;
		ohci_free_std(sc, std);
	}
}
#endif

ohci_soft_itd_t *
ohci_alloc_sitd(ohci_softc_t *sc)
{
	ohci_soft_itd_t *sitd;
	usbd_status err;
	int i, s, offs;
	usb_dma_t dma;

	if (sc->sc_freeitds == NULL) {
		DPRINTFN(2, ("ohci_alloc_sitd: allocating chunk\n"));
		err = usb_allocmem(&sc->sc_bus, OHCI_SITD_SIZE * OHCI_SITD_CHUNK,
			  OHCI_ITD_ALIGN, &dma);
		if (err)
			return (NULL);
		s = splusb();
		for(i = 0; i < OHCI_SITD_CHUNK; i++) {
			offs = i * OHCI_SITD_SIZE;
			sitd = KERNADDR(&dma, offs);
			sitd->physaddr = DMAADDR(&dma, offs);
			sitd->nextitd = sc->sc_freeitds;
			sc->sc_freeitds = sitd;
		}
		splx(s);
	}

	s = splusb();
	sitd = sc->sc_freeitds;
	sc->sc_freeitds = sitd->nextitd;
	memset(&sitd->itd, 0, sizeof(ohci_itd_t));
	sitd->nextitd = NULL;
	sitd->xfer = NULL;
	ohci_hash_add_itd(sc, sitd);
	splx(s);

#ifdef DIAGNOSTIC
	sitd->isdone = 0;
#endif

	return (sitd);
}

void
ohci_free_sitd(ohci_softc_t *sc, ohci_soft_itd_t *sitd)
{
	int s;

	DPRINTFN(10,("ohci_free_sitd: sitd=%p\n", sitd));

#ifdef DIAGNOSTIC
	if (!sitd->isdone) {
		panic("ohci_free_sitd: sitd=%p not done", sitd);
		return;
	}
	/* Warn double free */
	sitd->isdone = 0;
#endif

	s = splusb();
	ohci_hash_rem_itd(sc, sitd);
	sitd->nextitd = sc->sc_freeitds;
	sc->sc_freeitds = sitd;
	splx(s);
}

usbd_status
ohci_init(ohci_softc_t *sc)
{
	ohci_soft_ed_t *sed, *psed;
	usbd_status err;
	int i;
	u_int32_t rev;

	DPRINTF(("ohci_init: start\n"));
	printf("%s:", device_get_nameunit(sc->sc_bus.bdev));
	rev = OREAD4(sc, OHCI_REVISION);
	printf(" OHCI version %d.%d%s\n", OHCI_REV_HI(rev), OHCI_REV_LO(rev),
	       OHCI_REV_LEGACY(rev) ? ", legacy support" : "");

	if (OHCI_REV_HI(rev) != 1 || OHCI_REV_LO(rev) != 0) {
		printf("%s: unsupported OHCI revision\n",
		       device_get_nameunit(sc->sc_bus.bdev));
		sc->sc_bus.usbrev = USBREV_UNKNOWN;
		return (USBD_INVAL);
	}
	sc->sc_bus.usbrev = USBREV_1_0;

	for (i = 0; i < OHCI_HASH_SIZE; i++)
		LIST_INIT(&sc->sc_hash_tds[i]);
	for (i = 0; i < OHCI_HASH_SIZE; i++)
		LIST_INIT(&sc->sc_hash_itds[i]);

	STAILQ_INIT(&sc->sc_free_xfers);

	/* XXX determine alignment by R/W */
	/* Allocate the HCCA area. */
	err = usb_allocmem(&sc->sc_bus, OHCI_HCCA_SIZE,
			 OHCI_HCCA_ALIGN, &sc->sc_hccadma);
	if (err)
		return (err);
	sc->sc_hcca = KERNADDR(&sc->sc_hccadma, 0);
	memset(sc->sc_hcca, 0, OHCI_HCCA_SIZE);

	sc->sc_eintrs = OHCI_NORMAL_INTRS;

	/* Allocate dummy ED that starts the control list. */
	sc->sc_ctrl_head = ohci_alloc_sed(sc);
	if (sc->sc_ctrl_head == NULL) {
		err = USBD_NOMEM;
		goto bad1;
	}
	sc->sc_ctrl_head->ed.ed_flags |= htole32(OHCI_ED_SKIP);

	/* Allocate dummy ED that starts the bulk list. */
	sc->sc_bulk_head = ohci_alloc_sed(sc);
	if (sc->sc_bulk_head == NULL) {
		err = USBD_NOMEM;
		goto bad2;
	}
	sc->sc_bulk_head->ed.ed_flags |= htole32(OHCI_ED_SKIP);

	/* Allocate dummy ED that starts the isochronous list. */
	sc->sc_isoc_head = ohci_alloc_sed(sc);
	if (sc->sc_isoc_head == NULL) {
		err = USBD_NOMEM;
		goto bad3;
	}
	sc->sc_isoc_head->ed.ed_flags |= htole32(OHCI_ED_SKIP);

	/* Allocate all the dummy EDs that make up the interrupt tree. */
	for (i = 0; i < OHCI_NO_EDS; i++) {
		sed = ohci_alloc_sed(sc);
		if (sed == NULL) {
			while (--i >= 0)
				ohci_free_sed(sc, sc->sc_eds[i]);
			err = USBD_NOMEM;
			goto bad4;
		}
		/* All ED fields are set to 0. */
		sc->sc_eds[i] = sed;
		sed->ed.ed_flags |= htole32(OHCI_ED_SKIP);
		if (i != 0)
			psed = sc->sc_eds[(i-1) / 2];
		else
			psed= sc->sc_isoc_head;
		sed->next = psed;
		sed->ed.ed_nexted = htole32(psed->physaddr);
	}
	/*
	 * Fill HCCA interrupt table.  The bit reversal is to get
	 * the tree set up properly to spread the interrupts.
	 */
	for (i = 0; i < OHCI_NO_INTRS; i++)
		sc->sc_hcca->hcca_interrupt_table[revbits[i]] =
		    htole32(sc->sc_eds[OHCI_NO_EDS-OHCI_NO_INTRS+i]->physaddr);

#ifdef USB_DEBUG
	if (ohcidebug > 15) {
		for (i = 0; i < OHCI_NO_EDS; i++) {
			printf("ed#%d ", i);
			ohci_dump_ed(sc->sc_eds[i]);
		}
		printf("iso ");
		ohci_dump_ed(sc->sc_isoc_head);
	}
#endif

	err = ohci_controller_init(sc);
	if (err != USBD_NORMAL_COMPLETION)
		goto bad5;

	/* Set up the bus struct. */
	sc->sc_bus.methods = &ohci_bus_methods;
	sc->sc_bus.pipe_size = sizeof(struct ohci_pipe);

#if defined(__NetBSD__) || defined(__OpenBSD__)
	sc->sc_powerhook = powerhook_establish(ohci_power, sc);
	sc->sc_shutdownhook = shutdownhook_establish(ohci_shutdown, sc);
#endif

	callout_init(&sc->sc_tmo_rhsc, 0);

	return (USBD_NORMAL_COMPLETION);

 bad5:
	for (i = 0; i < OHCI_NO_EDS; i++)
		ohci_free_sed(sc, sc->sc_eds[i]);
 bad4:
	ohci_free_sed(sc, sc->sc_isoc_head);
 bad3:
	ohci_free_sed(sc, sc->sc_bulk_head);
 bad2:
	ohci_free_sed(sc, sc->sc_ctrl_head);
 bad1:
	usb_freemem(&sc->sc_bus, &sc->sc_hccadma);
	return (err);
}

static usbd_status
ohci_controller_init(ohci_softc_t *sc)
{
	int i;
	u_int32_t s, ctl, ival, hcr, fm, per, desca;

	/* Determine in what context we are running. */
	ctl = OREAD4(sc, OHCI_CONTROL);
	if (ctl & OHCI_IR) {
		/* SMM active, request change */
		DPRINTF(("ohci_init: SMM active, request owner change\n"));
		s = OREAD4(sc, OHCI_COMMAND_STATUS);
		OWRITE4(sc, OHCI_COMMAND_STATUS, s | OHCI_OCR);
		for (i = 0; i < 100 && (ctl & OHCI_IR); i++) {
			usb_delay_ms(&sc->sc_bus, 1);
			ctl = OREAD4(sc, OHCI_CONTROL);
		}
		if ((ctl & OHCI_IR) == 0) {
			printf("%s: SMM does not respond, resetting\n",
			       device_get_nameunit(sc->sc_bus.bdev));
			OWRITE4(sc, OHCI_CONTROL, OHCI_HCFS_RESET);
			goto reset;
		}
#if 0
/* Don't bother trying to reuse the BIOS init, we'll reset it anyway. */
	} else if ((ctl & OHCI_HCFS_MASK) != OHCI_HCFS_RESET) {
		/* BIOS started controller. */
		DPRINTF(("ohci_init: BIOS active\n"));
		if ((ctl & OHCI_HCFS_MASK) != OHCI_HCFS_OPERATIONAL) {
			OWRITE4(sc, OHCI_CONTROL, OHCI_HCFS_OPERATIONAL);
			usb_delay_ms(&sc->sc_bus, USB_RESUME_DELAY);
		}
#endif
	} else {
		DPRINTF(("ohci_init: cold started\n"));
	reset:
		/* Controller was cold started. */
		usb_delay_ms(&sc->sc_bus, USB_BUS_RESET_DELAY);
	}

	/*
	 * This reset should not be necessary according to the OHCI spec, but
	 * without it some controllers do not start.
	 */
	DPRINTF(("%s: resetting\n", device_get_nameunit(sc->sc_bus.bdev)));
	OWRITE4(sc, OHCI_CONTROL, OHCI_HCFS_RESET);
	usb_delay_ms(&sc->sc_bus, USB_BUS_RESET_DELAY);

	/* We now own the host controller and the bus has been reset. */
	ival = OHCI_GET_IVAL(OREAD4(sc, OHCI_FM_INTERVAL));

	OWRITE4(sc, OHCI_COMMAND_STATUS, OHCI_HCR); /* Reset HC */
	/* Nominal time for a reset is 10 us. */
	for (i = 0; i < 10; i++) {
		delay(10);
		hcr = OREAD4(sc, OHCI_COMMAND_STATUS) & OHCI_HCR;
		if (!hcr)
			break;
	}
	if (hcr) {
		printf("%s: reset timeout\n", device_get_nameunit(sc->sc_bus.bdev));
		return (USBD_IOERROR);
	}
#ifdef USB_DEBUG
	if (ohcidebug > 15)
		ohci_dumpregs(sc);
#endif

	/* The controller is now in SUSPEND state, we have 2ms to finish. */

	/* Set up HC registers. */
	OWRITE4(sc, OHCI_HCCA, DMAADDR(&sc->sc_hccadma, 0));
	OWRITE4(sc, OHCI_CONTROL_HEAD_ED, sc->sc_ctrl_head->physaddr);
	OWRITE4(sc, OHCI_BULK_HEAD_ED, sc->sc_bulk_head->physaddr);
	/* disable all interrupts and then switch on all desired interrupts */
	OWRITE4(sc, OHCI_INTERRUPT_DISABLE, OHCI_ALL_INTRS);
	OWRITE4(sc, OHCI_INTERRUPT_ENABLE, sc->sc_eintrs | OHCI_MIE);
	/* switch on desired functional features */
	ctl = OREAD4(sc, OHCI_CONTROL);
	ctl &= ~(OHCI_CBSR_MASK | OHCI_LES | OHCI_HCFS_MASK | OHCI_IR);
	ctl |= OHCI_PLE | OHCI_IE | OHCI_CLE | OHCI_BLE |
		OHCI_RATIO_1_4 | OHCI_HCFS_OPERATIONAL;
	/* And finally start it! */
	OWRITE4(sc, OHCI_CONTROL, ctl);

	/*
	 * The controller is now OPERATIONAL.  Set a some final
	 * registers that should be set earlier, but that the
	 * controller ignores when in the SUSPEND state.
	 */
	fm = (OREAD4(sc, OHCI_FM_INTERVAL) & OHCI_FIT) ^ OHCI_FIT;
	fm |= OHCI_FSMPS(ival) | ival;
	OWRITE4(sc, OHCI_FM_INTERVAL, fm);
	per = OHCI_PERIODIC(ival); /* 90% periodic */
	OWRITE4(sc, OHCI_PERIODIC_START, per);

	/* Fiddle the No OverCurrent Protection bit to avoid chip bug. */
	desca = OREAD4(sc, OHCI_RH_DESCRIPTOR_A);
	OWRITE4(sc, OHCI_RH_DESCRIPTOR_A, desca | OHCI_NOCP);
	OWRITE4(sc, OHCI_RH_STATUS, OHCI_LPSC); /* Enable port power */
	usb_delay_ms(&sc->sc_bus, OHCI_ENABLE_POWER_DELAY);
	OWRITE4(sc, OHCI_RH_DESCRIPTOR_A, desca);

	/*
	 * The AMD756 requires a delay before re-reading the register,
	 * otherwise it will occasionally report 0 ports.
	 */
  	sc->sc_noport = 0;
 	for (i = 0; i < 10 && sc->sc_noport == 0; i++) {
 		usb_delay_ms(&sc->sc_bus, OHCI_READ_DESC_DELAY);
 		sc->sc_noport = OHCI_GET_NDP(OREAD4(sc, OHCI_RH_DESCRIPTOR_A));
 	}

#ifdef USB_DEBUG
	if (ohcidebug > 5)
		ohci_dumpregs(sc);
#endif
	return (USBD_NORMAL_COMPLETION);
}

usbd_status
ohci_allocm(struct usbd_bus *bus, usb_dma_t *dma, u_int32_t size)
{
	return (usb_allocmem(bus, size, 0, dma));
}

void
ohci_freem(struct usbd_bus *bus, usb_dma_t *dma)
{
	usb_freemem(bus, dma);
}

usbd_xfer_handle
ohci_allocx(struct usbd_bus *bus)
{
	struct ohci_softc *sc = (struct ohci_softc *)bus;
	usbd_xfer_handle xfer;

	xfer = STAILQ_FIRST(&sc->sc_free_xfers);
	if (xfer != NULL) {
		STAILQ_REMOVE_HEAD(&sc->sc_free_xfers, next);
#ifdef DIAGNOSTIC
		if (xfer->busy_free != XFER_FREE) {
			printf("ohci_allocx: xfer=%p not free, 0x%08x\n", xfer,
			       xfer->busy_free);
		}
#endif
	} else {
		xfer = malloc(sizeof(struct ohci_xfer), M_USB, M_NOWAIT);
	}
	if (xfer != NULL) {
		memset(xfer, 0, sizeof (struct ohci_xfer));
		usb_init_task(&OXFER(xfer)->abort_task, ohci_timeout_task,
		    xfer);
		OXFER(xfer)->ohci_xfer_flags = 0;
#ifdef DIAGNOSTIC
		xfer->busy_free = XFER_BUSY;
#endif
	}
	return (xfer);
}

void
ohci_freex(struct usbd_bus *bus, usbd_xfer_handle xfer)
{
	struct ohci_softc *sc = (struct ohci_softc *)bus;

#ifdef DIAGNOSTIC
	if (xfer->busy_free != XFER_BUSY) {
		printf("ohci_freex: xfer=%p not busy, 0x%08x\n", xfer,
		       xfer->busy_free);
		return;
	}
	xfer->busy_free = XFER_FREE;
#endif
	STAILQ_INSERT_HEAD(&sc->sc_free_xfers, xfer, next);
}

/*
 * Shut down the controller when the system is going down.
 */
void
ohci_shutdown(void *v)
{
	ohci_softc_t *sc = v;

	DPRINTF(("ohci_shutdown: stopping the HC\n"));
	OWRITE4(sc, OHCI_CONTROL, OHCI_HCFS_RESET);
}

/*
 * Handle suspend/resume.
 *
 * We need to switch to polling mode here, because this routine is
 * called from an intterupt context.  This is all right since we
 * are almost suspended anyway.
 */
void
ohci_power(int why, void *v)
{
	ohci_softc_t *sc = v;
	u_int32_t ctl;
	int s;

#ifdef USB_DEBUG
	DPRINTF(("ohci_power: sc=%p, why=%d\n", sc, why));
	ohci_dumpregs(sc);
#endif

	s = splhardusb();
	if (why != PWR_RESUME) {
		sc->sc_bus.use_polling++;
		ctl = OREAD4(sc, OHCI_CONTROL) & ~OHCI_HCFS_MASK;
		if (sc->sc_control == 0) {
			/*
			 * Preserve register values, in case that APM BIOS
			 * does not recover them.
			 */
			sc->sc_control = ctl;
			sc->sc_intre = OREAD4(sc, OHCI_INTERRUPT_ENABLE);
		}
		ctl |= OHCI_HCFS_SUSPEND;
		OWRITE4(sc, OHCI_CONTROL, ctl);
		usb_delay_ms(&sc->sc_bus, USB_RESUME_WAIT);
		sc->sc_bus.use_polling--;
	} else {
		sc->sc_bus.use_polling++;

		/* Some broken BIOSes never initialize Controller chip */
		ohci_controller_init(sc);

		if (sc->sc_intre)
			OWRITE4(sc, OHCI_INTERRUPT_ENABLE,
				sc->sc_intre & (OHCI_ALL_INTRS | OHCI_MIE));
		if (sc->sc_control)
			ctl = sc->sc_control;
		else
			ctl = OREAD4(sc, OHCI_CONTROL);
		ctl |= OHCI_HCFS_RESUME;
		OWRITE4(sc, OHCI_CONTROL, ctl);
		usb_delay_ms(&sc->sc_bus, USB_RESUME_DELAY);
		ctl = (ctl & ~OHCI_HCFS_MASK) | OHCI_HCFS_OPERATIONAL;
		OWRITE4(sc, OHCI_CONTROL, ctl);
		usb_delay_ms(&sc->sc_bus, USB_RESUME_RECOVERY);
		sc->sc_control = sc->sc_intre = 0;
		sc->sc_bus.use_polling--;
	}
	splx(s);
}

#ifdef USB_DEBUG
void
ohci_dumpregs(ohci_softc_t *sc)
{
	DPRINTF(("ohci_dumpregs: rev=0x%08x control=0x%08x command=0x%08x\n",
		 OREAD4(sc, OHCI_REVISION),
		 OREAD4(sc, OHCI_CONTROL),
		 OREAD4(sc, OHCI_COMMAND_STATUS)));
	DPRINTF(("               intrstat=0x%08x intre=0x%08x intrd=0x%08x\n",
		 OREAD4(sc, OHCI_INTERRUPT_STATUS),
		 OREAD4(sc, OHCI_INTERRUPT_ENABLE),
		 OREAD4(sc, OHCI_INTERRUPT_DISABLE)));
	DPRINTF(("               hcca=0x%08x percur=0x%08x ctrlhd=0x%08x\n",
		 OREAD4(sc, OHCI_HCCA),
		 OREAD4(sc, OHCI_PERIOD_CURRENT_ED),
		 OREAD4(sc, OHCI_CONTROL_HEAD_ED)));
	DPRINTF(("               ctrlcur=0x%08x bulkhd=0x%08x bulkcur=0x%08x\n",
		 OREAD4(sc, OHCI_CONTROL_CURRENT_ED),
		 OREAD4(sc, OHCI_BULK_HEAD_ED),
		 OREAD4(sc, OHCI_BULK_CURRENT_ED)));
	DPRINTF(("               done=0x%08x fmival=0x%08x fmrem=0x%08x\n",
		 OREAD4(sc, OHCI_DONE_HEAD),
		 OREAD4(sc, OHCI_FM_INTERVAL),
		 OREAD4(sc, OHCI_FM_REMAINING)));
	DPRINTF(("               fmnum=0x%08x perst=0x%08x lsthrs=0x%08x\n",
		 OREAD4(sc, OHCI_FM_NUMBER),
		 OREAD4(sc, OHCI_PERIODIC_START),
		 OREAD4(sc, OHCI_LS_THRESHOLD)));
	DPRINTF(("               desca=0x%08x descb=0x%08x stat=0x%08x\n",
		 OREAD4(sc, OHCI_RH_DESCRIPTOR_A),
		 OREAD4(sc, OHCI_RH_DESCRIPTOR_B),
		 OREAD4(sc, OHCI_RH_STATUS)));
	DPRINTF(("               port1=0x%08x port2=0x%08x\n",
		 OREAD4(sc, OHCI_RH_PORT_STATUS(1)),
		 OREAD4(sc, OHCI_RH_PORT_STATUS(2))));
	DPRINTF(("         HCCA: frame_number=0x%04x done_head=0x%08x\n",
		 le32toh(sc->sc_hcca->hcca_frame_number),
		 le32toh(sc->sc_hcca->hcca_done_head)));
}
#endif

static int ohci_intr1(ohci_softc_t *);

void
ohci_intr(void *p)
{
	ohci_softc_t *sc = p;

	if (sc == NULL || sc->sc_dying)
		return;

	/* If we get an interrupt while polling, then just ignore it. */
	if (sc->sc_bus.use_polling) {
#ifdef DIAGNOSTIC
		printf("ohci_intr: ignored interrupt while polling\n");
#endif
		return;
	}

	ohci_intr1(sc);
}

static int
ohci_intr1(ohci_softc_t *sc)
{
	u_int32_t intrs, eintrs;
	ohci_physaddr_t done;

	DPRINTFN(14,("ohci_intr1: enter\n"));

	/* In case the interrupt occurs before initialization has completed. */
	if (sc == NULL || sc->sc_hcca == NULL) {
#ifdef DIAGNOSTIC
		printf("ohci_intr: sc->sc_hcca == NULL\n");
#endif
		return (0);
	}

	intrs = 0;
	done = le32toh(sc->sc_hcca->hcca_done_head);

	/* The LSb of done is used to inform the HC Driver that an interrupt
	 * condition exists for both the Done list and for another event
	 * recorded in HcInterruptStatus. On an interrupt from the HC, the HC
	 * Driver checks the HccaDoneHead Value. If this value is 0, then the
	 * interrupt was caused by other than the HccaDoneHead update and the
	 * HcInterruptStatus register needs to be accessed to determine that
	 * exact interrupt cause. If HccaDoneHead is nonzero, then a Done list
	 * update interrupt is indicated and if the LSb of done is nonzero,
	 * then an additional interrupt event is indicated and
	 * HcInterruptStatus should be checked to determine its cause.
	 */
	if (done != 0) {
		if (done & ~OHCI_DONE_INTRS)
			intrs = OHCI_WDH;
		if (done & OHCI_DONE_INTRS) {
			intrs |= OREAD4(sc, OHCI_INTERRUPT_STATUS);
			done &= ~OHCI_DONE_INTRS;
		}
		sc->sc_hcca->hcca_done_head = 0;
	} else
		intrs = OREAD4(sc, OHCI_INTERRUPT_STATUS) & ~OHCI_WDH;

	if (intrs == 0)		/* nothing to be done (PCI shared interrupt) */
		return (0);

	intrs &= ~OHCI_MIE;
	OWRITE4(sc, OHCI_INTERRUPT_STATUS, intrs); /* Acknowledge */
	eintrs = intrs & sc->sc_eintrs;
	if (!eintrs)
		return (0);

	sc->sc_bus.intr_context++;
	sc->sc_bus.no_intrs++;
	DPRINTFN(7, ("ohci_intr: sc=%p intrs=0x%x(0x%x) eintrs=0x%x\n",
		     sc, (u_int)intrs, OREAD4(sc, OHCI_INTERRUPT_STATUS),
		     (u_int)eintrs));

	if (eintrs & OHCI_SO) {
		sc->sc_overrun_cnt++;
		if (usbd_ratecheck(&sc->sc_overrun_ntc)) {
			printf("%s: %u scheduling overruns\n",
			    device_get_nameunit(sc->sc_bus.bdev), sc->sc_overrun_cnt);
			sc->sc_overrun_cnt = 0;
		}
		/* XXX do what */
		eintrs &= ~OHCI_SO;
	}
	if (eintrs & OHCI_WDH) {
		ohci_add_done(sc, done &~ OHCI_DONE_INTRS);
		usb_schedsoftintr(&sc->sc_bus);
		eintrs &= ~OHCI_WDH;
	}
	if (eintrs & OHCI_RD) {
		printf("%s: resume detect\n", device_get_nameunit(sc->sc_bus.bdev));
		/* XXX process resume detect */
	}
	if (eintrs & OHCI_UE) {
		printf("%s: unrecoverable error, controller halted\n",
		       device_get_nameunit(sc->sc_bus.bdev));
		OWRITE4(sc, OHCI_CONTROL, OHCI_HCFS_RESET);
		/* XXX what else */
	}
	if (eintrs & OHCI_RHSC) {
		ohci_rhsc(sc, sc->sc_intrxfer);
		/*
		 * Disable RHSC interrupt for now, because it will be
		 * on until the port has been reset.
		 */
		ohci_rhsc_able(sc, 0);
		/* Do not allow RHSC interrupts > 1 per second */
		callout_reset(&sc->sc_tmo_rhsc, hz, ohci_rhsc_enable, sc);
		eintrs &= ~OHCI_RHSC;
	}

	sc->sc_bus.intr_context--;

	if (eintrs != 0) {
		/* Block unprocessed interrupts. XXX */
		OWRITE4(sc, OHCI_INTERRUPT_DISABLE, eintrs);
		sc->sc_eintrs &= ~eintrs;
		printf("%s: blocking intrs 0x%x\n",
		       device_get_nameunit(sc->sc_bus.bdev), eintrs);
	}

	return (1);
}

void
ohci_rhsc_able(ohci_softc_t *sc, int on)
{
	DPRINTFN(4, ("ohci_rhsc_able: on=%d\n", on));
	if (on) {
		sc->sc_eintrs |= OHCI_RHSC;
		OWRITE4(sc, OHCI_INTERRUPT_ENABLE, OHCI_RHSC);
	} else {
		sc->sc_eintrs &= ~OHCI_RHSC;
		OWRITE4(sc, OHCI_INTERRUPT_DISABLE, OHCI_RHSC);
	}
}

void
ohci_rhsc_enable(void *v_sc)
{
	ohci_softc_t *sc = v_sc;
	int s;

	s = splhardusb();
	ohci_rhsc_able(sc, 1);
	splx(s);
}

#ifdef USB_DEBUG
char *ohci_cc_strs[] = {
	"NO_ERROR",
	"CRC",
	"BIT_STUFFING",
	"DATA_TOGGLE_MISMATCH",
	"STALL",
	"DEVICE_NOT_RESPONDING",
	"PID_CHECK_FAILURE",
	"UNEXPECTED_PID",
	"DATA_OVERRUN",
	"DATA_UNDERRUN",
	"BUFFER_OVERRUN",
	"BUFFER_UNDERRUN",
	"reserved",
	"reserved",
	"NOT_ACCESSED",
	"NOT_ACCESSED"
};
#endif

void
ohci_add_done(ohci_softc_t *sc, ohci_physaddr_t done)
{
	ohci_soft_itd_t *sitd, *sidone, **ip;
	ohci_soft_td_t  *std,  *sdone,  **p;

	/* Reverse the done list. */
	for (sdone = NULL, sidone = NULL; done != 0; ) {
		std = ohci_hash_find_td(sc, done);
		if (std != NULL) {
			std->dnext = sdone;
			done = le32toh(std->td.td_nexttd);
			sdone = std;
			DPRINTFN(10,("add TD %p\n", std));
			continue;
		}
		sitd = ohci_hash_find_itd(sc, done);
		if (sitd != NULL) {
			sitd->dnext = sidone;
			done = le32toh(sitd->itd.itd_nextitd);
			sidone = sitd;
			DPRINTFN(5,("add ITD %p\n", sitd));
			continue;
		}
		panic("ohci_add_done: addr 0x%08lx not found", (u_long)done);
	}

	/* sdone & sidone now hold the done lists. */
	/* Put them on the already processed lists. */
	for (p = &sc->sc_sdone; *p != NULL; p = &(*p)->dnext)
		;
	*p = sdone;
	for (ip = &sc->sc_sidone; *ip != NULL; ip = &(*ip)->dnext)
		;
	*ip = sidone;
}

void
ohci_softintr(void *v)
{
	ohci_softc_t *sc = v;
	ohci_soft_itd_t *sitd, *sidone, *sitdnext;
	ohci_soft_td_t  *std,  *sdone,  *stdnext, *p, *n;
	usbd_xfer_handle xfer;
	struct ohci_pipe *opipe;
	int len, cc, s;
	int i, j, iframes;
	
	DPRINTFN(10,("ohci_softintr: enter\n"));

	sc->sc_bus.intr_context++;

	s = splhardusb();
	sdone = sc->sc_sdone;
	sc->sc_sdone = NULL;
	sidone = sc->sc_sidone;
	sc->sc_sidone = NULL;
	splx(s);

	DPRINTFN(10,("ohci_softintr: sdone=%p sidone=%p\n", sdone, sidone));

#ifdef USB_DEBUG
	if (ohcidebug > 10) {
		DPRINTF(("ohci_process_done: TD done:\n"));
		ohci_dump_tds(sdone);
	}
#endif

	for (std = sdone; std; std = stdnext) {
		xfer = std->xfer;
		stdnext = std->dnext;
		DPRINTFN(10, ("ohci_process_done: std=%p xfer=%p hcpriv=%p\n",
				std, xfer, (xfer ? xfer->hcpriv : NULL)));
		if (xfer == NULL) {
			/*
			 * xfer == NULL: There seems to be no xfer associated
			 * with this TD. It is tailp that happened to end up on
			 * the done queue.
			 */
			continue;
		}
		if (xfer->status == USBD_CANCELLED ||
		    xfer->status == USBD_TIMEOUT) {
			DPRINTF(("ohci_process_done: cancel/timeout %p\n",
				 xfer));
			/* Handled by abort routine. */
			continue;
		}

		len = std->len;
		if (std->td.td_cbp != 0)
			len -= le32toh(std->td.td_be) -
			       le32toh(std->td.td_cbp) + 1;
		DPRINTFN(10, ("ohci_process_done: len=%d, flags=0x%x\n", len,
		    std->flags));
		if (std->flags & OHCI_ADD_LEN)
			xfer->actlen += len;

		cc = OHCI_TD_GET_CC(le32toh(std->td.td_flags));
		if (cc != OHCI_CC_NO_ERROR) {
			/*
			 * Endpoint is halted.  First unlink all the TDs
			 * belonging to the failed transfer, and then restart
			 * the endpoint.
			 */
			opipe = (struct ohci_pipe *)xfer->pipe;

			DPRINTFN(15,("ohci_process_done: error cc=%d (%s)\n",
			  OHCI_TD_GET_CC(le32toh(std->td.td_flags)),
			  ohci_cc_strs[OHCI_TD_GET_CC(le32toh(std->td.td_flags))]));
			callout_stop(&xfer->timeout_handle);
			usb_rem_task(OXFER(xfer)->xfer.pipe->device,
			    &OXFER(xfer)->abort_task);

			/* Remove all this xfer's TDs from the done queue. */
			for (p = std; p->dnext != NULL; p = p->dnext) {
				if (p->dnext->xfer != xfer)
					continue;
				p->dnext = p->dnext->dnext;
			}
			/* The next TD may have been removed. */
			stdnext = std->dnext;

			/* Remove all TDs belonging to this xfer. */
			for (p = xfer->hcpriv; p->xfer == xfer; p = n) {
				n = p->nexttd;
				ohci_free_std(sc, p);
			}

			/* clear halt */
			opipe->sed->ed.ed_headp = htole32(p->physaddr);
			OWRITE4(sc, OHCI_COMMAND_STATUS, OHCI_CLF);

			if (cc == OHCI_CC_STALL)
				xfer->status = USBD_STALLED;
			else
				xfer->status = USBD_IOERROR;
			s = splusb();
			usb_transfer_complete(xfer);
			splx(s);
			continue;
		}
		/*
		 * Skip intermediate TDs. They remain linked from
		 * xfer->hcpriv and we free them when the transfer completes.
		 */
		if ((std->flags & OHCI_CALL_DONE) == 0)
			continue;

		/* Normal transfer completion */
		callout_stop(&xfer->timeout_handle);
		usb_rem_task(OXFER(xfer)->xfer.pipe->device,
		    &OXFER(xfer)->abort_task);
		for (p = xfer->hcpriv; p->xfer == xfer; p = n) {
			n = p->nexttd;
			ohci_free_std(sc, p);
		}
		xfer->status = USBD_NORMAL_COMPLETION;
		s = splusb();
		usb_transfer_complete(xfer);
		splx(s);
	}

#ifdef USB_DEBUG
	if (ohcidebug > 10) {
		DPRINTF(("ohci_softintr: ITD done:\n"));
		ohci_dump_itds(sidone);
	}
#endif

	for (sitd = sidone; sitd != NULL; sitd = sitdnext) {
		xfer = sitd->xfer;
		sitdnext = sitd->dnext;
		sitd->flags |= OHCI_ITD_INTFIN;
		DPRINTFN(1, ("ohci_process_done: sitd=%p xfer=%p hcpriv=%p\n",
			     sitd, xfer, xfer ? xfer->hcpriv : 0));
		if (xfer == NULL)
			continue;
		if (xfer->status == USBD_CANCELLED ||
		    xfer->status == USBD_TIMEOUT) {
			DPRINTF(("ohci_process_done: cancel/timeout %p\n",
				 xfer));
			/* Handled by abort routine. */
			continue;
		}
		if (xfer->pipe)
			if (xfer->pipe->aborting)
				continue; /*Ignore.*/
#ifdef DIAGNOSTIC
		if (sitd->isdone)
			printf("ohci_softintr: sitd=%p is done\n", sitd);
		sitd->isdone = 1;
#endif
		opipe = (struct ohci_pipe *)xfer->pipe;
		if (opipe->aborting)
			continue;
 
		if (sitd->flags & OHCI_CALL_DONE) {
			ohci_soft_itd_t *next;

			opipe->u.iso.inuse -= xfer->nframes;
			xfer->status = USBD_NORMAL_COMPLETION;
		 	for (i = 0, sitd = xfer->hcpriv;;sitd = next) {
				next = sitd->nextitd;
				if (OHCI_ITD_GET_CC(sitd->itd.itd_flags) != OHCI_CC_NO_ERROR)
					xfer->status = USBD_IOERROR;

				if (xfer->status == USBD_NORMAL_COMPLETION) {
					iframes = OHCI_ITD_GET_FC(sitd->itd.itd_flags);
					for (j = 0; j < iframes; i++, j++) {
						len = le16toh(sitd->itd.itd_offset[j]);
						len =
						   (OHCI_ITD_PSW_GET_CC(len) ==
						    OHCI_CC_NOT_ACCESSED) ? 0 :
						    OHCI_ITD_PSW_LENGTH(len);
						xfer->frlengths[i] = len;
					}
				}
				if (sitd->flags & OHCI_CALL_DONE)
					break;
			}
		 	for (sitd = xfer->hcpriv; sitd->xfer == xfer;
			    sitd = next) {
				next = sitd->nextitd;
				ohci_free_sitd(sc, sitd);
			}

			s = splusb();
			usb_transfer_complete(xfer);
			splx(s);
		}
	}

#ifdef USB_USE_SOFTINTR
	if (sc->sc_softwake) {
		sc->sc_softwake = 0;
		wakeup(&sc->sc_softwake);
	}
#endif /* USB_USE_SOFTINTR */

	sc->sc_bus.intr_context--;
	DPRINTFN(10,("ohci_softintr: done:\n"));
}

void
ohci_device_ctrl_done(usbd_xfer_handle xfer)
{
	DPRINTFN(10,("ohci_device_ctrl_done: xfer=%p\n", xfer));

#ifdef DIAGNOSTIC
	if (!(xfer->rqflags & URQ_REQUEST)) {
		panic("ohci_device_ctrl_done: not a request");
	}
#endif
	xfer->hcpriv = NULL;
}

void
ohci_device_intr_done(usbd_xfer_handle xfer)
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)xfer->pipe;
	ohci_softc_t *sc = (ohci_softc_t *)opipe->pipe.device->bus;
	usbd_status err;

	DPRINTFN(10,("ohci_device_intr_done: xfer=%p, actlen=%d\n",
		     xfer, xfer->actlen));

	xfer->hcpriv = NULL;
	if (xfer->pipe->repeat) {
		err = ohci_device_intr_insert(sc, xfer);
		if (err) {
			xfer->status = err;
			return;
		}
	}
}

void
ohci_device_bulk_done(usbd_xfer_handle xfer)
{
	DPRINTFN(10,("ohci_device_bulk_done: xfer=%p, actlen=%d\n",
		     xfer, xfer->actlen));

	xfer->hcpriv = NULL;
}

void
ohci_rhsc(ohci_softc_t *sc, usbd_xfer_handle xfer)
{
	usbd_pipe_handle pipe;
	u_char *p;
	int i, m;
	int hstatus;

	hstatus = OREAD4(sc, OHCI_RH_STATUS);
	DPRINTF(("ohci_rhsc: sc=%p xfer=%p hstatus=0x%08x\n",
		 sc, xfer, hstatus));

	if (xfer == NULL) {
		/* Just ignore the change. */
		return;
	}

	pipe = xfer->pipe;

	p = xfer->buffer;
	m = min(sc->sc_noport, xfer->length * 8 - 1);
	memset(p, 0, xfer->length);
	for (i = 1; i <= m; i++) {
		/* Pick out CHANGE bits from the status reg. */
		if (OREAD4(sc, OHCI_RH_PORT_STATUS(i)) >> 16)
			p[i/8] |= 1 << (i%8);
	}
	DPRINTF(("ohci_rhsc: change=0x%02x\n", *p));
	xfer->actlen = xfer->length;
	xfer->status = USBD_NORMAL_COMPLETION;

	usb_transfer_complete(xfer);
}

void
ohci_root_intr_done(usbd_xfer_handle xfer)
{
	xfer->hcpriv = NULL;
}

void
ohci_root_ctrl_done(usbd_xfer_handle xfer)
{
	xfer->hcpriv = NULL;
}

/*
 * Wait here until controller claims to have an interrupt.
 * Then call ohci_intr and return.  Use timeout to avoid waiting
 * too long.
 */
void
ohci_waitintr(ohci_softc_t *sc, usbd_xfer_handle xfer)
{
	int timo = xfer->timeout;
	int usecs;
	u_int32_t intrs;

	xfer->status = USBD_IN_PROGRESS;
	for (usecs = timo * 1000000 / hz; usecs > 0; usecs -= 1000) {
		usb_delay_ms(&sc->sc_bus, 1);
		if (sc->sc_dying)
			break;
		intrs = OREAD4(sc, OHCI_INTERRUPT_STATUS) & sc->sc_eintrs;
		DPRINTFN(15,("ohci_waitintr: 0x%04x\n", intrs));
#ifdef USB_DEBUG
		if (ohcidebug > 15)
			ohci_dumpregs(sc);
#endif
		if (intrs) {
			ohci_intr1(sc);
			if (xfer->status != USBD_IN_PROGRESS)
				return;
		}
	}

	/* Timeout */
	DPRINTF(("ohci_waitintr: timeout\n"));
	xfer->status = USBD_TIMEOUT;
	usb_transfer_complete(xfer);
	/* XXX should free TD */
}

void
ohci_poll(struct usbd_bus *bus)
{
	ohci_softc_t *sc = (ohci_softc_t *)bus;
#ifdef USB_DEBUG
	static int last;
	int new;
	new = OREAD4(sc, OHCI_INTERRUPT_STATUS);
	if (new != last) {
		DPRINTFN(10,("ohci_poll: intrs=0x%04x\n", new));
		last = new;
	}
#endif

	if (OREAD4(sc, OHCI_INTERRUPT_STATUS) & sc->sc_eintrs)
		ohci_intr1(sc);
}

usbd_status
ohci_device_request(usbd_xfer_handle xfer)
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)xfer->pipe;
	usb_device_request_t *req = &xfer->request;
	usbd_device_handle dev = opipe->pipe.device;
	ohci_softc_t *sc = (ohci_softc_t *)dev->bus;
	ohci_soft_td_t *setup, *stat, *next, *tail;
	ohci_soft_ed_t *sed;
	int isread;
	int len;
	usbd_status err;
	int s;

	isread = req->bmRequestType & UT_READ;
	len = UGETW(req->wLength);

	DPRINTFN(3,("ohci_device_control type=0x%02x, request=0x%02x, "
		    "wValue=0x%04x, wIndex=0x%04x len=%d, addr=%d, endpt=%d\n",
		    req->bmRequestType, req->bRequest, UGETW(req->wValue),
		    UGETW(req->wIndex), len, dev->address,
		    opipe->pipe.endpoint->edesc->bEndpointAddress));

	setup = opipe->tail.td;
	stat = ohci_alloc_std(sc);
	if (stat == NULL) {
		err = USBD_NOMEM;
		goto bad1;
	}
	tail = ohci_alloc_std(sc);
	if (tail == NULL) {
		err = USBD_NOMEM;
		goto bad2;
	}
	tail->xfer = NULL;

	sed = opipe->sed;
	opipe->u.ctl.length = len;
	next = stat;

	/* Set up data transaction */
	if (len != 0) {
		ohci_soft_td_t *std = stat;

		err = ohci_alloc_std_chain(opipe, sc, len, isread, xfer,
			  std, &stat);
		stat = stat->nexttd; /* point at free TD */
		if (err)
			goto bad3;
		/* Start toggle at 1 and then use the carried toggle. */
		std->td.td_flags &= htole32(~OHCI_TD_TOGGLE_MASK);
		std->td.td_flags |= htole32(OHCI_TD_TOGGLE_1);
	}

	memcpy(KERNADDR(&opipe->u.ctl.reqdma, 0), req, sizeof *req);

	setup->td.td_flags = htole32(OHCI_TD_SETUP | OHCI_TD_NOCC |
				     OHCI_TD_TOGGLE_0 | OHCI_TD_SET_DI(6));
	setup->td.td_cbp = htole32(DMAADDR(&opipe->u.ctl.reqdma, 0));
	setup->nexttd = next;
	setup->td.td_nexttd = htole32(next->physaddr);
	setup->td.td_be = htole32(le32toh(setup->td.td_cbp) + sizeof *req - 1);
	setup->len = 0;
	setup->xfer = xfer;
	setup->flags = 0;
	xfer->hcpriv = setup;

	stat->td.td_flags = htole32(
		(isread ? OHCI_TD_OUT : OHCI_TD_IN) |
		OHCI_TD_NOCC | OHCI_TD_TOGGLE_1 | OHCI_TD_SET_DI(1));
	stat->td.td_cbp = 0;
	stat->nexttd = tail;
	stat->td.td_nexttd = htole32(tail->physaddr);
	stat->td.td_be = 0;
	stat->flags = OHCI_CALL_DONE;
	stat->len = 0;
	stat->xfer = xfer;

#ifdef USB_DEBUG
	if (ohcidebug > 5) {
		DPRINTF(("ohci_device_request:\n"));
		ohci_dump_ed(sed);
		ohci_dump_tds(setup);
	}
#endif

	/* Insert ED in schedule */
	s = splusb();
	sed->ed.ed_tailp = htole32(tail->physaddr);
	opipe->tail.td = tail;
	OWRITE4(sc, OHCI_COMMAND_STATUS, OHCI_CLF);
	if (xfer->timeout && !sc->sc_bus.use_polling) {
		callout_reset(&xfer->timeout_handle, MS_TO_TICKS(xfer->timeout),
		    ohci_timeout, xfer);
	}
	splx(s);

#ifdef USB_DEBUG
	if (ohcidebug > 20) {
		delay(10000);
		DPRINTF(("ohci_device_request: status=%x\n",
			 OREAD4(sc, OHCI_COMMAND_STATUS)));
		ohci_dumpregs(sc);
		printf("ctrl head:\n");
		ohci_dump_ed(sc->sc_ctrl_head);
		printf("sed:\n");
		ohci_dump_ed(sed);
		ohci_dump_tds(setup);
	}
#endif

	return (USBD_NORMAL_COMPLETION);

 bad3:
	ohci_free_std(sc, tail);
 bad2:
	ohci_free_std(sc, stat);
 bad1:
	return (err);
}

/*
 * Add an ED to the schedule.  Called at splusb().
 */
void
ohci_add_ed(ohci_soft_ed_t *sed, ohci_soft_ed_t *head)
{
	DPRINTFN(8,("ohci_add_ed: sed=%p head=%p\n", sed, head));

	SPLUSBCHECK;
	sed->next = head->next;
	sed->ed.ed_nexted = head->ed.ed_nexted;
	head->next = sed;
	head->ed.ed_nexted = htole32(sed->physaddr);
}

/*
 * Remove an ED from the schedule.  Called at splusb().
 */
void
ohci_rem_ed(ohci_soft_ed_t *sed, ohci_soft_ed_t *head)
{
	ohci_soft_ed_t *p;

	SPLUSBCHECK;

	/* XXX */
	for (p = head; p != NULL && p->next != sed; p = p->next)
		;
	if (p == NULL)
		panic("ohci_rem_ed: ED not found");
	p->next = sed->next;
	p->ed.ed_nexted = sed->ed.ed_nexted;
}

/*
 * When a transfer is completed the TD is added to the done queue by
 * the host controller.  This queue is the processed by software.
 * Unfortunately the queue contains the physical address of the TD
 * and we have no simple way to translate this back to a kernel address.
 * To make the translation possible (and fast) we use a hash table of
 * TDs currently in the schedule.  The physical address is used as the
 * hash value.
 */

#define HASH(a) (((a) >> 4) % OHCI_HASH_SIZE)
/* Called at splusb() */
void
ohci_hash_add_td(ohci_softc_t *sc, ohci_soft_td_t *std)
{
	int h = HASH(std->physaddr);

	SPLUSBCHECK;

	LIST_INSERT_HEAD(&sc->sc_hash_tds[h], std, hnext);
}

/* Called at splusb() */
void
ohci_hash_rem_td(ohci_softc_t *sc, ohci_soft_td_t *std)
{
	SPLUSBCHECK;

	LIST_REMOVE(std, hnext);
}

ohci_soft_td_t *
ohci_hash_find_td(ohci_softc_t *sc, ohci_physaddr_t a)
{
	int h = HASH(a);
	ohci_soft_td_t *std;

	/* if these are present they should be masked out at an earlier
	 * stage.
	 */
	KASSERT((a&~OHCI_HEADMASK) == 0, ("%s: 0x%b has lower bits set\n",
				      device_get_nameunit(sc->sc_bus.bdev),
				      (int) a, "\20\1HALT\2TOGGLE"));

	for (std = LIST_FIRST(&sc->sc_hash_tds[h]);
	     std != NULL;
	     std = LIST_NEXT(std, hnext))
		if (std->physaddr == a)
			return (std);

	DPRINTF(("%s: ohci_hash_find_td: addr 0x%08lx not found\n",
		device_get_nameunit(sc->sc_bus.bdev), (u_long) a));
	return (NULL);
}

/* Called at splusb() */
void
ohci_hash_add_itd(ohci_softc_t *sc, ohci_soft_itd_t *sitd)
{
	int h = HASH(sitd->physaddr);

	SPLUSBCHECK;

	DPRINTFN(10,("ohci_hash_add_itd: sitd=%p physaddr=0x%08lx\n",
		    sitd, (u_long)sitd->physaddr));

	LIST_INSERT_HEAD(&sc->sc_hash_itds[h], sitd, hnext);
}

/* Called at splusb() */
void
ohci_hash_rem_itd(ohci_softc_t *sc, ohci_soft_itd_t *sitd)
{
	SPLUSBCHECK;

	DPRINTFN(10,("ohci_hash_rem_itd: sitd=%p physaddr=0x%08lx\n",
		    sitd, (u_long)sitd->physaddr));

	LIST_REMOVE(sitd, hnext);
}

ohci_soft_itd_t *
ohci_hash_find_itd(ohci_softc_t *sc, ohci_physaddr_t a)
{
	int h = HASH(a);
	ohci_soft_itd_t *sitd;

	for (sitd = LIST_FIRST(&sc->sc_hash_itds[h]);
	     sitd != NULL;
	     sitd = LIST_NEXT(sitd, hnext))
		if (sitd->physaddr == a)
			return (sitd);
	return (NULL);
}

void
ohci_timeout(void *addr)
{
	struct ohci_xfer *oxfer = addr;
	struct ohci_pipe *opipe = (struct ohci_pipe *)oxfer->xfer.pipe;
	ohci_softc_t *sc = (ohci_softc_t *)opipe->pipe.device->bus;

	DPRINTF(("ohci_timeout: oxfer=%p\n", oxfer));

	if (sc->sc_dying) {
		ohci_abort_xfer(&oxfer->xfer, USBD_TIMEOUT);
		return;
	}

	/* Execute the abort in a process context. */
	usb_add_task(oxfer->xfer.pipe->device, &oxfer->abort_task,
	    USB_TASKQ_HC);
}

void
ohci_timeout_task(void *addr)
{
	usbd_xfer_handle xfer = addr;
	int s;

	DPRINTF(("ohci_timeout_task: xfer=%p\n", xfer));

	s = splusb();
	ohci_abort_xfer(xfer, USBD_TIMEOUT);
	splx(s);
}

#ifdef USB_DEBUG
void
ohci_dump_tds(ohci_soft_td_t *std)
{
	for (; std; std = std->nexttd)
		ohci_dump_td(std);
}

void
ohci_dump_td(ohci_soft_td_t *std)
{
	char sbuf[128];

	bitmask_snprintf((u_int32_t)le32toh(std->td.td_flags),
			 "\20\23R\24OUT\25IN\31TOG1\32SETTOGGLE",
			 sbuf, sizeof(sbuf));

	printf("TD(%p) at %08lx: %s delay=%d ec=%d cc=%d\ncbp=0x%08lx "
	       "nexttd=0x%08lx be=0x%08lx\n",
	       std, (u_long)std->physaddr, sbuf,
	       OHCI_TD_GET_DI(le32toh(std->td.td_flags)),
	       OHCI_TD_GET_EC(le32toh(std->td.td_flags)),
	       OHCI_TD_GET_CC(le32toh(std->td.td_flags)),
	       (u_long)le32toh(std->td.td_cbp),
	       (u_long)le32toh(std->td.td_nexttd),
	       (u_long)le32toh(std->td.td_be));
}

void
ohci_dump_itd(ohci_soft_itd_t *sitd)
{
	int i;

	printf("ITD(%p) at %08lx: sf=%d di=%d fc=%d cc=%d\n"
	       "bp0=0x%08lx next=0x%08lx be=0x%08lx\n",
	       sitd, (u_long)sitd->physaddr,
	       OHCI_ITD_GET_SF(le32toh(sitd->itd.itd_flags)),
	       OHCI_ITD_GET_DI(le32toh(sitd->itd.itd_flags)),
	       OHCI_ITD_GET_FC(le32toh(sitd->itd.itd_flags)),
	       OHCI_ITD_GET_CC(le32toh(sitd->itd.itd_flags)),
	       (u_long)le32toh(sitd->itd.itd_bp0),
	       (u_long)le32toh(sitd->itd.itd_nextitd),
	       (u_long)le32toh(sitd->itd.itd_be));
	for (i = 0; i < OHCI_ITD_NOFFSET; i++)
		printf("offs[%d]=0x%04x ", i,
		       (u_int)le16toh(sitd->itd.itd_offset[i]));
	printf("\n");
}

void
ohci_dump_itds(ohci_soft_itd_t *sitd)
{
	for (; sitd; sitd = sitd->nextitd)
		ohci_dump_itd(sitd);
}

void
ohci_dump_ed(ohci_soft_ed_t *sed)
{
	char sbuf[128], sbuf2[128];

	bitmask_snprintf((u_int32_t)le32toh(sed->ed.ed_flags),
			 "\20\14OUT\15IN\16LOWSPEED\17SKIP\20ISO",
			 sbuf, sizeof(sbuf));
	bitmask_snprintf((u_int32_t)le32toh(sed->ed.ed_headp),
			 "\20\1HALT\2CARRY", sbuf2, sizeof(sbuf2));

	printf("ED(%p) at 0x%08lx: addr=%d endpt=%d maxp=%d flags=%s\ntailp=0x%08lx "
		 "headflags=%s headp=0x%08lx nexted=0x%08lx\n",
		 sed, (u_long)sed->physaddr,
		 OHCI_ED_GET_FA(le32toh(sed->ed.ed_flags)),
		 OHCI_ED_GET_EN(le32toh(sed->ed.ed_flags)),
		 OHCI_ED_GET_MAXP(le32toh(sed->ed.ed_flags)), sbuf,
		 (u_long)le32toh(sed->ed.ed_tailp), sbuf2,
		 (u_long)le32toh(sed->ed.ed_headp),
		 (u_long)le32toh(sed->ed.ed_nexted));
}
#endif

usbd_status
ohci_open(usbd_pipe_handle pipe)
{
	usbd_device_handle dev = pipe->device;
	ohci_softc_t *sc = (ohci_softc_t *)dev->bus;
	usb_endpoint_descriptor_t *ed = pipe->endpoint->edesc;
	struct ohci_pipe *opipe = (struct ohci_pipe *)pipe;
	u_int8_t addr = dev->address;
	u_int8_t xfertype = ed->bmAttributes & UE_XFERTYPE;
	ohci_soft_ed_t *sed;
	ohci_soft_td_t *std;
	ohci_soft_itd_t *sitd;
	ohci_physaddr_t tdphys;
	u_int32_t fmt;
	usbd_status err;
	int s;
	int ival;

	DPRINTFN(1, ("ohci_open: pipe=%p, addr=%d, endpt=%d (%d)\n",
		     pipe, addr, ed->bEndpointAddress, sc->sc_addr));

	if (sc->sc_dying)
		return (USBD_IOERROR);

	std = NULL;
	sed = NULL;

	if (addr == sc->sc_addr) {
		switch (ed->bEndpointAddress) {
		case USB_CONTROL_ENDPOINT:
			pipe->methods = &ohci_root_ctrl_methods;
			break;
		case UE_DIR_IN | OHCI_INTR_ENDPT:
			pipe->methods = &ohci_root_intr_methods;
			break;
		default:
			return (USBD_INVAL);
		}
	} else {
		sed = ohci_alloc_sed(sc);
		if (sed == NULL)
			goto bad0;
		opipe->sed = sed;
		if (xfertype == UE_ISOCHRONOUS) {
			sitd = ohci_alloc_sitd(sc);
			if (sitd == NULL)
				goto bad1;
			opipe->tail.itd = sitd;
			opipe->aborting = 0;
			tdphys = sitd->physaddr;
			fmt = OHCI_ED_FORMAT_ISO;
			if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN)
				fmt |= OHCI_ED_DIR_IN;
			else
				fmt |= OHCI_ED_DIR_OUT;
		} else {
			std = ohci_alloc_std(sc);
			if (std == NULL)
				goto bad1;
			opipe->tail.td = std;
			tdphys = std->physaddr;
			fmt = OHCI_ED_FORMAT_GEN | OHCI_ED_DIR_TD;
		}
		sed->ed.ed_flags = htole32(
			OHCI_ED_SET_FA(addr) |
			OHCI_ED_SET_EN(UE_GET_ADDR(ed->bEndpointAddress)) |
			(dev->speed == USB_SPEED_LOW ? OHCI_ED_SPEED : 0) |
			fmt |
			OHCI_ED_SET_MAXP(UGETW(ed->wMaxPacketSize)));
		sed->ed.ed_headp = htole32(tdphys |
		    (pipe->endpoint->savedtoggle ? OHCI_TOGGLECARRY : 0));
		sed->ed.ed_tailp = htole32(tdphys);

		switch (xfertype) {
		case UE_CONTROL:
			pipe->methods = &ohci_device_ctrl_methods;
			err = usb_allocmem(&sc->sc_bus,
				  sizeof(usb_device_request_t),
				  0, &opipe->u.ctl.reqdma);
			if (err)
				goto bad;
			s = splusb();
			ohci_add_ed(sed, sc->sc_ctrl_head);
			splx(s);
			break;
		case UE_INTERRUPT:
			pipe->methods = &ohci_device_intr_methods;
			ival = pipe->interval;
			if (ival == USBD_DEFAULT_INTERVAL)
				ival = ed->bInterval;
			return (ohci_device_setintr(sc, opipe, ival));
		case UE_ISOCHRONOUS:
			pipe->methods = &ohci_device_isoc_methods;
			return (ohci_setup_isoc(pipe));
		case UE_BULK:
			pipe->methods = &ohci_device_bulk_methods;
			s = splusb();
			ohci_add_ed(sed, sc->sc_bulk_head);
			splx(s);
			break;
		}
	}
	return (USBD_NORMAL_COMPLETION);

 bad:
	if (std != NULL)
		ohci_free_std(sc, std);
 bad1:
	if (sed != NULL)
		ohci_free_sed(sc, sed);
 bad0:
	return (USBD_NOMEM);

}

/*
 * Close a reqular pipe.
 * Assumes that there are no pending transactions.
 */
void
ohci_close_pipe(usbd_pipe_handle pipe, ohci_soft_ed_t *head)
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)pipe;
	ohci_softc_t *sc = (ohci_softc_t *)pipe->device->bus;
	ohci_soft_ed_t *sed = opipe->sed;
	int s;

	s = splusb();
#ifdef DIAGNOSTIC
	sed->ed.ed_flags |= htole32(OHCI_ED_SKIP);
	if ((le32toh(sed->ed.ed_tailp) & OHCI_HEADMASK) !=
	    (le32toh(sed->ed.ed_headp) & OHCI_HEADMASK)) {
		ohci_soft_td_t *std;
		std = ohci_hash_find_td(sc, le32toh(sed->ed.ed_headp));
		printf("ohci_close_pipe: pipe not empty sed=%p hd=0x%x "
		       "tl=0x%x pipe=%p, std=%p\n", sed,
		       (int)le32toh(sed->ed.ed_headp),
		       (int)le32toh(sed->ed.ed_tailp),
		       pipe, std);
#ifdef USB_DEBUG
		usbd_dump_pipe(&opipe->pipe);
#endif
#ifdef USB_DEBUG
		ohci_dump_ed(sed);
		if (std)
			ohci_dump_td(std);
#endif
		usb_delay_ms(&sc->sc_bus, 2);
		if ((le32toh(sed->ed.ed_tailp) & OHCI_HEADMASK) !=
		    (le32toh(sed->ed.ed_headp) & OHCI_HEADMASK))
			printf("ohci_close_pipe: pipe still not empty\n");
	}
#endif
	ohci_rem_ed(sed, head);
	/* Make sure the host controller is not touching this ED */
	usb_delay_ms(&sc->sc_bus, 1);
	splx(s);
	pipe->endpoint->savedtoggle =
	    (le32toh(sed->ed.ed_headp) & OHCI_TOGGLECARRY) ? 1 : 0;
	ohci_free_sed(sc, opipe->sed);
}

/*
 * Abort a device request.
 * If this routine is called at splusb() it guarantees that the request
 * will be removed from the hardware scheduling and that the callback
 * for it will be called with USBD_CANCELLED status.
 * It's impossible to guarantee that the requested transfer will not
 * have happened since the hardware runs concurrently.
 * If the transaction has already happened we rely on the ordinary
 * interrupt processing to process it.
 */
void
ohci_abort_xfer(usbd_xfer_handle xfer, usbd_status status)
{
	struct ohci_xfer *oxfer = OXFER(xfer);
	struct ohci_pipe *opipe = (struct ohci_pipe *)xfer->pipe;
	ohci_softc_t *sc = (ohci_softc_t *)opipe->pipe.device->bus;
	ohci_soft_ed_t *sed = opipe->sed;
	ohci_soft_td_t *p, *n;
	ohci_physaddr_t headp;
	int s, hit;

	DPRINTF(("ohci_abort_xfer: xfer=%p pipe=%p sed=%p\n", xfer, opipe,sed));

	if (sc->sc_dying) {
		/* If we're dying, just do the software part. */
		s = splusb();
		xfer->status = status;	/* make software ignore it */
		callout_stop(&xfer->timeout_handle);
		usb_rem_task(xfer->pipe->device, &OXFER(xfer)->abort_task);
		usb_transfer_complete(xfer);
		splx(s);
		return;
	}

	if (xfer->device->bus->intr_context || !curproc)
		panic("ohci_abort_xfer: not in process context");

	/*
	 * If an abort is already in progress then just wait for it to
	 * complete and return.
	 */
	if (oxfer->ohci_xfer_flags & OHCI_XFER_ABORTING) {
		DPRINTFN(2, ("ohci_abort_xfer: already aborting\n"));
		/* No need to wait if we're aborting from a timeout. */
		if (status == USBD_TIMEOUT)
			return;
		/* Override the status which might be USBD_TIMEOUT. */
		xfer->status = status;
		DPRINTFN(2, ("ohci_abort_xfer: waiting for abort to finish\n"));
		oxfer->ohci_xfer_flags |= OHCI_XFER_ABORTWAIT;
		while (oxfer->ohci_xfer_flags & OHCI_XFER_ABORTING)
			tsleep(&oxfer->ohci_xfer_flags, PZERO, "ohciaw", 0);
		return;
	}

	/*
	 * Step 1: Make interrupt routine and hardware ignore xfer.
	 */
	s = splusb();
	oxfer->ohci_xfer_flags |= OHCI_XFER_ABORTING;
	xfer->status = status;	/* make software ignore it */
	callout_stop(&xfer->timeout_handle);
	usb_rem_task(xfer->pipe->device, &OXFER(xfer)->abort_task);
	splx(s);
	DPRINTFN(1,("ohci_abort_xfer: stop ed=%p\n", sed));
	sed->ed.ed_flags |= htole32(OHCI_ED_SKIP); /* force hardware skip */

	/*
	 * Step 2: Wait until we know hardware has finished any possible
	 * use of the xfer.  Also make sure the soft interrupt routine
	 * has run.
	 */
	usb_delay_ms(opipe->pipe.device->bus, 20); /* Hardware finishes in 1ms */
	s = splusb();
#ifdef USB_USE_SOFTINTR
	sc->sc_softwake = 1;
#endif /* USB_USE_SOFTINTR */
	usb_schedsoftintr(&sc->sc_bus);
#ifdef USB_USE_SOFTINTR
	tsleep(&sc->sc_softwake, PZERO, "ohciab", 0);
#endif /* USB_USE_SOFTINTR */
	splx(s);

	/*
	 * Step 3: Remove any vestiges of the xfer from the hardware.
	 * The complication here is that the hardware may have executed
	 * beyond the xfer we're trying to abort.  So as we're scanning
	 * the TDs of this xfer we check if the hardware points to
	 * any of them.
	 */
	s = splusb();		/* XXX why? */
	p = xfer->hcpriv;
#ifdef DIAGNOSTIC
	if (p == NULL) {
		oxfer->ohci_xfer_flags &= ~OHCI_XFER_ABORTING; /* XXX */
		splx(s);
		printf("ohci_abort_xfer: hcpriv is NULL\n");
		return;
	}
#endif
#ifdef USB_DEBUG
	if (ohcidebug > 1) {
		DPRINTF(("ohci_abort_xfer: sed=\n"));
		ohci_dump_ed(sed);
		ohci_dump_tds(p);
	}
#endif
	headp = le32toh(sed->ed.ed_headp) & OHCI_HEADMASK;
	hit = 0;
	for (; p->xfer == xfer; p = n) {
		hit |= headp == p->physaddr;
		n = p->nexttd;
		ohci_free_std(sc, p);
	}
	/* Zap headp register if hardware pointed inside the xfer. */
	if (hit) {
		DPRINTFN(1,("ohci_abort_xfer: set hd=0x08%x, tl=0x%08x\n",
			    (int)p->physaddr, (int)le32toh(sed->ed.ed_tailp)));
		sed->ed.ed_headp = htole32(p->physaddr); /* unlink TDs */
	} else {
		DPRINTFN(1,("ohci_abort_xfer: no hit\n"));
	}

	/*
	 * Step 4: Turn on hardware again.
	 */
	sed->ed.ed_flags &= htole32(~OHCI_ED_SKIP); /* remove hardware skip */

	/*
	 * Step 5: Execute callback.
	 */
	/* Do the wakeup first to avoid touching the xfer after the callback. */
	oxfer->ohci_xfer_flags &= ~OHCI_XFER_ABORTING;
	if (oxfer->ohci_xfer_flags & OHCI_XFER_ABORTWAIT) {
		oxfer->ohci_xfer_flags &= ~OHCI_XFER_ABORTWAIT;
		wakeup(&oxfer->ohci_xfer_flags);
	}
	usb_transfer_complete(xfer);

	splx(s);
}

/*
 * Data structures and routines to emulate the root hub.
 */
static usb_device_descriptor_t ohci_devd = {
	USB_DEVICE_DESCRIPTOR_SIZE,
	UDESC_DEVICE,		/* type */
	{0x00, 0x01},		/* USB version */
	UDCLASS_HUB,		/* class */
	UDSUBCLASS_HUB,		/* subclass */
	UDPROTO_FSHUB,		/* protocol */
	64,			/* max packet */
	{0},{0},{0x00,0x01},	/* device id */
	1,2,0,			/* string indicies */
	1			/* # of configurations */
};

static usb_config_descriptor_t ohci_confd = {
	USB_CONFIG_DESCRIPTOR_SIZE,
	UDESC_CONFIG,
	{USB_CONFIG_DESCRIPTOR_SIZE +
	 USB_INTERFACE_DESCRIPTOR_SIZE +
	 USB_ENDPOINT_DESCRIPTOR_SIZE},
	1,
	1,
	0,
	UC_SELF_POWERED,
	0			/* max power */
};

static usb_interface_descriptor_t ohci_ifcd = {
	USB_INTERFACE_DESCRIPTOR_SIZE,
	UDESC_INTERFACE,
	0,
	0,
	1,
	UICLASS_HUB,
	UISUBCLASS_HUB,
	UIPROTO_FSHUB,
	0
};

static usb_endpoint_descriptor_t ohci_endpd = {
	USB_ENDPOINT_DESCRIPTOR_SIZE,
	UDESC_ENDPOINT,
	UE_DIR_IN | OHCI_INTR_ENDPT,
	UE_INTERRUPT,
	{8, 0},			/* max packet */
	255
};

static usb_hub_descriptor_t ohci_hubd = {
	USB_HUB_DESCRIPTOR_SIZE,
	UDESC_HUB,
	0,
	{0,0},
	0,
	0,
	{0},
};

static int
ohci_str(usb_string_descriptor_t *p, int l, const char *s)
{
	int i;

	if (l == 0)
		return (0);
	p->bLength = 2 * strlen(s) + 2;
	if (l == 1)
		return (1);
	p->bDescriptorType = UDESC_STRING;
	l -= 2;
	for (i = 0; s[i] && l > 1; i++, l -= 2)
		USETW2(p->bString[i], 0, s[i]);
	return (2*i+2);
}

/*
 * Simulate a hardware hub by handling all the necessary requests.
 */
static usbd_status
ohci_root_ctrl_transfer(usbd_xfer_handle xfer)
{
	usbd_status err;

	/* Insert last in queue. */
	err = usb_insert_transfer(xfer);
	if (err)
		return (err);

	/* Pipe isn't running, start first */
	return (ohci_root_ctrl_start(STAILQ_FIRST(&xfer->pipe->queue)));
}

static usbd_status
ohci_root_ctrl_start(usbd_xfer_handle xfer)
{
	ohci_softc_t *sc = (ohci_softc_t *)xfer->pipe->device->bus;
	usb_device_request_t *req;
	void *buf = NULL;
	int port, i;
	int s, len, value, index, l, totlen = 0;
	usb_port_status_t ps;
	usb_hub_descriptor_t hubd;
	usbd_status err;
	u_int32_t v;

	if (sc->sc_dying)
		return (USBD_IOERROR);

#ifdef DIAGNOSTIC
	if (!(xfer->rqflags & URQ_REQUEST))
		/* XXX panic */
		return (USBD_INVAL);
#endif
	req = &xfer->request;

	DPRINTFN(4,("ohci_root_ctrl_control type=0x%02x request=%02x\n",
		    req->bmRequestType, req->bRequest));

	len = UGETW(req->wLength);
	value = UGETW(req->wValue);
	index = UGETW(req->wIndex);

	if (len != 0)
		buf = xfer->buffer;

#define C(x,y) ((x) | ((y) << 8))
	switch(C(req->bRequest, req->bmRequestType)) {
	case C(UR_CLEAR_FEATURE, UT_WRITE_DEVICE):
	case C(UR_CLEAR_FEATURE, UT_WRITE_INTERFACE):
	case C(UR_CLEAR_FEATURE, UT_WRITE_ENDPOINT):
		/*
		 * DEVICE_REMOTE_WAKEUP and ENDPOINT_HALT are no-ops
		 * for the integrated root hub.
		 */
		break;
	case C(UR_GET_CONFIG, UT_READ_DEVICE):
		if (len > 0) {
			*(u_int8_t *)buf = sc->sc_conf;
			totlen = 1;
		}
		break;
	case C(UR_GET_DESCRIPTOR, UT_READ_DEVICE):
		DPRINTFN(8,("ohci_root_ctrl_control wValue=0x%04x\n", value));
		switch(value >> 8) {
		case UDESC_DEVICE:
			if ((value & 0xff) != 0) {
				err = USBD_IOERROR;
				goto ret;
			}
			totlen = l = min(len, USB_DEVICE_DESCRIPTOR_SIZE);
			USETW(ohci_devd.idVendor, sc->sc_id_vendor);
			memcpy(buf, &ohci_devd, l);
			break;
		case UDESC_CONFIG:
			if ((value & 0xff) != 0) {
				err = USBD_IOERROR;
				goto ret;
			}
			totlen = l = min(len, USB_CONFIG_DESCRIPTOR_SIZE);
			memcpy(buf, &ohci_confd, l);
			buf = (char *)buf + l;
			len -= l;
			l = min(len, USB_INTERFACE_DESCRIPTOR_SIZE);
			totlen += l;
			memcpy(buf, &ohci_ifcd, l);
			buf = (char *)buf + l;
			len -= l;
			l = min(len, USB_ENDPOINT_DESCRIPTOR_SIZE);
			totlen += l;
			memcpy(buf, &ohci_endpd, l);
			break;
		case UDESC_STRING:
			if (len == 0)
				break;
			*(u_int8_t *)buf = 0;
			totlen = 1;
			switch (value & 0xff) {
			case 1: /* Vendor */
				totlen = ohci_str(buf, len, sc->sc_vendor);
				break;
			case 2: /* Product */
				totlen = ohci_str(buf, len, "OHCI root hub");
				break;
			}
			break;
		default:
			err = USBD_IOERROR;
			goto ret;
		}
		break;
	case C(UR_GET_INTERFACE, UT_READ_INTERFACE):
		if (len > 0) {
			*(u_int8_t *)buf = 0;
			totlen = 1;
		}
		break;
	case C(UR_GET_STATUS, UT_READ_DEVICE):
		if (len > 1) {
			USETW(((usb_status_t *)buf)->wStatus,UDS_SELF_POWERED);
			totlen = 2;
		}
		break;
	case C(UR_GET_STATUS, UT_READ_INTERFACE):
	case C(UR_GET_STATUS, UT_READ_ENDPOINT):
		if (len > 1) {
			USETW(((usb_status_t *)buf)->wStatus, 0);
			totlen = 2;
		}
		break;
	case C(UR_SET_ADDRESS, UT_WRITE_DEVICE):
		if (value >= USB_MAX_DEVICES) {
			err = USBD_IOERROR;
			goto ret;
		}
		sc->sc_addr = value;
		break;
	case C(UR_SET_CONFIG, UT_WRITE_DEVICE):
		if (value != 0 && value != 1) {
			err = USBD_IOERROR;
			goto ret;
		}
		sc->sc_conf = value;
		break;
	case C(UR_SET_DESCRIPTOR, UT_WRITE_DEVICE):
		break;
	case C(UR_SET_FEATURE, UT_WRITE_DEVICE):
	case C(UR_SET_FEATURE, UT_WRITE_INTERFACE):
	case C(UR_SET_FEATURE, UT_WRITE_ENDPOINT):
		err = USBD_IOERROR;
		goto ret;
	case C(UR_SET_INTERFACE, UT_WRITE_INTERFACE):
		break;
	case C(UR_SYNCH_FRAME, UT_WRITE_ENDPOINT):
		break;
	/* Hub requests */
	case C(UR_CLEAR_FEATURE, UT_WRITE_CLASS_DEVICE):
		break;
	case C(UR_CLEAR_FEATURE, UT_WRITE_CLASS_OTHER):
		DPRINTFN(8, ("ohci_root_ctrl_control: UR_CLEAR_PORT_FEATURE "
			     "port=%d feature=%d\n",
			     index, value));
		if (index < 1 || index > sc->sc_noport) {
			err = USBD_IOERROR;
			goto ret;
		}
		port = OHCI_RH_PORT_STATUS(index);
		switch(value) {
		case UHF_PORT_ENABLE:
			OWRITE4(sc, port, UPS_CURRENT_CONNECT_STATUS);
			break;
		case UHF_PORT_SUSPEND:
			OWRITE4(sc, port, UPS_OVERCURRENT_INDICATOR);
			break;
		case UHF_PORT_POWER:
			/* Yes, writing to the LOW_SPEED bit clears power. */
			OWRITE4(sc, port, UPS_LOW_SPEED);
			break;
		case UHF_C_PORT_CONNECTION:
			OWRITE4(sc, port, UPS_C_CONNECT_STATUS << 16);
			break;
		case UHF_C_PORT_ENABLE:
			OWRITE4(sc, port, UPS_C_PORT_ENABLED << 16);
			break;
		case UHF_C_PORT_SUSPEND:
			OWRITE4(sc, port, UPS_C_SUSPEND << 16);
			break;
		case UHF_C_PORT_OVER_CURRENT:
			OWRITE4(sc, port, UPS_C_OVERCURRENT_INDICATOR << 16);
			break;
		case UHF_C_PORT_RESET:
			OWRITE4(sc, port, UPS_C_PORT_RESET << 16);
			break;
		default:
			err = USBD_IOERROR;
			goto ret;
		}
		switch(value) {
		case UHF_C_PORT_CONNECTION:
		case UHF_C_PORT_ENABLE:
		case UHF_C_PORT_SUSPEND:
		case UHF_C_PORT_OVER_CURRENT:
		case UHF_C_PORT_RESET:
			/* Enable RHSC interrupt if condition is cleared. */
			if ((OREAD4(sc, port) >> 16) == 0)
				ohci_rhsc_able(sc, 1);
			break;
		default:
			break;
		}
		break;
	case C(UR_GET_DESCRIPTOR, UT_READ_CLASS_DEVICE):
		if ((value & 0xff) != 0) {
			err = USBD_IOERROR;
			goto ret;
		}
		v = OREAD4(sc, OHCI_RH_DESCRIPTOR_A);
		hubd = ohci_hubd;
		hubd.bNbrPorts = sc->sc_noport;
		USETW(hubd.wHubCharacteristics,
		      (v & OHCI_NPS ? UHD_PWR_NO_SWITCH :
		       v & OHCI_PSM ? UHD_PWR_GANGED : UHD_PWR_INDIVIDUAL)
		      /* XXX overcurrent */
		      );
		hubd.bPwrOn2PwrGood = OHCI_GET_POTPGT(v);
		v = OREAD4(sc, OHCI_RH_DESCRIPTOR_B);
		for (i = 0, l = sc->sc_noport; l > 0; i++, l -= 8, v >>= 8)
			hubd.DeviceRemovable[i++] = (u_int8_t)v;
		hubd.bDescLength = USB_HUB_DESCRIPTOR_SIZE + i;
		l = min(len, hubd.bDescLength);
		totlen = l;
		memcpy(buf, &hubd, l);
		break;
	case C(UR_GET_STATUS, UT_READ_CLASS_DEVICE):
		if (len != 4) {
			err = USBD_IOERROR;
			goto ret;
		}
		memset(buf, 0, len); /* ? XXX */
		totlen = len;
		break;
	case C(UR_GET_STATUS, UT_READ_CLASS_OTHER):
		DPRINTFN(8,("ohci_root_ctrl_transfer: get port status i=%d\n",
			    index));
		if (index < 1 || index > sc->sc_noport) {
			err = USBD_IOERROR;
			goto ret;
		}
		if (len != 4) {
			err = USBD_IOERROR;
			goto ret;
		}
		v = OREAD4(sc, OHCI_RH_PORT_STATUS(index));
		DPRINTFN(8,("ohci_root_ctrl_transfer: port status=0x%04x\n",
			    v));
		USETW(ps.wPortStatus, v);
		USETW(ps.wPortChange, v >> 16);
		l = min(len, sizeof ps);
		memcpy(buf, &ps, l);
		totlen = l;
		break;
	case C(UR_SET_DESCRIPTOR, UT_WRITE_CLASS_DEVICE):
		err = USBD_IOERROR;
		goto ret;
	case C(UR_SET_FEATURE, UT_WRITE_CLASS_DEVICE):
		break;
	case C(UR_SET_FEATURE, UT_WRITE_CLASS_OTHER):
		if (index < 1 || index > sc->sc_noport) {
			err = USBD_IOERROR;
			goto ret;
		}
		port = OHCI_RH_PORT_STATUS(index);
		switch(value) {
		case UHF_PORT_ENABLE:
			OWRITE4(sc, port, UPS_PORT_ENABLED);
			break;
		case UHF_PORT_SUSPEND:
			OWRITE4(sc, port, UPS_SUSPEND);
			break;
		case UHF_PORT_RESET:
			DPRINTFN(5,("ohci_root_ctrl_transfer: reset port %d\n",
				    index));
			OWRITE4(sc, port, UPS_RESET);
			for (i = 0; i < 5; i++) {
				usb_delay_ms(&sc->sc_bus,
					     USB_PORT_ROOT_RESET_DELAY);
				if (sc->sc_dying) {
					err = USBD_IOERROR;
					goto ret;
				}
				if ((OREAD4(sc, port) & UPS_RESET) == 0)
					break;
			}
			DPRINTFN(8,("ohci port %d reset, status = 0x%04x\n",
				    index, OREAD4(sc, port)));
			break;
		case UHF_PORT_POWER:
			DPRINTFN(2,("ohci_root_ctrl_transfer: set port power "
				    "%d\n", index));
			OWRITE4(sc, port, UPS_PORT_POWER);
			break;
		default:
			err = USBD_IOERROR;
			goto ret;
		}
		break;
	default:
		err = USBD_IOERROR;
		goto ret;
	}
	xfer->actlen = totlen;
	err = USBD_NORMAL_COMPLETION;
 ret:
	xfer->status = err;
	s = splusb();
	usb_transfer_complete(xfer);
	splx(s);
	return (USBD_IN_PROGRESS);
}

/* Abort a root control request. */
static void
ohci_root_ctrl_abort(usbd_xfer_handle xfer)
{
	/* Nothing to do, all transfers are synchronous. */
}

/* Close the root pipe. */
static void
ohci_root_ctrl_close(usbd_pipe_handle pipe)
{
	DPRINTF(("ohci_root_ctrl_close\n"));
	/* Nothing to do. */
}

static usbd_status
ohci_root_intr_transfer(usbd_xfer_handle xfer)
{
	usbd_status err;

	/* Insert last in queue. */
	err = usb_insert_transfer(xfer);
	if (err)
		return (err);

	/* Pipe isn't running, start first */
	return (ohci_root_intr_start(STAILQ_FIRST(&xfer->pipe->queue)));
}

static usbd_status
ohci_root_intr_start(usbd_xfer_handle xfer)
{
	usbd_pipe_handle pipe = xfer->pipe;
	ohci_softc_t *sc = (ohci_softc_t *)pipe->device->bus;

	if (sc->sc_dying)
		return (USBD_IOERROR);

	sc->sc_intrxfer = xfer;

	return (USBD_IN_PROGRESS);
}

/* Abort a root interrupt request. */
static void
ohci_root_intr_abort(usbd_xfer_handle xfer)
{
	int s;

	if (xfer->pipe->intrxfer == xfer) {
		DPRINTF(("ohci_root_intr_abort: remove\n"));
		xfer->pipe->intrxfer = NULL;
	}
	xfer->status = USBD_CANCELLED;
	s = splusb();
	usb_transfer_complete(xfer);
	splx(s);
}

/* Close the root pipe. */
static void
ohci_root_intr_close(usbd_pipe_handle pipe)
{
	ohci_softc_t *sc = (ohci_softc_t *)pipe->device->bus;

	DPRINTF(("ohci_root_intr_close\n"));

	sc->sc_intrxfer = NULL;
}

/************************/

static usbd_status
ohci_device_ctrl_transfer(usbd_xfer_handle xfer)
{
	usbd_status err;

	/* Insert last in queue. */
	err = usb_insert_transfer(xfer);
	if (err)
		return (err);

	/* Pipe isn't running, start first */
	return (ohci_device_ctrl_start(STAILQ_FIRST(&xfer->pipe->queue)));
}

static usbd_status
ohci_device_ctrl_start(usbd_xfer_handle xfer)
{
	ohci_softc_t *sc = (ohci_softc_t *)xfer->pipe->device->bus;
	usbd_status err;

	if (sc->sc_dying)
		return (USBD_IOERROR);

#ifdef DIAGNOSTIC
	if (!(xfer->rqflags & URQ_REQUEST)) {
		/* XXX panic */
		printf("ohci_device_ctrl_transfer: not a request\n");
		return (USBD_INVAL);
	}
#endif

	err = ohci_device_request(xfer);
	if (err)
		return (err);

	if (sc->sc_bus.use_polling)
		ohci_waitintr(sc, xfer);
	return (USBD_IN_PROGRESS);
}

/* Abort a device control request. */
static void
ohci_device_ctrl_abort(usbd_xfer_handle xfer)
{
	DPRINTF(("ohci_device_ctrl_abort: xfer=%p\n", xfer));
	ohci_abort_xfer(xfer, USBD_CANCELLED);
}

/* Close a device control pipe. */
static void
ohci_device_ctrl_close(usbd_pipe_handle pipe)
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)pipe;
	ohci_softc_t *sc = (ohci_softc_t *)pipe->device->bus;

	DPRINTF(("ohci_device_ctrl_close: pipe=%p\n", pipe));
	ohci_close_pipe(pipe, sc->sc_ctrl_head);
	ohci_free_std(sc, opipe->tail.td);
}

/************************/

static void
ohci_device_clear_toggle(usbd_pipe_handle pipe)
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)pipe;

	opipe->sed->ed.ed_headp &= htole32(~OHCI_TOGGLECARRY);
}

static void
ohci_noop(usbd_pipe_handle pipe)
{
}

static usbd_status
ohci_device_bulk_transfer(usbd_xfer_handle xfer)
{
	usbd_status err;

	/* Insert last in queue. */
	err = usb_insert_transfer(xfer);
	if (err)
		return (err);

	/* Pipe isn't running, start first */
	return (ohci_device_bulk_start(STAILQ_FIRST(&xfer->pipe->queue)));
}

static usbd_status
ohci_device_bulk_start(usbd_xfer_handle xfer)
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)xfer->pipe;
	usbd_device_handle dev = opipe->pipe.device;
	ohci_softc_t *sc = (ohci_softc_t *)dev->bus;
	int addr = dev->address;
	ohci_soft_td_t *data, *tail, *tdp;
	ohci_soft_ed_t *sed;
	int s, len, isread, endpt;
	usbd_status err;

	if (sc->sc_dying)
		return (USBD_IOERROR);

#ifdef DIAGNOSTIC
	if (xfer->rqflags & URQ_REQUEST) {
		/* XXX panic */
		printf("ohci_device_bulk_start: a request\n");
		return (USBD_INVAL);
	}
#endif

	len = xfer->length;
	endpt = xfer->pipe->endpoint->edesc->bEndpointAddress;
	isread = UE_GET_DIR(endpt) == UE_DIR_IN;
	sed = opipe->sed;

	DPRINTFN(4,("ohci_device_bulk_start: xfer=%p len=%d isread=%d "
		    "flags=%d endpt=%d\n", xfer, len, isread, xfer->flags,
		    endpt));

	opipe->u.bulk.isread = isread;
	opipe->u.bulk.length = len;

	/* Update device address */
	sed->ed.ed_flags = htole32(
		(le32toh(sed->ed.ed_flags) & ~OHCI_ED_ADDRMASK) |
		OHCI_ED_SET_FA(addr));

	/* Allocate a chain of new TDs (including a new tail). */
	data = opipe->tail.td;
	err = ohci_alloc_std_chain(opipe, sc, len, isread, xfer,
		  data, &tail);
	/* We want interrupt at the end of the transfer. */
	tail->td.td_flags &= htole32(~OHCI_TD_INTR_MASK);
	tail->td.td_flags |= htole32(OHCI_TD_SET_DI(1));
	tail->flags |= OHCI_CALL_DONE;
	tail = tail->nexttd;	/* point at sentinel */
	if (err)
		return (err);

	tail->xfer = NULL;
	xfer->hcpriv = data;

	DPRINTFN(4,("ohci_device_bulk_start: ed_flags=0x%08x td_flags=0x%08x "
		    "td_cbp=0x%08x td_be=0x%08x\n",
		    (int)le32toh(sed->ed.ed_flags),
		    (int)le32toh(data->td.td_flags),
		    (int)le32toh(data->td.td_cbp),
		    (int)le32toh(data->td.td_be)));

#ifdef USB_DEBUG
	if (ohcidebug > 5) {
		ohci_dump_ed(sed);
		ohci_dump_tds(data);
	}
#endif

	/* Insert ED in schedule */
	s = splusb();
	for (tdp = data; tdp != tail; tdp = tdp->nexttd) {
		tdp->xfer = xfer;
	}
	sed->ed.ed_tailp = htole32(tail->physaddr);
	opipe->tail.td = tail;
	sed->ed.ed_flags &= htole32(~OHCI_ED_SKIP);
	OWRITE4(sc, OHCI_COMMAND_STATUS, OHCI_BLF);
	if (xfer->timeout && !sc->sc_bus.use_polling) {
                callout_reset(&xfer->timeout_handle, MS_TO_TICKS(xfer->timeout),
		    ohci_timeout, xfer);
	}

#if 0
/* This goes wrong if we are too slow. */
	if (ohcidebug > 10) {
		delay(10000);
		DPRINTF(("ohci_device_intr_transfer: status=%x\n",
			 OREAD4(sc, OHCI_COMMAND_STATUS)));
		ohci_dump_ed(sed);
		ohci_dump_tds(data);
	}
#endif

	splx(s);

	if (sc->sc_bus.use_polling)
		ohci_waitintr(sc, xfer);

	return (USBD_IN_PROGRESS);
}

static void
ohci_device_bulk_abort(usbd_xfer_handle xfer)
{
	DPRINTF(("ohci_device_bulk_abort: xfer=%p\n", xfer));
	ohci_abort_xfer(xfer, USBD_CANCELLED);
}

/*
 * Close a device bulk pipe.
 */
static void
ohci_device_bulk_close(usbd_pipe_handle pipe)
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)pipe;
	ohci_softc_t *sc = (ohci_softc_t *)pipe->device->bus;

	DPRINTF(("ohci_device_bulk_close: pipe=%p\n", pipe));
	ohci_close_pipe(pipe, sc->sc_bulk_head);
	ohci_free_std(sc, opipe->tail.td);
}

/************************/

static usbd_status
ohci_device_intr_transfer(usbd_xfer_handle xfer)
{
	usbd_status err;

	/* Insert last in queue. */
	err = usb_insert_transfer(xfer);
	if (err)
		return (err);

	/* Pipe isn't running, start first */
	return (ohci_device_intr_start(STAILQ_FIRST(&xfer->pipe->queue)));
}

static usbd_status
ohci_device_intr_start(usbd_xfer_handle xfer)
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)xfer->pipe;
	ohci_softc_t *sc = (ohci_softc_t *)opipe->pipe.device->bus;
	ohci_soft_ed_t *sed = opipe->sed;
	usbd_status err;

	if (sc->sc_dying)
		return (USBD_IOERROR);

	DPRINTFN(3, ("ohci_device_intr_start: xfer=%p len=%d "
		     "flags=%d priv=%p\n",
		     xfer, xfer->length, xfer->flags, xfer->priv));

#ifdef DIAGNOSTIC
	if (xfer->rqflags & URQ_REQUEST)
		panic("ohci_device_intr_start: a request");
#endif

	err = ohci_device_intr_insert(sc, xfer);
	if (err)
		return (err);

	sed->ed.ed_flags &= htole32(~OHCI_ED_SKIP);

	return (USBD_IN_PROGRESS);
}

/*
 * Insert an interrupt transfer into an endpoint descriptor list
 */
static usbd_status
ohci_device_intr_insert(ohci_softc_t *sc, usbd_xfer_handle xfer)
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)xfer->pipe;
	ohci_soft_ed_t *sed = opipe->sed;
	ohci_soft_td_t *data, *tail;
	ohci_physaddr_t dataphys, physend;
	int s;

	DPRINTFN(4, ("ohci_device_intr_insert: xfer=%p", xfer));

	data = opipe->tail.td;
	tail = ohci_alloc_std(sc);
	if (tail == NULL)
		return (USBD_NOMEM);
	tail->xfer = NULL;

	data->td.td_flags = htole32(
		OHCI_TD_IN | OHCI_TD_NOCC |
		OHCI_TD_SET_DI(1) | OHCI_TD_TOGGLE_CARRY);
	if (xfer->flags & USBD_SHORT_XFER_OK)
		data->td.td_flags |= htole32(OHCI_TD_R);
	/*
	 * Assume a short mapping with no complications, which
	 * should always be true for <= 4k buffers in contiguous
	 * virtual memory. The data can take the following forms:
	 *	1 segment in 1 OHCI page
	 *	1 segment in 2 OHCI pages
	 *	2 segments in 2 OHCI pages
	 * (see comment in ohci_alloc_std_chain() for details)
	 */
	KASSERT(xfer->length > 0 && xfer->length <= OHCI_PAGE_SIZE,
	    ("ohci_device_intr_insert: bad length %d", xfer->length));
	dataphys = xfer->dmamap.segs[0].ds_addr;
	physend = dataphys + xfer->length - 1;
	if (xfer->dmamap.nsegs == 2) {
		KASSERT(OHCI_PAGE_OFFSET(dataphys +
		    xfer->dmamap.segs[0].ds_len) == 0,
		    ("ohci_device_intr_insert: bad seg 0 termination"));
		physend = xfer->dmamap.segs[1].ds_addr + xfer->length -
		    xfer->dmamap.segs[0].ds_len - 1;
	} else {
		KASSERT(xfer->dmamap.nsegs == 1,
		    ("ohci_device_intr_insert: bad seg count %d",
		    (u_int)xfer->dmamap.nsegs));
	}
	data->td.td_cbp = htole32(dataphys);
	data->nexttd = tail;
	data->td.td_nexttd = htole32(tail->physaddr);
	data->td.td_be = htole32(physend);
	data->len = xfer->length;
	data->xfer = xfer;
	data->flags = OHCI_CALL_DONE | OHCI_ADD_LEN;
	xfer->hcpriv = data;
	xfer->actlen = 0;

#ifdef USB_DEBUG
	if (ohcidebug > 5) {
		DPRINTF(("ohci_device_intr_insert:\n"));
		ohci_dump_ed(sed);
		ohci_dump_tds(data);
	}
#endif

	/* Insert ED in schedule */
	s = splusb();
	sed->ed.ed_tailp = htole32(tail->physaddr);
	opipe->tail.td = tail;
	splx(s);

	return (USBD_NORMAL_COMPLETION);
}

/* Abort a device control request. */
static void
ohci_device_intr_abort(usbd_xfer_handle xfer)
{
	if (xfer->pipe->intrxfer == xfer) {
		DPRINTF(("ohci_device_intr_abort: remove\n"));
		xfer->pipe->intrxfer = NULL;
	}
	ohci_abort_xfer(xfer, USBD_CANCELLED);
}

/* Close a device interrupt pipe. */
static void
ohci_device_intr_close(usbd_pipe_handle pipe)
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)pipe;
	ohci_softc_t *sc = (ohci_softc_t *)pipe->device->bus;
	int nslots = opipe->u.intr.nslots;
	int pos = opipe->u.intr.pos;
	int j;
	ohci_soft_ed_t *p, *sed = opipe->sed;
	int s;

	DPRINTFN(1,("ohci_device_intr_close: pipe=%p nslots=%d pos=%d\n",
		    pipe, nslots, pos));
	s = splusb();
	sed->ed.ed_flags |= htole32(OHCI_ED_SKIP);
	if ((le32toh(sed->ed.ed_tailp) & OHCI_HEADMASK) !=
	    (le32toh(sed->ed.ed_headp) & OHCI_HEADMASK))
		usb_delay_ms(&sc->sc_bus, 2);
#ifdef DIAGNOSTIC
	if ((le32toh(sed->ed.ed_tailp) & OHCI_HEADMASK) !=
	    (le32toh(sed->ed.ed_headp) & OHCI_HEADMASK))
		panic("%s: Intr pipe %p still has TDs queued",
			device_get_nameunit(sc->sc_bus.bdev), pipe);
#endif

	for (p = sc->sc_eds[pos]; p && p->next != sed; p = p->next)
		;
#ifdef DIAGNOSTIC
	if (p == NULL)
		panic("ohci_device_intr_close: ED not found");
#endif
	p->next = sed->next;
	p->ed.ed_nexted = sed->ed.ed_nexted;
	splx(s);

	for (j = 0; j < nslots; j++)
		--sc->sc_bws[(pos * nslots + j) % OHCI_NO_INTRS];

	ohci_free_std(sc, opipe->tail.td);
	ohci_free_sed(sc, opipe->sed);
}

static usbd_status
ohci_device_setintr(ohci_softc_t *sc, struct ohci_pipe *opipe, int ival)
{
	int i, j, s, best;
	u_int npoll, slow, shigh, nslots;
	u_int bestbw, bw;
	ohci_soft_ed_t *hsed, *sed = opipe->sed;

	DPRINTFN(2, ("ohci_setintr: pipe=%p\n", opipe));
	if (ival == 0) {
		printf("ohci_setintr: 0 interval\n");
		return (USBD_INVAL);
	}

	npoll = OHCI_NO_INTRS;
	while (npoll > ival)
		npoll /= 2;
	DPRINTFN(2, ("ohci_setintr: ival=%d npoll=%d\n", ival, npoll));

	/*
	 * We now know which level in the tree the ED must go into.
	 * Figure out which slot has most bandwidth left over.
	 * Slots to examine:
	 * npoll
	 * 1	0
	 * 2	1 2
	 * 4	3 4 5 6
	 * 8	7 8 9 10 11 12 13 14
	 * N    (N-1) .. (N-1+N-1)
	 */
	slow = npoll-1;
	shigh = slow + npoll;
	nslots = OHCI_NO_INTRS / npoll;
	for (best = i = slow, bestbw = ~0; i < shigh; i++) {
		bw = 0;
		for (j = 0; j < nslots; j++)
			bw += sc->sc_bws[(i * nslots + j) % OHCI_NO_INTRS];
		if (bw < bestbw) {
			best = i;
			bestbw = bw;
		}
	}
	DPRINTFN(2, ("ohci_setintr: best=%d(%d..%d) bestbw=%d\n",
		     best, slow, shigh, bestbw));

	s = splusb();
	hsed = sc->sc_eds[best];
	sed->next = hsed->next;
	sed->ed.ed_nexted = hsed->ed.ed_nexted;
	hsed->next = sed;
	hsed->ed.ed_nexted = htole32(sed->physaddr);
	splx(s);

	for (j = 0; j < nslots; j++)
		++sc->sc_bws[(best * nslots + j) % OHCI_NO_INTRS];
	opipe->u.intr.nslots = nslots;
	opipe->u.intr.pos = best;

	DPRINTFN(5, ("ohci_setintr: returns %p\n", opipe));
	return (USBD_NORMAL_COMPLETION);
}

/***********************/

usbd_status
ohci_device_isoc_transfer(usbd_xfer_handle xfer)
{
	usbd_status err;

	DPRINTFN(5,("ohci_device_isoc_transfer: xfer=%p\n", xfer));

	/* Put it on our queue, */
	err = usb_insert_transfer(xfer);

	/* bail out on error, */
	if (err && err != USBD_IN_PROGRESS)
		return (err);

	/* XXX should check inuse here */

	/* insert into schedule, */
	ohci_device_isoc_enter(xfer);

	/* and start if the pipe wasn't running */
	if (!err)
		ohci_device_isoc_start(STAILQ_FIRST(&xfer->pipe->queue));

	return (err);
}

void
ohci_device_isoc_enter(usbd_xfer_handle xfer)
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)xfer->pipe;
	usbd_device_handle dev = opipe->pipe.device;
	ohci_softc_t *sc = (ohci_softc_t *)dev->bus;
	ohci_soft_ed_t *sed = opipe->sed;
	struct iso *iso = &opipe->u.iso;
	struct usb_dma_mapping *dma = &xfer->dmamap;
	ohci_soft_itd_t *sitd, *nsitd;
	ohci_physaddr_t dataphys, bp0, physend, prevpage;
	int curlen, i, len, ncur, nframes, npages, seg, segoff;
	int s;

	DPRINTFN(1,("ohci_device_isoc_enter: used=%d next=%d xfer=%p "
		    "nframes=%d\n",
		    iso->inuse, iso->next, xfer, xfer->nframes));

	if (sc->sc_dying)
		return;

	if (iso->next == -1) {
		/* Not in use yet, schedule it a few frames ahead. */
		iso->next = le32toh(sc->sc_hcca->hcca_frame_number) + 5;
		DPRINTFN(2,("ohci_device_isoc_enter: start next=%d\n",
			    iso->next));
	}

	sitd = opipe->tail.itd;
	nframes = xfer->nframes;
	xfer->hcpriv = sitd;
	seg = 0;
	segoff = 0;
	i = 0;
	while (i < nframes) {
		/*
		 * Fill in as many ITD frames as possible.
		 */
		KASSERT(seg < dma->nsegs, ("ohci_device_isoc_enter: overrun"));
		bp0 = dma->segs[seg].ds_addr + segoff;
		sitd->itd.itd_bp0 = htole32(bp0);
		prevpage = OHCI_PAGE(bp0);
		npages = 1;

		ncur = 0;
		while (ncur < OHCI_ITD_NOFFSET && i < nframes) {
			/* Find the frame start and end physical addresses. */
			len = xfer->frlengths[i];
			dataphys = dma->segs[seg].ds_addr + segoff;
			curlen = dma->segs[seg].ds_len - segoff;
			if (len > curlen) {
				KASSERT(seg + 1 < dma->nsegs,
				    ("ohci_device_isoc_enter: overrun2"));
				seg++;
				segoff = len - curlen;
			} else {
				segoff += len;
			}
			KASSERT(segoff <= dma->segs[seg].ds_len,
			    ("ohci_device_isoc_enter: overrun3"));
			physend = dma->segs[seg].ds_addr + segoff - 1;

			/* Check if there would be more than 2 pages . */
			if (OHCI_PAGE(dataphys) != prevpage) {
				prevpage = OHCI_PAGE(dataphys);
				npages++;
			}
			if (OHCI_PAGE(physend) != prevpage) {
				prevpage = OHCI_PAGE(physend);
				npages++;
			}
			if (npages > 2) {
				/* We cannot fit this frame now. */
				segoff -= len;
				if (segoff < 0) {
					seg--;
					segoff += dma->segs[seg].ds_len;
				}
				break;
			}

			sitd->itd.itd_be = htole32(physend);
			sitd->itd.itd_offset[ncur] =
			    htole16(OHCI_ITD_MK_OFFS(OHCI_PAGE(dataphys) ==
			    OHCI_PAGE(bp0) ? 0 : 1, dataphys));
			i++;
			ncur++;
		}
		if (segoff >= dma->segs[seg].ds_len) {
			KASSERT(segoff == dma->segs[seg].ds_len,
			    ("ohci_device_isoc_enter: overlap"));
			seg++;
			segoff = 0;
		}

		/* Allocate next ITD */
		nsitd = ohci_alloc_sitd(sc);
		if (nsitd == NULL) {
			/* XXX what now? */
			printf("%s: isoc TD alloc failed\n",
			       device_get_nameunit(sc->sc_bus.bdev));
			return;
		}

		/* Fill out remaining fields of current ITD */
		sitd->nextitd = nsitd;
		sitd->itd.itd_nextitd = htole32(nsitd->physaddr);
		sitd->xfer = xfer;
		if (i < nframes) {
			sitd->itd.itd_flags = htole32(
				OHCI_ITD_NOCC |
				OHCI_ITD_SET_SF(iso->next) |
				OHCI_ITD_SET_DI(6) | /* delay intr a little */
				OHCI_ITD_SET_FC(ncur));
			sitd->flags = OHCI_ITD_ACTIVE;
		} else {
			sitd->itd.itd_flags = htole32(
				OHCI_ITD_NOCC |
				OHCI_ITD_SET_SF(iso->next) |
				OHCI_ITD_SET_DI(0) |
				OHCI_ITD_SET_FC(ncur));
			sitd->flags = OHCI_CALL_DONE | OHCI_ITD_ACTIVE;
		}
		iso->next += ncur;

		sitd = nsitd;
	}

	iso->inuse += nframes;

	/* XXX pretend we did it all */
	xfer->actlen = 0;
	for (i = 0; i < nframes; i++)
		xfer->actlen += xfer->frlengths[i];

	xfer->status = USBD_IN_PROGRESS;

#ifdef USB_DEBUG
	if (ohcidebug > 5) {
		DPRINTF(("ohci_device_isoc_enter: frame=%d\n",
			 le32toh(sc->sc_hcca->hcca_frame_number)));
		ohci_dump_itds(xfer->hcpriv);
		ohci_dump_ed(sed);
	}
#endif

	s = splusb();
	opipe->tail.itd = sitd;
	sed->ed.ed_flags &= htole32(~OHCI_ED_SKIP);
	sed->ed.ed_tailp = htole32(sitd->physaddr);
	splx(s);

#ifdef USB_DEBUG
	if (ohcidebug > 5) {
		delay(150000);
		DPRINTF(("ohci_device_isoc_enter: after frame=%d\n",
			 le32toh(sc->sc_hcca->hcca_frame_number)));
		ohci_dump_itds(xfer->hcpriv);
		ohci_dump_ed(sed);
	}
#endif
}

usbd_status
ohci_device_isoc_start(usbd_xfer_handle xfer)
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)xfer->pipe;
	ohci_softc_t *sc = (ohci_softc_t *)opipe->pipe.device->bus;
	ohci_soft_ed_t *sed;
	int s;

	DPRINTFN(5,("ohci_device_isoc_start: xfer=%p\n", xfer));

	if (sc->sc_dying)
		return (USBD_IOERROR);

#ifdef DIAGNOSTIC
	if (xfer->status != USBD_IN_PROGRESS)
		printf("ohci_device_isoc_start: not in progress %p\n", xfer);
#endif

	/* XXX anything to do? */

	s = splusb();
	sed = opipe->sed;  /*  Turn off ED skip-bit to start processing */
	sed->ed.ed_flags &= htole32(~OHCI_ED_SKIP);    /* ED's ITD list.*/
	splx(s);

	return (USBD_IN_PROGRESS);
}

void
ohci_device_isoc_abort(usbd_xfer_handle xfer)
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)xfer->pipe;
	ohci_softc_t *sc = (ohci_softc_t *)opipe->pipe.device->bus;
	ohci_soft_ed_t *sed;
	ohci_soft_itd_t *sitd, *sitdnext, *tmp_sitd;
	int s,undone,num_sitds;

	s = splusb();
	opipe->aborting = 1;

	DPRINTFN(1,("ohci_device_isoc_abort: xfer=%p\n", xfer));

	/* Transfer is already done. */
	if (xfer->status != USBD_NOT_STARTED &&
	    xfer->status != USBD_IN_PROGRESS) {
		splx(s);
		printf("ohci_device_isoc_abort: early return\n");
		return;
	}

	/* Give xfer the requested abort code. */
	xfer->status = USBD_CANCELLED;

	sed = opipe->sed;
	sed->ed.ed_flags |= htole32(OHCI_ED_SKIP); /* force hardware skip */

	num_sitds = 0;
	sitd = xfer->hcpriv;
#ifdef DIAGNOSTIC
	if (sitd == NULL) {
		splx(s);
		printf("ohci_device_isoc_abort: hcpriv==0\n");
		return;
	}
#endif
	for (; sitd != NULL && sitd->xfer == xfer; sitd = sitd->nextitd) {
		num_sitds++;
#ifdef DIAGNOSTIC
		DPRINTFN(1,("abort sets done sitd=%p\n", sitd));
		sitd->isdone = 1;
#endif
	}

	splx(s);

	/*
	 * Each sitd has up to OHCI_ITD_NOFFSET transfers, each can
	 * take a usb 1ms cycle. Conservatively wait for it to drain.
	 * Even with DMA done, it can take awhile for the "batch"
	 * delivery of completion interrupts to occur thru the controller.
	 */
 
	do {
		usb_delay_ms(&sc->sc_bus, 2*(num_sitds*OHCI_ITD_NOFFSET));

		undone   = 0;
		tmp_sitd = xfer->hcpriv;
		for (; tmp_sitd != NULL && tmp_sitd->xfer == xfer;
		    tmp_sitd = tmp_sitd->nextitd) {
			if (OHCI_CC_NO_ERROR ==
			    OHCI_ITD_GET_CC(le32toh(tmp_sitd->itd.itd_flags)) &&
			    tmp_sitd->flags & OHCI_ITD_ACTIVE &&
			    (tmp_sitd->flags & OHCI_ITD_INTFIN) == 0)
				undone++;
		}
	} while( undone != 0 );

	/* Free the sitds */
 	for (sitd = xfer->hcpriv; sitd->xfer == xfer;
	    sitd = sitdnext) {
		sitdnext = sitd->nextitd;
		ohci_free_sitd(sc, sitd);
	}

	s = splusb();

	/* Run callback. */
	usb_transfer_complete(xfer);

	/* There is always a `next' sitd so link it up. */
	sed->ed.ed_headp = htole32(sitd->physaddr);

	sed->ed.ed_flags &= htole32(~OHCI_ED_SKIP); /* remove hardware skip */

	splx(s);
}

void
ohci_device_isoc_done(usbd_xfer_handle xfer)
{
	/* This null routine corresponds to non-isoc "done()" routines
	 * that free the stds associated with an xfer after a completed
	 * xfer interrupt. However, in the case of isoc transfers, the
	 * sitds associated with the transfer have already been processed
	 * and reallocated for the next iteration by
	 * "ohci_device_isoc_transfer()".
	 *
	 * Routine "usb_transfer_complete()" is called at the end of every
	 * relevant usb interrupt. "usb_transfer_complete()" indirectly
	 * calls 1) "ohci_device_isoc_transfer()" (which keeps pumping the
	 * pipeline by setting up the next transfer iteration) and 2) then 
	 * calls "ohci_device_isoc_done()". Isoc transfers have not been 
	 * working for the ohci usb because this routine was trashing the
	 * xfer set up for the next iteration (thus, only the first 
	 * UGEN_NISOREQS xfers outstanding on an open would work). Perhaps
	 * this could all be re-factored, but that's another pass...
	 */
}

usbd_status
ohci_setup_isoc(usbd_pipe_handle pipe)
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)pipe;
	ohci_softc_t *sc = (ohci_softc_t *)pipe->device->bus;
	struct iso *iso = &opipe->u.iso;
	int s;

	iso->next = -1;
	iso->inuse = 0;

	s = splusb();
	ohci_add_ed(opipe->sed, sc->sc_isoc_head);
	splx(s);

	return (USBD_NORMAL_COMPLETION);
}

void
ohci_device_isoc_close(usbd_pipe_handle pipe)
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)pipe;
	ohci_softc_t *sc = (ohci_softc_t *)pipe->device->bus;
	ohci_soft_ed_t *sed;

	DPRINTF(("ohci_device_isoc_close: pipe=%p\n", pipe));

	sed = opipe->sed;
	sed->ed.ed_flags |= htole32(OHCI_ED_SKIP); /* Stop device. */

	ohci_close_pipe(pipe, sc->sc_isoc_head); /* Stop isoc list, free ED.*/

	/* up to NISOREQs xfers still outstanding. */

#ifdef DIAGNOSTIC
	opipe->tail.itd->isdone = 1;
#endif
	ohci_free_sitd(sc, opipe->tail.itd);	/* Next `avail free' sitd.*/
}
