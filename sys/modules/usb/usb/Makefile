#
#
# Copyright (c) 2008 Hans Petter Selasky. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

S=     ${SRCTOP}/sys

.PATH: $S/dev/usb $S/dev/usb/controller

KMOD=	usb
SRCS=	bus_if.h device_if.h usb_if.h usb_if.c vnode_if.h \
	opt_usb.h opt_bus.h opt_ddb.h \
	usb_pf.h usbdevs.h usbdevs_data.h \
	usb_busdma.c usb_controller.c usb_core.c usb_debug.c \
	usb_dev.c usb_device.c usb_dynamic.c usb_error.c usb_generic.c \
	usb_handle_request.c usb_hid.c usb_hub.c usb_lookup.c usb_mbuf.c \
	usb_msctest.c usb_parse.c usb_pf.c usb_process.c usb_request.c \
	usb_transfer.c usb_util.c 

.if !empty(OPT_FDT)
SRCS+=	usb_fdt_support.c ofw_bus_if.h
.endif

.include <bsd.kmod.mk>
