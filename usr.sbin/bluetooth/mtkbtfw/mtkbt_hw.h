/*-
 * SPDX-License-Identifier: BSD-2-Clause AND ISC
 *
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
 *
 * The firmware image layout, register addresses and WMT command encoding
 * are derived from the Linux btmtk driver (drivers/bluetooth/btmtk.[ch]),
 * which carries the following notice:
 *
 * Copyright (C) 2021 MediaTek Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifndef	__MTKBT_HW_H__
#define	__MTKBT_HW_H__

#include <stdint.h>

#include <libusb.h>

#include "mtkbt_fw.h"

#define	MTKBT_CONTROL_TIMEOUT		2000	/* ms */

/* Chip registers, read through the vendor control request 0x63 */
#define	MTKBT_REG_DEV_ID		0x70010200
#define	MTKBT_REG_FW_FLAVOR		0x70010020
#define	MTKBT_REG_FW_VERSION		0x80021004

/* "UHW" registers, accessed through vendor control requests 0x01/0x02 */
#define	MTKBT_UHW_EP_RST_OPT		0x74011890
#define	MTKBT_UHW_EP_RST_IN_OUT_OPT	0x00010001

/* Vendor HCI command carrying a WMT command */
#define	MTKBT_HCI_OPCODE_WMT		0xfc6f
#define	MTKBT_HCI_EVENT_WMT		0xe4

#define	MTKBT_HCI_MAX_CMD_SIZE		(3 + 255)
#define	MTKBT_HCI_MAX_EVENT_SIZE	64

/* WMT header: dir, op, dlen (le16), flag */
#define	MTKBT_WMT_HDR_SIZE		5
#define	MTKBT_WMT_MAX_DATA		(255 - MTKBT_WMT_HDR_SIZE)

/* WMT operations */
#define	MTKBT_WMT_PATCH_DWNLD		0x01
#define	MTKBT_WMT_FUNC_CTRL		0x06

/* WMT_PATCH_DWNLD packet position flags */
#define	MTKBT_WMT_PKT_START		1
#define	MTKBT_WMT_PKT_CONTINUE		2
#define	MTKBT_WMT_PKT_END		3

/* WMT_FUNC_CTRL flag for a set request */
#define	MTKBT_WMT_FUNC_SET		0

/* Decoded WMT status */
enum {
	MTKBT_WMT_INVALID,
	MTKBT_WMT_PATCH_UNDONE,
	MTKBT_WMT_PATCH_PROGRESS,
	MTKBT_WMT_PATCH_DONE,
	MTKBT_WMT_ON_UNDONE,
	MTKBT_WMT_ON_DONE,
	MTKBT_WMT_ON_PROGRESS,
};

struct mtkbt_chip {
	uint32_t	dev_id;
	uint32_t	fw_version;
	uint32_t	fw_flavor;
};

int	mtkbt_get_chip(struct libusb_device_handle *hdl,
	    struct mtkbt_chip *chip);
int	mtkbt_func_enable(struct libusb_device_handle *hdl);
int	mtkbt_load_fw(struct libusb_device_handle *hdl,
	    const struct mtkbt_firmware *fw, int *downloaded);
int	mtkbt_ep_reset(struct libusb_device_handle *hdl);
int	mtkbt_hci_read_local_ver(struct libusb_device_handle *hdl,
	    uint8_t *status);

#endif
