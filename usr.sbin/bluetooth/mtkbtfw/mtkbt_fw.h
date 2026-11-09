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
#ifndef	__MTKBT_FW_H__
#define	__MTKBT_FW_H__

#include <stddef.h>
#include <stdint.h>

/*
 * MT79xx "_hdr.bin" ROM patch layout: a 32 byte patch header, a 64 byte
 * global descriptor and an array of 64 byte section maps.
 */
#define	MTKBT_FW_HDR_SIZE		32
#define	MTKBT_FW_GD_SIZE		64
#define	MTKBT_FW_SECMAP_SIZE		64
/* sectype, secoffset and secsize, then the part sent to the chip */
#define	MTKBT_FW_SECMAP_COMMON_SIZE	12
#define	MTKBT_FW_SECMAP_SEND_SIZE	52

/* Offsets of little endian 32-bit fields */
#define	MTKBT_FW_HDR_SWVER_OFF		22	/* 16-bit */
#define	MTKBT_FW_GD_SECNUM_OFF		12
#define	MTKBT_FW_SECMAP_OFFSET_OFF	4
#define	MTKBT_FW_SECMAP_DLSIZE_OFF	16

/* Refuse anything unreasonably large; current images are under 1 MiB. */
#define	MTKBT_FW_MAX_SIZE		0x800000	/* 8 MiB */

struct mtkbt_firmware {
	char		*fwname;
	size_t		 len;
	unsigned char	*buf;
	uint32_t	 nsections;	/* validated by mtkbt_fw_validate() */
};

struct mtkbt_fw_section {
	const unsigned char	*map;		/* MTKBT_FW_SECMAP_SIZE bytes */
	const unsigned char	*data;
	uint32_t		 dlsize;
};

int	mtkbt_fw_read(struct mtkbt_firmware *fw, const char *fwname);
void	mtkbt_fw_free(struct mtkbt_firmware *fw);
int	mtkbt_fw_validate(struct mtkbt_firmware *fw);
int	mtkbt_fw_section(const struct mtkbt_firmware *fw, uint32_t idx,
	    struct mtkbt_fw_section *sec);
char	*mtkbt_get_fwname(uint32_t dev_id, uint32_t fw_ver, uint32_t flavor,
	    const char *dir);

#endif
