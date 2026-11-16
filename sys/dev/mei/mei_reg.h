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
 * Intel Management Engine Interface (MEI, also known as HECI):
 * registers and host bus message (HBM) protocol definitions.
 */

#ifndef _DEV_MEI_MEI_REG_H_
#define	_DEV_MEI_MEI_REG_H_

/* BAR0 memory mapped registers, all 32 bits wide. */
#define	MEI_H_CB_WW		0x00	/* host circular buffer write window */
#define	MEI_H_CSR		0x04	/* host control and status */
#define	MEI_ME_CB_RW		0x08	/* ME circular buffer read window */
#define	MEI_ME_CSR_HA		0x0c	/* ME control and status (read only) */
#define	MEI_REG_SIZE		0x10	/* minimum BAR0 size we touch */

/*
 * Bits shared by H_CSR and ME_CSR_HA.  In H_CSR the IS bit is write one
 * to clear: it must be masked out of every read-modify-write unless the
 * interrupt is being acknowledged on purpose.
 */
#define	MEI_CSR_IE		0x00000001	/* interrupt enable */
#define	MEI_CSR_IS		0x00000002	/* interrupt status */
#define	MEI_CSR_IG		0x00000004	/* interrupt generate */
#define	MEI_CSR_RDY		0x00000008	/* ready */
#define	MEI_CSR_RST		0x00000010	/* reset */
#define	MEI_CSR_CBRP_MASK	0x0000ff00	/* buffer read pointer */
#define	MEI_CSR_CBRP_SHIFT	8
#define	MEI_CSR_CBWP_MASK	0x00ff0000	/* buffer write pointer */
#define	MEI_CSR_CBWP_SHIFT	16
#define	MEI_CSR_CBD_MASK	0xff000000	/* buffer depth in dwords */
#define	MEI_CSR_CBD_SHIFT	24

#define	MEI_CSR_RP(csr)	(((csr) & MEI_CSR_CBRP_MASK) >> MEI_CSR_CBRP_SHIFT)
#define	MEI_CSR_WP(csr)	(((csr) & MEI_CSR_CBWP_MASK) >> MEI_CSR_CBWP_SHIFT)
#define	MEI_CSR_CBD(csr) (((csr) & MEI_CSR_CBD_MASK) >> MEI_CSR_CBD_SHIFT)

/* PCI configuration space firmware status registers (FWSTS1..6). */
#define	MEI_PCI_FWSTS1		0x40
#define	MEI_PCI_FWSTS2		0x48
#define	MEI_PCI_FWSTS3		0x60
#define	MEI_PCI_FWSTS4		0x64
#define	MEI_PCI_FWSTS5		0x68
#define	MEI_PCI_FWSTS6		0x6c
#define	MEI_FWSTS_COUNT		6

/* FWSTS1 fields used for reporting only. */
#define	MEI_FWSTS1_CWS(v)	((v) & 0xf)		/* current state */
#define	MEI_FWSTS1_CWS_NORMAL	0x5
#define	MEI_FWSTS1_OPMODE(v)	(((v) >> 16) & 0xf)	/* operation mode */
#define	MEI_FWSTS1_ERRCODE(v)	(((v) >> 12) & 0xf)	/* error code */

/*
 * Message header, the first dword of every message in either circular
 * buffer.  The payload follows, padded to a dword boundary.
 */
#define	MEI_HDR_ME_ADDR_MASK	0x000000ff
#define	MEI_HDR_HOST_ADDR_MASK	0x0000ff00
#define	MEI_HDR_HOST_ADDR_SHIFT	8
#define	MEI_HDR_LEN_MASK	0x01ff0000	/* payload length in bytes */
#define	MEI_HDR_LEN_SHIFT	16
#define	MEI_HDR_DMA_RING	0x20000000	/* not negotiated, never used */
#define	MEI_HDR_EXTENDED	0x40000000	/* not negotiated, never used */
#define	MEI_HDR_MSG_COMPLETE	0x80000000	/* last fragment of a message */

#define	MEI_HDR_ME_ADDR(h)	((h) & MEI_HDR_ME_ADDR_MASK)
#define	MEI_HDR_HOST_ADDR(h)						\
	(((h) & MEI_HDR_HOST_ADDR_MASK) >> MEI_HDR_HOST_ADDR_SHIFT)
#define	MEI_HDR_LEN(h)		(((h) & MEI_HDR_LEN_MASK) >> MEI_HDR_LEN_SHIFT)
#define	MEI_HDR_MAX_LEN		(MEI_HDR_LEN_MASK >> MEI_HDR_LEN_SHIFT)
#define	MEI_HDR_MAKE(me, host, len, complete)				\
	((uint32_t)(me) | ((uint32_t)(host) << MEI_HDR_HOST_ADDR_SHIFT) | \
	 ((uint32_t)(len) << MEI_HDR_LEN_SHIFT) |			\
	 ((complete) ? MEI_HDR_MSG_COMPLETE : 0))

/* Host bus messages travel with me_addr == 0 and host_addr == 0. */
#define	MEI_HBM_ADDR		0

/* HBM protocol version requested by this driver. */
#define	MEI_HBM_MAJOR		2
#define	MEI_HBM_MINOR		0

/* HBM commands; responses have MEI_HBM_RESPONSE set. */
#define	MEI_HBM_RESPONSE		0x80
#define	MEI_HBM_HOST_START_REQ		0x01
#define	MEI_HBM_HOST_STOP_REQ		0x02
#define	MEI_HBM_ME_STOP_REQ		0x03
#define	MEI_HBM_HOST_ENUM_REQ		0x04
#define	MEI_HBM_CLIENT_PROPS_REQ	0x05
#define	MEI_HBM_CLIENT_CONNECT_REQ	0x06
#define	MEI_HBM_CLIENT_DISCONNECT_REQ	0x07
#define	MEI_HBM_FLOW_CONTROL		0x08
#define	MEI_HBM_RES(cmd)		((cmd) | MEI_HBM_RESPONSE)
#define	MEI_HBM_HOST_START_RES		MEI_HBM_RES(MEI_HBM_HOST_START_REQ)
#define	MEI_HBM_HOST_ENUM_RES		MEI_HBM_RES(MEI_HBM_HOST_ENUM_REQ)
#define	MEI_HBM_CLIENT_PROPS_RES	MEI_HBM_RES(MEI_HBM_CLIENT_PROPS_REQ)
#define	MEI_HBM_CLIENT_CONNECT_RES	MEI_HBM_RES(MEI_HBM_CLIENT_CONNECT_REQ)
#define	MEI_HBM_CLIENT_DISCONNECT_RES	\
	MEI_HBM_RES(MEI_HBM_CLIENT_DISCONNECT_REQ)

/* Status codes in connect/disconnect responses. */
#define	MEI_CL_STATUS_SUCCESS		0x00
#define	MEI_CL_STATUS_NOT_FOUND		0x01
#define	MEI_CL_STATUS_ALREADY_STARTED	0x02
#define	MEI_CL_STATUS_OUT_OF_RESOURCES	0x03
#define	MEI_CL_STATUS_MESSAGE_SMALL	0x04

/* Status codes in the client properties response. */
#define	MEI_PROPS_STATUS_SUCCESS	0x00

/* HBM payloads.  All fields are little endian. */
struct mei_hbm_version_req {
	uint8_t		cmd;		/* MEI_HBM_HOST_START_REQ */
	uint8_t		reserved;
	uint8_t		minor;
	uint8_t		major;
} __packed;

struct mei_hbm_version_res {
	uint8_t		cmd;
	uint8_t		supported;	/* non-zero: requested version ok */
	uint8_t		minor;		/* highest version the ME speaks */
	uint8_t		major;
} __packed;

struct mei_hbm_stop_req {
	uint8_t		cmd;		/* HOST_STOP_REQ or ME_STOP_REQ */
	uint8_t		reason;
	uint8_t		reserved[2];
} __packed;

struct mei_hbm_enum_req {
	uint8_t		cmd;		/* MEI_HBM_HOST_ENUM_REQ */
	uint8_t		flags;		/* always 0: no dynamic clients */
	uint8_t		reserved[2];
} __packed;

struct mei_hbm_enum_res {
	uint8_t		cmd;
	uint8_t		reserved[3];
	uint8_t		valid_addresses[32];	/* bitmap of ME clients */
} __packed;

struct mei_hbm_props_req {
	uint8_t		cmd;		/* MEI_HBM_CLIENT_PROPS_REQ */
	uint8_t		me_addr;
	uint8_t		reserved[2];
} __packed;

struct mei_client_props {
	uint8_t		uuid[16];	/* protocol name, GUID byte order */
	uint8_t		protocol_version;
	uint8_t		max_connections;
	uint8_t		fixed_address;
	uint8_t		single_recv_buf;
	uint32_t	max_msg_length;
} __packed;

struct mei_hbm_props_res {
	uint8_t		cmd;
	uint8_t		me_addr;
	uint8_t		status;
	uint8_t		reserved;
	struct mei_client_props props;
} __packed;

/* Used for CLIENT_CONNECT and CLIENT_DISCONNECT requests and responses. */
struct mei_hbm_client_conn {
	uint8_t		cmd;
	uint8_t		me_addr;
	uint8_t		host_addr;
	uint8_t		status;		/* responses only; 0 in requests */
} __packed;

/*
 * Flow control: grants the receiver of this message permission to send
 * one more message.  host_addr == 0 refers to a single_recv_buf ME
 * client as a whole.
 */
struct mei_hbm_flow_control {
	uint8_t		cmd;		/* MEI_HBM_FLOW_CONTROL */
	uint8_t		me_addr;
	uint8_t		host_addr;
	uint8_t		reserved[5];
} __packed;

#endif /* _DEV_MEI_MEI_REG_H_ */
