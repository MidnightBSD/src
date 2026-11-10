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

#include <err.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libusb.h>

#include "mtkbt_fw.h"
#include "mtkbt_hw.h"
#include "mtkbt_dbg.h"

#define	MTKBT_DEFAULT_FIRMWARE_PATH	"/usr/local/share/mtkbt-firmware"

int	mtkbt_do_debug = 0;
int	mtkbt_do_info = 0;

struct mtkbt_devid {
	uint16_t product_id;
	uint16_t vendor_id;
};

/*
 * MT79xx USB Bluetooth devices.  Keep in sync with ubt_mtk_devs[] in
 * sys/netgraph/bluetooth/drivers/ubt/ng_ubt_mtk.c and with mtkbtfw.conf.
 */
static const struct mtkbt_devid mtkbt_list[] = {
	/* MT7920 */
	{ .vendor_id = 0x0489, .product_id = 0xe134 },
	{ .vendor_id = 0x0489, .product_id = 0xe135 },
	{ .vendor_id = 0x0489, .product_id = 0xe158 },
	{ .vendor_id = 0x13d3, .product_id = 0x3620 },
	{ .vendor_id = 0x13d3, .product_id = 0x3621 },
	{ .vendor_id = 0x13d3, .product_id = 0x3622 },
	/* MT7921 */
	{ .vendor_id = 0x0e8d, .product_id = 0x0608 },
	{ .vendor_id = 0x0489, .product_id = 0xe0c8 },
	{ .vendor_id = 0x0489, .product_id = 0xe0cd },
	{ .vendor_id = 0x0489, .product_id = 0xe0e0 },
	{ .vendor_id = 0x0489, .product_id = 0xe0f2 },
	{ .vendor_id = 0x04ca, .product_id = 0x3802 },
	{ .vendor_id = 0x13d3, .product_id = 0x3563 },
	{ .vendor_id = 0x13d3, .product_id = 0x3564 },
	{ .vendor_id = 0x13d3, .product_id = 0x3567 },
	{ .vendor_id = 0x13d3, .product_id = 0x3576 },
	{ .vendor_id = 0x13d3, .product_id = 0x3578 },
	{ .vendor_id = 0x13d3, .product_id = 0x3583 },
	{ .vendor_id = 0x13d3, .product_id = 0x3606 },
	/* MT7902 */
	{ .vendor_id = 0x0e8d, .product_id = 0x1ede },
	{ .vendor_id = 0x0489, .product_id = 0xe156 },
	{ .vendor_id = 0x13d3, .product_id = 0x3579 },
	{ .vendor_id = 0x13d3, .product_id = 0x3580 },
	{ .vendor_id = 0x13d3, .product_id = 0x3594 },
	{ .vendor_id = 0x13d3, .product_id = 0x3596 },
	/* MT7922 */
	{ .vendor_id = 0x0e8d, .product_id = 0x223c },
	{ .vendor_id = 0x0489, .product_id = 0xe0d8 },
	{ .vendor_id = 0x0489, .product_id = 0xe0d9 },
	{ .vendor_id = 0x0489, .product_id = 0xe0e2 },
	{ .vendor_id = 0x0489, .product_id = 0xe0e4 },
	{ .vendor_id = 0x0489, .product_id = 0xe0f1 },
	{ .vendor_id = 0x0489, .product_id = 0xe0f5 },
	{ .vendor_id = 0x0489, .product_id = 0xe0f6 },
	{ .vendor_id = 0x0489, .product_id = 0xe102 },
	{ .vendor_id = 0x0489, .product_id = 0xe11d },
	{ .vendor_id = 0x0489, .product_id = 0xe152 },
	{ .vendor_id = 0x0489, .product_id = 0xe153 },
	{ .vendor_id = 0x0489, .product_id = 0xe170 },
	{ .vendor_id = 0x0489, .product_id = 0xe174 },
	{ .vendor_id = 0x04ca, .product_id = 0x3804 },
	{ .vendor_id = 0x04ca, .product_id = 0x3807 },
	{ .vendor_id = 0x04ca, .product_id = 0x38e4 },
	{ .vendor_id = 0x13d3, .product_id = 0x3568 },
	{ .vendor_id = 0x13d3, .product_id = 0x3584 },
	{ .vendor_id = 0x13d3, .product_id = 0x3585 },
	{ .vendor_id = 0x13d3, .product_id = 0x3605 },
	{ .vendor_id = 0x13d3, .product_id = 0x3607 },
	{ .vendor_id = 0x13d3, .product_id = 0x3610 },
	{ .vendor_id = 0x13d3, .product_id = 0x3614 },
	{ .vendor_id = 0x13d3, .product_id = 0x3615 },
	{ .vendor_id = 0x13d3, .product_id = 0x3633 },
	{ .vendor_id = 0x35f5, .product_id = 0x7922 },
	/* MT7925 */
	{ .vendor_id = 0x0e8d, .product_id = 0x8c38 },
	{ .vendor_id = 0x0489, .product_id = 0xe111 },
	{ .vendor_id = 0x0489, .product_id = 0xe113 },
	{ .vendor_id = 0x0489, .product_id = 0xe118 },
	{ .vendor_id = 0x0489, .product_id = 0xe11e },
	{ .vendor_id = 0x0489, .product_id = 0xe124 },
	{ .vendor_id = 0x0489, .product_id = 0xe139 },
	{ .vendor_id = 0x0489, .product_id = 0xe14e },
	{ .vendor_id = 0x0489, .product_id = 0xe14f },
	{ .vendor_id = 0x0489, .product_id = 0xe150 },
	{ .vendor_id = 0x0489, .product_id = 0xe151 },
	{ .vendor_id = 0x13d3, .product_id = 0x3602 },
	{ .vendor_id = 0x13d3, .product_id = 0x3603 },
	{ .vendor_id = 0x13d3, .product_id = 0x3604 },
	{ .vendor_id = 0x13d3, .product_id = 0x3608 },
	{ .vendor_id = 0x13d3, .product_id = 0x3609 },
	{ .vendor_id = 0x13d3, .product_id = 0x3613 },
	{ .vendor_id = 0x13d3, .product_id = 0x3625 },
	{ .vendor_id = 0x13d3, .product_id = 0x3627 },
};

static int
mtkbt_is_listed(const struct libusb_device_descriptor *d)
{
	size_t i;

	for (i = 0; i < nitems(mtkbt_list); i++)
		if (mtkbt_list[i].product_id == d->idProduct &&
		    mtkbt_list[i].vendor_id == d->idVendor)
			return (1);

	return (0);
}

/* Interface 0 must be a Bluetooth HCI interface (e0/01/01). */
static int
mtkbt_is_bluetooth(struct libusb_device *dev)
{
	struct libusb_config_descriptor *cfg;
	const struct libusb_interface_descriptor *id;
	int r, ret;

	r = libusb_get_active_config_descriptor(dev, &cfg);
	if (r < 0) {
		mtkbt_err("libusb_get_active_config_descriptor: %s",
		    libusb_strerror(r));
		return (0);
	}

	ret = 0;
	if (cfg->bNumInterfaces > 0 && cfg->interface[0].num_altsetting > 0) {
		id = &cfg->interface[0].altsetting[0];
		ret = id->bInterfaceClass == LIBUSB_CLASS_WIRELESS &&
		    id->bInterfaceSubClass == 0x01 &&
		    id->bInterfaceProtocol == 0x01;
	}
	libusb_free_config_descriptor(cfg);

	return (ret);
}

static libusb_device *
mtkbt_find_device(libusb_context *ctx, int bus_id, int dev_id)
{
	libusb_device **list, *dev = NULL, *found = NULL;
	struct libusb_device_descriptor d;
	ssize_t cnt, i;
	int r;

	cnt = libusb_get_device_list(ctx, &list);
	if (cnt < 0) {
		mtkbt_err("libusb_get_device_list() failed: code %lld",
		    (long long int) cnt);
		return (NULL);
	}

	for (i = 0; i < cnt; i++) {
		dev = list[i];
		if (bus_id != libusb_get_bus_number(dev) ||
		    dev_id != libusb_get_device_address(dev))
			continue;

		r = libusb_get_device_descriptor(dev, &d);
		if (r != 0) {
			mtkbt_err("libusb_get_device_descriptor: %s",
			    libusb_strerror(r));
			break;
		}

		if (mtkbt_is_listed(&d) && mtkbt_is_bluetooth(dev))
			found = libusb_ref_device(dev);
		else
			mtkbt_err("%04x:%04x is not a supported device",
			    d.idVendor, d.idProduct);
		break;
	}

	libusb_free_device_list(list, 1);
	return (found);
}

static int
parse_ugen_name(char const *ugen, uint8_t *bus, uint8_t *addr)
{
	char *ep;

	if (strncmp(ugen, "ugen", 4) != 0)
		return (-1);

	*bus = (uint8_t) strtoul(ugen + 4, &ep, 10);
	if (*ep != '.')
		return (-1);

	*addr = (uint8_t) strtoul(ep + 1, &ep, 10);
	if (*ep != '\0')
		return (-1);

	return (0);
}

static void
usage(void)
{
	fprintf(stderr,
	    "Usage: mtkbtfw (-D) -d ugenX.Y (-f firmware path) (-I)\n");
	fprintf(stderr, "    -D: enable debugging\n");
	fprintf(stderr, "    -d: device to operate upon\n");
	fprintf(stderr, "    -f: firmware path, if not default\n");
	fprintf(stderr, "    -I: enable informational output\n");
	exit(127);
}

int
main(int argc, char *argv[])
{
	libusb_context *ctx = NULL;
	libusb_device *dev = NULL;
	libusb_device_handle *hdl = NULL;
	struct mtkbt_firmware fw;
	struct mtkbt_chip chip;
	uint8_t bus_id = 0, dev_id = 0;
	int devid_set = 0;
	int downloaded, n, r;
	uint8_t hci_status;
	char *firmware_dir = NULL;
	char *firmware_path = NULL;
	int retcode = 1;

	memset(&fw, 0, sizeof(fw));

	/* Parse command line arguments */
	while ((n = getopt(argc, argv, "Dd:f:hI")) != -1) {
		switch (n) {
		case 'd': /* ugen device name */
			devid_set = 1;
			if (parse_ugen_name(optarg, &bus_id, &dev_id) < 0)
				usage();
			break;
		case 'D':
			mtkbt_do_debug = 1;
			break;
		case 'f': /* firmware dir */
			free(firmware_dir);
			firmware_dir = strdup(optarg);
			if (firmware_dir == NULL)
				err(1, "strdup");
			break;
		case 'I':
			mtkbt_do_info = 1;
			break;
		case 'h':
		default:
			usage();
			break;
			/* NOT REACHED */
		}
	}

	/* Ensure the devid was given! */
	if (devid_set == 0) {
		usage();
		/* NOTREACHED */
	}

	/* libusb setup */
	r = libusb_init(&ctx);
	if (r != 0) {
		mtkbt_err("libusb_init failed: code %d", r);
		exit(127);
	}

	mtkbt_debug("opening dev %d.%d", (int) bus_id, (int) dev_id);

	/* Find a device based on the bus/dev id */
	dev = mtkbt_find_device(ctx, bus_id, dev_id);
	if (dev == NULL) {
		mtkbt_err("device not found");
		goto shutdown;
	}

	/* Grab device handle */
	r = libusb_open(dev, &hdl);
	if (r != 0) {
		mtkbt_err("libusb_open() failed: code %d", r);
		goto shutdown;
	}

	/* Check if ng_ubt is attached */
	r = libusb_kernel_driver_active(hdl, 0);
	if (r < 0) {
		mtkbt_err("libusb_kernel_driver_active() failed: code %d", r);
		goto shutdown;
	}
	if (r > 0) {
		mtkbt_info("Firmware has already been downloaded");
		retcode = 0;
		goto shutdown;
	}

	/*
	 * A controller that already accepts HCI commands has its patch
	 * loaded and its Bluetooth function on.  Do not reset it: the kernel
	 * would refuse it again only if ng_ubt is missing, and a reset would
	 * then bring us straight back here from devd.
	 */
	if (mtkbt_hci_read_local_ver(hdl, &hci_status) != 0)
		goto shutdown;
	if (hci_status == 0) {
		mtkbt_info("Firmware has already been downloaded");
		retcode = 0;
		goto shutdown;
	}

	if (mtkbt_get_chip(hdl, &chip) != 0)
		goto shutdown;
	mtkbt_info("chip 0x%04x, fw version 0x%08x, flavor %u",
	    chip.dev_id & 0xffff, chip.fw_version, chip.fw_flavor);

	/* Default the firmware path */
	if (firmware_dir == NULL) {
		firmware_dir = strdup(MTKBT_DEFAULT_FIRMWARE_PATH);
		if (firmware_dir == NULL)
			err(1, "strdup");
	}

	firmware_path = mtkbt_get_fwname(chip.dev_id, chip.fw_version,
	    chip.fw_flavor, firmware_dir);
	if (firmware_path == NULL)
		goto shutdown;
	mtkbt_info("firmware: %s", firmware_path);

	if (mtkbt_fw_read(&fw, firmware_path) == 0)
		goto shutdown;
	if (mtkbt_fw_validate(&fw) != 0)
		goto shutdown;

	if (mtkbt_load_fw(hdl, &fw, &downloaded) != 0)
		goto shutdown;

	/* Reset the device endpoints before turning the function on */
	if (mtkbt_ep_reset(hdl) != 0)
		goto shutdown;

	if (mtkbt_func_enable(hdl) != 0)
		goto shutdown;

	/* The controller must now accept standard HCI commands */
	if (mtkbt_hci_read_local_ver(hdl, &hci_status) != 0)
		goto shutdown;
	if (hci_status != 0) {
		mtkbt_err("controller still refuses HCI commands: "
		    "status 0x%02x", hci_status);
		goto shutdown;
	}

	retcode = 0;

	/*
	 * Ask the kernel driver to probe and attach the device again.  The
	 * downloaded patch survives the port reset.
	 */
	r = libusb_reset_device(hdl);
	if (r != 0)
		mtkbt_err("libusb_reset_device() failed: %s",
		    libusb_strerror(r));

shutdown:

	if (hdl != NULL)
		libusb_close(hdl);

	if (dev != NULL)
		libusb_unref_device(dev);

	if (ctx != NULL)
		libusb_exit(ctx);

	mtkbt_fw_free(&fw);
	free(firmware_path);
	free(firmware_dir);

	if (retcode == 0)
		mtkbt_info("Firmware download is successful!");
	else
		mtkbt_err("Firmware download failed!");

	return (retcode);
}
