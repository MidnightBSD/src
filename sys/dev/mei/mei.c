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
 * mei(4): driver for the Intel Management Engine Interface (HECI).
 *
 * This file holds the PCI glue, the circular buffer transport, the host
 * bus message (HBM) protocol and the host client core used by the
 * character device in mei_cdev.c.  See mei_var.h for the locking and
 * sending rules.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/poll.h>
#include <sys/rman.h>
#include <sys/sbuf.h>
#include <sys/selinfo.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>
#include <sys/time.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/mei/mei_var.h>

MALLOC_DEFINE(M_MEI, "mei", "Intel MEI driver");

#define	MEI_HOST_RESET_DELAY	20	/* ms with H_RST asserted */
#define	MEI_RETRY_DELAY		200	/* ms between failed link inits */
#define	MEI_POLL_FAST		10	/* ms, polling during link init */
#define	MEI_POLL_SLOW		1000	/* ms, guard against lost interrupts */

static const struct mei_ident {
	uint16_t	devid;
	const char	*desc;
} mei_idents[] = {
	{ 0x9d3a, "Intel Sunrise Point-LP MEI controller" },
	{ 0xa13a, "Intel 100 Series MEI controller" },
	{ 0xa2ba, "Intel 200 Series MEI controller" },
	{ 0x9de0, "Intel Cannon Point-LP MEI controller" },
	{ 0xa360, "Intel Cannon Point-H MEI controller" },
	{ 0x02e0, "Intel Comet Lake-LP MEI controller" },
	{ 0x06e0, "Intel Comet Lake-H MEI controller" },
	{ 0xa3ba, "Intel Comet Lake-V MEI controller" },
	{ 0x34e0, "Intel Ice Lake-LP MEI controller" },
	{ 0x38e0, "Intel Ice Lake-N MEI controller" },
	{ 0x4de0, "Intel Jasper Lake MEI controller" },
	{ 0x4b70, "Intel Elkhart Lake MEI controller" },
	{ 0xa0e0, "Intel Tiger Lake-LP MEI controller" },
	{ 0x43e0, "Intel Tiger Lake-H MEI controller" },
	{ 0x51e0, "Intel Alder Lake-P MEI controller" },
	{ 0x54e0, "Intel Alder Lake-N MEI controller" },
	{ 0x7ae8, "Intel Alder Lake-S MEI controller" },
	{ 0x7a68, "Intel Raptor Lake-S MEI controller" },
	{ 0x7e70, "Intel Meteor Lake-P MEI controller" },
	{ 0x7770, "Intel Arrow Lake MEI controller" },
	{ 0xa870, "Intel Lunar Lake-M MEI controller" },
	{ 0x7f68, "Intel 800 Series MEI controller" },
};

static const bus_size_t mei_fwsts_reg[MEI_FWSTS_COUNT] = {
	MEI_PCI_FWSTS1, MEI_PCI_FWSTS2, MEI_PCI_FWSTS3,
	MEI_PCI_FWSTS4, MEI_PCI_FWSTS5, MEI_PCI_FWSTS6,
};

static devclass_t mei_devclass;

static int	mei_probe(device_t dev);
static int	mei_attach(device_t dev);
static int	mei_detach(device_t dev);
static int	mei_suspend(device_t dev);
static int	mei_resume(device_t dev);
static void	mei_release(struct mei_softc *sc);
static void	mei_sysctl_init(struct mei_softc *sc);
static int	mei_sysctl_fwsts(SYSCTL_HANDLER_ARGS);
static int	mei_sysctl_hbm_version(SYSCTL_HANDLER_ARGS);
static int	mei_sysctl_clients(SYSCTL_HANDLER_ARGS);
static int	mei_sysctl_state(SYSCTL_HANDLER_ARGS);

static void	mei_hcsr_update(struct mei_softc *sc, uint32_t set,
		    uint32_t clr);
static int	mei_sleep(struct mei_softc *sc, void *chan, int pri,
		    const char *wmesg, sbintime_t deadline, int poll_ms);
static sbintime_t mei_deadline(int ms);
static int	mei_send_locked(struct mei_softc *sc, uint32_t hdr,
		    const void *payload, size_t len);
static int	mei_ctl_send(struct mei_softc *sc, const void *payload,
		    size_t len);
static void	mei_ctl_flush(struct mei_softc *sc);
static void	mei_link_reset_locked(struct mei_softc *sc);
static void	mei_reset_bookkeeping(struct mei_softc *sc);
static void	mei_reset_task(void *arg, int pending);
static int	mei_link_init(struct mei_softc *sc);
static bool	mei_init_aborted(struct mei_softc *sc, uint32_t gen);
static int	mei_hbm_request(struct mei_softc *sc, uint32_t gen,
		    const void *req, size_t len, uint8_t rescmd, size_t minlen);
static void	mei_intr(void *arg);
static void	mei_drain_locked(struct mei_softc *sc);
static void	mei_hbm_dispatch(struct mei_softc *sc, const uint8_t *buf,
		    size_t len);
static void	mei_cl_rx(struct mei_softc *sc, uint32_t hdr,
		    const uint8_t *buf, size_t len);
static struct mei_cl *mei_cl_find(struct mei_softc *sc, uint8_t host_addr,
		    uint8_t me_addr, enum mei_cl_state state);
static struct mei_cl *mei_cl_find_conn(struct mei_softc *sc,
		    uint8_t host_addr, uint8_t me_addr);
static void	mei_cl_grant(struct mei_softc *sc, struct mei_cl *cl);
static void	mei_cl_wake(struct mei_cl *cl);
static void	mei_wake_writers(struct mei_softc *sc);
static int	mei_cl_check_conn(struct mei_cl *cl);
static int	*mei_cl_credits(struct mei_cl *cl);
static void	mei_cl_tx_release(struct mei_cl *cl);

/*
 * Register helpers.
 */

/*
 * Read-modify-write H_CSR.  IS (write one to clear) and IG (write one to
 * notify the ME) are never carried over from the value read; pass them
 * in "set" to acknowledge an interrupt or to notify the ME.
 */
static void
mei_hcsr_update(struct mei_softc *sc, uint32_t set, uint32_t clr)
{
	uint32_t csr;

	MEI_LOCK_ASSERT(sc);
	csr = bus_read_4(sc->sc_mem, MEI_H_CSR);
	csr &= MEI_CSR_IE | MEI_CSR_RDY | MEI_CSR_RST;
	csr &= ~clr;
	csr |= set;
	bus_write_4(sc->sc_mem, MEI_H_CSR, csr);
}

/* Absolute deadline ms milliseconds from now. */
static sbintime_t
mei_deadline(int ms)
{

	return (getsbinuptime() + SBT_1MS * ms);
}

/*
 * Sleep on chan with sc_mtx for at most poll_ms milliseconds, but never
 * past deadline.  Returns EWOULDBLOCK only if the deadline had already
 * passed on entry, 0 after a wakeup or a poll tick, or the error from a
 * caught signal.  Callers loop and re-check their condition.
 */
static int
mei_sleep(struct mei_softc *sc, void *chan, int pri, const char *wmesg,
    sbintime_t deadline, int poll_ms)
{
	sbintime_t now, sbt;
	int error;

	MEI_LOCK_ASSERT(sc);
	now = getsbinuptime();
	if (now >= deadline)
		return (EWOULDBLOCK);
	sbt = SBT_1MS * poll_ms;
	if (sbt > deadline - now)
		sbt = deadline - now;
	error = msleep_sbt(chan, &sc->sc_mtx, pri, wmesg, sbt, 0, 0);
	if (error == EWOULDBLOCK)
		error = 0;
	return (error);
}

/*
 * Write one fragment (header plus len payload bytes) to the host
 * circular buffer and notify the ME.  Writes nothing and returns EAGAIN
 * if the fragment does not fit in the free slots.
 */
static int
mei_send_locked(struct mei_softc *sc, uint32_t hdr, const void *payload,
    size_t len)
{
	const uint8_t *p;
	uint32_t csr, v;
	size_t ndw, i, rem;
	u_int filled, cbd;

	MEI_LOCK_ASSERT(sc);
	if (sc->sc_state == MEI_LINK_DETACHING ||
	    sc->sc_state == MEI_LINK_DISABLED)
		return (ENXIO);
	cbd = sc->sc_host_cbd;
	if (cbd < 2 || len > MEI_HDR_MAX_LEN ||
	    len > (size_t)(cbd - 1) * 4)
		return (EINVAL);
	KASSERT(MEI_HDR_LEN(hdr) == len, ("mei: header length mismatch"));

	csr = bus_read_4(sc->sc_mem, MEI_H_CSR);
	filled = (uint8_t)(MEI_CSR_WP(csr) - MEI_CSR_RP(csr));
	if (filled > cbd) {
		device_printf(sc->sc_dev,
		    "host buffer inconsistent (filled %u, depth %u)\n",
		    filled, cbd);
		mei_link_reset_locked(sc);
		return (EIO);
	}
	ndw = howmany(len, 4);
	if (1 + ndw > cbd - filled)
		return (EAGAIN);

	p = payload;
	bus_write_4(sc->sc_mem, MEI_H_CB_WW, hdr);
	for (i = 0; i < ndw; i++) {
		rem = len - i * 4;
		if (rem >= 4)
			v = le32dec(p + i * 4);
		else {
			uint8_t pad[4];

			memset(pad, 0, sizeof(pad));
			memcpy(pad, p + i * 4, rem);
			v = le32dec(pad);
		}
		bus_write_4(sc->sc_mem, MEI_H_CB_WW, v);
	}
	mei_hcsr_update(sc, MEI_CSR_IG, 0);
	return (0);
}

/*
 * Send a single fragment control (HBM) message, or queue it on sc_ctlq
 * if a user message is in flight, earlier control messages are queued,
 * or the buffer is full.  Never sleeps.  A full queue resets the link.
 */
static int
mei_ctl_send(struct mei_softc *sc, const void *payload, size_t len)
{
	struct mei_ctl_msg *m;
	uint32_t hdr;
	int error, idx;

	MEI_LOCK_ASSERT(sc);
	KASSERT(len > 0 && len <= MEI_CTL_MAX_LEN,
	    ("mei: bad control length %zu", len));
	hdr = MEI_HDR_MAKE(MEI_HBM_ADDR, MEI_HBM_ADDR, len, 1);
	if (!sc->sc_tx_busy && sc->sc_ctlq_count == 0) {
		error = mei_send_locked(sc, hdr, payload, len);
		if (error != EAGAIN)
			return (error);
	}
	if (sc->sc_ctlq_count >= MEI_CTLQ_LEN) {
		device_printf(sc->sc_dev, "control queue overflow\n");
		mei_link_reset_locked(sc);
		return (ENOBUFS);
	}
	idx = (sc->sc_ctlq_head + sc->sc_ctlq_count) % MEI_CTLQ_LEN;
	m = &sc->sc_ctlq[idx];
	m->hdr = hdr;
	m->len = len;
	memset(m->data, 0, sizeof(m->data));
	memcpy(m->data, payload, len);
	sc->sc_ctlq_count++;
	return (0);
}

/*
 * Send queued control messages in order while they fit.  The caller
 * makes sure no other thread holds sc_tx_busy (it is either clear or
 * owned by the calling writer, before its first fragment).
 */
static void
mei_ctl_flush(struct mei_softc *sc)
{
	struct mei_ctl_msg *m;
	uint32_t gen;

	MEI_LOCK_ASSERT(sc);
	gen = sc->sc_link_gen;
	while (sc->sc_ctlq_count > 0) {
		m = &sc->sc_ctlq[sc->sc_ctlq_head];
		if (mei_send_locked(sc, m->hdr, m->data, m->len) != 0)
			break;
		if (gen != sc->sc_link_gen)
			break;
		sc->sc_ctlq_head = (sc->sc_ctlq_head + 1) % MEI_CTLQ_LEN;
		sc->sc_ctlq_count--;
	}
}

/*
 * Link resets.
 */

/*
 * Reset bookkeeping: drop all connections and protocol state.  Does
 * not schedule the reset task.
 */
static void
mei_reset_bookkeeping(struct mei_softc *sc)
{
	struct mei_cl *cl;

	MEI_LOCK_ASSERT(sc);
	sc->sc_state = MEI_LINK_INIT;
	sc->sc_link_gen++;
	TAILQ_FOREACH(cl, &sc->sc_clients, link) {
		if (cl->state == MEI_CL_CONNECTING ||
		    cl->state == MEI_CL_CONNECTED ||
		    cl->state == MEI_CL_DISCONNECTING) {
			cl->state = MEI_CL_DISCONNECTED;
			cl->conn_answered = true;
		}
		cl->tx_sending = false;
		cl->rx_credit_out = false;
		cl->rx_asm_len = 0;
		cl->rx_overflow = false;
		mei_cl_wake(cl);
	}
	memset(sc->sc_me_clients, 0, sizeof(sc->sc_me_clients));
	sc->sc_nme_clients = 0;
	sc->sc_ctlq_head = 0;
	sc->sc_ctlq_count = 0;
	sc->sc_tx_busy = false;
	sc->sc_hbm_wait_cmd = 0;
	sc->sc_hbm_answered = false;
	sc->sc_rx_hdr_valid = false;
	wakeup(&sc->sc_hbm_answered);
	wakeup(&sc->sc_host_cbd);
	wakeup(&sc->sc_me_cbd);
}

/* Trigger a link reset from any context; sc_mtx held, never sleeps. */
static void
mei_link_reset_locked(struct mei_softc *sc)
{

	MEI_LOCK_ASSERT(sc);
	if (sc->sc_state == MEI_LINK_DETACHING ||
	    sc->sc_state == MEI_LINK_DISABLED)
		return;
	mei_reset_bookkeeping(sc);
	if (sc->sc_suspended)
		return;
	sc->sc_reset_pending = true;
	if (!sc->sc_task_running)
		taskqueue_enqueue(sc->sc_tq, &sc->sc_reset_task);
}

/* True if the link init sequence started at generation gen must stop. */
static bool
mei_init_aborted(struct mei_softc *sc, uint32_t gen)
{

	MEI_LOCK_ASSERT(sc);
	return (sc->sc_state != MEI_LINK_INIT || sc->sc_suspended ||
	    gen != sc->sc_link_gen);
}

/*
 * Send an HBM request during link init and wait for the response
 * command rescmd.  The response is left in sc_hbm_resp.
 */
static int
mei_hbm_request(struct mei_softc *sc, uint32_t gen, const void *req,
    size_t len, uint8_t rescmd, size_t minlen)
{
	sbintime_t deadline;
	uint32_t hdr;
	int error;

	MEI_LOCK_ASSERT(sc);
	sc->sc_hbm_wait_cmd = rescmd;
	sc->sc_hbm_answered = false;
	sc->sc_hbm_resp_len = 0;
	deadline = mei_deadline(MEI_HBM_TIMEOUT);
	hdr = MEI_HDR_MAKE(MEI_HBM_ADDR, MEI_HBM_ADDR, len, 1);

	for (;;) {
		if (mei_init_aborted(sc, gen)) {
			error = ENXIO;
			goto out;
		}
		error = mei_send_locked(sc, hdr, req, len);
		if (error != EAGAIN)
			break;
		error = mei_sleep(sc, &sc->sc_host_cbd, PZERO, "meihbms",
		    deadline, MEI_POLL_FAST);
		if (error != 0)
			goto out;
	}
	if (error != 0)
		goto out;

	while (!sc->sc_hbm_answered) {
		if (mei_init_aborted(sc, gen)) {
			error = ENXIO;
			goto out;
		}
		error = mei_sleep(sc, &sc->sc_hbm_answered, PZERO, "meihbmr",
		    deadline, MEI_POLL_FAST);
		if (error != 0)
			goto out;
	}
	if (mei_init_aborted(sc, gen))
		error = ENXIO;
	else if (sc->sc_hbm_resp_len < minlen)
		error = EIO;
out:
	if (error == EWOULDBLOCK) {
		device_printf(sc->sc_dev, "HBM command 0x%02x timed out\n",
		    rescmd & ~MEI_HBM_RESPONSE);
		error = ETIMEDOUT;
	}
	sc->sc_hbm_wait_cmd = 0;
	return (error);
}

/*
 * Reset the hardware link and run the HBM start/enumeration sequence.
 * Called from the reset task with sc_mtx held; sleeps.
 */
static int
mei_link_init(struct mei_softc *sc)
{
	struct mei_hbm_version_req vreq;
	struct mei_hbm_version_res vres;
	struct mei_hbm_enum_req ereq;
	struct mei_hbm_props_req preq;
	struct mei_hbm_props_res pres;
	struct mei_me_client *mc;
	uint8_t valid[sizeof(((struct mei_hbm_enum_res *)0)->valid_addresses)];
	sbintime_t deadline;
	uint32_t gen, hcsr, mecsr, maxlen;
	uint8_t major, minor;
	u_int addr, host_cbd, me_cbd;
	int error, try;

	MEI_LOCK_ASSERT(sc);
	gen = sc->sc_link_gen;

	/* 1. Host reset, then wait for the ME to come ready. */
	mei_hcsr_update(sc, MEI_CSR_RST | MEI_CSR_IG, MEI_CSR_IE | MEI_CSR_RDY);
	deadline = mei_deadline(MEI_HOST_RESET_DELAY);
	while (mei_sleep(sc, &sc->sc_reset_count, PZERO, "meirst", deadline,
	    MEI_HOST_RESET_DELAY) == 0) {
		if (mei_init_aborted(sc, gen))
			return (ENXIO);
	}
	if (mei_init_aborted(sc, gen))
		return (ENXIO);
	mei_hcsr_update(sc, MEI_CSR_IG | MEI_CSR_IE, MEI_CSR_RST);

	deadline = mei_deadline(MEI_HW_READY_TIMEOUT);
	for (;;) {
		mecsr = bus_read_4(sc->sc_mem, MEI_ME_CSR_HA);
		if ((mecsr & MEI_CSR_RDY) != 0 && (mecsr & MEI_CSR_RST) == 0)
			break;
		error = mei_sleep(sc, &sc->sc_me_cbd, PZERO, "meihwrdy",
		    deadline, MEI_POLL_FAST);
		if (error == EWOULDBLOCK) {
			device_printf(sc->sc_dev,
			    "ME not ready (ME_CSR 0x%08x)\n", mecsr);
			return (ETIMEDOUT);
		}
		if (mei_init_aborted(sc, gen))
			return (ENXIO);
	}
	mei_hcsr_update(sc, MEI_CSR_RDY | MEI_CSR_IG | MEI_CSR_IE, 0);

	/* 2. Buffer depths. */
	hcsr = bus_read_4(sc->sc_mem, MEI_H_CSR);
	mecsr = bus_read_4(sc->sc_mem, MEI_ME_CSR_HA);
	host_cbd = MEI_CSR_CBD(hcsr);
	me_cbd = MEI_CSR_CBD(mecsr);
	if (host_cbd < 2 || me_cbd < 2 || me_cbd > nitems(sc->sc_rdbuf)) {
		device_printf(sc->sc_dev,
		    "bad buffer depth (host %u, ME %u)\n", host_cbd, me_cbd);
		return (EIO);
	}
	sc->sc_host_cbd = host_cbd;
	sc->sc_me_cbd = me_cbd;

	/* 3. Protocol version. */
	major = MEI_HBM_MAJOR;
	minor = MEI_HBM_MINOR;
	for (try = 0;; try++) {
		memset(&vreq, 0, sizeof(vreq));
		vreq.cmd = MEI_HBM_HOST_START_REQ;
		vreq.major = major;
		vreq.minor = minor;
		error = mei_hbm_request(sc, gen, &vreq, sizeof(vreq),
		    MEI_HBM_HOST_START_RES, sizeof(vres));
		if (error != 0)
			return (error);
		memcpy(&vres, sc->sc_hbm_resp, sizeof(vres));
		if (vres.supported != 0)
			break;
		if (try == 0 && major > 1 && vres.major >= 1) {
			major = 1;
			minor = 0;
			continue;
		}
		device_printf(sc->sc_dev,
		    "HBM version %u.%u not supported (ME offers %u.%u)\n",
		    major, minor, vres.major, vres.minor);
		return (EIO);
	}
	sc->sc_hbm_major = major;
	sc->sc_hbm_minor = minor;

	/* 4. Enumerate ME client addresses. */
	memset(&ereq, 0, sizeof(ereq));
	ereq.cmd = MEI_HBM_HOST_ENUM_REQ;
	ereq.flags = 0;
	error = mei_hbm_request(sc, gen, &ereq, sizeof(ereq),
	    MEI_HBM_HOST_ENUM_RES, sizeof(struct mei_hbm_enum_res));
	if (error != 0)
		return (error);
	memcpy(valid, sc->sc_hbm_resp +
	    offsetof(struct mei_hbm_enum_res, valid_addresses), sizeof(valid));

	/* 5. Properties of every ME client. */
	for (addr = 1; addr < MEI_MAX_ME_CLIENTS; addr++) {
		if ((valid[addr / 8] & (1u << (addr % 8))) == 0)
			continue;
		memset(&preq, 0, sizeof(preq));
		preq.cmd = MEI_HBM_CLIENT_PROPS_REQ;
		preq.me_addr = addr;
		error = mei_hbm_request(sc, gen, &preq, sizeof(preq),
		    MEI_HBM_CLIENT_PROPS_RES, sizeof(pres));
		if (error != 0)
			return (error);
		memcpy(&pres, sc->sc_hbm_resp, sizeof(pres));
		if (pres.status != MEI_PROPS_STATUS_SUCCESS ||
		    pres.me_addr != addr) {
			device_printf(sc->sc_dev,
			    "bad properties for ME client %u "
			    "(status %u, address %u)\n",
			    addr, pres.status, pres.me_addr);
			return (EIO);
		}
		mc = &sc->sc_me_clients[addr];
		mc->valid = true;
		mc->addr = addr;
		memcpy(mc->uuid, pres.props.uuid, sizeof(mc->uuid));
		mc->protocol_version = pres.props.protocol_version;
		mc->max_connections = pres.props.max_connections;
		mc->fixed_address = pres.props.fixed_address;
		mc->single_recv_buf = pres.props.single_recv_buf & 1;
		maxlen = le32toh(pres.props.max_msg_length);
		if (maxlen > MEI_MAX_MSG_LEN)
			maxlen = MEI_MAX_MSG_LEN;
		mc->max_msg_length = maxlen;
		mc->nconnections = 0;
		mc->tx_credits = 0;
		sc->sc_nme_clients++;
	}

	/* 6. Ready. */
	sc->sc_state = MEI_LINK_READY;
	sc->sc_reset_count = 0;
	wakeup(&sc->sc_state);
	device_printf(sc->sc_dev, "HBM %u.%u, %d ME clients\n",
	    sc->sc_hbm_major, sc->sc_hbm_minor, sc->sc_nme_clients);
	return (0);
}

/* Taskqueue handler: (re)initialise the link until it works. */
static void
mei_reset_task(void *arg, int pending __unused)
{
	struct mei_softc *sc;
	sbintime_t deadline;
	int error;

	sc = arg;
	MEI_LOCK(sc);
	sc->sc_task_running = true;
	while (sc->sc_reset_pending) {
		sc->sc_reset_pending = false;
		if (sc->sc_state != MEI_LINK_INIT || sc->sc_suspended)
			break;
		error = mei_link_init(sc);
		if (error == 0)
			continue;
		if (sc->sc_state == MEI_LINK_DETACHING ||
		    sc->sc_state == MEI_LINK_DISABLED || sc->sc_suspended)
			break;
		/* Discard whatever the failed attempt enumerated. */
		mei_reset_bookkeeping(sc);
		if (++sc->sc_reset_count >= MEI_MAX_RESETS) {
			sc->sc_state = MEI_LINK_DISABLED;
			wakeup(&sc->sc_state);
			device_printf(sc->sc_dev,
			    "link initialisation failed (error %d), "
			    "giving up\n", error);
			break;
		}
		device_printf(sc->sc_dev,
		    "link initialisation failed (error %d), retrying\n",
		    error);
		deadline = mei_deadline(MEI_RETRY_DELAY);
		while (mei_sleep(sc, &sc->sc_reset_count, PZERO, "meiretry",
		    deadline, MEI_RETRY_DELAY) == 0) {
			if (sc->sc_state != MEI_LINK_INIT || sc->sc_suspended)
				break;
		}
		sc->sc_reset_pending = true;
	}
	sc->sc_task_running = false;
	MEI_UNLOCK(sc);
}

/*
 * Interrupt handling and receive path.
 */

static void
mei_intr(void *arg)
{
	struct mei_softc *sc;
	uint32_t hcsr, mecsr, gen;

	sc = arg;
	MEI_LOCK(sc);
	if (sc->sc_suspended) {
		/* Device may be powered down; do not touch it. */
		MEI_UNLOCK(sc);
		return;
	}
	hcsr = bus_read_4(sc->sc_mem, MEI_H_CSR);
	if ((hcsr & MEI_CSR_IS) == 0) {
		MEI_UNLOCK(sc);
		return;
	}
	mei_hcsr_update(sc, MEI_CSR_IS, 0);

	if (sc->sc_state == MEI_LINK_DETACHING ||
	    sc->sc_state == MEI_LINK_DISABLED || sc->sc_suspended)
		goto out;
	mecsr = bus_read_4(sc->sc_mem, MEI_ME_CSR_HA);
	if (sc->sc_state == MEI_LINK_INIT && sc->sc_hbm_wait_cmd == 0) {
		/* Hardware handshake in progress, or reset not yet run. */
		wakeup(&sc->sc_me_cbd);
		goto out;
	}
	if ((mecsr & MEI_CSR_RST) != 0 || (mecsr & MEI_CSR_RDY) == 0) {
		device_printf(sc->sc_dev, "ME reset (ME_CSR 0x%08x)\n",
		    mecsr);
		mei_link_reset_locked(sc);
		goto out;
	}

	gen = sc->sc_link_gen;
	mei_drain_locked(sc);
	if (gen != sc->sc_link_gen)
		goto out;
	if (!sc->sc_tx_busy)
		mei_ctl_flush(sc);
	wakeup(&sc->sc_host_cbd);
out:
	MEI_UNLOCK(sc);
}

/* Read and dispatch every complete message in the ME circular buffer. */
static void
mei_drain_locked(struct mei_softc *sc)
{
	const uint8_t *p;
	uint32_t mecsr, hdr, gen, v;
	size_t len, ndw, i;
	u_int avail, filled;
	bool drained;

	MEI_LOCK_ASSERT(sc);
	gen = sc->sc_link_gen;
	drained = false;
	for (;;) {
		mecsr = bus_read_4(sc->sc_mem, MEI_ME_CSR_HA);
		filled = (uint8_t)(MEI_CSR_WP(mecsr) - MEI_CSR_RP(mecsr));
		if (filled > sc->sc_me_cbd) {
			device_printf(sc->sc_dev,
			    "ME buffer inconsistent (filled %u, depth %u)\n",
			    filled, sc->sc_me_cbd);
			mei_link_reset_locked(sc);
			return;
		}
		if (sc->sc_rx_hdr_valid) {
			/* Header already read; payload was incomplete. */
			hdr = sc->sc_rx_hdr;
			avail = filled;
		} else {
			if (filled == 0)
				break;
			hdr = bus_read_4(sc->sc_mem, MEI_ME_CB_RW);
			drained = true;
			avail = filled - 1;
			if ((hdr & (MEI_HDR_DMA_RING | MEI_HDR_EXTENDED)) !=
			    0) {
				device_printf(sc->sc_dev,
				    "unsupported message header 0x%08x\n", hdr);
				mei_link_reset_locked(sc);
				return;
			}
		}
		len = MEI_HDR_LEN(hdr);
		ndw = howmany(len, 4);
		if (ndw > nitems(sc->sc_rdbuf) || ndw + 1 > sc->sc_me_cbd) {
			device_printf(sc->sc_dev,
			    "oversized message (header 0x%08x, depth %u)\n",
			    hdr, sc->sc_me_cbd);
			mei_link_reset_locked(sc);
			return;
		}
		if (ndw > avail) {
			/*
			 * The ME is still writing the payload; keep the
			 * header and finish on the next interrupt.
			 */
			sc->sc_rx_hdr = hdr;
			sc->sc_rx_hdr_valid = true;
			break;
		}
		sc->sc_rx_hdr_valid = false;
		if (ndw > 0)
			drained = true;
		for (i = 0; i < ndw; i++) {
			v = bus_read_4(sc->sc_mem, MEI_ME_CB_RW);
			le32enc(&sc->sc_rdbuf[i], v);
		}
		p = (const uint8_t *)sc->sc_rdbuf;
		if (MEI_HDR_ME_ADDR(hdr) == MEI_HBM_ADDR &&
		    MEI_HDR_HOST_ADDR(hdr) == MEI_HBM_ADDR)
			mei_hbm_dispatch(sc, p, len);
		else
			mei_cl_rx(sc, hdr, p, len);
		if (gen != sc->sc_link_gen ||
		    sc->sc_state == MEI_LINK_DETACHING ||
		    sc->sc_state == MEI_LINK_DISABLED)
			return;
	}
	if (drained)
		mei_hcsr_update(sc, MEI_CSR_IG, 0);
}

/* Find the client with the given addresses in the given state. */
static struct mei_cl *
mei_cl_find(struct mei_softc *sc, uint8_t host_addr, uint8_t me_addr,
    enum mei_cl_state state)
{
	struct mei_cl *cl;

	MEI_LOCK_ASSERT(sc);
	TAILQ_FOREACH(cl, &sc->sc_clients, link) {
		if (cl->host_addr == host_addr && cl->me_addr == me_addr &&
		    cl->state == state && cl->link_gen == sc->sc_link_gen)
			return (cl);
	}
	return (NULL);
}

/*
 * Find a connected client, including one whose successful connect
 * response has arrived but whose thread has not yet run: the ME may
 * send flow control or a disconnect request right behind the response.
 */
static struct mei_cl *
mei_cl_find_conn(struct mei_softc *sc, uint8_t host_addr, uint8_t me_addr)
{
	struct mei_cl *cl;

	MEI_LOCK_ASSERT(sc);
	cl = mei_cl_find(sc, host_addr, me_addr, MEI_CL_CONNECTED);
	if (cl != NULL)
		return (cl);
	cl = mei_cl_find(sc, host_addr, me_addr, MEI_CL_CONNECTING);
	if (cl != NULL && cl->conn_answered &&
	    cl->conn_status == MEI_CL_STATUS_SUCCESS)
		return (cl);
	return (NULL);
}

/* Wake every sleeper and poller of a client. */
static void
mei_cl_wake(struct mei_cl *cl)
{

	wakeup(cl);
	selwakeup(&cl->rsel);
}

/* Wake writers waiting for credits or buffer space, and pollers. */
static void
mei_wake_writers(struct mei_softc *sc)
{
	struct mei_cl *cl;

	MEI_LOCK_ASSERT(sc);
	wakeup(&sc->sc_host_cbd);
	TAILQ_FOREACH(cl, &sc->sc_clients, link) {
		if (cl->state == MEI_CL_CONNECTED)
			selwakeup(&cl->rsel);
	}
}

static void
mei_hbm_dispatch(struct mei_softc *sc, const uint8_t *buf, size_t len)
{
	struct mei_hbm_client_conn conn, res;
	struct mei_hbm_flow_control fc;
	struct mei_me_client *mc;
	struct mei_cl *cl;
	size_t minlen;
	int *credits;
	uint8_t cmd;

	MEI_LOCK_ASSERT(sc);
	if (len == 0) {
		device_printf(sc->sc_dev, "empty HBM message\n");
		return;
	}
	cmd = buf[0];
	switch (cmd) {
	case MEI_HBM_HOST_START_RES:
		minlen = sizeof(struct mei_hbm_version_res);
		break;
	case MEI_HBM_HOST_ENUM_RES:
		minlen = sizeof(struct mei_hbm_enum_res);
		break;
	case MEI_HBM_CLIENT_PROPS_RES:
		minlen = sizeof(struct mei_hbm_props_res);
		break;
	case MEI_HBM_CLIENT_CONNECT_RES:
	case MEI_HBM_CLIENT_DISCONNECT_RES:
	case MEI_HBM_CLIENT_DISCONNECT_REQ:
		minlen = sizeof(struct mei_hbm_client_conn);
		break;
	case MEI_HBM_FLOW_CONTROL:
		minlen = sizeof(struct mei_hbm_flow_control);
		break;
	case MEI_HBM_ME_STOP_REQ:
		minlen = 1;
		break;
	default:
		device_printf(sc->sc_dev,
		    "ignoring HBM command 0x%02x (%zu bytes)\n", cmd, len);
		return;
	}
	if (len < minlen) {
		device_printf(sc->sc_dev,
		    "short HBM command 0x%02x (%zu < %zu bytes)\n",
		    cmd, len, minlen);
		return;
	}

	switch (cmd) {
	case MEI_HBM_HOST_START_RES:
	case MEI_HBM_HOST_ENUM_RES:
	case MEI_HBM_CLIENT_PROPS_RES:
		if (sc->sc_state != MEI_LINK_INIT || sc->sc_hbm_wait_cmd != cmd)
			break;
		sc->sc_hbm_resp_len = MIN(len, sizeof(sc->sc_hbm_resp));
		memcpy(sc->sc_hbm_resp, buf, sc->sc_hbm_resp_len);
		sc->sc_hbm_answered = true;
		wakeup(&sc->sc_hbm_answered);
		break;
	case MEI_HBM_CLIENT_CONNECT_RES:
	case MEI_HBM_CLIENT_DISCONNECT_RES:
		memcpy(&conn, buf, sizeof(conn));
		cl = mei_cl_find(sc, conn.host_addr, conn.me_addr,
		    cmd == MEI_HBM_CLIENT_CONNECT_RES ? MEI_CL_CONNECTING :
		    MEI_CL_DISCONNECTING);
		if (cl == NULL)
			break;
		cl->conn_status = conn.status;
		cl->conn_answered = true;
		wakeup(cl);
		break;
	case MEI_HBM_CLIENT_DISCONNECT_REQ:
		if (sc->sc_state != MEI_LINK_READY)
			break;
		memcpy(&conn, buf, sizeof(conn));
		cl = mei_cl_find_conn(sc, conn.host_addr, conn.me_addr);
		if (cl != NULL) {
			cl->state = MEI_CL_DISCONNECTED;
			mc = &sc->sc_me_clients[cl->me_addr];
			if (mc->nconnections > 0)
				mc->nconnections--;
			mei_cl_wake(cl);
			wakeup(&sc->sc_host_cbd);
		} else {
			/* Crossed with our own disconnect request. */
			cl = mei_cl_find(sc, conn.host_addr, conn.me_addr,
			    MEI_CL_DISCONNECTING);
			if (cl != NULL) {
				cl->conn_status = MEI_CL_STATUS_SUCCESS;
				cl->conn_answered = true;
				wakeup(cl);
			}
		}
		memset(&res, 0, sizeof(res));
		res.cmd = MEI_HBM_CLIENT_DISCONNECT_RES;
		res.me_addr = conn.me_addr;
		res.host_addr = conn.host_addr;
		res.status = MEI_CL_STATUS_SUCCESS;
		(void)mei_ctl_send(sc, &res, sizeof(res));
		break;
	case MEI_HBM_FLOW_CONTROL:
		if (sc->sc_state != MEI_LINK_READY)
			break;
		memcpy(&fc, buf, sizeof(fc));
		if (fc.host_addr == MEI_HBM_ADDR) {
			mc = &sc->sc_me_clients[fc.me_addr];
			if (!mc->valid || !mc->single_recv_buf)
				break;
			if (mc->tx_credits < INT_MAX)
				mc->tx_credits++;
		} else {
			cl = mei_cl_find_conn(sc, fc.host_addr, fc.me_addr);
			if (cl == NULL)
				break;
			/* Credit the counter the writer actually uses. */
			credits = mei_cl_credits(cl);
			if (*credits < INT_MAX)
				(*credits)++;
		}
		mei_wake_writers(sc);
		break;
	case MEI_HBM_ME_STOP_REQ:
		device_printf(sc->sc_dev, "ME requested stop\n");
		mei_link_reset_locked(sc);
		break;
	}
}

/* Data message for a host client. */
static void
mei_cl_rx(struct mei_softc *sc, uint32_t hdr, const uint8_t *buf,
    size_t len)
{
	struct mei_me_client *mc;
	struct mei_rx_msg *m;
	struct mei_cl *cl;

	MEI_LOCK_ASSERT(sc);
	if (sc->sc_state != MEI_LINK_READY)
		return;
	cl = mei_cl_find(sc, MEI_HDR_HOST_ADDR(hdr), MEI_HDR_ME_ADDR(hdr),
	    MEI_CL_CONNECTED);
	if (cl == NULL)
		return;
	mc = &sc->sc_me_clients[cl->me_addr];

	if (!cl->rx_overflow) {
		if (cl->rx_asm == NULL && mc->max_msg_length > 0) {
			cl->rx_asm = malloc(mc->max_msg_length, M_MEI,
			    M_NOWAIT);
			cl->rx_asm_size = cl->rx_asm != NULL ?
			    mc->max_msg_length : 0;
			cl->rx_asm_len = 0;
		}
		if (cl->rx_asm == NULL ||
		    cl->rx_asm_len > cl->rx_asm_size ||
		    len > cl->rx_asm_size - cl->rx_asm_len)
			cl->rx_overflow = true;
		else {
			memcpy(cl->rx_asm + cl->rx_asm_len, buf, len);
			cl->rx_asm_len += len;
		}
	}
	if ((hdr & MEI_HDR_MSG_COMPLETE) == 0)
		return;

	if (cl->rx_overflow)
		device_printf(sc->sc_dev,
		    "client %u: dropped oversized message\n", cl->host_addr);
	else if (cl->rx_asm_len == 0)
		device_printf(sc->sc_dev,
		    "client %u: dropped empty message\n", cl->host_addr);
	else if (cl->rxq_len >= MEI_RXQ_MAX)
		device_printf(sc->sc_dev,
		    "client %u: receive queue full, message dropped\n",
		    cl->host_addr);
	else {
		m = malloc(sizeof(*m) + cl->rx_asm_len, M_MEI, M_NOWAIT);
		if (m == NULL)
			device_printf(sc->sc_dev,
			    "client %u: no memory, message dropped\n",
			    cl->host_addr);
		else {
			m->len = cl->rx_asm_len;
			m->off = 0;
			memcpy(m->data, cl->rx_asm, m->len);
			STAILQ_INSERT_TAIL(&cl->rxq, m, link);
			cl->rxq_len++;
			mei_cl_wake(cl);
		}
	}
	cl->rx_asm_len = 0;
	cl->rx_overflow = false;

	/* The ME used its credit; grant another if there is room. */
	cl->rx_credit_out = false;
	if (cl->rxq_len < MEI_RXQ_MAX)
		mei_cl_grant(sc, cl);
}

/* Give the ME permission to send this client one more message. */
static void
mei_cl_grant(struct mei_softc *sc, struct mei_cl *cl)
{
	struct mei_hbm_flow_control fc;

	MEI_LOCK_ASSERT(sc);
	memset(&fc, 0, sizeof(fc));
	fc.cmd = MEI_HBM_FLOW_CONTROL;
	fc.me_addr = cl->me_addr;
	fc.host_addr = cl->host_addr;
	if (mei_ctl_send(sc, &fc, sizeof(fc)) == 0)
		cl->rx_credit_out = true;
}

/*
 * Host client API used by mei_cdev.c.
 */

int
mei_wait_ready(struct mei_softc *sc)
{
	sbintime_t deadline;
	int error;

	MEI_LOCK(sc);
	deadline = mei_deadline(MEI_INIT_TIMEOUT);
	error = 0;
	while (sc->sc_state == MEI_LINK_INIT) {
		error = mei_sleep(sc, &sc->sc_state, PZERO | PCATCH, "meirdy",
		    deadline, MEI_POLL_SLOW);
		if (error != 0)
			break;
	}
	if (error == EWOULDBLOCK)
		error = ETIMEDOUT;
	else if (error == 0 && sc->sc_state != MEI_LINK_READY)
		error = ENXIO;
	MEI_UNLOCK(sc);
	return (error);
}

int
mei_cl_alloc(struct mei_softc *sc, struct mei_cl **clp)
{
	struct mei_cl *cl;
	u_int addr;
	int error;

	*clp = NULL;
	cl = malloc(sizeof(*cl), M_MEI, M_WAITOK | M_ZERO);
	MEI_LOCK(sc);
	if (sc->sc_state != MEI_LINK_READY) {
		error = ENXIO;
		goto fail;
	}
	for (addr = 1; addr <= MEI_MAX_HOST_CLIENTS; addr++) {
		if (isclr(sc->sc_host_addr_map, addr))
			break;
	}
	if (addr > MEI_MAX_HOST_CLIENTS) {
		error = EBUSY;
		goto fail;
	}
	setbit(sc->sc_host_addr_map, addr);
	cl->sc = sc;
	cl->state = MEI_CL_IDLE;
	cl->host_addr = addr;
	cl->link_gen = sc->sc_link_gen;
	STAILQ_INIT(&cl->rxq);
	TAILQ_INSERT_TAIL(&sc->sc_clients, cl, link);
	sc->sc_nopen++;
	MEI_UNLOCK(sc);
	*clp = cl;
	return (0);
fail:
	MEI_UNLOCK(sc);
	free(cl, M_MEI);
	return (error);
}

void
mei_cl_free(struct mei_cl *cl)
{
	struct mei_hbm_client_conn req;
	struct mei_softc *sc;
	struct mei_me_client *mc;
	struct mei_rx_msg *m;
	sbintime_t deadline;
	uint32_t gen;
	int error;

	sc = cl->sc;
	MEI_LOCK(sc);
	gen = sc->sc_link_gen;
	if (cl->state == MEI_CL_CONNECTED && cl->link_gen == gen &&
	    sc->sc_state == MEI_LINK_READY) {
		memset(&req, 0, sizeof(req));
		req.cmd = MEI_HBM_CLIENT_DISCONNECT_REQ;
		req.me_addr = cl->me_addr;
		req.host_addr = cl->host_addr;
		cl->conn_answered = false;
		cl->state = MEI_CL_DISCONNECTING;
		error = mei_ctl_send(sc, &req, sizeof(req));
		deadline = mei_deadline(MEI_CONNECT_TIMEOUT);
		while (error == 0 && !cl->conn_answered &&
		    gen == sc->sc_link_gen &&
		    sc->sc_state == MEI_LINK_READY)
			error = mei_sleep(sc, cl, PZERO, "meidisc", deadline,
			    MEI_POLL_SLOW);
		if (error == EWOULDBLOCK && gen == sc->sc_link_gen) {
			device_printf(sc->sc_dev,
			    "client %u: disconnect timed out\n",
			    cl->host_addr);
			mei_link_reset_locked(sc);
		}
	}
	/* Release the connection slot reserved by mei_cl_connect(). */
	if (cl->link_gen == sc->sc_link_gen &&
	    (cl->state == MEI_CL_CONNECTING ||
	    cl->state == MEI_CL_CONNECTED ||
	    cl->state == MEI_CL_DISCONNECTING)) {
		mc = &sc->sc_me_clients[cl->me_addr];
		if (mc->nconnections > 0)
			mc->nconnections--;
	}
	if (cl->tx_sending) {
		cl->tx_sending = false;
		sc->sc_tx_busy = false;
		mei_wake_writers(sc);
	}
	cl->state = MEI_CL_DISCONNECTED;
	TAILQ_REMOVE(&sc->sc_clients, cl, link);
	clrbit(sc->sc_host_addr_map, cl->host_addr);
	wakeup(cl);
	MEI_UNLOCK(sc);

	/* Unlinked: no other thread can reach cl now. */
	free(cl->rx_asm, M_MEI);
	while ((m = STAILQ_FIRST(&cl->rxq)) != NULL) {
		STAILQ_REMOVE_HEAD(&cl->rxq, link);
		free(m, M_MEI);
	}
	seldrain(&cl->rsel);
	free(cl, M_MEI);

	/* Last use of sc: detach may free it once sc_nopen is 0. */
	MEI_LOCK(sc);
	sc->sc_nopen--;
	wakeup(&sc->sc_nopen);
	MEI_UNLOCK(sc);
}

int
mei_cl_connect(struct mei_cl *cl, const uint8_t uuid[16],
    struct mei_client_props *props)
{
	struct mei_hbm_client_conn req;
	struct mei_softc *sc;
	struct mei_me_client *mc;
	sbintime_t deadline;
	uint32_t gen;
	u_int addr, maxconn;
	int error;

	sc = cl->sc;
	MEI_LOCK(sc);
	if (sc->sc_state != MEI_LINK_READY) {
		error = ENXIO;
		goto out;
	}
	if (cl->state != MEI_CL_IDLE) {
		error = EALREADY;
		goto out;
	}
	mc = NULL;
	for (addr = 1; addr < MEI_MAX_ME_CLIENTS; addr++) {
		if (sc->sc_me_clients[addr].valid &&
		    memcmp(sc->sc_me_clients[addr].uuid, uuid, 16) == 0) {
			mc = &sc->sc_me_clients[addr];
			break;
		}
	}
	if (mc == NULL || mc->max_msg_length == 0) {
		error = ENOTTY;
		goto out;
	}
	/* A client that accepts no connections cannot be opened. */
	if (mc->max_connections == 0) {
		error = ENOTTY;
		goto out;
	}
	maxconn = mc->max_connections;
	if (mc->nconnections >= (int)maxconn) {
		error = EBUSY;
		goto out;
	}

	/* Reserve the connection slot; released on failure. */
	gen = sc->sc_link_gen;
	mc->nconnections++;
	cl->me_addr = mc->addr;
	cl->link_gen = gen;
	cl->conn_status = 0;
	cl->conn_answered = false;
	cl->tx_credits = 0;
	cl->rx_credit_out = false;
	cl->rx_asm_len = 0;
	cl->rx_overflow = false;
	cl->state = MEI_CL_CONNECTING;
	memset(&req, 0, sizeof(req));
	req.cmd = MEI_HBM_CLIENT_CONNECT_REQ;
	req.me_addr = cl->me_addr;
	req.host_addr = cl->host_addr;
	error = mei_ctl_send(sc, &req, sizeof(req));
	deadline = mei_deadline(MEI_CONNECT_TIMEOUT);
	while (error == 0 && !cl->conn_answered &&
	    gen == sc->sc_link_gen && sc->sc_state == MEI_LINK_READY)
		error = mei_sleep(sc, cl, PZERO, "meiconn", deadline,
		    MEI_POLL_SLOW);

	if (sc->sc_state == MEI_LINK_DETACHING) {
		cl->state = MEI_CL_DISCONNECTED;
		error = ENXIO;
		goto out;
	}
	if (gen != sc->sc_link_gen) {
		/* Reset bookkeeping marked us DISCONNECTED. */
		error = ENODEV;
		goto out;
	}
	if (cl->state != MEI_CL_CONNECTING) {
		/*
		 * The ME disconnected us right after accepting; the
		 * dispatcher already released the connection slot.
		 */
		error = ENODEV;
		goto out;
	}
	if (error == EWOULDBLOCK) {
		device_printf(sc->sc_dev, "client %u: connect timed out\n",
		    cl->host_addr);
		mei_link_reset_locked(sc);
		error = ETIMEDOUT;
		goto out;
	}
	if (error != 0 || !cl->conn_answered) {
		/* Not expected: ctl send failure without a reset. */
		mc->nconnections--;
		cl->state = MEI_CL_IDLE;
		if (error == 0)
			error = EIO;
		goto out;
	}

	switch (cl->conn_status) {
	case MEI_CL_STATUS_SUCCESS:
		error = 0;
		break;
	case MEI_CL_STATUS_NOT_FOUND:
		error = ENOTTY;
		break;
	case MEI_CL_STATUS_ALREADY_STARTED:
	case MEI_CL_STATUS_OUT_OF_RESOURCES:
		error = EBUSY;
		break;
	default:
		error = EIO;
		break;
	}
	if (error != 0) {
		mc->nconnections--;
		cl->state = MEI_CL_IDLE;
		goto out;
	}

	cl->state = MEI_CL_CONNECTED;
	mei_cl_grant(sc, cl);
	if (props != NULL) {
		memset(props, 0, sizeof(*props));
		memcpy(props->uuid, mc->uuid, sizeof(props->uuid));
		props->protocol_version = mc->protocol_version;
		props->max_connections = mc->max_connections;
		props->fixed_address = mc->fixed_address;
		props->single_recv_buf = mc->single_recv_buf;
		props->max_msg_length = mc->max_msg_length;
	}
out:
	MEI_UNLOCK(sc);
	return (error);
}

/*
 * 0 if cl is connected on the current link generation, else the errno
 * a read or write should report.
 */
static int
mei_cl_check_conn(struct mei_cl *cl)
{
	struct mei_softc *sc;

	sc = cl->sc;
	MEI_LOCK_ASSERT(sc);
	if (sc->sc_state == MEI_LINK_DETACHING)
		return (ENXIO);
	switch (cl->state) {
	case MEI_CL_IDLE:
	case MEI_CL_CONNECTING:
		return (ENOTCONN);
	case MEI_CL_CONNECTED:
		if (cl->link_gen == sc->sc_link_gen &&
		    sc->sc_state == MEI_LINK_READY)
			return (0);
		return (ENODEV);
	default:
		return (ENODEV);
	}
}

/* The credit counter a write by a connected client consumes. */
static int *
mei_cl_credits(struct mei_cl *cl)
{
	struct mei_me_client *mc;

	mc = &cl->sc->sc_me_clients[cl->me_addr];
	if (mc->single_recv_buf)
		return (&mc->tx_credits);
	return (&cl->tx_credits);
}

/* Give up sc_tx_busy if cl still holds it and let others proceed. */
static void
mei_cl_tx_release(struct mei_cl *cl)
{
	struct mei_softc *sc;

	sc = cl->sc;
	MEI_LOCK_ASSERT(sc);
	if (cl->tx_sending) {
		cl->tx_sending = false;
		sc->sc_tx_busy = false;
		if (sc->sc_state == MEI_LINK_READY)
			mei_ctl_flush(sc);
	}
	mei_wake_writers(sc);
}

int
mei_cl_write(struct mei_cl *cl, const void *buf, size_t len, bool nonblock)
{
	struct mei_softc *sc;
	struct mei_me_client *mc;
	const uint8_t *p;
	size_t off, n, maxfrag;
	sbintime_t fragdl;
	uint32_t hdr;
	int *credits;
	int error;

	sc = cl->sc;
	p = buf;
	off = 0;
	MEI_LOCK(sc);
	error = mei_cl_check_conn(cl);
	if (error != 0)
		goto out;
	mc = &sc->sc_me_clients[cl->me_addr];
	if (len == 0 || len > mc->max_msg_length) {
		error = EMSGSIZE;
		goto out;
	}

	/* Wait for a flow control credit and for the transmitter. */
	for (;;) {
		credits = mei_cl_credits(cl);
		if (*credits > 0 && !sc->sc_tx_busy)
			break;
		if (nonblock) {
			error = EAGAIN;
			goto out;
		}
		error = mei_sleep(sc, &sc->sc_host_cbd, PZERO | PCATCH,
		    "meicred", SBT_MAX, MEI_POLL_SLOW);
		if (error == 0)
			error = mei_cl_check_conn(cl);
		if (error != 0)
			goto out;
	}
	(*credits)--;
	sc->sc_tx_busy = true;
	cl->tx_sending = true;

	/* Queued control messages go out before our data. */
	for (;;) {
		mei_ctl_flush(sc);
		error = mei_cl_check_conn(cl);
		if (error != 0)
			goto out_release;
		if (sc->sc_ctlq_count == 0)
			break;
		error = mei_sleep(sc, &sc->sc_host_cbd, PZERO | PCATCH,
		    "meictl", SBT_MAX, MEI_POLL_FAST);
		if (error == 0)
			error = mei_cl_check_conn(cl);
		if (error != 0)
			goto out_release;
	}

	fragdl = SBT_MAX;
	maxfrag = (size_t)(sc->sc_host_cbd - 1) * 4;
	if (maxfrag > rounddown(MEI_HDR_MAX_LEN, 4))
		maxfrag = rounddown(MEI_HDR_MAX_LEN, 4);
	off = 0;
	while (off < len) {
		n = MIN(len - off, maxfrag);
		hdr = MEI_HDR_MAKE(cl->me_addr, cl->host_addr, n,
		    off + n == len);
		error = mei_send_locked(sc, hdr, p + off, n);
		if (error == 0) {
			off += n;
			fragdl = mei_deadline(MEI_HBM_TIMEOUT);
			continue;
		}
		if (error != EAGAIN)
			goto out_abort;
		/*
		 * Before the first fragment a signal can still cancel the
		 * write cleanly.  After it, only a link failure can.
		 */
		if (off == 0)
			error = mei_sleep(sc, &sc->sc_host_cbd,
			    PZERO | PCATCH, "meitx", SBT_MAX, MEI_POLL_FAST);
		else
			error = mei_sleep(sc, &sc->sc_host_cbd, PZERO,
			    "meitx", fragdl, MEI_POLL_FAST);
		if (error == 0)
			error = mei_cl_check_conn(cl);
		if (error != 0)
			goto out_abort;
	}
	mei_cl_tx_release(cl);
	error = 0;
	goto out;

out_abort:
	if (off > 0 && sc->sc_state == MEI_LINK_READY &&
	    cl->link_gen == sc->sc_link_gen) {
		/* Part of the message is out; protocol state is unknown. */
		device_printf(sc->sc_dev,
		    "client %u: write aborted mid-message (error %d)\n",
		    cl->host_addr, error);
		mei_link_reset_locked(sc);
	}
	/*
	 * A partly sent message cannot be resent: report the lost
	 * connection, never EINTR/ERESTART, so the syscall is not
	 * restarted with the same data.
	 */
	if (off > 0)
		error = ENODEV;
	/* FALLTHROUGH */
out_release:
	if (off == 0 && cl->tx_sending && mei_cl_check_conn(cl) == 0)
		(*mei_cl_credits(cl))++;
	mei_cl_tx_release(cl);
out:
	MEI_UNLOCK(sc);
	return (error);
}

int
mei_cl_read(struct mei_cl *cl, void *buf, size_t len, bool nonblock,
    size_t *done)
{
	struct mei_softc *sc;
	struct mei_rx_msg *m;
	size_t n;
	int error;

	sc = cl->sc;
	*done = 0;
	MEI_LOCK(sc);
	for (;;) {
		m = STAILQ_FIRST(&cl->rxq);
		if (m != NULL)
			break;
		error = mei_cl_check_conn(cl);
		if (error != 0)
			goto out;
		if (nonblock) {
			error = EAGAIN;
			goto out;
		}
		error = msleep(cl, &sc->sc_mtx, PZERO | PCATCH, "meird", 0);
		if (error != 0)
			goto out;
	}

	KASSERT(m->off < m->len, ("mei: consumed message on queue"));
	n = MIN(len, m->len - m->off);
	memcpy(buf, m->data + m->off, n);
	m->off += n;
	*done = n;
	error = 0;
	if (m->off >= m->len) {
		STAILQ_REMOVE_HEAD(&cl->rxq, link);
		cl->rxq_len--;
		free(m, M_MEI);
		if (mei_cl_check_conn(cl) == 0 && !cl->rx_credit_out &&
		    cl->rxq_len < MEI_RXQ_MAX)
			mei_cl_grant(sc, cl);
	}
out:
	MEI_UNLOCK(sc);
	return (error);
}

int
mei_cl_poll(struct mei_cl *cl, int events, struct thread *td)
{
	struct mei_softc *sc;
	int revents;

	sc = cl->sc;
	revents = 0;
	MEI_LOCK(sc);
	if ((events & (POLLIN | POLLRDNORM)) != 0 &&
	    !STAILQ_EMPTY(&cl->rxq))
		revents |= events & (POLLIN | POLLRDNORM);
	if ((events & (POLLOUT | POLLWRNORM)) != 0 &&
	    mei_cl_check_conn(cl) == 0 && *mei_cl_credits(cl) > 0 &&
	    !sc->sc_tx_busy)
		revents |= events & (POLLOUT | POLLWRNORM);
	if (cl->state == MEI_CL_DISCONNECTED ||
	    cl->state == MEI_CL_DISCONNECTING ||
	    sc->sc_state == MEI_LINK_DETACHING ||
	    (cl->state == MEI_CL_CONNECTED &&
	    cl->link_gen != sc->sc_link_gen))
		revents |= POLLHUP;
	if (revents == 0)
		selrecord(td, &cl->rsel);
	MEI_UNLOCK(sc);
	return (revents);
}

/*
 * Sysctl handlers.
 */

static int
mei_sysctl_fwsts(SYSCTL_HANDLER_ARGS)
{
	struct mei_softc *sc;
	uint32_t val;
	int idx;

	sc = arg1;
	idx = arg2;
	if (idx < 0 || idx >= MEI_FWSTS_COUNT)
		return (EINVAL);
	val = pci_read_config(sc->sc_dev, mei_fwsts_reg[idx], 4);
	MEI_LOCK(sc);
	sc->sc_fwsts[idx] = val;
	MEI_UNLOCK(sc);
	return (sysctl_handle_32(oidp, &val, 0, req));
}

static int
mei_sysctl_hbm_version(SYSCTL_HANDLER_ARGS)
{
	struct mei_softc *sc;
	char buf[8];

	sc = arg1;
	MEI_LOCK(sc);
	snprintf(buf, sizeof(buf), "%u.%u", sc->sc_hbm_major,
	    sc->sc_hbm_minor);
	MEI_UNLOCK(sc);
	return (sysctl_handle_string(oidp, buf, sizeof(buf), req));
}

static int
mei_sysctl_clients(SYSCTL_HANDLER_ARGS)
{
	struct mei_softc *sc;
	struct mei_me_client *tab, *mc;
	struct sbuf *sb;
	u_int addr;
	int error;

	sc = arg1;
	tab = malloc(sizeof(sc->sc_me_clients), M_MEI, M_WAITOK);
	MEI_LOCK(sc);
	memcpy(tab, sc->sc_me_clients, sizeof(sc->sc_me_clients));
	MEI_UNLOCK(sc);

	sb = sbuf_new_for_sysctl(NULL, NULL, 128, req);
	if (sb == NULL) {
		free(tab, M_MEI);
		return (ENOMEM);
	}
	for (addr = 0; addr < MEI_MAX_ME_CLIENTS; addr++) {
		mc = &tab[addr];
		if (!mc->valid)
			continue;
		sbuf_printf(sb, "%u %08x-%04x-%04x-%02x%02x-"
		    "%02x%02x%02x%02x%02x%02x %u %u %u\n", mc->addr,
		    le32dec(&mc->uuid[0]), le16dec(&mc->uuid[4]),
		    le16dec(&mc->uuid[6]), mc->uuid[8], mc->uuid[9],
		    mc->uuid[10], mc->uuid[11], mc->uuid[12], mc->uuid[13],
		    mc->uuid[14], mc->uuid[15], mc->max_msg_length,
		    mc->single_recv_buf, mc->fixed_address);
	}
	error = sbuf_finish(sb);
	sbuf_delete(sb);
	free(tab, M_MEI);
	return (error);
}

static int
mei_sysctl_state(SYSCTL_HANDLER_ARGS)
{
	struct mei_softc *sc;
	int val;

	sc = arg1;
	MEI_LOCK(sc);
	val = sc->sc_state;
	MEI_UNLOCK(sc);
	return (sysctl_handle_int(oidp, &val, 0, req));
}

static void
mei_sysctl_init(struct mei_softc *sc)
{
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid_list *list;
	char name[8];
	int i;

	ctx = &sc->sc_sysctl_ctx;
	sysctl_ctx_init(ctx);
	sc->sc_sysctl_init = true;
	list = SYSCTL_CHILDREN(device_get_sysctl_tree(sc->sc_dev));
	for (i = 0; i < MEI_FWSTS_COUNT; i++) {
		snprintf(name, sizeof(name), "fwsts%d", i + 1);
		SYSCTL_ADD_PROC(ctx, list, OID_AUTO, name,
		    CTLTYPE_U32 | CTLFLAG_RD | CTLFLAG_MPSAFE, sc, i,
		    mei_sysctl_fwsts, "IU", "Firmware status register");
	}
	SYSCTL_ADD_PROC(ctx, list, OID_AUTO, "hbm_version",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, sc, 0,
	    mei_sysctl_hbm_version, "A", "Negotiated HBM protocol version");
	SYSCTL_ADD_PROC(ctx, list, OID_AUTO, "clients",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, sc, 0,
	    mei_sysctl_clients, "A",
	    "ME clients: address uuid max_msg_length single_recv_buf "
	    "fixed_address");
	SYSCTL_ADD_PROC(ctx, list, OID_AUTO, "state",
	    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, sc, 0,
	    mei_sysctl_state, "I",
	    "Link state (0 init, 1 ready, 2 disabled, 3 detaching)");
}

/*
 * Device methods.
 */

static int
mei_probe(device_t dev)
{
	uint16_t devid;
	size_t i;

	if (pci_get_vendor(dev) != 0x8086)
		return (ENXIO);
	devid = pci_get_device(dev);
	for (i = 0; i < nitems(mei_idents); i++) {
		if (mei_idents[i].devid == devid) {
			device_set_desc(dev, mei_idents[i].desc);
			return (BUS_PROBE_DEFAULT);
		}
	}
	return (ENXIO);
}

/* Release bus resources; used by detach and failed attach. */
static void
mei_release(struct mei_softc *sc)
{
	device_t dev;

	dev = sc->sc_dev;
	if (sc->sc_mem != NULL) {
		MEI_LOCK(sc);
		mei_hcsr_update(sc, 0, MEI_CSR_IE);
		MEI_UNLOCK(sc);
	}
	if (sc->sc_intrhand != NULL) {
		bus_teardown_intr(dev, sc->sc_irq, sc->sc_intrhand);
		sc->sc_intrhand = NULL;
	}
	if (sc->sc_irq != NULL) {
		bus_release_resource(dev, SYS_RES_IRQ, sc->sc_irq_rid,
		    sc->sc_irq);
		sc->sc_irq = NULL;
	}
	if (sc->sc_msi) {
		pci_release_msi(dev);
		sc->sc_msi = false;
	}
	if (sc->sc_mem != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, sc->sc_mem_rid,
		    sc->sc_mem);
		sc->sc_mem = NULL;
	}
	if (sc->sc_tq != NULL) {
		taskqueue_free(sc->sc_tq);
		sc->sc_tq = NULL;
	}
	mtx_destroy(&sc->sc_mtx);
}

static int
mei_attach(device_t dev)
{
	struct mei_softc *sc;
	uint32_t fw1;
	int count, error, i;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;
	mtx_init(&sc->sc_mtx, device_get_nameunit(dev), "mei", MTX_DEF);
	TASK_INIT(&sc->sc_reset_task, 0, mei_reset_task, sc);
	sc->sc_tq = taskqueue_create("mei", M_WAITOK,
	    taskqueue_thread_enqueue, &sc->sc_tq);
	taskqueue_start_threads(&sc->sc_tq, 1, PWAIT, "%s taskq",
	    device_get_nameunit(dev));
	TAILQ_INIT(&sc->sc_clients);
	sc->sc_state = MEI_LINK_INIT;

	pci_enable_busmaster(dev);

	sc->sc_mem_rid = PCIR_BAR(0);
	sc->sc_mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &sc->sc_mem_rid, RF_ACTIVE);
	if (sc->sc_mem == NULL) {
		device_printf(dev, "cannot map registers\n");
		error = ENXIO;
		goto fail;
	}
	if (rman_get_size(sc->sc_mem) < MEI_REG_SIZE) {
		device_printf(dev, "register window too small\n");
		error = ENXIO;
		goto fail;
	}

	for (i = 0; i < MEI_FWSTS_COUNT; i++)
		sc->sc_fwsts[i] = pci_read_config(dev, mei_fwsts_reg[i], 4);
	fw1 = sc->sc_fwsts[0];
	device_printf(dev, "FWSTS1 0x%08x (state %u%s, opmode %u, "
	    "error %u)\n", fw1, MEI_FWSTS1_CWS(fw1),
	    MEI_FWSTS1_CWS(fw1) == MEI_FWSTS1_CWS_NORMAL ? " normal" : "",
	    MEI_FWSTS1_OPMODE(fw1), MEI_FWSTS1_ERRCODE(fw1));

	/* Keep the device quiet until the reset task runs. */
	MEI_LOCK(sc);
	mei_hcsr_update(sc, 0, MEI_CSR_IE);
	MEI_UNLOCK(sc);

	sc->sc_irq_rid = 0;
	if (pci_msi_count(dev) >= 1) {
		count = 1;
		if (pci_alloc_msi(dev, &count) == 0) {
			if (count == 1) {
				sc->sc_msi = true;
				sc->sc_irq_rid = 1;
			} else
				pci_release_msi(dev);
		}
	}
	sc->sc_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &sc->sc_irq_rid,
	    RF_ACTIVE | (sc->sc_msi ? 0 : RF_SHAREABLE));
	if (sc->sc_irq == NULL) {
		device_printf(dev, "cannot allocate interrupt\n");
		error = ENXIO;
		goto fail;
	}
	error = bus_setup_intr(dev, sc->sc_irq, INTR_TYPE_MISC | INTR_MPSAFE,
	    NULL, mei_intr, sc, &sc->sc_intrhand);
	if (error != 0) {
		device_printf(dev, "cannot set up interrupt (%d)\n", error);
		sc->sc_intrhand = NULL;
		goto fail;
	}

	error = mei_cdev_create(sc);
	if (error != 0) {
		device_printf(dev, "cannot create device node (%d)\n", error);
		goto fail;
	}
	mei_sysctl_init(sc);

	MEI_LOCK(sc);
	sc->sc_state = MEI_LINK_INIT;
	sc->sc_reset_pending = true;
	taskqueue_enqueue(sc->sc_tq, &sc->sc_reset_task);
	MEI_UNLOCK(sc);
	return (0);

fail:
	mei_release(sc);
	return (error);
}

static int
mei_detach(device_t dev)
{
	struct mei_softc *sc;
	struct mei_cl *cl;

	sc = device_get_softc(dev);
	/* No sysctl handler may run once the mutex is gone. */
	if (sc->sc_sysctl_init) {
		sysctl_ctx_free(&sc->sc_sysctl_ctx);
		sc->sc_sysctl_init = false;
	}
	MEI_LOCK(sc);
	sc->sc_state = MEI_LINK_DETACHING;
	sc->sc_reset_pending = false;
	wakeup(&sc->sc_state);
	wakeup(&sc->sc_host_cbd);
	wakeup(&sc->sc_me_cbd);
	wakeup(&sc->sc_hbm_answered);
	wakeup(&sc->sc_reset_count);
	TAILQ_FOREACH(cl, &sc->sc_clients, link)
		mei_cl_wake(cl);
	MEI_UNLOCK(sc);

	mei_cdev_destroy(sc);
	taskqueue_drain(sc->sc_tq, &sc->sc_reset_task);

	/*
	 * destroy_dev() does not wait for a cdevpriv destructor already
	 * started by close(2); wait for every client to be freed.
	 */
	MEI_LOCK(sc);
	while (sc->sc_nopen != 0)
		msleep(&sc->sc_nopen, &sc->sc_mtx, PZERO, "meidtch", hz / 10);
	MEI_UNLOCK(sc);
	mei_release(sc);
	return (0);
}

static int
mei_suspend(device_t dev)
{
	struct mei_softc *sc;

	sc = device_get_softc(dev);
	MEI_LOCK(sc);
	sc->sc_suspended = true;
	mei_hcsr_update(sc, 0, MEI_CSR_IE);
	if (sc->sc_state != MEI_LINK_DETACHING &&
	    sc->sc_state != MEI_LINK_DISABLED)
		mei_reset_bookkeeping(sc);
	MEI_UNLOCK(sc);
	taskqueue_drain(sc->sc_tq, &sc->sc_reset_task);
	return (0);
}

static int
mei_resume(device_t dev)
{
	struct mei_softc *sc;

	sc = device_get_softc(dev);
	MEI_LOCK(sc);
	sc->sc_suspended = false;
	if (sc->sc_state != MEI_LINK_DETACHING) {
		/* A resume gives a disabled link a fresh set of attempts. */
		if (sc->sc_state == MEI_LINK_DISABLED)
			mei_reset_bookkeeping(sc);
		sc->sc_reset_count = 0;
		sc->sc_reset_pending = true;
		if (!sc->sc_task_running)
			taskqueue_enqueue(sc->sc_tq, &sc->sc_reset_task);
	}
	MEI_UNLOCK(sc);
	return (0);
}

static device_method_t mei_methods[] = {
	DEVMETHOD(device_probe,		mei_probe),
	DEVMETHOD(device_attach,	mei_attach),
	DEVMETHOD(device_detach,	mei_detach),
	DEVMETHOD(device_suspend,	mei_suspend),
	DEVMETHOD(device_resume,	mei_resume),

	DEVMETHOD_END
};

static driver_t mei_driver = {
	"mei",
	mei_methods,
	sizeof(struct mei_softc)
};

DRIVER_MODULE(mei, pci, mei_driver, mei_devclass, NULL, NULL);
MODULE_VERSION(mei, 1);
MODULE_DEPEND(mei, pci, 1, 1, 1);
