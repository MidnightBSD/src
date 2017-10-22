/*
 * Copyright (c) 2006 Maxim Sobolev <sobomax@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: release/7.0.0/sys/dev/powermac_nvram/powermac_nvramvar.h 160892 2006-08-01 22:19:01Z sobomax $
 */

#ifndef	_POWERPC_POWERMAC_POWERMAC_NVRAMVAR_H_
#define	_POWERPC_POWERMAC_POWERMAC_NVRAMVAR_H_

#define	NVRAM_SIZE		0x2000

#define	CORE99_SIGNATURE	0x5a

#ifdef _KERNEL

struct powermac_nvram_softc {
	device_t		sc_dev;
	phandle_t		sc_node;
	vm_offset_t		sc_bank;
	vm_offset_t		sc_bank0;
	vm_offset_t		sc_bank1;
	uint8_t			sc_data[NVRAM_SIZE];

	struct cdev *		sc_cdev;
	int			sc_isopen;
	int			sc_rpos;
	int			sc_wpos;
};

#endif /* _KERNEL */

struct chrp_header {
	uint8_t			signature;
	uint8_t			chrp_checksum;
	uint16_t		length;
	char			name[12];
};

struct core99_header {
	struct chrp_header	chrp_header;
	uint32_t		adler_checksum;
	uint32_t		generation;
	uint32_t		reserved[2];
};

#endif  /* _POWERPC_POWERMAC_POWERMAC_NVRAMVAR_H_ */
