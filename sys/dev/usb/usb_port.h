/*	$OpenBSD: usb_port.h,v 1.18 2000/09/06 22:42:10 rahnds Exp $ */
/*	$NetBSD: usb_port.h,v 1.54 2002/03/28 21:49:19 ichiro Exp $	*/
/*	$FreeBSD: release/7.0.0/sys/dev/usb/usb_port.h 171286 2007-07-06 20:02:37Z imp $       */

/* Also already merged from NetBSD:
 *	$NetBSD: usb_port.h,v 1.57 2002/09/27 20:42:01 thorpej Exp $
 *	$NetBSD: usb_port.h,v 1.58 2002/10/01 01:25:26 thorpej Exp $
 */

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

#ifndef _USB_PORT_H
#define _USB_PORT_H

/*
 * Macro's to cope with the differences between operating systems.
 */

/*
 * FreeBSD
 */

/* We don't use the soft interrupt code in FreeBSD. */
#if 0
#define USB_USE_SOFTINTR
#endif

#define Static static

#define device_ptr_t device_t
#define USBBASEDEVICE device_t
#define USBDEV(bdev) (bdev)
#define USBDEVNAME(bdev) device_get_nameunit(bdev)
#define USBDEVPTRNAME(bdev) device_get_nameunit(bdev)
#define USBDEVUNIT(bdev) device_get_unit(bdev)
#define USBGETSOFTC(bdev) (device_get_softc(bdev))

#define DECLARE_USB_DMA_T \
	struct usb_dma_block; \
	typedef struct { \
		struct usb_dma_block *block; \
		u_int offs; \
		u_int len; \
	} usb_dma_t

typedef struct thread *usb_proc_ptr;

#define uio_procp uio_td

#define usb_kthread_create1(f, s, p, a0, a1) \
		kthread_create((f), (s), (p), RFHIGHPID, 0, (a0), (a1))
#define usb_kthread_create2(f, s, p, a0) \
		kthread_create((f), (s), (p), RFHIGHPID, 0, (a0))
#define usb_kthread_create	kthread_create

#define	config_pending_incr()
#define	config_pending_decr()

typedef struct callout usb_callout_t;
#define usb_callout_init(h)     callout_init(&(h), 0)
#define usb_callout(h, t, f, d) callout_reset(&(h), (t), (f), (d))
#define usb_uncallout(h, f, d)  callout_stop(&(h))
#define usb_uncallout_drain(h, f, d)  callout_drain(&(h))

#define clalloc(p, s, x) (clist_alloc_cblocks((p), (s), (s)), 0)
#define clfree(p) clist_free_cblocks((p))

#define config_detach(dev, flag) \
	do { \
		device_detach(dev); \
		free(device_get_ivars(dev), M_USB); \
		device_delete_child(device_get_parent(dev), dev); \
	} while (0);

typedef struct malloc_type *usb_malloc_type;

#define USB_DECLARE_DRIVER_INIT(dname, init...) \
static device_probe_t __CONCAT(dname,_match); \
static device_attach_t __CONCAT(dname,_attach); \
static device_detach_t __CONCAT(dname,_detach); \
\
static devclass_t __CONCAT(dname,_devclass); \
\
static device_method_t __CONCAT(dname,_methods)[] = { \
        DEVMETHOD(device_probe, __CONCAT(dname,_match)), \
        DEVMETHOD(device_attach, __CONCAT(dname,_attach)), \
        DEVMETHOD(device_detach, __CONCAT(dname,_detach)), \
	init, \
        {0,0} \
}; \
\
static driver_t __CONCAT(dname,_driver) = { \
        #dname, \
        __CONCAT(dname,_methods), \
        sizeof(struct __CONCAT(dname,_softc)) \
}; \
MODULE_DEPEND(dname, usb, 1, 1, 1)


#define METHODS_NONE			{0,0}
#define USB_DECLARE_DRIVER(dname)	USB_DECLARE_DRIVER_INIT(dname, METHODS_NONE)

#define USB_MATCH(dname) \
static int \
__CONCAT(dname,_match)(device_t self)

#define USB_MATCH_START(dname, uaa) \
        struct usb_attach_arg *uaa = device_get_ivars(self)

#define USB_MATCH_SETUP \
	sc->sc_dev = self

#define USB_ATTACH(dname) \
static int \
__CONCAT(dname,_attach)(device_t self)

#define USB_ATTACH_START(dname, sc, uaa) \
        struct __CONCAT(dname,_softc) *sc = device_get_softc(self); \
        struct usb_attach_arg *uaa = device_get_ivars(self)

/* Returns from attach */
#define USB_ATTACH_ERROR_RETURN	return ENXIO
#define USB_ATTACH_SUCCESS_RETURN	return 0

#define USB_ATTACH_SETUP \
	sc->sc_dev = self; \

#define USB_DETACH(dname) \
static int \
__CONCAT(dname,_detach)(device_t self)

#define USB_DETACH_START(dname, sc) \
	struct __CONCAT(dname,_softc) *sc = device_get_softc(self)

#define USB_GET_SC_OPEN(dname, unit, sc) \
	sc = devclass_get_softc(__CONCAT(dname,_devclass), unit); \
	if (sc == NULL) \
		return (ENXIO)

#define USB_GET_SC(dname, unit, sc) \
	sc = devclass_get_softc(__CONCAT(dname,_devclass), unit)

#define USB_DO_ATTACH(dev, bdev, parent, args, print, sub) \
	(device_probe_and_attach((bdev)) == 0 ? (bdev) : 0)

/* conversion from one type of queue to the other */
#define SIMPLEQ_REMOVE_HEAD	STAILQ_REMOVE_HEAD
#define SIMPLEQ_INSERT_HEAD	STAILQ_INSERT_HEAD
#define SIMPLEQ_INSERT_TAIL	STAILQ_INSERT_TAIL
#define SIMPLEQ_NEXT		STAILQ_NEXT
#define SIMPLEQ_FIRST		STAILQ_FIRST
#define SIMPLEQ_HEAD		STAILQ_HEAD
#define SIMPLEQ_EMPTY		STAILQ_EMPTY
#define SIMPLEQ_FOREACH		STAILQ_FOREACH
#define SIMPLEQ_INIT		STAILQ_INIT
#define SIMPLEQ_HEAD_INITIALIZER	STAILQ_HEAD_INITIALIZER
#define SIMPLEQ_ENTRY		STAILQ_ENTRY

#include <sys/syslog.h>
/*
#define logprintf(args...)	log(LOG_DEBUG, args)
*/
#define logprintf		printf

#endif /* _USB_PORT_H */
