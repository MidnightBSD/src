/*-
 * SPDX-License-Identifier: BSD-2-Clause
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * User interface of mei(4), the Intel Management Engine Interface driver.
 *
 * Each open of /dev/mei0 is one host client.  IOCTL_MEI_CONNECT_CLIENT
 * connects it to the ME client whose protocol GUID is given, after which
 * write(2) sends one message and read(2) returns one received message.
 * The structure layout and ioctl number match the Linux <linux/mei.h>
 * interface so that existing tools can be used unchanged.
 */

#ifndef _DEV_MEI_MEIIO_H_
#define	_DEV_MEI_MEIIO_H_

#include <sys/types.h>
#include <sys/ioccom.h>

/* Properties of the ME client returned by IOCTL_MEI_CONNECT_CLIENT. */
struct mei_client {
	uint32_t	max_msg_length;
	uint8_t		protocol_version;
	uint8_t		reserved[3];
};

struct mei_connect_client_data {
	union {
		/* in: protocol GUID of the ME client, GUID byte order */
		uint8_t			in_client_uuid[16];
		/* out: properties of the connected ME client */
		struct mei_client	out_client_properties;
	};
};

#define	IOCTL_MEI_CONNECT_CLIENT					\
	_IOWR('H', 0x01, struct mei_connect_client_data)

#endif /* _DEV_MEI_MEIIO_H_ */
