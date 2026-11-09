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

#include <sys/param.h>
#include <sys/endian.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <libusb.h>

#include "mtkbt_fw.h"
#include "mtkbt_hw.h"
#include "mtkbt_dbg.h"

#define	MTKBT_INTERRUPT_ENDPOINT_ADDR	0x81

/* WMT event polling: up to MTKBT_WMT_POLL_MAX tries, MTKBT_WMT_POLL_US apart */
#define	MTKBT_WMT_POLL_US		500
#define	MTKBT_WMT_POLL_MAX		20000

/* Retries while the chip reports a patch download already in progress */
#define	MTKBT_PATCH_PROGRESS_RETRY	20
#define	MTKBT_PATCH_PROGRESS_US		100000

/* Time the chip needs to start the downloaded patch */
#define	MTKBT_PATCH_ACTIVATE_US		100000

#define	MTKBT_PATCH_CHUNK		250

static int
mtkbt_reg_read(struct libusb_device_handle *hdl, uint32_t reg, uint32_t *val)
{
	uint8_t buf[4];
	int ret;

	ret = libusb_control_transfer(hdl,
	    LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
	    LIBUSB_RECIPIENT_DEVICE,
	    0x63, reg >> 16, reg & 0xffff, buf, sizeof(buf),
	    MTKBT_CONTROL_TIMEOUT);
	if (ret != (int)sizeof(buf)) {
		mtkbt_err("register 0x%08x read failed: %s", reg,
		    ret < 0 ? libusb_strerror(ret) : "short transfer");
		return (-1);
	}

	*val = le32dec(buf);
	mtkbt_debug("reg 0x%08x = 0x%08x", reg, *val);

	return (0);
}

static int
mtkbt_uhw_reg_write(struct libusb_device_handle *hdl, uint32_t reg,
    uint32_t val)
{
	uint8_t buf[4];
	int ret;

	le32enc(buf, val);
	/* bmRequestType 0x5e: host to device, vendor, vendor recipient */
	ret = libusb_control_transfer(hdl, 0x5e, 0x02, reg >> 16, reg & 0xffff,
	    buf, sizeof(buf), MTKBT_CONTROL_TIMEOUT);
	if (ret != (int)sizeof(buf)) {
		mtkbt_err("uhw register 0x%08x write failed: %s", reg,
		    ret < 0 ? libusb_strerror(ret) : "short transfer");
		return (-1);
	}

	mtkbt_debug("uhw reg 0x%08x := 0x%08x", reg, val);

	return (0);
}

int
mtkbt_get_chip(struct libusb_device_handle *hdl, struct mtkbt_chip *chip)
{

	if (mtkbt_reg_read(hdl, MTKBT_REG_DEV_ID, &chip->dev_id) != 0 ||
	    mtkbt_reg_read(hdl, MTKBT_REG_FW_VERSION, &chip->fw_version) != 0 ||
	    mtkbt_reg_read(hdl, MTKBT_REG_FW_FLAVOR, &chip->fw_flavor) != 0)
		return (-1);

	chip->fw_flavor = (chip->fw_flavor & 0x80) >> 7;

	return (0);
}

/*
 * Send a WMT command wrapped in vendor HCI command 0xfc6f and wait for
 * the matching WMT event.  The chip does not deliver WMT events on the
 * interrupt pipe; it hands them out through vendor control request 0x01
 * which returns zero bytes until the event is ready.
 */
static int
mtkbt_wmt_cmd(struct libusb_device_handle *hdl, uint8_t op, uint8_t flag,
    const void *data, uint16_t dlen, int *status)
{
	uint8_t cmd[MTKBT_HCI_MAX_CMD_SIZE];
	uint8_t evt[MTKBT_HCI_MAX_EVENT_SIZE];
	size_t evlen, cmdlen;
	uint16_t funcc;
	int i, ret;

	if (dlen > MTKBT_WMT_MAX_DATA)
		return (-1);

	/* HCI command header */
	cmdlen = 3 + MTKBT_WMT_HDR_SIZE + dlen;
	le16enc(cmd, MTKBT_HCI_OPCODE_WMT);
	cmd[2] = MTKBT_WMT_HDR_SIZE + dlen;
	/* WMT header; dlen counts the flag byte too */
	cmd[3] = 1;
	cmd[4] = op;
	le16enc(cmd + 5, dlen + 1);
	cmd[7] = flag;
	if (dlen > 0)
		memcpy(cmd + 8, data, dlen);

	ret = libusb_control_transfer(hdl,
	    LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_DEVICE,
	    0, 0, 0, cmd, cmdlen, MTKBT_CONTROL_TIMEOUT);
	if (ret != (int)cmdlen) {
		mtkbt_err("WMT op 0x%02x send failed: %s", op,
		    ret < 0 ? libusb_strerror(ret) : "short transfer");
		return (-1);
	}

	for (i = 0; i < MTKBT_WMT_POLL_MAX; i++) {
		ret = libusb_control_transfer(hdl,
		    LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
		    LIBUSB_RECIPIENT_DEVICE,
		    0x01, 0x30, 0, evt, sizeof(evt), MTKBT_CONTROL_TIMEOUT);
		if (ret < 0) {
			mtkbt_err("WMT op 0x%02x event read failed: %s", op,
			    libusb_strerror(ret));
			return (-1);
		}
		if (ret == 0) {
			usleep(MTKBT_WMT_POLL_US);
			continue;
		}

		evlen = (size_t)ret;
		/* event code, parameter length, parameters */
		if (evlen < 2 || (size_t)evt[1] + 2 > evlen) {
			mtkbt_err("WMT op 0x%02x: malformed event (%zu bytes)",
			    op, evlen);
			return (-1);
		}
		if (evt[0] != MTKBT_HCI_EVENT_WMT) {
			mtkbt_debug("skipping HCI event 0x%02x", evt[0]);
			continue;
		}
		break;
	}
	if (i == MTKBT_WMT_POLL_MAX) {
		mtkbt_err("WMT op 0x%02x: timed out waiting for event", op);
		return (-1);
	}

	/* Only trust the bytes the event header says are there */
	evlen = (size_t)evt[1] + 2;
	if (evlen < 2 + MTKBT_WMT_HDR_SIZE) {
		mtkbt_err("WMT op 0x%02x: short event (%zu bytes)", op, evlen);
		return (-1);
	}

	if (evt[3] != op) {
		mtkbt_err("WMT event for op 0x%02x, expected 0x%02x", evt[3],
		    op);
		return (-1);
	}

	mtkbt_debug("WMT op 0x%02x flag 0x%02x -> event flag 0x%02x len %zu",
	    op, flag, evt[6], evlen);

	if (status == NULL)
		return (0);

	switch (op) {
	case MTKBT_WMT_PATCH_DWNLD:
		if (evt[6] == 2)
			*status = MTKBT_WMT_PATCH_DONE;
		else if (evt[6] == 1)
			*status = MTKBT_WMT_PATCH_PROGRESS;
		else
			*status = MTKBT_WMT_PATCH_UNDONE;
		break;
	case MTKBT_WMT_FUNC_CTRL:
		if (evlen < 2 + MTKBT_WMT_HDR_SIZE + 2) {
			/* Plain set request: result is in the flag byte */
			*status = evt[6] != 0 ? MTKBT_WMT_ON_UNDONE :
			    MTKBT_WMT_ON_DONE;
			break;
		}
		funcc = be16dec(evt + 2 + MTKBT_WMT_HDR_SIZE);
		if (funcc == 0x404)
			*status = MTKBT_WMT_ON_DONE;
		else if (funcc == 0x420)
			*status = MTKBT_WMT_ON_PROGRESS;
		else
			*status = MTKBT_WMT_ON_UNDONE;
		break;
	default:
		*status = MTKBT_WMT_INVALID;
		break;
	}

	return (0);
}

/*
 * Turn the Bluetooth function on.  Do not use the FUNC_CTRL query form
 * (flag 4) on MT79xx: the chip never answers it and stops processing
 * HCI commands until it is reset.
 */
int
mtkbt_func_enable(struct libusb_device_handle *hdl)
{
	uint8_t param = 1;
	int status;

	if (mtkbt_wmt_cmd(hdl, MTKBT_WMT_FUNC_CTRL, MTKBT_WMT_FUNC_SET,
	    &param, sizeof(param), &status) != 0)
		return (-1);
	if (status != MTKBT_WMT_ON_DONE) {
		mtkbt_err("Bluetooth function enable refused");
		return (-1);
	}

	return (0);
}

int
mtkbt_ep_reset(struct libusb_device_handle *hdl)
{

	return (mtkbt_uhw_reg_write(hdl, MTKBT_UHW_EP_RST_OPT,
	    MTKBT_UHW_EP_RST_IN_OUT_OPT));
}

/*
 * Ask the chip whether section sec still has to be downloaded.  The query
 * carries the tail of the section map, prefixed with 0 (legacy mode).
 */
static int
mtkbt_patch_query(struct libusb_device_handle *hdl,
    const struct mtkbt_fw_section *sec, int *status)
{
	uint8_t q[1 + MTKBT_FW_SECMAP_SEND_SIZE];
	int retry;

	q[0] = 0;
	memcpy(q + 1, sec->map + MTKBT_FW_SECMAP_COMMON_SIZE,
	    MTKBT_FW_SECMAP_SEND_SIZE);

	for (retry = 0; retry < MTKBT_PATCH_PROGRESS_RETRY; retry++) {
		if (mtkbt_wmt_cmd(hdl, MTKBT_WMT_PATCH_DWNLD, 0, q, sizeof(q),
		    status) != 0)
			return (-1);
		if (*status != MTKBT_WMT_PATCH_PROGRESS)
			return (0);
		usleep(MTKBT_PATCH_PROGRESS_US);
	}

	mtkbt_err("chip stuck in patch download");
	return (-1);
}

int
mtkbt_load_fw(struct libusb_device_handle *hdl,
    const struct mtkbt_firmware *fw, int *downloaded)
{
	struct mtkbt_fw_section sec;
	const uint8_t *p;
	uint32_t i, left;
	uint16_t len;
	uint8_t flag;
	int status;

	*downloaded = 0;

	for (i = 0; i < fw->nsections; i++) {
		if (mtkbt_fw_section(fw, i, &sec) != 0)
			return (-1);
		if (sec.dlsize == 0)
			continue;

		if (mtkbt_patch_query(hdl, &sec, &status) != 0)
			return (-1);
		if (status == MTKBT_WMT_PATCH_DONE) {
			mtkbt_info("section %u already loaded", i);
			continue;
		}
		if (status != MTKBT_WMT_PATCH_UNDONE) {
			mtkbt_err("section %u: unexpected status %d", i,
			    status);
			return (-1);
		}

		mtkbt_info("loading section %u (%u bytes)", i, sec.dlsize);

		p = sec.data;
		left = sec.dlsize;
		flag = MTKBT_WMT_PKT_START;
		while (left > 0) {
			len = MIN(left, MTKBT_PATCH_CHUNK);
			if (flag != MTKBT_WMT_PKT_START)
				flag = left == len ? MTKBT_WMT_PKT_END :
				    MTKBT_WMT_PKT_CONTINUE;

			if (mtkbt_wmt_cmd(hdl, MTKBT_WMT_PATCH_DWNLD, flag, p,
			    len, &status) != 0)
				return (-1);
			if (status == MTKBT_WMT_PATCH_PROGRESS) {
				mtkbt_err("section %u: unexpected progress "
				    "status", i);
				return (-1);
			}

			p += len;
			left -= len;
			flag = MTKBT_WMT_PKT_CONTINUE;
		}
		*downloaded = 1;
	}

	if (*downloaded)
		usleep(MTKBT_PATCH_ACTIVATE_US);

	return (0);
}

/*
 * Issue HCI_Read_Local_Version_Information and report its status byte.
 * Used to tell whether the controller accepts standard HCI commands.
 */
int
mtkbt_hci_read_local_ver(struct libusb_device_handle *hdl, uint8_t *status)
{
	uint8_t cmd[3] = { 0x01, 0x10, 0x00 };
	uint8_t evt[MTKBT_HCI_MAX_EVENT_SIZE];
	int i, ret, transferred;

	ret = libusb_control_transfer(hdl,
	    LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_DEVICE,
	    0, 0, 0, cmd, sizeof(cmd), MTKBT_CONTROL_TIMEOUT);
	if (ret != (int)sizeof(cmd)) {
		mtkbt_err("HCI command send failed: %s",
		    ret < 0 ? libusb_strerror(ret) : "short transfer");
		return (-1);
	}

	/* Skip unrelated events, but only a few */
	for (i = 0; i < 8; i++) {
		ret = libusb_interrupt_transfer(hdl,
		    MTKBT_INTERRUPT_ENDPOINT_ADDR, evt, sizeof(evt),
		    &transferred, MTKBT_CONTROL_TIMEOUT);
		if (ret != 0) {
			mtkbt_err("HCI event read failed: %s",
			    libusb_strerror(ret));
			return (-1);
		}
		mtkbt_debug("HCI event 0x%02x, %d bytes", transferred > 0 ?
		    evt[0] : 0, transferred);
		/* Command Complete: code, len, ncmd, opcode(2), status */
		if (transferred >= 6 && evt[0] == 0x0e &&
		    le16dec(evt + 3) == 0x1001) {
			*status = evt[5];
			return (0);
		}
		/* Command Status: code, len, status, ncmd, opcode(2) */
		if (transferred >= 6 && evt[0] == 0x0f &&
		    le16dec(evt + 4) == 0x1001) {
			*status = evt[2];
			return (0);
		}
	}

	mtkbt_err("no response to HCI_Read_Local_Version_Information");
	return (-1);
}
