/*-
 * Copyright (c) 2012 Chelsio Communications, Inc.
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
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"

#ifdef TCP_OFFLOAD
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/rwlock.h>
#include <sys/socket.h>
#include <sys/sbuf.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/ethernet.h>
#include <net/if_vlan_var.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/toecore.h>

#include "common/common.h"
#include "common/jhash.h"
#include "common/t4_msg.h"
#include "tom/t4_tom_l2t.h"
#include "tom/t4_tom.h"

#define VLAN_NONE	0xfff

#define SA(x)           ((struct sockaddr *)(x))
#define SIN(x)          ((struct sockaddr_in *)(x))
#define SINADDR(x)      (SIN(x)->sin_addr.s_addr)

static inline void
l2t_hold(struct l2t_data *d, struct l2t_entry *e)
{
	if (atomic_fetchadd_int(&e->refcnt, 1) == 0)  /* 0 -> 1 transition */
		atomic_subtract_int(&d->nfree, 1);
}

static inline unsigned int
arp_hash(const uint32_t key, int ifindex)
{
	return jhash_2words(key, ifindex, 0) & (L2T_SIZE - 1);
}

/*
 * Add a WR to an L2T entry's queue of work requests awaiting resolution.
 * Must be called with the entry's lock held.
 */
static inline void
arpq_enqueue(struct l2t_entry *e, struct wrqe *wr)
{
	mtx_assert(&e->lock, MA_OWNED);

	STAILQ_INSERT_TAIL(&e->wr_list, wr, link);
}

static inline void
send_pending(struct adapter *sc, struct l2t_entry *e)
{
	struct wrqe *wr;

	mtx_assert(&e->lock, MA_OWNED);

	while ((wr = STAILQ_FIRST(&e->wr_list)) != NULL) {
		STAILQ_REMOVE_HEAD(&e->wr_list, link);
		t4_wrq_tx(sc, wr);
	}
}

static void
resolution_failed_for_wr(struct wrqe *wr)
{
	log(LOG_ERR, "%s: leaked work request %p, wr_len %d", __func__, wr,
	    wr->wr_len);

	/* free(wr, M_CXGBE); */
}

static void
resolution_failed(struct l2t_entry *e)
{
	struct wrqe *wr;

	mtx_assert(&e->lock, MA_OWNED);

	while ((wr = STAILQ_FIRST(&e->wr_list)) != NULL) {
		STAILQ_REMOVE_HEAD(&e->wr_list, link);
		resolution_failed_for_wr(wr);
	}
}

static void
update_entry(struct adapter *sc, struct l2t_entry *e, uint8_t *lladdr,
    uint16_t vtag)
{

	mtx_assert(&e->lock, MA_OWNED);

	/*
	 * The entry may be in active use (e->refcount > 0) or not.  We update
	 * it even when it's not as this simplifies the case where we decide to
	 * reuse the entry later.
	 */

	if (lladdr == NULL &&
	    (e->state == L2T_STATE_RESOLVING || e->state == L2T_STATE_FAILED)) {
		/*
		 * Never got a valid L2 address for this one.  Just mark it as
		 * failed instead of removing it from the hash (for which we'd
		 * need to wlock the table).
		 */
		e->state = L2T_STATE_FAILED;
		resolution_failed(e);
		return;

	} else if (lladdr == NULL) {

		/* Valid or already-stale entry was deleted (or expired) */

		KASSERT(e->state == L2T_STATE_VALID ||
		    e->state == L2T_STATE_STALE,
		    ("%s: lladdr NULL, state %d", __func__, e->state));

		e->state = L2T_STATE_STALE;

	} else {

		if (e->state == L2T_STATE_RESOLVING ||
		    e->state == L2T_STATE_FAILED ||
		    memcmp(e->dmac, lladdr, ETHER_ADDR_LEN)) {

			/* unresolved -> resolved; or dmac changed */

			memcpy(e->dmac, lladdr, ETHER_ADDR_LEN);
			e->vlan = vtag;
			t4_write_l2e(sc, e, 1);
		}
		e->state = L2T_STATE_VALID;
	}
}

static int
resolve_entry(struct adapter *sc, struct l2t_entry *e)
{
	struct tom_data *td = sc->tom_softc;
	struct toedev *tod = &td->tod;
	struct sockaddr_in sin = {0};
	uint8_t dmac[ETHER_ADDR_LEN];
	uint16_t vtag = VLAN_NONE;
	int rc;

	sin.sin_family = AF_INET;
	sin.sin_len = sizeof(struct sockaddr_in);
	SINADDR(&sin) = e->addr;

	rc = toe_l2_resolve(tod, e->ifp, SA(&sin), dmac, &vtag);
	if (rc == EWOULDBLOCK)
		return (rc);

	mtx_lock(&e->lock);
	update_entry(sc, e, rc == 0 ? dmac : NULL, vtag);
	mtx_unlock(&e->lock);

	return (rc);
}

int
t4_l2t_send_slow(struct adapter *sc, struct wrqe *wr, struct l2t_entry *e)
{

again:
	switch (e->state) {
	case L2T_STATE_STALE:     /* entry is stale, kick off revalidation */

		if (resolve_entry(sc, e) != EWOULDBLOCK)
			goto again;	/* entry updated, re-examine state */

		/* Fall through */

	case L2T_STATE_VALID:     /* fast-path, send the packet on */

		t4_wrq_tx(sc, wr);
		return (0);

	case L2T_STATE_RESOLVING:
	case L2T_STATE_SYNC_WRITE:

		mtx_lock(&e->lock);
		if (e->state != L2T_STATE_SYNC_WRITE &&
		    e->state != L2T_STATE_RESOLVING) {
			/* state changed by the time we got here */
			mtx_unlock(&e->lock);
			goto again;
		}
		arpq_enqueue(e, wr);
		mtx_unlock(&e->lock);

		if (resolve_entry(sc, e) == EWOULDBLOCK)
			break;

		mtx_lock(&e->lock);
		if (e->state == L2T_STATE_VALID && !STAILQ_EMPTY(&e->wr_list))
			send_pending(sc, e);
		if (e->state == L2T_STATE_FAILED)
			resolution_failed(e);
		mtx_unlock(&e->lock);
		break;

	case L2T_STATE_FAILED:
		resolution_failed_for_wr(wr);
		return (EHOSTUNREACH);
	}

	return (0);
}

/*
 * Called when an L2T entry has no more users.  The entry is left in the hash
 * table since it is likely to be reused but we also bump nfree to indicate
 * that the entry can be reallocated for a different neighbor.  We also drop
 * the existing neighbor reference in case the neighbor is going away and is
 * waiting on our reference.
 *
 * Because entries can be reallocated to other neighbors once their ref count
 * drops to 0 we need to take the entry's lock to avoid races with a new
 * incarnation.
 */

static int
do_l2t_write_rpl2(struct sge_iq *iq, const struct rss_header *rss,
    struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	const struct cpl_l2t_write_rpl *rpl = (const void *)(rss + 1);
	unsigned int tid = GET_TID(rpl);
	unsigned int idx = tid & (L2T_SIZE - 1);
	int rc;

	rc = do_l2t_write_rpl(iq, rss, m);
	if (rc != 0)
		return (rc);

	if (tid & F_SYNC_WR) {
		struct l2t_entry *e = &sc->l2t->l2tab[idx];

		mtx_lock(&e->lock);
		if (e->state != L2T_STATE_SWITCHING) {
			send_pending(sc, e);
			e->state = L2T_STATE_VALID;
		}
		mtx_unlock(&e->lock);
	}

	return (0);
}

void
t4_init_l2t_cpl_handlers(struct adapter *sc)
{

	t4_register_cpl_handler(sc, CPL_L2T_WRITE_RPL, do_l2t_write_rpl2);
}

void
t4_uninit_l2t_cpl_handlers(struct adapter *sc)
{

	t4_register_cpl_handler(sc, CPL_L2T_WRITE_RPL, do_l2t_write_rpl);
}

/*
 * The TOE wants an L2 table entry that it can use to reach the next hop over
 * the specified port.  Produce such an entry - create one if needed.
 *
 * Note that the ifnet could be a pseudo-device like if_vlan, if_lagg, etc. on
 * top of the real cxgbe interface.
 */
struct l2t_entry *
t4_l2t_get(struct port_info *pi, struct ifnet *ifp, struct sockaddr *sa)
{
	struct l2t_entry *e;
	struct l2t_data *d = pi->adapter->l2t;
	uint32_t addr = SINADDR(sa);
	int hash = arp_hash(addr, ifp->if_index);
	unsigned int smt_idx = pi->port_id;

	if (sa->sa_family != AF_INET)
		return (NULL);	/* XXX: no IPv6 support right now */

#ifndef VLAN_TAG
	if (ifp->if_type == IFT_L2VLAN)
		return (NULL);
#endif

	rw_wlock(&d->lock);
	for (e = d->l2tab[hash].first; e; e = e->next) {
		if (e->addr == addr && e->ifp == ifp && e->smt_idx == smt_idx) {
			l2t_hold(d, e);
			goto done;
		}
	}

	/* Need to allocate a new entry */
	e = t4_alloc_l2e(d);
	if (e) {
		mtx_lock(&e->lock);          /* avoid race with t4_l2t_free */
		e->next = d->l2tab[hash].first;
		d->l2tab[hash].first = e;

		e->state = L2T_STATE_RESOLVING;
		e->addr = addr;
		e->ifp = ifp;
		e->smt_idx = smt_idx;
		e->hash = hash;
		e->lport = pi->lport;
		atomic_store_rel_int(&e->refcnt, 1);
#ifdef VLAN_TAG
		if (ifp->if_type == IFT_L2VLAN)
			VLAN_TAG(ifp, &e->vlan);
		else
			e->vlan = VLAN_NONE;
#endif
		mtx_unlock(&e->lock);
	}
done:
	rw_wunlock(&d->lock);
	return e;
}

/*
 * Called when the host's ARP layer makes a change to some entry that is loaded
 * into the HW L2 table.
 */
void
t4_l2_update(struct toedev *tod, struct ifnet *ifp, struct sockaddr *sa,
    uint8_t *lladdr, uint16_t vtag)
{
	struct adapter *sc = tod->tod_softc;
	struct l2t_entry *e;
	struct l2t_data *d = sc->l2t;
	uint32_t addr = SINADDR(sa);
	int hash = arp_hash(addr, ifp->if_index);

	KASSERT(d != NULL, ("%s: no L2 table", __func__));

	rw_rlock(&d->lock);
	for (e = d->l2tab[hash].first; e; e = e->next) {
		if (e->addr == addr && e->ifp == ifp) {
			mtx_lock(&e->lock);
			if (atomic_load_acq_int(&e->refcnt))
				goto found;
			e->state = L2T_STATE_STALE;
			mtx_unlock(&e->lock);
			break;
		}
	}
	rw_runlock(&d->lock);

	/*
	 * This is of no interest to us.  We've never had an offloaded
	 * connection to this destination, and we aren't attempting one right
	 * now.
	 */
	return;

found:
	rw_runlock(&d->lock);

	KASSERT(e->state != L2T_STATE_UNUSED,
	    ("%s: unused entry in the hash.", __func__));

	update_entry(sc, e, lladdr, vtag);
	mtx_unlock(&e->lock);
}
#endif
