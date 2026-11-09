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
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mtkbt_fw.h"
#include "mtkbt_dbg.h"

int
mtkbt_fw_read(struct mtkbt_firmware *fw, const char *fwname)
{
	struct stat sb;
	unsigned char *buf;
	size_t len, off;
	ssize_t r;
	int fd;

	fd = open(fwname, O_RDONLY);
	if (fd < 0) {
		warn("%s: open: %s", __func__, fwname);
		return (0);
	}

	if (fstat(fd, &sb) != 0) {
		warn("%s: stat: %s", __func__, fwname);
		close(fd);
		return (0);
	}

	if (!S_ISREG(sb.st_mode) || sb.st_size <= 0 ||
	    sb.st_size > MTKBT_FW_MAX_SIZE) {
		mtkbt_err("%s: not a regular file or bad size %jd", fwname,
		    (intmax_t)sb.st_size);
		close(fd);
		return (0);
	}
	len = (size_t)sb.st_size;

	buf = malloc(len);
	if (buf == NULL) {
		warn("%s: malloc", __func__);
		close(fd);
		return (0);
	}

	for (off = 0; off < len; off += (size_t)r) {
		r = read(fd, buf + off, len - off);
		if (r < 0 && errno == EINTR) {
			r = 0;
			continue;
		}
		if (r <= 0) {
			if (r < 0)
				warn("%s: read", __func__);
			else
				mtkbt_err("%s: short read (%zu of %zu)",
				    fwname, off, len);
			free(buf);
			close(fd);
			return (0);
		}
	}
	close(fd);

	memset(fw, 0, sizeof(*fw));
	fw->fwname = strdup(fwname);
	if (fw->fwname == NULL) {
		warn("%s: strdup", __func__);
		free(buf);
		return (0);
	}
	fw->len = len;
	fw->buf = buf;

	return (1);
}

void
mtkbt_fw_free(struct mtkbt_firmware *fw)
{
	free(fw->fwname);
	free(fw->buf);
	memset(fw, 0, sizeof(*fw));
}

/*
 * Make sure the header, the global descriptor and the whole section map
 * array lie inside the image.  On success the section count is cached in
 * fw->nsections and every later section lookup is bounded by it.
 */
int
mtkbt_fw_validate(struct mtkbt_firmware *fw)
{
	const size_t fixed = MTKBT_FW_HDR_SIZE + MTKBT_FW_GD_SIZE;
	uint32_t n;

	if (fw->len < fixed) {
		mtkbt_err("firmware too small: %zu bytes", fw->len);
		return (-1);
	}

	n = le32dec(fw->buf + MTKBT_FW_HDR_SIZE + MTKBT_FW_GD_SECNUM_OFF);
	if (n == 0) {
		mtkbt_err("firmware declares no sections");
		return (-1);
	}

	/* fw->len >= fixed, so this subtraction cannot wrap. */
	if (n > (fw->len - fixed) / MTKBT_FW_SECMAP_SIZE) {
		mtkbt_err("firmware truncated: %u sections in %zu bytes", n,
		    fw->len);
		return (-1);
	}

	fw->nsections = n;
	mtkbt_info("firmware build %.16s, sw version 0x%04x, %u sections",
	    (const char *)fw->buf,
	    le16dec(fw->buf + MTKBT_FW_HDR_SWVER_OFF), n);

	return (0);
}

/*
 * Return section idx, rejecting a payload that does not fit in the image.
 */
int
mtkbt_fw_section(const struct mtkbt_firmware *fw, uint32_t idx,
    struct mtkbt_fw_section *sec)
{
	const unsigned char *map;
	uint32_t offset, size;

	if (idx >= fw->nsections)
		return (-1);

	map = fw->buf + MTKBT_FW_HDR_SIZE + MTKBT_FW_GD_SIZE +
	    (size_t)idx * MTKBT_FW_SECMAP_SIZE;
	offset = le32dec(map + MTKBT_FW_SECMAP_OFFSET_OFF);
	size = le32dec(map + MTKBT_FW_SECMAP_DLSIZE_OFF);

	if (offset > fw->len || size > fw->len - offset) {
		mtkbt_err("section %u out of bounds: offset %u size %u",
		    idx, offset, size);
		return (-1);
	}

	sec->map = map;
	sec->data = fw->buf + offset;
	sec->dlsize = size;

	return (0);
}

/*
 * Build the firmware path the same way linux-firmware lays out the
 * mediatek/ directory, relative to dir.
 */
char *
mtkbt_get_fwname(uint32_t dev_id, uint32_t fw_ver, uint32_t flavor,
    const char *dir)
{
	char *fwname;
	unsigned int id, rev;
	int r;

	id = dev_id & 0xffff;
	rev = (fw_ver & 0xff) + 1;

	switch (id) {
	case 0x7925:
		r = asprintf(&fwname,
		    "%s/mt%04x/BT_RAM_CODE_MT%04x_1_%x_hdr.bin", dir, id, id,
		    rev);
		break;
	case 0x7961:
		if (flavor != 0) {
			r = asprintf(&fwname,
			    "%s/BT_RAM_CODE_MT%04x_1a_%x_hdr.bin", dir, id,
			    rev);
			break;
		}
		/* FALLTHROUGH */
	case 0x7902:
	case 0x7922:
		r = asprintf(&fwname, "%s/BT_RAM_CODE_MT%04x_1_%x_hdr.bin",
		    dir, id, rev);
		break;
	default:
		mtkbt_err("unsupported chip 0x%04x", id);
		return (NULL);
	}

	if (r < 0)
		return (NULL);

	return (fwname);
}
