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
 * mei(4) internal definitions shared by the core (mei.c) and the
 * character device (mei_cdev.c).
 *
 * Locking: a single sleep mutex, sc_mtx, protects all softc and client
 * state and all register access.  The interrupt handler runs as an
 * ithread and takes sc_mtx.  Sleepers use msleep(9) on sc_mtx.  Data is
 * never copied to or from user space with sc_mtx held: the cdev layer
 * stages user data in kernel buffers.
 *
 * Sending (host to ME):
 * - mei_send_locked() in mei.c is the only routine that writes H_CB_WW.
 *   It is called with sc_mtx held and writes one fragment (header plus
 *   payload) only if the whole fragment fits in the free slots of the
 *   host circular buffer; otherwise it returns EAGAIN and writes nothing.
 * - The interrupt thread never sleeps.  Control messages it has to send
 *   (flow control grants, the reply to an ME initiated disconnect) are
 *   queued on sc_ctlq when they cannot be sent at once, and the queue is
 *   flushed from the interrupt handler whenever the ME has consumed data.
 *   Queued control messages always go out before new user data.
 * - Only user writers (mei_cl_write) and the link init task sleep waiting
 *   for buffer space or credits.
 * - Messages are never interleaved: while a multi fragment user message
 *   is being sent, sc_tx_busy is set and every other sender, control
 *   messages included, waits or queues until its last fragment is out.
 *
 * Link resets:
 * - Any of ME_RST set, ME_RDY cleared, an HBM ME_STOP_REQ, or a buffer
 *   fill count larger than the buffer depth triggers a link reset: the
 *   state goes to MEI_LINK_INIT, sc_link_gen is incremented, every
 *   connected client becomes MEI_CL_DISCONNECTED (sleepers woken), the ME
 *   client table and sc_ctlq are cleared, and sc_reset_task re-runs the
 *   hardware handshake and HBM enumeration.  Unknown HBM commands are
 *   logged and ignored, not treated as a reset.
 * - Per connection accounting (nconnections, credits) is only touched
 *   when cl->link_gen == sc_link_gen; a stale client from before a reset
 *   never changes the new client table.
 * - mei_cl_free() performs the disconnect handshake only if the link is
 *   READY and the generation matches, with a bounded wait.
 */

#ifndef _DEV_MEI_MEI_VAR_H_
#define	_DEV_MEI_MEI_VAR_H_

/*
 * Include after <sys/param.h>, <sys/systm.h>, <sys/lock.h>,
 * <sys/mutex.h> and <sys/malloc.h>.
 */
#include <sys/_task.h>
#include <sys/queue.h>
#include <sys/selinfo.h>
#include <sys/sysctl.h>

#include <dev/mei/mei_reg.h>

#define	MEI_MAX_ME_CLIENTS	256	/* ME addresses 0..255; 0 is HBM */
#define	MEI_MAX_HOST_CLIENTS	255	/* host addresses 1..255; 0 is HBM */
#define	MEI_RXQ_MAX		8	/* queued messages per host client */
#define	MEI_MAX_MSG_LEN		(64 * 1024) /* cap on max_msg_length */
#define	MEI_CTLQ_LEN		32	/* pending control messages */
#define	MEI_CTL_MAX_LEN		8	/* largest control payload */
#define	MEI_MAX_RESETS		3	/* consecutive failures before
					   giving up (MEI_LINK_DISABLED) */

/* Timeouts, in milliseconds. */
#define	MEI_HW_READY_TIMEOUT	2000	/* ME ready after link reset */
#define	MEI_HBM_TIMEOUT		2000	/* each HBM request/response */
#define	MEI_CONNECT_TIMEOUT	2000	/* client connect/disconnect */
#define	MEI_INIT_TIMEOUT	10000	/* open(2) waiting for READY */

MALLOC_DECLARE(M_MEI);

enum mei_link_state {
	MEI_LINK_INIT = 0,	/* reset/initialisation in progress */
	MEI_LINK_READY,		/* clients enumerated, usable */
	MEI_LINK_DISABLED,	/* gave up after repeated failures */
	MEI_LINK_DETACHING,	/* driver is going away */
};

/* ME client, learned from HBM enumeration.  Indexed by ME address. */
struct mei_me_client {
	bool		valid;
	uint8_t		addr;
	uint8_t		uuid[16];
	uint8_t		protocol_version;
	uint8_t		max_connections;
	uint8_t		fixed_address;
	uint8_t		single_recv_buf;
	uint32_t	max_msg_length;	/* clamped to MEI_MAX_MSG_LEN */
	int		nconnections;	/* host clients connected to it */
	int		tx_credits;	/* shared credits if single_recv_buf */
};

enum mei_cl_state {
	MEI_CL_IDLE = 0,	/* opened, not connected */
	MEI_CL_CONNECTING,	/* connect request sent */
	MEI_CL_CONNECTED,
	MEI_CL_DISCONNECTING,	/* disconnect request sent */
	MEI_CL_DISCONNECTED,	/* was connected, now gone (ME/reset) */
};

/* A control message waiting for room in the host circular buffer. */
struct mei_ctl_msg {
	uint32_t	hdr;			/* MEI_HDR_MAKE(...) */
	uint8_t		len;			/* payload bytes */
	uint8_t		data[MEI_CTL_MAX_LEN];
};

/* One fully received message waiting to be read. */
struct mei_rx_msg {
	STAILQ_ENTRY(mei_rx_msg) link;
	size_t		len;		/* bytes in data[] */
	size_t		off;		/* bytes already returned by read */
	uint8_t		data[];
};

/* Host client: one per open file. */
struct mei_cl {
	struct mei_softc *sc;
	TAILQ_ENTRY(mei_cl) link;
	enum mei_cl_state state;
	uint8_t		host_addr;	/* 1..255 */
	uint8_t		me_addr;	/* valid once connecting */
	uint8_t		conn_status;	/* MEI_CL_STATUS_* of last response */
	bool		conn_answered;	/* connect/disconnect answered */
	uint32_t	link_gen;	/* sc_link_gen when connected */
	int		tx_credits;	/* if !single_recv_buf */
	bool		rx_credit_out;	/* ME may send us one message */
	uint8_t		*rx_asm;	/* fragment reassembly buffer */
	size_t		rx_asm_len;	/* bytes collected in rx_asm */
	size_t		rx_asm_size;	/* allocated size of rx_asm */
	bool		rx_overflow;	/* drop until message complete */
	STAILQ_HEAD(, mei_rx_msg) rxq;	/* complete messages */
	int		rxq_len;
	bool		tx_sending;	/* this client holds sc_tx_busy */
	struct selinfo	rsel;
};

struct mei_softc {
	device_t	sc_dev;
	struct mtx	sc_mtx;
	struct resource	*sc_mem;
	int		sc_mem_rid;
	struct resource	*sc_irq;
	int		sc_irq_rid;
	void		*sc_intrhand;
	bool		sc_msi;
	struct cdev	*sc_cdev;

	enum mei_link_state sc_state;
	uint32_t	sc_link_gen;	/* bumped on every link reset */
	int		sc_reset_count;	/* consecutive failed resets */
	struct taskqueue *sc_tq;	/* private, runs sc_reset_task */
	struct task	sc_reset_task;	/* runs mei_link_init() */
	struct sysctl_ctx_list sc_sysctl_ctx;
	bool		sc_sysctl_init;
	bool		sc_reset_pending;
	bool		sc_task_running;

	/* HBM request/response tracking, used by mei_link_init(). */
	uint8_t		sc_hbm_wait_cmd;	/* response cmd awaited, 0 none */
	bool		sc_hbm_answered;
	uint8_t		sc_hbm_resp[64];	/* copy of the last HBM response */
	size_t		sc_hbm_resp_len;
	uint8_t		sc_hbm_major;		/* negotiated version */
	uint8_t		sc_hbm_minor;

	/* Send side, see "Sending" above. */
	bool		sc_tx_busy;	/* multi fragment message in flight */
	struct mei_ctl_msg sc_ctlq[MEI_CTLQ_LEN]; /* ring of pending ctl */
	int		sc_ctlq_head;	/* next to send */
	int		sc_ctlq_count;
	bool		sc_suspended;

	uint8_t		sc_host_cbd;	/* host buffer depth, dwords */
	uint8_t		sc_me_cbd;	/* ME buffer depth, dwords */

	struct mei_me_client sc_me_clients[MEI_MAX_ME_CLIENTS];
	int		sc_nme_clients;
	TAILQ_HEAD(, mei_cl) sc_clients;
	uint8_t		sc_host_addr_map[(MEI_MAX_HOST_CLIENTS + 1 + 7) / 8];
	int		sc_nopen;

	uint32_t	sc_fwsts[MEI_FWSTS_COUNT];
	uint32_t	sc_rdbuf[256];	/* one message read from ME_CB_RW */
	uint32_t	sc_rx_hdr;	/* header of a partly written message */
	bool		sc_rx_hdr_valid;
};

#define	MEI_LOCK(sc)		mtx_lock(&(sc)->sc_mtx)
#define	MEI_UNLOCK(sc)		mtx_unlock(&(sc)->sc_mtx)
#define	MEI_LOCK_ASSERT(sc)	mtx_assert(&(sc)->sc_mtx, MA_OWNED)

/*
 * Core interface used by mei_cdev.c, implemented in mei.c.  Unless noted
 * all functions are called WITHOUT sc_mtx held and may sleep.  Errors
 * are errno values.
 */

/* Wait (up to MEI_INIT_TIMEOUT) for the link to become READY. */
int	mei_wait_ready(struct mei_softc *sc);

/*
 * Allocate a host client with a free host address and link it into
 * sc_clients.  Fails with EBUSY if all addresses are used, ENXIO if the
 * link is not READY.
 */
int	mei_cl_alloc(struct mei_softc *sc, struct mei_cl **clp);

/*
 * Disconnect if connected, drop queued data and free the client and its
 * host address.  Never fails.
 */
void	mei_cl_free(struct mei_cl *cl);

/*
 * Connect an IDLE client to the ME client with the given protocol GUID.
 * On success fills *props (max_msg_length is the clamped value).
 * ENOTTY: no such ME client; EBUSY: client already connected or ME
 * client out of connections; EALREADY: this client is not IDLE;
 * ETIMEDOUT, EIO, ENXIO as appropriate.
 */
int	mei_cl_connect(struct mei_cl *cl, const uint8_t uuid[16],
	    struct mei_client_props *props);

/*
 * Send one message of len bytes (1..max_msg_length) from the kernel
 * buffer buf.  Waits for a flow control credit and for room in the host
 * circular buffer, splitting the message into fragments as needed.
 * If nonblock and no credit is available, returns EAGAIN.  ENOTCONN if
 * not connected; ENODEV if the connection was lost; EMSGSIZE if too
 * long; EINTR/ERESTART if a signal arrives while waiting.
 */
int	mei_cl_write(struct mei_cl *cl, const void *buf, size_t len,
	    bool nonblock);

/*
 * Copy up to len bytes of the oldest received message into the kernel
 * buffer buf and set *done.  A message that does not fit is returned
 * over several calls.  Waits for a message unless nonblock (EAGAIN).
 * ENOTCONN if never connected; ENODEV if the connection was lost and
 * nothing is queued.
 */
int	mei_cl_read(struct mei_cl *cl, void *buf, size_t len, bool nonblock,
	    size_t *done);

/* poll(2) support: POLLIN/POLLRDNORM when a message is queued,
 * POLLOUT/POLLWRNORM when a write would not block for credits,
 * POLLHUP when the connection was lost. */
int	mei_cl_poll(struct mei_cl *cl, int events, struct thread *td);

/* Implemented in mei_cdev.c, called by mei.c attach/detach. */
int	mei_cdev_create(struct mei_softc *sc);
void	mei_cdev_destroy(struct mei_softc *sc);

#endif /* _DEV_MEI_MEI_VAR_H_ */
