/*
 * ng_ubt_mtk.c
 */

/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019, 2021 Vladimir Kondratyev <wulf@FreeBSD.org>
 * Copyright (c) 2026 Lucas Holt <luke@foolishgames.com>
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * MediaTek MT79xx Bluetooth controllers come up running a boot ROM that
 * refuses every standard HCI command with "Command Disallowed" until a
 * patch is downloaded and the Bluetooth function is turned on with vendor
 * WMT commands.  This driver keeps ng_ubt away from such a controller
 * until mtkbtfw(8) has done that, then lets it attach normally.
 */

#include <sys/types.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/taskqueue.h>

#include "usbdevs.h"
#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>

#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>
#include <netgraph/ng_parse.h>
#include <netgraph/bluetooth/include/ng_bluetooth.h>
#include <netgraph/bluetooth/include/ng_hci.h>
#include <netgraph/bluetooth/include/ng_ubt.h>
#include <netgraph/bluetooth/drivers/ubt/ng_ubt_var.h>

static device_probe_t	ubt_mtk_probe;

/*
 * List of supported bluetooth devices. If you add a new device PID here
 * ensure that it is supported by the mtkbtfw utility and listed in
 * mtkbtfw.conf.  ng_ubt ignores every device on this list.
 */

const STRUCT_USB_HOST_ID ubt_mtk_devs[] = {
	/* MT7920 */
	{ USB_VPI(0x0489, 0xe134, 0) },
	{ USB_VPI(0x0489, 0xe135, 0) },
	{ USB_VPI(0x0489, 0xe158, 0) },
	{ USB_VPI(0x13d3, 0x3620, 0) },
	{ USB_VPI(0x13d3, 0x3621, 0) },
	{ USB_VPI(0x13d3, 0x3622, 0) },
	/* MT7921 */
	{ USB_VPI(0x0e8d, 0x0608, 0) },
	{ USB_VPI(0x0489, 0xe0c8, 0) },
	{ USB_VPI(0x0489, 0xe0cd, 0) },
	{ USB_VPI(0x0489, 0xe0e0, 0) },
	{ USB_VPI(0x0489, 0xe0f2, 0) },
	{ USB_VPI(0x04ca, 0x3802, 0) },
	{ USB_VPI(0x13d3, 0x3563, 0) },
	{ USB_VPI(0x13d3, 0x3564, 0) },
	{ USB_VPI(0x13d3, 0x3567, 0) },
	{ USB_VPI(0x13d3, 0x3576, 0) },
	{ USB_VPI(0x13d3, 0x3578, 0) },
	{ USB_VPI(0x13d3, 0x3583, 0) },
	{ USB_VPI(0x13d3, 0x3606, 0) },
	/* MT7902 */
	{ USB_VPI(0x0e8d, 0x1ede, 0) },
	{ USB_VPI(0x0489, 0xe156, 0) },
	{ USB_VPI(0x13d3, 0x3579, 0) },
	{ USB_VPI(0x13d3, 0x3580, 0) },
	{ USB_VPI(0x13d3, 0x3594, 0) },
	{ USB_VPI(0x13d3, 0x3596, 0) },
	/* MT7922 */
	{ USB_VPI(0x0e8d, 0x223c, 0) },
	{ USB_VPI(0x0489, 0xe0d8, 0) },
	{ USB_VPI(0x0489, 0xe0d9, 0) },
	{ USB_VPI(0x0489, 0xe0e2, 0) },
	{ USB_VPI(0x0489, 0xe0e4, 0) },
	{ USB_VPI(0x0489, 0xe0f1, 0) },
	{ USB_VPI(0x0489, 0xe0f5, 0) },
	{ USB_VPI(0x0489, 0xe0f6, 0) },
	{ USB_VPI(0x0489, 0xe102, 0) },
	{ USB_VPI(0x0489, 0xe11d, 0) },
	{ USB_VPI(0x0489, 0xe152, 0) },
	{ USB_VPI(0x0489, 0xe153, 0) },
	{ USB_VPI(0x0489, 0xe170, 0) },
	{ USB_VPI(0x0489, 0xe174, 0) },
	{ USB_VPI(0x04ca, 0x3804, 0) },
	{ USB_VPI(0x04ca, 0x3807, 0) },
	{ USB_VPI(0x04ca, 0x38e4, 0) },
	{ USB_VPI(0x13d3, 0x3568, 0) },
	{ USB_VPI(0x13d3, 0x3584, 0) },
	{ USB_VPI(0x13d3, 0x3585, 0) },
	{ USB_VPI(0x13d3, 0x3605, 0) },
	{ USB_VPI(0x13d3, 0x3607, 0) },
	{ USB_VPI(0x13d3, 0x3610, 0) },
	{ USB_VPI(0x13d3, 0x3614, 0) },
	{ USB_VPI(0x13d3, 0x3615, 0) },
	{ USB_VPI(0x13d3, 0x3633, 0) },
	{ USB_VPI(0x35f5, 0x7922, 0) },
	/* MT7925 */
	{ USB_VPI(0x0e8d, 0x8c38, 0) },
	{ USB_VPI(0x0489, 0xe111, 0) },
	{ USB_VPI(0x0489, 0xe113, 0) },
	{ USB_VPI(0x0489, 0xe118, 0) },
	{ USB_VPI(0x0489, 0xe11e, 0) },
	{ USB_VPI(0x0489, 0xe124, 0) },
	{ USB_VPI(0x0489, 0xe139, 0) },
	{ USB_VPI(0x0489, 0xe14e, 0) },
	{ USB_VPI(0x0489, 0xe14f, 0) },
	{ USB_VPI(0x0489, 0xe150, 0) },
	{ USB_VPI(0x0489, 0xe151, 0) },
	{ USB_VPI(0x13d3, 0x3602, 0) },
	{ USB_VPI(0x13d3, 0x3603, 0) },
	{ USB_VPI(0x13d3, 0x3604, 0) },
	{ USB_VPI(0x13d3, 0x3608, 0) },
	{ USB_VPI(0x13d3, 0x3609, 0) },
	{ USB_VPI(0x13d3, 0x3613, 0) },
	{ USB_VPI(0x13d3, 0x3625, 0) },
	{ USB_VPI(0x13d3, 0x3627, 0) },
};
const size_t ubt_mtk_devs_sizeof = sizeof(ubt_mtk_devs);

/*
 * Execute generic HCI command and return response in provided buffer.
 */

static usb_error_t
ubt_mtk_do_hci_request(struct usb_device *udev, uint16_t opcode,
    void *resp, uint8_t resp_len)
{
#define	UBT_MTK_HCICMD_TIMEOUT	2000	/* ms */
	struct ubt_hci_event_command_compl *evt;
	struct ubt_hci_cmd cmd;
	usb_error_t error;

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = htole16(opcode);
	evt = malloc(offsetof(struct ubt_hci_event_command_compl, data) +
	    resp_len, M_TEMP, M_ZERO | M_WAITOK);
	evt->header.length = resp_len + UBT_HCI_EVENT_COMPL_HEAD_SIZE;

	error = ubt_do_hci_request(udev, &cmd, evt, UBT_MTK_HCICMD_TIMEOUT);
	if (error != USB_ERR_NORMAL_COMPLETION)
		goto exit;

	if (evt->header.event == NG_HCI_EVENT_COMMAND_COMPL &&
	    evt->header.length == resp_len + UBT_HCI_EVENT_COMPL_HEAD_SIZE)
		memcpy(resp, evt->data, resp_len);
	else
		error = USB_ERR_INVAL;
exit:
	free(evt, M_TEMP);
	return (error);
}

/*
 * Probe for a MediaTek Bluetooth device.  The boot ROM answers
 * Read_Local_Version_Information with a short Command Complete event
 * carrying status 0x0c, so only an operational controller passes.
 */

static int
ubt_mtk_probe(device_t dev)
{
	struct usb_attach_arg	*uaa = device_get_ivars(dev);
	ng_hci_read_local_ver_rp ver;
	int error;

	if (uaa->usb_mode != USB_MODE_HOST)
		return (ENXIO);

	if (uaa->info.bIfaceIndex != 0)
		return (ENXIO);

	error = usbd_lookup_id_by_uaa(ubt_mtk_devs, sizeof(ubt_mtk_devs), uaa);
	if (error != 0)
		return (error);

	if (ubt_mtk_do_hci_request(uaa->device,
	    NG_HCI_OPCODE(NG_HCI_OGF_INFO, NG_HCI_OCF_READ_LOCAL_VER),
	    &ver, sizeof(ver)) != USB_ERR_NORMAL_COMPLETION)
		return (ENXIO);

	if (ver.status != 0)
		return (ENXIO);

	return (BUS_PROBE_DEFAULT);
}

/*
 * Module interface. Attach and detach methods, netgraph node type
 * registration and PNP string are inherited from ng_ubt.c driver.
 */

static device_method_t	ubt_mtk_methods[] = {
	DEVMETHOD(device_probe,	ubt_mtk_probe),
	DEVMETHOD_END
};

DEFINE_CLASS_1(ubt, ubt_mtk_driver, ubt_mtk_methods,
    sizeof(struct ubt_softc), ubt_driver);
DRIVER_MODULE(ng_ubt_mtk, uhub, ubt_mtk_driver, ubt_devclass, 0, 0);
MODULE_VERSION(ng_ubt_mtk, NG_BLUETOOTH_VERSION);
MODULE_DEPEND(ng_ubt_mtk, netgraph, NG_ABI_VERSION, NG_ABI_VERSION,
    NG_ABI_VERSION);
MODULE_DEPEND(ng_ubt_mtk, ng_hci, NG_BLUETOOTH_VERSION, NG_BLUETOOTH_VERSION,
    NG_BLUETOOTH_VERSION);
MODULE_DEPEND(ng_ubt_mtk, usb, 1, 1, 1);
