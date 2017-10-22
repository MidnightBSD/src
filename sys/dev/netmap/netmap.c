/*
 * Copyright (C) 2011-2013 Matteo Landi, Luigi Rizzo. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
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

#define NM_BRIDGE

/*
 * This module supports memory mapped access to network devices,
 * see netmap(4).
 *
 * The module uses a large, memory pool allocated by the kernel
 * and accessible as mmapped memory by multiple userspace threads/processes.
 * The memory pool contains packet buffers and "netmap rings",
 * i.e. user-accessible copies of the interface's queues.
 *
 * Access to the network card works like this:
 * 1. a process/thread issues one or more open() on /dev/netmap, to create
 *    select()able file descriptor on which events are reported.
 * 2. on each descriptor, the process issues an ioctl() to identify
 *    the interface that should report events to the file descriptor.
 * 3. on each descriptor, the process issues an mmap() request to
 *    map the shared memory region within the process' address space.
 *    The list of interesting queues is indicated by a location in
 *    the shared memory region.
 * 4. using the functions in the netmap(4) userspace API, a process
 *    can look up the occupation state of a queue, access memory buffers,
 *    and retrieve received packets or enqueue packets to transmit.
 * 5. using some ioctl()s the process can synchronize the userspace view
 *    of the queue with the actual status in the kernel. This includes both
 *    receiving the notification of new packets, and transmitting new
 *    packets on the output interface.
 * 6. select() or poll() can be used to wait for events on individual
 *    transmit or receive queues (or all queues for a given interface).
 */

#ifdef linux
#include "bsd_glue.h"
static netdev_tx_t linux_netmap_start(struct sk_buff *skb, struct net_device *dev);
#endif /* linux */

#ifdef __APPLE__
#include "osx_glue.h"
#endif /* __APPLE__ */

#ifdef __FreeBSD__
#include <sys/cdefs.h> /* prerequisite */
__FBSDID("$FreeBSD: release/10.0.0/sys/dev/netmap/netmap.c 251425 2013-06-05 17:27:59Z luigi $");

#include <sys/types.h>
#include <sys/module.h>
#include <sys/errno.h>
#include <sys/param.h>	/* defines used in kernel.h */
#include <sys/jail.h>
#include <sys/kernel.h>	/* types used in module initialization */
#include <sys/conf.h>	/* cdevsw struct */
#include <sys/uio.h>	/* uio struct */
#include <sys/sockio.h>
#include <sys/socketvar.h>	/* struct socket */
#include <sys/malloc.h>
#include <sys/mman.h>	/* PROT_EXEC */
#include <sys/poll.h>
#include <sys/proc.h>
#include <sys/rwlock.h>
#include <vm/vm.h>	/* vtophys */
#include <vm/pmap.h>	/* vtophys */
#include <sys/socket.h> /* sockaddrs */
#include <machine/bus.h>
#include <sys/selinfo.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <net/bpf.h>		/* BIOCIMMEDIATE */
#include <net/vnet.h>
#include <machine/bus.h>	/* bus_dmamap_* */

MALLOC_DEFINE(M_NETMAP, "netmap", "Network memory map");
#endif /* __FreeBSD__ */

#include <net/netmap.h>
#include <dev/netmap/netmap_kern.h>

/* XXX the following variables must be deprecated and included in nm_mem */
u_int netmap_total_buffers;
u_int netmap_buf_size;
char *netmap_buffer_base;	/* address of an invalid buffer */

/* user-controlled variables */
int netmap_verbose;

static int netmap_no_timestamp; /* don't timestamp on rxsync */

SYSCTL_NODE(_dev, OID_AUTO, netmap, CTLFLAG_RW, 0, "Netmap args");
SYSCTL_INT(_dev_netmap, OID_AUTO, verbose,
    CTLFLAG_RW, &netmap_verbose, 0, "Verbose mode");
SYSCTL_INT(_dev_netmap, OID_AUTO, no_timestamp,
    CTLFLAG_RW, &netmap_no_timestamp, 0, "no_timestamp");
int netmap_mitigate = 1;
SYSCTL_INT(_dev_netmap, OID_AUTO, mitigate, CTLFLAG_RW, &netmap_mitigate, 0, "");
int netmap_no_pendintr = 1;
SYSCTL_INT(_dev_netmap, OID_AUTO, no_pendintr,
    CTLFLAG_RW, &netmap_no_pendintr, 0, "Always look for new received packets.");
int netmap_txsync_retry = 2;
SYSCTL_INT(_dev_netmap, OID_AUTO, txsync_retry, CTLFLAG_RW,
    &netmap_txsync_retry, 0 , "Number of txsync loops in bridge's flush.");

int netmap_drop = 0;	/* debugging */
int netmap_flags = 0;	/* debug flags */
int netmap_fwd = 0;	/* force transparent mode */

SYSCTL_INT(_dev_netmap, OID_AUTO, drop, CTLFLAG_RW, &netmap_drop, 0 , "");
SYSCTL_INT(_dev_netmap, OID_AUTO, flags, CTLFLAG_RW, &netmap_flags, 0 , "");
SYSCTL_INT(_dev_netmap, OID_AUTO, fwd, CTLFLAG_RW, &netmap_fwd, 0 , "");

#ifdef NM_BRIDGE /* support for netmap virtual switch, called VALE */

/*
 * system parameters (most of them in netmap_kern.h)
 * NM_NAME	prefix for switch port names, default "vale"
 * NM_MAXPORTS	number of ports
 * NM_BRIDGES	max number of switches in the system.
 *	XXX should become a sysctl or tunable
 *
 * Switch ports are named valeX:Y where X is the switch name and Y
 * is the port. If Y matches a physical interface name, the port is
 * connected to a physical device.
 *
 * Unlike physical interfaces, switch ports use their own memory region
 * for rings and buffers.
 * The virtual interfaces use per-queue lock instead of core lock.
 * In the tx loop, we aggregate traffic in batches to make all operations
 * faster. The batch size is NM_BDG_BATCH
 */
#define NM_BDG_MAXRINGS		16	/* XXX unclear how many. */
#define NM_BRIDGE_RINGSIZE	1024	/* in the device */
#define NM_BDG_HASH		1024	/* forwarding table entries */
#define NM_BDG_BATCH		1024	/* entries in the forwarding buffer */
#define	NM_BRIDGES		8	/* number of bridges */


int netmap_bridge = NM_BDG_BATCH; /* bridge batch size */
SYSCTL_INT(_dev_netmap, OID_AUTO, bridge, CTLFLAG_RW, &netmap_bridge, 0 , "");

#ifdef linux

#define	refcount_acquire(_a)	atomic_add(1, (atomic_t *)_a)
#define	refcount_release(_a)	atomic_dec_and_test((atomic_t *)_a)

#else /* !linux */

#ifdef __FreeBSD__
#include <sys/endian.h>
#include <sys/refcount.h>
#endif /* __FreeBSD__ */

#define prefetch(x)	__builtin_prefetch(x)

#endif /* !linux */

/*
 * These are used to handle reference counters for bridge ports.
 */
#define	ADD_BDG_REF(ifp)	refcount_acquire(&NA(ifp)->na_bdg_refcount)
#define	DROP_BDG_REF(ifp)	refcount_release(&NA(ifp)->na_bdg_refcount)

static void bdg_netmap_attach(struct netmap_adapter *);
static int bdg_netmap_reg(struct ifnet *ifp, int onoff);
static int kern_netmap_regif(struct nmreq *nmr);

/* per-tx-queue entry */
struct nm_bdg_fwd {	/* forwarding entry for a bridge */
	void *ft_buf;
	uint16_t _ft_dst;	/* dst port, unused */
	uint16_t ft_flags;	/* flags, e.g. indirect */
	uint16_t ft_len;	/* src len */
	uint16_t ft_next;	/* next packet to same destination */
};

/* We need to build a list of buffers going to each destination.
 * Each buffer is in one entry of struct nm_bdg_fwd, we use ft_next
 * to build the list, and struct nm_bdg_q below for the queue.
 * The structure should compact because potentially we have a lot
 * of destinations.
 */
struct nm_bdg_q {
	uint16_t bq_head;
	uint16_t bq_tail;
};

struct nm_hash_ent {
	uint64_t	mac;	/* the top 2 bytes are the epoch */
	uint64_t	ports;
};

/*
 * Interfaces for a bridge are all in bdg_ports[].
 * The array has fixed size, an empty entry does not terminate
 * the search. But lookups only occur on attach/detach so we
 * don't mind if they are slow.
 *
 * The bridge is non blocking on the transmit ports.
 *
 * bdg_lock protects accesses to the bdg_ports array.
 * This is a rw lock (or equivalent).
 */
struct nm_bridge {
	int namelen;	/* 0 means free */

	/* XXX what is the proper alignment/layout ? */
	NM_RWLOCK_T bdg_lock;	/* protects bdg_ports */
	struct netmap_adapter *bdg_ports[NM_BDG_MAXPORTS];

	char basename[IFNAMSIZ];
	/*
	 * The function to decide the destination port.
	 * It returns either of an index of the destination port,
	 * NM_BDG_BROADCAST to broadcast this packet, or NM_BDG_NOPORT not to
	 * forward this packet.  ring_nr is the source ring index, and the
	 * function may overwrite this value to forward this packet to a
	 * different ring index.
	 * This function must be set by netmap_bdgctl().
	 */
	bdg_lookup_fn_t nm_bdg_lookup;

	/* the forwarding table, MAC+ports */
	struct nm_hash_ent ht[NM_BDG_HASH];
};

struct nm_bridge nm_bridges[NM_BRIDGES];
NM_LOCK_T	netmap_bridge_mutex;

/* other OS will have these macros defined in their own glue code. */

#ifdef __FreeBSD__
#define BDG_LOCK()		mtx_lock(&netmap_bridge_mutex)
#define BDG_UNLOCK()		mtx_unlock(&netmap_bridge_mutex)
#define BDG_WLOCK(b)		rw_wlock(&(b)->bdg_lock)
#define BDG_WUNLOCK(b)		rw_wunlock(&(b)->bdg_lock)
#define BDG_RLOCK(b)		rw_rlock(&(b)->bdg_lock)
#define BDG_RUNLOCK(b)		rw_runlock(&(b)->bdg_lock)

/* set/get variables. OS-specific macros may wrap these
 * assignments into read/write lock or similar
 */
#define BDG_SET_VAR(lval, p)	(lval = p)
#define BDG_GET_VAR(lval)	(lval)
#define BDG_FREE(p)		free(p, M_DEVBUF)
#endif /* __FreeBSD__ */

static __inline int
nma_is_vp(struct netmap_adapter *na)
{
	return na->nm_register == bdg_netmap_reg;
}
static __inline int
nma_is_host(struct netmap_adapter *na)
{
	return na->nm_register == NULL;
}
static __inline int
nma_is_hw(struct netmap_adapter *na)
{
	/* In case of sw adapter, nm_register is NULL */
	return !nma_is_vp(na) && !nma_is_host(na);
}

/*
 * Regarding holding a NIC, if the NIC is owned by the kernel
 * (i.e., bridge), neither another bridge nor user can use it;
 * if the NIC is owned by a user, only users can share it.
 * Evaluation must be done under NMA_LOCK().
 */
#define NETMAP_OWNED_BY_KERN(ifp)	(!nma_is_vp(NA(ifp)) && NA(ifp)->na_bdg)
#define NETMAP_OWNED_BY_ANY(ifp) \
	(NETMAP_OWNED_BY_KERN(ifp) || (NA(ifp)->refcount > 0))

/*
 * NA(ifp)->bdg_port	port index
 */

// XXX only for multiples of 64 bytes, non overlapped.
static inline void
pkt_copy(void *_src, void *_dst, int l)
{
        uint64_t *src = _src;
        uint64_t *dst = _dst;
        if (unlikely(l >= 1024)) {
                bcopy(src, dst, l);
                return;
        }
        for (; likely(l > 0); l-=64) {
                *dst++ = *src++;
                *dst++ = *src++;
                *dst++ = *src++;
                *dst++ = *src++;
                *dst++ = *src++;
                *dst++ = *src++;
                *dst++ = *src++;
                *dst++ = *src++;
        }
}


/*
 * locate a bridge among the existing ones.
 * a ':' in the name terminates the bridge name. Otherwise, just NM_NAME.
 * We assume that this is called with a name of at least NM_NAME chars.
 */
static struct nm_bridge *
nm_find_bridge(const char *name, int create)
{
	int i, l, namelen;
	struct nm_bridge *b = NULL;

	namelen = strlen(NM_NAME);	/* base length */
	l = strlen(name);		/* actual length */
	for (i = namelen + 1; i < l; i++) {
		if (name[i] == ':') {
			namelen = i;
			break;
		}
	}
	if (namelen >= IFNAMSIZ)
		namelen = IFNAMSIZ;
	ND("--- prefix is '%.*s' ---", namelen, name);

	BDG_LOCK();
	/* lookup the name, remember empty slot if there is one */
	for (i = 0; i < NM_BRIDGES; i++) {
		struct nm_bridge *x = nm_bridges + i;

		if (x->namelen == 0) {
			if (create && b == NULL)
				b = x;	/* record empty slot */
		} else if (x->namelen != namelen) {
			continue;
		} else if (strncmp(name, x->basename, namelen) == 0) {
			ND("found '%.*s' at %d", namelen, name, i);
			b = x;
			break;
		}
	}
	if (i == NM_BRIDGES && b) { /* name not found, can create entry */
		strncpy(b->basename, name, namelen);
		b->namelen = namelen;
		/* set the default function */
		b->nm_bdg_lookup = netmap_bdg_learning;
		/* reset the MAC address table */
		bzero(b->ht, sizeof(struct nm_hash_ent) * NM_BDG_HASH);
	}
	BDG_UNLOCK();
	return b;
}


/*
 * Free the forwarding tables for rings attached to switch ports.
 */
static void
nm_free_bdgfwd(struct netmap_adapter *na)
{
	int nrings, i;
	struct netmap_kring *kring;

	nrings = nma_is_vp(na) ? na->num_tx_rings : na->num_rx_rings;
	kring = nma_is_vp(na) ? na->tx_rings : na->rx_rings;
	for (i = 0; i < nrings; i++) {
		if (kring[i].nkr_ft) {
			free(kring[i].nkr_ft, M_DEVBUF);
			kring[i].nkr_ft = NULL; /* protect from freeing twice */
		}
	}
	if (nma_is_hw(na))
		nm_free_bdgfwd(SWNA(na->ifp));
}


/*
 * Allocate the forwarding tables for the rings attached to the bridge ports.
 */
static int
nm_alloc_bdgfwd(struct netmap_adapter *na)
{
	int nrings, l, i, num_dstq;
	struct netmap_kring *kring;

	/* all port:rings + broadcast */
	num_dstq = NM_BDG_MAXPORTS * NM_BDG_MAXRINGS + 1;
	l = sizeof(struct nm_bdg_fwd) * NM_BDG_BATCH;
	l += sizeof(struct nm_bdg_q) * num_dstq;
	l += sizeof(uint16_t) * NM_BDG_BATCH;

	nrings = nma_is_vp(na) ? na->num_tx_rings : na->num_rx_rings;
	kring = nma_is_vp(na) ? na->tx_rings : na->rx_rings;
	for (i = 0; i < nrings; i++) {
		struct nm_bdg_fwd *ft;
		struct nm_bdg_q *dstq;
		int j;

		ft = malloc(l, M_DEVBUF, M_NOWAIT | M_ZERO);
		if (!ft) {
			nm_free_bdgfwd(na);
			return ENOMEM;
		}
		dstq = (struct nm_bdg_q *)(ft + NM_BDG_BATCH);
		for (j = 0; j < num_dstq; j++)
			dstq[j].bq_head = dstq[j].bq_tail = NM_BDG_BATCH;
		kring[i].nkr_ft = ft;
	}
	if (nma_is_hw(na))
		nm_alloc_bdgfwd(SWNA(na->ifp));
	return 0;
}

#endif /* NM_BRIDGE */


/*
 * Fetch configuration from the device, to cope with dynamic
 * reconfigurations after loading the module.
 */
static int
netmap_update_config(struct netmap_adapter *na)
{
	struct ifnet *ifp = na->ifp;
	u_int txr, txd, rxr, rxd;

	txr = txd = rxr = rxd = 0;
	if (na->nm_config) {
		na->nm_config(ifp, &txr, &txd, &rxr, &rxd);
	} else {
		/* take whatever we had at init time */
		txr = na->num_tx_rings;
		txd = na->num_tx_desc;
		rxr = na->num_rx_rings;
		rxd = na->num_rx_desc;
	}

	if (na->num_tx_rings == txr && na->num_tx_desc == txd &&
	    na->num_rx_rings == rxr && na->num_rx_desc == rxd)
		return 0; /* nothing changed */
	if (netmap_verbose || na->refcount > 0) {
		D("stored config %s: txring %d x %d, rxring %d x %d",
			ifp->if_xname,
			na->num_tx_rings, na->num_tx_desc,
			na->num_rx_rings, na->num_rx_desc);
		D("new config %s: txring %d x %d, rxring %d x %d",
			ifp->if_xname, txr, txd, rxr, rxd);
	}
	if (na->refcount == 0) {
		D("configuration changed (but fine)");
		na->num_tx_rings = txr;
		na->num_tx_desc = txd;
		na->num_rx_rings = rxr;
		na->num_rx_desc = rxd;
		return 0;
	}
	D("configuration changed while active, this is bad...");
	return 1;
}

/*------------- memory allocator -----------------*/
#include "netmap_mem2.c"
/*------------ end of memory allocator ----------*/


/* Structure associated to each thread which registered an interface.
 *
 * The first 4 fields of this structure are written by NIOCREGIF and
 * read by poll() and NIOC?XSYNC.
 * There is low contention among writers (actually, a correct user program
 * should have no contention among writers) and among writers and readers,
 * so we use a single global lock to protect the structure initialization.
 * Since initialization involves the allocation of memory, we reuse the memory
 * allocator lock.
 * Read access to the structure is lock free. Readers must check that
 * np_nifp is not NULL before using the other fields.
 * If np_nifp is NULL initialization has not been performed, so they should
 * return an error to userlevel.
 *
 * The ref_done field is used to regulate access to the refcount in the
 * memory allocator. The refcount must be incremented at most once for
 * each open("/dev/netmap"). The increment is performed by the first
 * function that calls netmap_get_memory() (currently called by
 * mmap(), NIOCGINFO and NIOCREGIF).
 * If the refcount is incremented, it is then decremented when the
 * private structure is destroyed.
 */
struct netmap_priv_d {
	struct netmap_if * volatile np_nifp;	/* netmap interface descriptor. */

	struct ifnet	*np_ifp;	/* device for which we hold a reference */
	int		np_ringid;	/* from the ioctl */
	u_int		np_qfirst, np_qlast;	/* range of rings to scan */
	uint16_t	np_txpoll;

	unsigned long	ref_done;	/* use with NMA_LOCK held */
};


static int
netmap_get_memory(struct netmap_priv_d* p)
{
	int error = 0;
	NMA_LOCK();
	if (!p->ref_done) {
		error = netmap_memory_finalize();
		if (!error)
			p->ref_done = 1;
	}
	NMA_UNLOCK();
	return error;
}

/*
 * File descriptor's private data destructor.
 *
 * Call nm_register(ifp,0) to stop netmap mode on the interface and
 * revert to normal operation. We expect that np_ifp has not gone.
 */
/* call with NMA_LOCK held */
static void
netmap_dtor_locked(void *data)
{
	struct netmap_priv_d *priv = data;
	struct ifnet *ifp = priv->np_ifp;
	struct netmap_adapter *na = NA(ifp);
	struct netmap_if *nifp = priv->np_nifp;

	na->refcount--;
	if (na->refcount <= 0) {	/* last instance */
		u_int i, j, lim;

		if (netmap_verbose)
			D("deleting last instance for %s", ifp->if_xname);
		/*
		 * (TO CHECK) This function is only called
		 * when the last reference to this file descriptor goes
		 * away. This means we cannot have any pending poll()
		 * or interrupt routine operating on the structure.
		 */
		na->nm_register(ifp, 0); /* off, clear IFCAP_NETMAP */
		/* Wake up any sleeping threads. netmap_poll will
		 * then return POLLERR
		 */
		for (i = 0; i < na->num_tx_rings + 1; i++)
			selwakeuppri(&na->tx_rings[i].si, PI_NET);
		for (i = 0; i < na->num_rx_rings + 1; i++)
			selwakeuppri(&na->rx_rings[i].si, PI_NET);
		selwakeuppri(&na->tx_si, PI_NET);
		selwakeuppri(&na->rx_si, PI_NET);
#ifdef NM_BRIDGE
		nm_free_bdgfwd(na);
#endif /* NM_BRIDGE */
		/* release all buffers */
		for (i = 0; i < na->num_tx_rings + 1; i++) {
			struct netmap_ring *ring = na->tx_rings[i].ring;
			lim = na->tx_rings[i].nkr_num_slots;
			for (j = 0; j < lim; j++)
				netmap_free_buf(nifp, ring->slot[j].buf_idx);
			/* knlist_destroy(&na->tx_rings[i].si.si_note); */
			mtx_destroy(&na->tx_rings[i].q_lock);
		}
		for (i = 0; i < na->num_rx_rings + 1; i++) {
			struct netmap_ring *ring = na->rx_rings[i].ring;
			lim = na->rx_rings[i].nkr_num_slots;
			for (j = 0; j < lim; j++)
				netmap_free_buf(nifp, ring->slot[j].buf_idx);
			/* knlist_destroy(&na->rx_rings[i].si.si_note); */
			mtx_destroy(&na->rx_rings[i].q_lock);
		}
		/* XXX kqueue(9) needed; these will mirror knlist_init. */
		/* knlist_destroy(&na->tx_si.si_note); */
		/* knlist_destroy(&na->rx_si.si_note); */
		netmap_free_rings(na);
		if (nma_is_hw(na))
			SWNA(ifp)->tx_rings = SWNA(ifp)->rx_rings = NULL;
	}
	netmap_if_free(nifp);
}


/* we assume netmap adapter exists */
static void
nm_if_rele(struct ifnet *ifp)
{
#ifndef NM_BRIDGE
	if_rele(ifp);
#else /* NM_BRIDGE */
	int i, full = 0, is_hw;
	struct nm_bridge *b;
	struct netmap_adapter *na;

	/* I can be called not only for get_ifp()-ed references where netmap's
	 * capability is guaranteed, but also for non-netmap-capable NICs.
	 */
	if (!NETMAP_CAPABLE(ifp) || !NA(ifp)->na_bdg) {
		if_rele(ifp);
		return;
	}
	if (!DROP_BDG_REF(ifp))
		return;

	na = NA(ifp);
	b = na->na_bdg;
	is_hw = nma_is_hw(na);

	BDG_WLOCK(b);
	ND("want to disconnect %s from the bridge", ifp->if_xname);
	full = 0;
	/* remove the entry from the bridge, also check
	 * if there are any leftover interfaces
	 * XXX we should optimize this code, e.g. going directly
	 * to na->bdg_port, and having a counter of ports that
	 * are connected. But it is not in a critical path.
	 * In NIC's case, index of sw na is always higher than hw na
	 */
	for (i = 0; i < NM_BDG_MAXPORTS; i++) {
		struct netmap_adapter *tmp = BDG_GET_VAR(b->bdg_ports[i]);

		if (tmp == na) {
			/* disconnect from bridge */
			BDG_SET_VAR(b->bdg_ports[i], NULL);
			na->na_bdg = NULL;
			if (is_hw && SWNA(ifp)->na_bdg) {
				/* disconnect sw adapter too */
				int j = SWNA(ifp)->bdg_port;
				BDG_SET_VAR(b->bdg_ports[j], NULL);
				SWNA(ifp)->na_bdg = NULL;
			}
		} else if (tmp != NULL) {
			full = 1;
		}
	}
	BDG_WUNLOCK(b);
	if (full == 0) {
		ND("marking bridge %d as free", b - nm_bridges);
		b->namelen = 0;
		b->nm_bdg_lookup = NULL;
	}
	if (na->na_bdg) { /* still attached to the bridge */
		D("ouch, cannot find ifp to remove");
	} else if (is_hw) {
		if_rele(ifp);
	} else {
		bzero(na, sizeof(*na));
		free(na, M_DEVBUF);
		bzero(ifp, sizeof(*ifp));
		free(ifp, M_DEVBUF);
	}
#endif /* NM_BRIDGE */
}

static void
netmap_dtor(void *data)
{
	struct netmap_priv_d *priv = data;
	struct ifnet *ifp = priv->np_ifp;

	NMA_LOCK();
	if (ifp) {
		struct netmap_adapter *na = NA(ifp);

		if (na->na_bdg)
			BDG_WLOCK(na->na_bdg);
		na->nm_lock(ifp, NETMAP_REG_LOCK, 0);
		netmap_dtor_locked(data);
		na->nm_lock(ifp, NETMAP_REG_UNLOCK, 0);
		if (na->na_bdg)
			BDG_WUNLOCK(na->na_bdg);

		nm_if_rele(ifp); /* might also destroy *na */
	}
	if (priv->ref_done) {
		netmap_memory_deref();
	}
	NMA_UNLOCK();
	bzero(priv, sizeof(*priv));	/* XXX for safety */
	free(priv, M_DEVBUF);
}


#ifdef __FreeBSD__
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/uma.h>

/*
 * In order to track whether pages are still mapped, we hook into
 * the standard cdev_pager and intercept the constructor and
 * destructor.
 * XXX but then ? Do we really use the information ?
 * Need to investigate.
 */
static struct cdev_pager_ops saved_cdev_pager_ops;


static int
netmap_dev_pager_ctor(void *handle, vm_ooffset_t size, vm_prot_t prot,
    vm_ooffset_t foff, struct ucred *cred, u_short *color)
{
	if (netmap_verbose)
		D("first mmap for %p", handle);
	return saved_cdev_pager_ops.cdev_pg_ctor(handle,
			size, prot, foff, cred, color);
}


static void
netmap_dev_pager_dtor(void *handle)
{
	saved_cdev_pager_ops.cdev_pg_dtor(handle);
	ND("ready to release memory for %p", handle);
}


static struct cdev_pager_ops netmap_cdev_pager_ops = {
        .cdev_pg_ctor = netmap_dev_pager_ctor,
        .cdev_pg_dtor = netmap_dev_pager_dtor,
        .cdev_pg_fault = NULL,
};


// XXX check whether we need netmap_mmap_single _and_ netmap_mmap
static int
netmap_mmap_single(struct cdev *cdev, vm_ooffset_t *foff,
	vm_size_t objsize,  vm_object_t *objp, int prot)
{
	vm_object_t obj;

	ND("cdev %p foff %jd size %jd objp %p prot %d", cdev,
	    (intmax_t )*foff, (intmax_t )objsize, objp, prot);
	obj = vm_pager_allocate(OBJT_DEVICE, cdev, objsize, prot, *foff,
            curthread->td_ucred);
	ND("returns obj %p", obj);
	if (obj == NULL)
		return EINVAL;
	if (saved_cdev_pager_ops.cdev_pg_fault == NULL) {
		ND("initialize cdev_pager_ops");
		saved_cdev_pager_ops = *(obj->un_pager.devp.ops);
		netmap_cdev_pager_ops.cdev_pg_fault =
			saved_cdev_pager_ops.cdev_pg_fault;
	};
	obj->un_pager.devp.ops = &netmap_cdev_pager_ops;
	*objp = obj;
	return 0;
}
#endif /* __FreeBSD__ */


/*
 * mmap(2) support for the "netmap" device.
 *
 * Expose all the memory previously allocated by our custom memory
 * allocator: this way the user has only to issue a single mmap(2), and
 * can work on all the data structures flawlessly.
 *
 * Return 0 on success, -1 otherwise.
 */

#ifdef __FreeBSD__
static int
netmap_mmap(__unused struct cdev *dev,
#if __FreeBSD_version < 900000
		vm_offset_t offset, vm_paddr_t *paddr, int nprot
#else
		vm_ooffset_t offset, vm_paddr_t *paddr, int nprot,
		__unused vm_memattr_t *memattr
#endif
	)
{
	int error = 0;
	struct netmap_priv_d *priv;

	if (nprot & PROT_EXEC)
		return (-1);	// XXX -1 or EINVAL ?

	error = devfs_get_cdevpriv((void **)&priv);
	if (error == EBADF) {	/* called on fault, memory is initialized */
		ND(5, "handling fault at ofs 0x%x", offset);
		error = 0;
	} else if (error == 0)	/* make sure memory is set */
		error = netmap_get_memory(priv);
	if (error)
		return (error);

	ND("request for offset 0x%x", (uint32_t)offset);
	*paddr = netmap_ofstophys(offset);

	return (*paddr ? 0 : ENOMEM);
}


static int
netmap_close(struct cdev *dev, int fflag, int devtype, struct thread *td)
{
	if (netmap_verbose)
		D("dev %p fflag 0x%x devtype %d td %p",
			dev, fflag, devtype, td);
	return 0;
}


static int
netmap_open(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	struct netmap_priv_d *priv;
	int error;

	priv = malloc(sizeof(struct netmap_priv_d), M_DEVBUF,
			      M_NOWAIT | M_ZERO);
	if (priv == NULL)
		return ENOMEM;

	error = devfs_set_cdevpriv(priv, netmap_dtor);
	if (error)
	        return error;

	return 0;
}
#endif /* __FreeBSD__ */


/*
 * Handlers for synchronization of the queues from/to the host.
 * Netmap has two operating modes:
 * - in the default mode, the rings connected to the host stack are
 *   just another ring pair managed by userspace;
 * - in transparent mode (XXX to be defined) incoming packets
 *   (from the host or the NIC) are marked as NS_FORWARD upon
 *   arrival, and the user application has a chance to reset the
 *   flag for packets that should be dropped.
 *   On the RXSYNC or poll(), packets in RX rings between
 *   kring->nr_kcur and ring->cur with NS_FORWARD still set are moved
 *   to the other side.
 * The transfer NIC --> host is relatively easy, just encapsulate
 * into mbufs and we are done. The host --> NIC side is slightly
 * harder because there might not be room in the tx ring so it
 * might take a while before releasing the buffer.
 */


/*
 * pass a chain of buffers to the host stack as coming from 'dst'
 */
static void
netmap_send_up(struct ifnet *dst, struct mbuf *head)
{
	struct mbuf *m;

	/* send packets up, outside the lock */
	while ((m = head) != NULL) {
		head = head->m_nextpkt;
		m->m_nextpkt = NULL;
		if (netmap_verbose & NM_VERB_HOST)
			D("sending up pkt %p size %d", m, MBUF_LEN(m));
		NM_SEND_UP(dst, m);
	}
}

struct mbq {
	struct mbuf *head;
	struct mbuf *tail;
	int count;
};


/*
 * put a copy of the buffers marked NS_FORWARD into an mbuf chain.
 * Run from hwcur to cur - reserved
 */
static void
netmap_grab_packets(struct netmap_kring *kring, struct mbq *q, int force)
{
	/* Take packets from hwcur to cur-reserved and pass them up.
	 * In case of no buffers we give up. At the end of the loop,
	 * the queue is drained in all cases.
	 * XXX handle reserved
	 */
	int k = kring->ring->cur - kring->ring->reserved;
	u_int n, lim = kring->nkr_num_slots - 1;
	struct mbuf *m, *tail = q->tail;

	if (k < 0)
		k = k + kring->nkr_num_slots;
	for (n = kring->nr_hwcur; n != k;) {
		struct netmap_slot *slot = &kring->ring->slot[n];

		n = (n == lim) ? 0 : n + 1;
		if ((slot->flags & NS_FORWARD) == 0 && !force)
			continue;
		if (slot->len < 14 || slot->len > NETMAP_BUF_SIZE) {
			D("bad pkt at %d len %d", n, slot->len);
			continue;
		}
		slot->flags &= ~NS_FORWARD; // XXX needed ?
		m = m_devget(NMB(slot), slot->len, 0, kring->na->ifp, NULL);

		if (m == NULL)
			break;
		if (tail)
			tail->m_nextpkt = m;
		else
			q->head = m;
		tail = m;
		q->count++;
		m->m_nextpkt = NULL;
	}
	q->tail = tail;
}


/*
 * called under main lock to send packets from the host to the NIC
 * The host ring has packets from nr_hwcur to (cur - reserved)
 * to be sent down. We scan the tx rings, which have just been
 * flushed so nr_hwcur == cur. Pushing packets down means
 * increment cur and decrement avail.
 * XXX to be verified
 */
static void
netmap_sw_to_nic(struct netmap_adapter *na)
{
	struct netmap_kring *kring = &na->rx_rings[na->num_rx_rings];
	struct netmap_kring *k1 = &na->tx_rings[0];
	int i, howmany, src_lim, dst_lim;

	howmany = kring->nr_hwavail;	/* XXX otherwise cur - reserved - nr_hwcur */

	src_lim = kring->nkr_num_slots;
	for (i = 0; howmany > 0 && i < na->num_tx_rings; i++, k1++) {
		ND("%d packets left to ring %d (space %d)", howmany, i, k1->nr_hwavail);
		dst_lim = k1->nkr_num_slots;
		while (howmany > 0 && k1->ring->avail > 0) {
			struct netmap_slot *src, *dst, tmp;
			src = &kring->ring->slot[kring->nr_hwcur];
			dst = &k1->ring->slot[k1->ring->cur];
			tmp = *src;
			src->buf_idx = dst->buf_idx;
			src->flags = NS_BUF_CHANGED;

			dst->buf_idx = tmp.buf_idx;
			dst->len = tmp.len;
			dst->flags = NS_BUF_CHANGED;
			ND("out len %d buf %d from %d to %d",
				dst->len, dst->buf_idx,
				kring->nr_hwcur, k1->ring->cur);

			if (++kring->nr_hwcur >= src_lim)
				kring->nr_hwcur = 0;
			howmany--;
			kring->nr_hwavail--;
			if (++k1->ring->cur >= dst_lim)
				k1->ring->cur = 0;
			k1->ring->avail--;
		}
		kring->ring->cur = kring->nr_hwcur; // XXX
		k1++;
	}
}


/*
 * netmap_sync_to_host() passes packets up. We are called from a
 * system call in user process context, and the only contention
 * can be among multiple user threads erroneously calling
 * this routine concurrently.
 */
static void
netmap_sync_to_host(struct netmap_adapter *na)
{
	struct netmap_kring *kring = &na->tx_rings[na->num_tx_rings];
	struct netmap_ring *ring = kring->ring;
	u_int k, lim = kring->nkr_num_slots - 1;
	struct mbq q = { NULL, NULL };

	k = ring->cur;
	if (k > lim) {
		netmap_ring_reinit(kring);
		return;
	}
	// na->nm_lock(na->ifp, NETMAP_CORE_LOCK, 0);

	/* Take packets from hwcur to cur and pass them up.
	 * In case of no buffers we give up. At the end of the loop,
	 * the queue is drained in all cases.
	 */
	netmap_grab_packets(kring, &q, 1);
	kring->nr_hwcur = k;
	kring->nr_hwavail = ring->avail = lim;
	// na->nm_lock(na->ifp, NETMAP_CORE_UNLOCK, 0);

	netmap_send_up(na->ifp, q.head);
}


/* SWNA(ifp)->txrings[0] is always NA(ifp)->txrings[NA(ifp)->num_txrings] */
static int
netmap_bdg_to_host(struct ifnet *ifp, u_int ring_nr, int do_lock)
{
	(void)ring_nr;
	(void)do_lock;
	netmap_sync_to_host(NA(ifp));
	return 0;
}


/*
 * rxsync backend for packets coming from the host stack.
 * They have been put in the queue by netmap_start() so we
 * need to protect access to the kring using a lock.
 *
 * This routine also does the selrecord if called from the poll handler
 * (we know because td != NULL).
 *
 * NOTE: on linux, selrecord() is defined as a macro and uses pwait
 *     as an additional hidden argument.
 */
static void
netmap_sync_from_host(struct netmap_adapter *na, struct thread *td, void *pwait)
{
	struct netmap_kring *kring = &na->rx_rings[na->num_rx_rings];
	struct netmap_ring *ring = kring->ring;
	u_int j, n, lim = kring->nkr_num_slots;
	u_int k = ring->cur, resvd = ring->reserved;

	(void)pwait;	/* disable unused warnings */
	na->nm_lock(na->ifp, NETMAP_CORE_LOCK, 0);
	if (k >= lim) {
		netmap_ring_reinit(kring);
		return;
	}
	/* new packets are already set in nr_hwavail */
	/* skip past packets that userspace has released */
	j = kring->nr_hwcur;
	if (resvd > 0) {
		if (resvd + ring->avail >= lim + 1) {
			D("XXX invalid reserve/avail %d %d", resvd, ring->avail);
			ring->reserved = resvd = 0; // XXX panic...
		}
		k = (k >= resvd) ? k - resvd : k + lim - resvd;
        }
	if (j != k) {
		n = k >= j ? k - j : k + lim - j;
		kring->nr_hwavail -= n;
		kring->nr_hwcur = k;
	}
	k = ring->avail = kring->nr_hwavail - resvd;
	if (k == 0 && td)
		selrecord(td, &kring->si);
	if (k && (netmap_verbose & NM_VERB_HOST))
		D("%d pkts from stack", k);
	na->nm_lock(na->ifp, NETMAP_CORE_UNLOCK, 0);
}


/*
 * get a refcounted reference to an interface.
 * Return ENXIO if the interface does not exist, EINVAL if netmap
 * is not supported by the interface.
 * If successful, hold a reference.
 *
 * During the NIC is attached to a bridge, reference is managed
 * at na->na_bdg_refcount using ADD/DROP_BDG_REF() as well as
 * virtual ports.  Hence, on the final DROP_BDG_REF(), the NIC
 * is detached from the bridge, then ifp's refcount is dropped (this
 * is equivalent to that ifp is destroyed in case of virtual ports.
 *
 * This function uses if_rele() when we want to prevent the NIC from
 * being detached from the bridge in error handling.  But once refcount
 * is acquired by this function, it must be released using nm_if_rele().
 */
static int
get_ifp(struct nmreq *nmr, struct ifnet **ifp)
{
	const char *name = nmr->nr_name;
	int namelen = strlen(name);
#ifdef NM_BRIDGE
	struct ifnet *iter = NULL;
	int no_prefix = 0;

	do {
		struct nm_bridge *b;
		struct netmap_adapter *na;
		int i, cand = -1, cand2 = -1;

		if (strncmp(name, NM_NAME, sizeof(NM_NAME) - 1)) {
			no_prefix = 1;
			break;
		}
		b = nm_find_bridge(name, 1 /* create a new one if no exist */ );
		if (b == NULL) {
			D("no bridges available for '%s'", name);
			return (ENXIO);
		}
		/* Now we are sure that name starts with the bridge's name */
		BDG_WLOCK(b);
		/* lookup in the local list of ports */
		for (i = 0; i < NM_BDG_MAXPORTS; i++) {
			na = BDG_GET_VAR(b->bdg_ports[i]);
			if (na == NULL) {
				if (cand == -1)
					cand = i; /* potential insert point */
				else if (cand2 == -1)
					cand2 = i; /* for host stack */
				continue;
			}
			iter = na->ifp;
			/* XXX make sure the name only contains one : */
			if (!strcmp(iter->if_xname, name) /* virtual port */ ||
			    (namelen > b->namelen && !strcmp(iter->if_xname,
			    name + b->namelen + 1)) /* NIC */) {
				ADD_BDG_REF(iter);
				ND("found existing interface");
				BDG_WUNLOCK(b);
				break;
			}
		}
		if (i < NM_BDG_MAXPORTS) /* already unlocked */
			break;
		if (cand == -1) {
			D("bridge full, cannot create new port");
no_port:
			BDG_WUNLOCK(b);
			*ifp = NULL;
			return EINVAL;
		}
		ND("create new bridge port %s", name);
		/*
		 * create a struct ifnet for the new port.
		 * The forwarding table is attached to the kring(s).
		 */
		/*
		 * try see if there is a matching NIC with this name
		 * (after the bridge's name)
		 */
		iter = ifunit_ref(name + b->namelen + 1);
		if (!iter) { /* this is a virtual port */
			/* Create a temporary NA with arguments, then
			 * bdg_netmap_attach() will allocate the real one
			 * and attach it to the ifp
			 */
			struct netmap_adapter tmp_na;

			if (nmr->nr_cmd) /* nr_cmd must be for a NIC */
				goto no_port;
			bzero(&tmp_na, sizeof(tmp_na));
			/* bound checking */
			if (nmr->nr_tx_rings < 1)
				nmr->nr_tx_rings = 1;
			if (nmr->nr_tx_rings > NM_BDG_MAXRINGS)
				nmr->nr_tx_rings = NM_BDG_MAXRINGS;
			tmp_na.num_tx_rings = nmr->nr_tx_rings;
			if (nmr->nr_rx_rings < 1)
				nmr->nr_rx_rings = 1;
			if (nmr->nr_rx_rings > NM_BDG_MAXRINGS)
				nmr->nr_rx_rings = NM_BDG_MAXRINGS;
			tmp_na.num_rx_rings = nmr->nr_rx_rings;

			iter = malloc(sizeof(*iter), M_DEVBUF, M_NOWAIT | M_ZERO);
			if (!iter)
				goto no_port;
			strcpy(iter->if_xname, name);
			tmp_na.ifp = iter;
			/* bdg_netmap_attach creates a struct netmap_adapter */
			bdg_netmap_attach(&tmp_na);
		} else if (NETMAP_CAPABLE(iter)) { /* this is a NIC */
			/* cannot attach the NIC that any user or another
			 * bridge already holds.
			 */
			if (NETMAP_OWNED_BY_ANY(iter) || cand2 == -1) {
ifunit_rele:
				if_rele(iter); /* don't detach from bridge */
				goto no_port;
			}
			/* bind the host stack to the bridge */
			if (nmr->nr_arg1 == NETMAP_BDG_HOST) {
				BDG_SET_VAR(b->bdg_ports[cand2], SWNA(iter));
				SWNA(iter)->bdg_port = cand2;
				SWNA(iter)->na_bdg = b;
			}
		} else /* not a netmap-capable NIC */
			goto ifunit_rele;
		na = NA(iter);
		na->bdg_port = cand;
		/* bind the port to the bridge (virtual ports are not active) */
		BDG_SET_VAR(b->bdg_ports[cand], na);
		na->na_bdg = b;
		ADD_BDG_REF(iter);
		BDG_WUNLOCK(b);
		ND("attaching virtual bridge %p", b);
	} while (0);
	*ifp = iter;
	if (! *ifp)
#endif /* NM_BRIDGE */
	*ifp = ifunit_ref(name);
	if (*ifp == NULL)
		return (ENXIO);
	/* can do this if the capability exists and if_pspare[0]
	 * points to the netmap descriptor.
	 */
	if (NETMAP_CAPABLE(*ifp)) {
#ifdef NM_BRIDGE
		/* Users cannot use the NIC attached to a bridge directly */
		if (no_prefix && NETMAP_OWNED_BY_KERN(*ifp)) {
			if_rele(*ifp); /* don't detach from bridge */
			return EINVAL;
		} else
#endif /* NM_BRIDGE */
		return 0;	/* valid pointer, we hold the refcount */
	}
	nm_if_rele(*ifp);
	return EINVAL;	// not NETMAP capable
}


/*
 * Error routine called when txsync/rxsync detects an error.
 * Can't do much more than resetting cur = hwcur, avail = hwavail.
 * Return 1 on reinit.
 *
 * This routine is only called by the upper half of the kernel.
 * It only reads hwcur (which is changed only by the upper half, too)
 * and hwavail (which may be changed by the lower half, but only on
 * a tx ring and only to increase it, so any error will be recovered
 * on the next call). For the above, we don't strictly need to call
 * it under lock.
 */
int
netmap_ring_reinit(struct netmap_kring *kring)
{
	struct netmap_ring *ring = kring->ring;
	u_int i, lim = kring->nkr_num_slots - 1;
	int errors = 0;

	RD(10, "called for %s", kring->na->ifp->if_xname);
	if (ring->cur > lim)
		errors++;
	for (i = 0; i <= lim; i++) {
		u_int idx = ring->slot[i].buf_idx;
		u_int len = ring->slot[i].len;
		if (idx < 2 || idx >= netmap_total_buffers) {
			if (!errors++)
				D("bad buffer at slot %d idx %d len %d ", i, idx, len);
			ring->slot[i].buf_idx = 0;
			ring->slot[i].len = 0;
		} else if (len > NETMAP_BUF_SIZE) {
			ring->slot[i].len = 0;
			if (!errors++)
				D("bad len %d at slot %d idx %d",
					len, i, idx);
		}
	}
	if (errors) {
		int pos = kring - kring->na->tx_rings;
		int n = kring->na->num_tx_rings + 1;

		RD(10, "total %d errors", errors);
		errors++;
		RD(10, "%s %s[%d] reinit, cur %d -> %d avail %d -> %d",
			kring->na->ifp->if_xname,
			pos < n ?  "TX" : "RX", pos < n ? pos : pos - n,
			ring->cur, kring->nr_hwcur,
			ring->avail, kring->nr_hwavail);
		ring->cur = kring->nr_hwcur;
		ring->avail = kring->nr_hwavail;
	}
	return (errors ? 1 : 0);
}


/*
 * Set the ring ID. For devices with a single queue, a request
 * for all rings is the same as a single ring.
 */
static int
netmap_set_ringid(struct netmap_priv_d *priv, u_int ringid)
{
	struct ifnet *ifp = priv->np_ifp;
	struct netmap_adapter *na = NA(ifp);
	u_int i = ringid & NETMAP_RING_MASK;
	/* initially (np_qfirst == np_qlast) we don't want to lock */
	int need_lock = (priv->np_qfirst != priv->np_qlast);
	int lim = na->num_rx_rings;

	if (na->num_tx_rings > lim)
		lim = na->num_tx_rings;
	if ( (ringid & NETMAP_HW_RING) && i >= lim) {
		D("invalid ring id %d", i);
		return (EINVAL);
	}
	if (need_lock)
		na->nm_lock(ifp, NETMAP_CORE_LOCK, 0);
	priv->np_ringid = ringid;
	if (ringid & NETMAP_SW_RING) {
		priv->np_qfirst = NETMAP_SW_RING;
		priv->np_qlast = 0;
	} else if (ringid & NETMAP_HW_RING) {
		priv->np_qfirst = i;
		priv->np_qlast = i + 1;
	} else {
		priv->np_qfirst = 0;
		priv->np_qlast = NETMAP_HW_RING ;
	}
	priv->np_txpoll = (ringid & NETMAP_NO_TX_POLL) ? 0 : 1;
	if (need_lock)
		na->nm_lock(ifp, NETMAP_CORE_UNLOCK, 0);
    if (netmap_verbose) {
	if (ringid & NETMAP_SW_RING)
		D("ringid %s set to SW RING", ifp->if_xname);
	else if (ringid & NETMAP_HW_RING)
		D("ringid %s set to HW RING %d", ifp->if_xname,
			priv->np_qfirst);
	else
		D("ringid %s set to all %d HW RINGS", ifp->if_xname, lim);
    }
	return 0;
}


/*
 * possibly move the interface to netmap-mode.
 * If success it returns a pointer to netmap_if, otherwise NULL.
 * This must be called with NMA_LOCK held.
 */
static struct netmap_if *
netmap_do_regif(struct netmap_priv_d *priv, struct ifnet *ifp,
	uint16_t ringid, int *err)
{
	struct netmap_adapter *na = NA(ifp);
	struct netmap_if *nifp = NULL;
	int i, error;

	if (na->na_bdg)
		BDG_WLOCK(na->na_bdg);
	na->nm_lock(ifp, NETMAP_REG_LOCK, 0);

	/* ring configuration may have changed, fetch from the card */
	netmap_update_config(na);
	priv->np_ifp = ifp;     /* store the reference */
	error = netmap_set_ringid(priv, ringid);
	if (error)
		goto out;
	nifp = netmap_if_new(ifp->if_xname, na);
	if (nifp == NULL) { /* allocation failed */
		error = ENOMEM;
	} else if (ifp->if_capenable & IFCAP_NETMAP) {
		/* was already set */
	} else {
		/* Otherwise set the card in netmap mode
		 * and make it use the shared buffers.
		 */
		for (i = 0 ; i < na->num_tx_rings + 1; i++)
			mtx_init(&na->tx_rings[i].q_lock, "nm_txq_lock",
			    MTX_NETWORK_LOCK, MTX_DEF);
		for (i = 0 ; i < na->num_rx_rings + 1; i++) {
			mtx_init(&na->rx_rings[i].q_lock, "nm_rxq_lock",
			    MTX_NETWORK_LOCK, MTX_DEF);
		}
		if (nma_is_hw(na)) {
			SWNA(ifp)->tx_rings = &na->tx_rings[na->num_tx_rings];
			SWNA(ifp)->rx_rings = &na->rx_rings[na->num_rx_rings];
		}
		error = na->nm_register(ifp, 1); /* mode on */
#ifdef NM_BRIDGE
		if (!error)
			error = nm_alloc_bdgfwd(na);
#endif /* NM_BRIDGE */
		if (error) {
			netmap_dtor_locked(priv);
			/* nifp is not yet in priv, so free it separately */
			netmap_if_free(nifp);
			nifp = NULL;
		}

	}
out:
	*err = error;
	na->nm_lock(ifp, NETMAP_REG_UNLOCK, 0);
	if (na->na_bdg)
		BDG_WUNLOCK(na->na_bdg);
	return nifp;
}


/* Process NETMAP_BDG_ATTACH and NETMAP_BDG_DETACH */
static int
kern_netmap_regif(struct nmreq *nmr)
{
	struct ifnet *ifp;
	struct netmap_if *nifp;
	struct netmap_priv_d *npriv;
	int error;

	npriv = malloc(sizeof(*npriv), M_DEVBUF, M_NOWAIT|M_ZERO);
	if (npriv == NULL)
		return ENOMEM;
	error = netmap_get_memory(npriv);
	if (error) {
free_exit:
		bzero(npriv, sizeof(*npriv));
		free(npriv, M_DEVBUF);
		return error;
	}

	NMA_LOCK();
	error = get_ifp(nmr, &ifp);
	if (error) { /* no device, or another bridge or user owns the device */
		NMA_UNLOCK();
		goto free_exit;
	} else if (!NETMAP_OWNED_BY_KERN(ifp)) {
		/* got reference to a virtual port or direct access to a NIC.
		 * perhaps specified no bridge's prefix or wrong NIC's name
		 */
		error = EINVAL;
unref_exit:
		nm_if_rele(ifp);
		NMA_UNLOCK();
		goto free_exit;
	}

	if (nmr->nr_cmd == NETMAP_BDG_DETACH) {
		if (NA(ifp)->refcount == 0) { /* not registered */
			error = EINVAL;
			goto unref_exit;
		}
		NMA_UNLOCK();

		netmap_dtor(NA(ifp)->na_kpriv); /* unregister */
		NA(ifp)->na_kpriv = NULL;
		nm_if_rele(ifp); /* detach from the bridge */
		goto free_exit;
	} else if (NA(ifp)->refcount > 0) { /* already registered */
		error = EINVAL;
		goto unref_exit;
	}

	nifp = netmap_do_regif(npriv, ifp, nmr->nr_ringid, &error);
	if (!nifp)
		goto unref_exit;
	wmb(); // XXX do we need it ?
	npriv->np_nifp = nifp;
	NA(ifp)->na_kpriv = npriv;
	NMA_UNLOCK();
	D("registered %s to netmap-mode", ifp->if_xname);
	return 0;
}


/* CORE_LOCK is not necessary */
static void
netmap_swlock_wrapper(struct ifnet *dev, int what, u_int queueid)
{
	struct netmap_adapter *na = SWNA(dev);

	switch (what) {
	case NETMAP_TX_LOCK:
		mtx_lock(&na->tx_rings[queueid].q_lock);
		break;

	case NETMAP_TX_UNLOCK:
		mtx_unlock(&na->tx_rings[queueid].q_lock);
		break;

	case NETMAP_RX_LOCK:
		mtx_lock(&na->rx_rings[queueid].q_lock);
		break;

	case NETMAP_RX_UNLOCK:
		mtx_unlock(&na->rx_rings[queueid].q_lock);
		break;
	}
}


/* Initialize necessary fields of sw adapter located in right after hw's
 * one.  sw adapter attaches a pair of sw rings of the netmap-mode NIC.
 * It is always activated and deactivated at the same tie with the hw's one.
 * Thus we don't need refcounting on the sw adapter.
 * Regardless of NIC's feature we use separate lock so that anybody can lock
 * me independently from the hw adapter.
 * Make sure nm_register is NULL to be handled as FALSE in nma_is_hw
 */
static void
netmap_attach_sw(struct ifnet *ifp)
{
	struct netmap_adapter *hw_na = NA(ifp);
	struct netmap_adapter *na = SWNA(ifp);

	na->ifp = ifp;
	na->separate_locks = 1;
	na->nm_lock = netmap_swlock_wrapper;
	na->num_rx_rings = na->num_tx_rings = 1;
	na->num_tx_desc = hw_na->num_tx_desc;
	na->num_rx_desc = hw_na->num_rx_desc;
	na->nm_txsync = netmap_bdg_to_host;
}


/* exported to kernel callers */
int
netmap_bdg_ctl(struct nmreq *nmr, bdg_lookup_fn_t func)
{
	struct nm_bridge *b;
	struct netmap_adapter *na;
	struct ifnet *iter;
	char *name = nmr->nr_name;
	int cmd = nmr->nr_cmd, namelen = strlen(name);
	int error = 0, i, j;

	switch (cmd) {
	case NETMAP_BDG_ATTACH:
	case NETMAP_BDG_DETACH:
		error = kern_netmap_regif(nmr);
		break;

	case NETMAP_BDG_LIST:
		/* this is used to enumerate bridges and ports */
		if (namelen) { /* look up indexes of bridge and port */
			if (strncmp(name, NM_NAME, strlen(NM_NAME))) {
				error = EINVAL;
				break;
			}
			b = nm_find_bridge(name, 0 /* don't create */);
			if (!b) {
				error = ENOENT;
				break;
			}

			BDG_RLOCK(b);
			error = ENOENT;
			for (i = 0; i < NM_BDG_MAXPORTS; i++) {
				na = BDG_GET_VAR(b->bdg_ports[i]);
				if (na == NULL)
					continue;
				iter = na->ifp;
				/* the former and the latter identify a
				 * virtual port and a NIC, respectively
				 */
				if (!strcmp(iter->if_xname, name) ||
				    (namelen > b->namelen &&
				    !strcmp(iter->if_xname,
				    name + b->namelen + 1))) {
					/* bridge index */
					nmr->nr_arg1 = b - nm_bridges;
					nmr->nr_arg2 = i; /* port index */
					error = 0;
					break;
				}
			}
			BDG_RUNLOCK(b);
		} else {
			/* return the first non-empty entry starting from
			 * bridge nr_arg1 and port nr_arg2.
			 *
			 * Users can detect the end of the same bridge by
			 * seeing the new and old value of nr_arg1, and can
			 * detect the end of all the bridge by error != 0
			 */
			i = nmr->nr_arg1;
			j = nmr->nr_arg2;

			for (error = ENOENT; error && i < NM_BRIDGES; i++) {
				b = nm_bridges + i;
				BDG_RLOCK(b);
				for (; j < NM_BDG_MAXPORTS; j++) {
					na = BDG_GET_VAR(b->bdg_ports[j]);
					if (na == NULL)
						continue;
					iter = na->ifp;
					nmr->nr_arg1 = i;
					nmr->nr_arg2 = j;
					strncpy(name, iter->if_xname, IFNAMSIZ);
					error = 0;
					break;
				}
				BDG_RUNLOCK(b);
				j = 0; /* following bridges scan from 0 */
			}
		}
		break;

	case NETMAP_BDG_LOOKUP_REG:
		/* register a lookup function to the given bridge.
		 * nmr->nr_name may be just bridge's name (including ':'
		 * if it is not just NM_NAME).
		 */
		if (!func) {
			error = EINVAL;
			break;
		}
		b = nm_find_bridge(name, 0 /* don't create */);
		if (!b) {
			error = EINVAL;
			break;
		}
		BDG_WLOCK(b);
		b->nm_bdg_lookup = func;
		BDG_WUNLOCK(b);
		break;
	default:
		D("invalid cmd (nmr->nr_cmd) (0x%x)", cmd);
		error = EINVAL;
		break;
	}
	return error;
}


/*
 * ioctl(2) support for the "netmap" device.
 *
 * Following a list of accepted commands:
 * - NIOCGINFO
 * - SIOCGIFADDR	just for convenience
 * - NIOCREGIF
 * - NIOCUNREGIF
 * - NIOCTXSYNC
 * - NIOCRXSYNC
 *
 * Return 0 on success, errno otherwise.
 */
static int
netmap_ioctl(struct cdev *dev, u_long cmd, caddr_t data,
	int fflag, struct thread *td)
{
	struct netmap_priv_d *priv = NULL;
	struct ifnet *ifp;
	struct nmreq *nmr = (struct nmreq *) data;
	struct netmap_adapter *na;
	int error;
	u_int i, lim;
	struct netmap_if *nifp;

	(void)dev;	/* UNUSED */
	(void)fflag;	/* UNUSED */
#ifdef linux
#define devfs_get_cdevpriv(pp)				\
	({ *(struct netmap_priv_d **)pp = ((struct file *)td)->private_data; 	\
		(*pp ? 0 : ENOENT); })

/* devfs_set_cdevpriv cannot fail on linux */
#define devfs_set_cdevpriv(p, fn)				\
	({ ((struct file *)td)->private_data = p; (p ? 0 : EINVAL); })


#define devfs_clear_cdevpriv()	do {				\
		netmap_dtor(priv); ((struct file *)td)->private_data = 0;	\
	} while (0)
#endif /* linux */

	CURVNET_SET(TD_TO_VNET(td));

	error = devfs_get_cdevpriv((void **)&priv);
	if (error) {
		CURVNET_RESTORE();
		/* XXX ENOENT should be impossible, since the priv
		 * is now created in the open */
		return (error == ENOENT ? ENXIO : error);
	}

	nmr->nr_name[sizeof(nmr->nr_name) - 1] = '\0';	/* truncate name */
	switch (cmd) {
	case NIOCGINFO:		/* return capabilities etc */
		if (nmr->nr_version != NETMAP_API) {
			D("API mismatch got %d have %d",
				nmr->nr_version, NETMAP_API);
			nmr->nr_version = NETMAP_API;
			error = EINVAL;
			break;
		}
		if (nmr->nr_cmd == NETMAP_BDG_LIST) {
			error = netmap_bdg_ctl(nmr, NULL);
			break;
		}
		/* update configuration */
		error = netmap_get_memory(priv);
		ND("get_memory returned %d", error);
		if (error)
			break;
		/* memsize is always valid */
		nmr->nr_memsize = nm_mem.nm_totalsize;
		nmr->nr_offset = 0;
		nmr->nr_rx_slots = nmr->nr_tx_slots = 0;
		if (nmr->nr_name[0] == '\0')	/* just get memory info */
			break;
		/* lock because get_ifp and update_config see na->refcount */
		NMA_LOCK();
		error = get_ifp(nmr, &ifp); /* get a refcount */
		if (error) {
			NMA_UNLOCK();
			break;
		}
		na = NA(ifp); /* retrieve netmap_adapter */
		netmap_update_config(na);
		NMA_UNLOCK();
		nmr->nr_rx_rings = na->num_rx_rings;
		nmr->nr_tx_rings = na->num_tx_rings;
		nmr->nr_rx_slots = na->num_rx_desc;
		nmr->nr_tx_slots = na->num_tx_desc;
		nm_if_rele(ifp);	/* return the refcount */
		break;

	case NIOCREGIF:
		if (nmr->nr_version != NETMAP_API) {
			nmr->nr_version = NETMAP_API;
			error = EINVAL;
			break;
		}
		/* possibly attach/detach NIC and VALE switch */
		i = nmr->nr_cmd;
		if (i == NETMAP_BDG_ATTACH || i == NETMAP_BDG_DETACH) {
			error = netmap_bdg_ctl(nmr, NULL);
			break;
		} else if (i != 0) {
			D("nr_cmd must be 0 not %d", i);
			error = EINVAL;
			break;
		}

		/* ensure allocators are ready */
		error = netmap_get_memory(priv);
		ND("get_memory returned %d", error);
		if (error)
			break;

		/* protect access to priv from concurrent NIOCREGIF */
		NMA_LOCK();
		if (priv->np_ifp != NULL) {	/* thread already registered */
			error = netmap_set_ringid(priv, nmr->nr_ringid);
unlock_out:
			NMA_UNLOCK();
			break;
		}
		/* find the interface and a reference */
		error = get_ifp(nmr, &ifp); /* keep reference */
		if (error)
			goto unlock_out;
		else if (NETMAP_OWNED_BY_KERN(ifp)) {
			nm_if_rele(ifp);
			goto unlock_out;
		}
		nifp = netmap_do_regif(priv, ifp, nmr->nr_ringid, &error);
		if (!nifp) {    /* reg. failed, release priv and ref */
			nm_if_rele(ifp);        /* return the refcount */
			priv->np_ifp = NULL;
			priv->np_nifp = NULL;
			goto unlock_out;
		}

		/* the following assignment is a commitment.
		 * Readers (i.e., poll and *SYNC) check for
		 * np_nifp != NULL without locking
		 */
		wmb(); /* make sure previous writes are visible to all CPUs */
		priv->np_nifp = nifp;
		NMA_UNLOCK();

		/* return the offset of the netmap_if object */
		na = NA(ifp); /* retrieve netmap adapter */
		nmr->nr_rx_rings = na->num_rx_rings;
		nmr->nr_tx_rings = na->num_tx_rings;
		nmr->nr_rx_slots = na->num_rx_desc;
		nmr->nr_tx_slots = na->num_tx_desc;
		nmr->nr_memsize = nm_mem.nm_totalsize;
		nmr->nr_offset = netmap_if_offset(nifp);
		break;

	case NIOCUNREGIF:
		// XXX we have no data here ?
		D("deprecated, data is %p", nmr);
		error = EINVAL;
		break;

	case NIOCTXSYNC:
	case NIOCRXSYNC:
		nifp = priv->np_nifp;

		if (nifp == NULL) {
			error = ENXIO;
			break;
		}
		rmb(); /* make sure following reads are not from cache */


		ifp = priv->np_ifp;	/* we have a reference */

		if (ifp == NULL) {
			D("Internal error: nifp != NULL && ifp == NULL");
			error = ENXIO;
			break;
		}

		na = NA(ifp); /* retrieve netmap adapter */
		if (priv->np_qfirst == NETMAP_SW_RING) { /* host rings */
			if (cmd == NIOCTXSYNC)
				netmap_sync_to_host(na);
			else
				netmap_sync_from_host(na, NULL, NULL);
			break;
		}
		/* find the last ring to scan */
		lim = priv->np_qlast;
		if (lim == NETMAP_HW_RING)
			lim = (cmd == NIOCTXSYNC) ?
			    na->num_tx_rings : na->num_rx_rings;

		for (i = priv->np_qfirst; i < lim; i++) {
			if (cmd == NIOCTXSYNC) {
				struct netmap_kring *kring = &na->tx_rings[i];
				if (netmap_verbose & NM_VERB_TXSYNC)
					D("pre txsync ring %d cur %d hwcur %d",
					    i, kring->ring->cur,
					    kring->nr_hwcur);
				na->nm_txsync(ifp, i, 1 /* do lock */);
				if (netmap_verbose & NM_VERB_TXSYNC)
					D("post txsync ring %d cur %d hwcur %d",
					    i, kring->ring->cur,
					    kring->nr_hwcur);
			} else {
				na->nm_rxsync(ifp, i, 1 /* do lock */);
				microtime(&na->rx_rings[i].ring->ts);
			}
		}

		break;

#ifdef __FreeBSD__
	case BIOCIMMEDIATE:
	case BIOCGHDRCMPLT:
	case BIOCSHDRCMPLT:
	case BIOCSSEESENT:
		D("ignore BIOCIMMEDIATE/BIOCSHDRCMPLT/BIOCSHDRCMPLT/BIOCSSEESENT");
		break;

	default:	/* allow device-specific ioctls */
	    {
		struct socket so;
		bzero(&so, sizeof(so));
		error = get_ifp(nmr, &ifp); /* keep reference */
		if (error)
			break;
		so.so_vnet = ifp->if_vnet;
		// so->so_proto not null.
		error = ifioctl(&so, cmd, data, td);
		nm_if_rele(ifp);
		break;
	    }

#else /* linux */
	default:
		error = EOPNOTSUPP;
#endif /* linux */
	}

	CURVNET_RESTORE();
	return (error);
}


/*
 * select(2) and poll(2) handlers for the "netmap" device.
 *
 * Can be called for one or more queues.
 * Return true the event mask corresponding to ready events.
 * If there are no ready events, do a selrecord on either individual
 * selfd or on the global one.
 * Device-dependent parts (locking and sync of tx/rx rings)
 * are done through callbacks.
 *
 * On linux, arguments are really pwait, the poll table, and 'td' is struct file *
 * The first one is remapped to pwait as selrecord() uses the name as an
 * hidden argument.
 */
static int
netmap_poll(struct cdev *dev, int events, struct thread *td)
{
	struct netmap_priv_d *priv = NULL;
	struct netmap_adapter *na;
	struct ifnet *ifp;
	struct netmap_kring *kring;
	u_int core_lock, i, check_all, want_tx, want_rx, revents = 0;
	u_int lim_tx, lim_rx, host_forwarded = 0;
	struct mbq q = { NULL, NULL, 0 };
	enum {NO_CL, NEED_CL, LOCKED_CL }; /* see below */
	void *pwait = dev;	/* linux compatibility */

	(void)pwait;

	if (devfs_get_cdevpriv((void **)&priv) != 0 || priv == NULL)
		return POLLERR;

	if (priv->np_nifp == NULL) {
		D("No if registered");
		return POLLERR;
	}
	rmb(); /* make sure following reads are not from cache */

	ifp = priv->np_ifp;
	// XXX check for deleting() ?
	if ( (ifp->if_capenable & IFCAP_NETMAP) == 0)
		return POLLERR;

	if (netmap_verbose & 0x8000)
		D("device %s events 0x%x", ifp->if_xname, events);
	want_tx = events & (POLLOUT | POLLWRNORM);
	want_rx = events & (POLLIN | POLLRDNORM);

	na = NA(ifp); /* retrieve netmap adapter */

	lim_tx = na->num_tx_rings;
	lim_rx = na->num_rx_rings;
	/* how many queues we are scanning */
	if (priv->np_qfirst == NETMAP_SW_RING) {
		if (priv->np_txpoll || want_tx) {
			/* push any packets up, then we are always ready */
			netmap_sync_to_host(na);
			revents |= want_tx;
		}
		if (want_rx) {
			kring = &na->rx_rings[lim_rx];
			if (kring->ring->avail == 0)
				netmap_sync_from_host(na, td, dev);
			if (kring->ring->avail > 0) {
				revents |= want_rx;
			}
		}
		return (revents);
	}

	/* if we are in transparent mode, check also the host rx ring */
	kring = &na->rx_rings[lim_rx];
	if ( (priv->np_qlast == NETMAP_HW_RING) // XXX check_all
			&& want_rx
			&& (netmap_fwd || kring->ring->flags & NR_FORWARD) ) {
		if (kring->ring->avail == 0)
			netmap_sync_from_host(na, td, dev);
		if (kring->ring->avail > 0)
			revents |= want_rx;
	}

	/*
	 * check_all is set if the card has more than one queue and
	 * the client is polling all of them. If true, we sleep on
	 * the "global" selfd, otherwise we sleep on individual selfd
	 * (we can only sleep on one of them per direction).
	 * The interrupt routine in the driver should always wake on
	 * the individual selfd, and also on the global one if the card
	 * has more than one ring.
	 *
	 * If the card has only one lock, we just use that.
	 * If the card has separate ring locks, we just use those
	 * unless we are doing check_all, in which case the whole
	 * loop is wrapped by the global lock.
	 * We acquire locks only when necessary: if poll is called
	 * when buffers are available, we can just return without locks.
	 *
	 * rxsync() is only called if we run out of buffers on a POLLIN.
	 * txsync() is called if we run out of buffers on POLLOUT, or
	 * there are pending packets to send. The latter can be disabled
	 * passing NETMAP_NO_TX_POLL in the NIOCREG call.
	 */
	check_all = (priv->np_qlast == NETMAP_HW_RING) && (lim_tx > 1 || lim_rx > 1);

	/*
	 * core_lock indicates what to do with the core lock.
	 * The core lock is used when either the card has no individual
	 * locks, or it has individual locks but we are cheking all
	 * rings so we need the core lock to avoid missing wakeup events.
	 *
	 * It has three possible states:
	 * NO_CL	we don't need to use the core lock, e.g.
	 *		because we are protected by individual locks.
	 * NEED_CL	we need the core lock. In this case, when we
	 *		call the lock routine, move to LOCKED_CL
	 *		to remember to release the lock once done.
	 * LOCKED_CL	core lock is set, so we need to release it.
	 */
	core_lock = (check_all || !na->separate_locks) ? NEED_CL : NO_CL;
#ifdef NM_BRIDGE
	/* the bridge uses separate locks */
	if (na->nm_register == bdg_netmap_reg) {
		ND("not using core lock for %s", ifp->if_xname);
		core_lock = NO_CL;
	}
#endif /* NM_BRIDGE */
	if (priv->np_qlast != NETMAP_HW_RING) {
		lim_tx = lim_rx = priv->np_qlast;
	}

	/*
	 * We start with a lock free round which is good if we have
	 * data available. If this fails, then lock and call the sync
	 * routines.
	 */
	for (i = priv->np_qfirst; want_rx && i < lim_rx; i++) {
		kring = &na->rx_rings[i];
		if (kring->ring->avail > 0) {
			revents |= want_rx;
			want_rx = 0;	/* also breaks the loop */
		}
	}
	for (i = priv->np_qfirst; want_tx && i < lim_tx; i++) {
		kring = &na->tx_rings[i];
		if (kring->ring->avail > 0) {
			revents |= want_tx;
			want_tx = 0;	/* also breaks the loop */
		}
	}

	/*
	 * If we to push packets out (priv->np_txpoll) or want_tx is
	 * still set, we do need to run the txsync calls (on all rings,
	 * to avoid that the tx rings stall).
	 */
	if (priv->np_txpoll || want_tx) {
flush_tx:
		for (i = priv->np_qfirst; i < lim_tx; i++) {
			kring = &na->tx_rings[i];
			/*
			 * Skip the current ring if want_tx == 0
			 * (we have already done a successful sync on
			 * a previous ring) AND kring->cur == kring->hwcur
			 * (there are no pending transmissions for this ring).
			 */
			if (!want_tx && kring->ring->cur == kring->nr_hwcur)
				continue;
			if (core_lock == NEED_CL) {
				na->nm_lock(ifp, NETMAP_CORE_LOCK, 0);
				core_lock = LOCKED_CL;
			}
			if (na->separate_locks)
				na->nm_lock(ifp, NETMAP_TX_LOCK, i);
			if (netmap_verbose & NM_VERB_TXSYNC)
				D("send %d on %s %d",
					kring->ring->cur,
					ifp->if_xname, i);
			if (na->nm_txsync(ifp, i, 0 /* no lock */))
				revents |= POLLERR;

			/* Check avail/call selrecord only if called with POLLOUT */
			if (want_tx) {
				if (kring->ring->avail > 0) {
					/* stop at the first ring. We don't risk
					 * starvation.
					 */
					revents |= want_tx;
					want_tx = 0;
				} else if (!check_all)
					selrecord(td, &kring->si);
			}
			if (na->separate_locks)
				na->nm_lock(ifp, NETMAP_TX_UNLOCK, i);
		}
	}

	/*
	 * now if want_rx is still set we need to lock and rxsync.
	 * Do it on all rings because otherwise we starve.
	 */
	if (want_rx) {
		for (i = priv->np_qfirst; i < lim_rx; i++) {
			kring = &na->rx_rings[i];
			if (core_lock == NEED_CL) {
				na->nm_lock(ifp, NETMAP_CORE_LOCK, 0);
				core_lock = LOCKED_CL;
			}
			if (na->separate_locks)
				na->nm_lock(ifp, NETMAP_RX_LOCK, i);
			if (netmap_fwd ||kring->ring->flags & NR_FORWARD) {
				ND(10, "forwarding some buffers up %d to %d",
				    kring->nr_hwcur, kring->ring->cur);
				netmap_grab_packets(kring, &q, netmap_fwd);
			}

			if (na->nm_rxsync(ifp, i, 0 /* no lock */))
				revents |= POLLERR;
			if (netmap_no_timestamp == 0 ||
					kring->ring->flags & NR_TIMESTAMP) {
				microtime(&kring->ring->ts);
			}

			if (kring->ring->avail > 0)
				revents |= want_rx;
			else if (!check_all)
				selrecord(td, &kring->si);
			if (na->separate_locks)
				na->nm_lock(ifp, NETMAP_RX_UNLOCK, i);
		}
	}
	if (check_all && revents == 0) { /* signal on the global queue */
		if (want_tx)
			selrecord(td, &na->tx_si);
		if (want_rx)
			selrecord(td, &na->rx_si);
	}

	/* forward host to the netmap ring */
	kring = &na->rx_rings[lim_rx];
	if (kring->nr_hwavail > 0)
		ND("host rx %d has %d packets", lim_rx, kring->nr_hwavail);
	if ( (priv->np_qlast == NETMAP_HW_RING) // XXX check_all
			&& (netmap_fwd || kring->ring->flags & NR_FORWARD)
			 && kring->nr_hwavail > 0 && !host_forwarded) {
		if (core_lock == NEED_CL) {
			na->nm_lock(ifp, NETMAP_CORE_LOCK, 0);
			core_lock = LOCKED_CL;
		}
		netmap_sw_to_nic(na);
		host_forwarded = 1; /* prevent another pass */
		want_rx = 0;
		goto flush_tx;
	}

	if (core_lock == LOCKED_CL)
		na->nm_lock(ifp, NETMAP_CORE_UNLOCK, 0);
	if (q.head)
		netmap_send_up(na->ifp, q.head);

	return (revents);
}

/*------- driver support routines ------*/


/*
 * default lock wrapper.
 */
static void
netmap_lock_wrapper(struct ifnet *dev, int what, u_int queueid)
{
	struct netmap_adapter *na = NA(dev);

	switch (what) {
#ifdef linux	/* some system do not need lock on register */
	case NETMAP_REG_LOCK:
	case NETMAP_REG_UNLOCK:
		break;
#endif /* linux */

	case NETMAP_CORE_LOCK:
		mtx_lock(&na->core_lock);
		break;

	case NETMAP_CORE_UNLOCK:
		mtx_unlock(&na->core_lock);
		break;

	case NETMAP_TX_LOCK:
		mtx_lock(&na->tx_rings[queueid].q_lock);
		break;

	case NETMAP_TX_UNLOCK:
		mtx_unlock(&na->tx_rings[queueid].q_lock);
		break;

	case NETMAP_RX_LOCK:
		mtx_lock(&na->rx_rings[queueid].q_lock);
		break;

	case NETMAP_RX_UNLOCK:
		mtx_unlock(&na->rx_rings[queueid].q_lock);
		break;
	}
}


/*
 * Initialize a ``netmap_adapter`` object created by driver on attach.
 * We allocate a block of memory with room for a struct netmap_adapter
 * plus two sets of N+2 struct netmap_kring (where N is the number
 * of hardware rings):
 * krings	0..N-1	are for the hardware queues.
 * kring	N	is for the host stack queue
 * kring	N+1	is only used for the selinfo for all queues.
 * Return 0 on success, ENOMEM otherwise.
 *
 * By default the receive and transmit adapter ring counts are both initialized
 * to num_queues.  na->num_tx_rings can be set for cards with different tx/rx
 * setups.
 */
int
netmap_attach(struct netmap_adapter *arg, int num_queues)
{
	struct netmap_adapter *na = NULL;
	struct ifnet *ifp = arg ? arg->ifp : NULL;
	int len;

	if (arg == NULL || ifp == NULL)
		goto fail;
	len = nma_is_vp(arg) ? sizeof(*na) : sizeof(*na) * 2;
	na = malloc(len, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (na == NULL)
		goto fail;
	WNA(ifp) = na;
	*na = *arg; /* copy everything, trust the driver to not pass junk */
	NETMAP_SET_CAPABLE(ifp);
	if (na->num_tx_rings == 0)
		na->num_tx_rings = num_queues;
	na->num_rx_rings = num_queues;
	na->refcount = na->na_single = na->na_multi = 0;
	/* Core lock initialized here, others after netmap_if_new. */
	mtx_init(&na->core_lock, "netmap core lock", MTX_NETWORK_LOCK, MTX_DEF);
	if (na->nm_lock == NULL) {
		ND("using default locks for %s", ifp->if_xname);
		na->nm_lock = netmap_lock_wrapper;
	}
#ifdef linux
	if (ifp->netdev_ops) {
		ND("netdev_ops %p", ifp->netdev_ops);
		/* prepare a clone of the netdev ops */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 28)
		na->nm_ndo.ndo_start_xmit = ifp->netdev_ops;
#else
		na->nm_ndo = *ifp->netdev_ops;
#endif
	}
	na->nm_ndo.ndo_start_xmit = linux_netmap_start;
#endif
	if (!nma_is_vp(arg))
		netmap_attach_sw(ifp);
	D("success for %s", ifp->if_xname);
	return 0;

fail:
	D("fail, arg %p ifp %p na %p", arg, ifp, na);
	netmap_detach(ifp);
	return (na ? EINVAL : ENOMEM);
}


/*
 * Free the allocated memory linked to the given ``netmap_adapter``
 * object.
 */
void
netmap_detach(struct ifnet *ifp)
{
	struct netmap_adapter *na = NA(ifp);

	if (!na)
		return;

	mtx_destroy(&na->core_lock);

	if (na->tx_rings) { /* XXX should not happen */
		D("freeing leftover tx_rings");
		free(na->tx_rings, M_DEVBUF);
	}
	bzero(na, sizeof(*na));
	WNA(ifp) = NULL;
	free(na, M_DEVBUF);
}


int
nm_bdg_flush(struct nm_bdg_fwd *ft, int n, struct netmap_adapter *na, u_int ring_nr);

/* we don't need to lock myself */
static int
bdg_netmap_start(struct ifnet *ifp, struct mbuf *m)
{
	struct netmap_adapter *na = SWNA(ifp);
	struct nm_bdg_fwd *ft = na->rx_rings[0].nkr_ft;
	char *buf = NMB(&na->rx_rings[0].ring->slot[0]);
	u_int len = MBUF_LEN(m);

	if (!na->na_bdg) /* SWNA is not configured to be attached */
		return EBUSY;
	m_copydata(m, 0, len, buf);
	ft->ft_flags = 0;	// XXX could be indirect ?
	ft->ft_len = len;
	ft->ft_buf = buf;
	ft->ft_next = NM_BDG_BATCH; // XXX is it needed ?
	nm_bdg_flush(ft, 1, na, 0);

	/* release the mbuf in either cases of success or failure. As an
	 * alternative, put the mbuf in a free list and free the list
	 * only when really necessary.
	 */
	m_freem(m);

	return (0);
}


/*
 * Intercept packets from the network stack and pass them
 * to netmap as incoming packets on the 'software' ring.
 * We are not locked when called.
 */
int
netmap_start(struct ifnet *ifp, struct mbuf *m)
{
	struct netmap_adapter *na = NA(ifp);
	struct netmap_kring *kring = &na->rx_rings[na->num_rx_rings];
	u_int i, len = MBUF_LEN(m);
	u_int error = EBUSY, lim = kring->nkr_num_slots - 1;
	struct netmap_slot *slot;

	if (netmap_verbose & NM_VERB_HOST)
		D("%s packet %d len %d from the stack", ifp->if_xname,
			kring->nr_hwcur + kring->nr_hwavail, len);
	if (len > NETMAP_BUF_SIZE) { /* too long for us */
		D("%s from_host, drop packet size %d > %d", ifp->if_xname,
			len, NETMAP_BUF_SIZE);
		m_freem(m);
		return EINVAL;
	}
	if (na->na_bdg)
		return bdg_netmap_start(ifp, m);

	na->nm_lock(ifp, NETMAP_CORE_LOCK, 0);
	if (kring->nr_hwavail >= lim) {
		if (netmap_verbose)
			D("stack ring %s full\n", ifp->if_xname);
		goto done;	/* no space */
	}

	/* compute the insert position */
	i = kring->nr_hwcur + kring->nr_hwavail;
	if (i > lim)
		i -= lim + 1;
	slot = &kring->ring->slot[i];
	m_copydata(m, 0, len, NMB(slot));
	slot->len = len;
	slot->flags = kring->nkr_slot_flags;
	kring->nr_hwavail++;
	if (netmap_verbose  & NM_VERB_HOST)
		D("wake up host ring %s %d", na->ifp->if_xname, na->num_rx_rings);
	selwakeuppri(&kring->si, PI_NET);
	error = 0;
done:
	na->nm_lock(ifp, NETMAP_CORE_UNLOCK, 0);

	/* release the mbuf in either cases of success or failure. As an
	 * alternative, put the mbuf in a free list and free the list
	 * only when really necessary.
	 */
	m_freem(m);

	return (error);
}


/*
 * netmap_reset() is called by the driver routines when reinitializing
 * a ring. The driver is in charge of locking to protect the kring.
 * If netmap mode is not set just return NULL.
 */
struct netmap_slot *
netmap_reset(struct netmap_adapter *na, enum txrx tx, int n,
	u_int new_cur)
{
	struct netmap_kring *kring;
	int new_hwofs, lim;

	if (na == NULL)
		return NULL;	/* no netmap support here */
	if (!(na->ifp->if_capenable & IFCAP_NETMAP))
		return NULL;	/* nothing to reinitialize */

	if (tx == NR_TX) {
		if (n >= na->num_tx_rings)
			return NULL;
		kring = na->tx_rings + n;
		new_hwofs = kring->nr_hwcur - new_cur;
	} else {
		if (n >= na->num_rx_rings)
			return NULL;
		kring = na->rx_rings + n;
		new_hwofs = kring->nr_hwcur + kring->nr_hwavail - new_cur;
	}
	lim = kring->nkr_num_slots - 1;
	if (new_hwofs > lim)
		new_hwofs -= lim + 1;

	/* Alwayws set the new offset value and realign the ring. */
	kring->nkr_hwofs = new_hwofs;
	if (tx == NR_TX)
		kring->nr_hwavail = kring->nkr_num_slots - 1;
	ND(10, "new hwofs %d on %s %s[%d]",
			kring->nkr_hwofs, na->ifp->if_xname,
			tx == NR_TX ? "TX" : "RX", n);

#if 0 // def linux
	/* XXX check that the mappings are correct */
	/* need ring_nr, adapter->pdev, direction */
	buffer_info->dma = dma_map_single(&pdev->dev, addr, adapter->rx_buffer_len, DMA_FROM_DEVICE);
	if (dma_mapping_error(&adapter->pdev->dev, buffer_info->dma)) {
		D("error mapping rx netmap buffer %d", i);
		// XXX fix error handling
	}

#endif /* linux */
	/*
	 * Wakeup on the individual and global lock
	 * We do the wakeup here, but the ring is not yet reconfigured.
	 * However, we are under lock so there are no races.
	 */
	selwakeuppri(&kring->si, PI_NET);
	selwakeuppri(tx == NR_TX ? &na->tx_si : &na->rx_si, PI_NET);
	return kring->ring->slot;
}


/* returns the next position in the ring */
static int
nm_bdg_preflush(struct netmap_adapter *na, u_int ring_nr,
	struct netmap_kring *kring, u_int end)
{
	struct netmap_ring *ring = kring->ring;
	struct nm_bdg_fwd *ft = kring->nkr_ft;
	u_int j = kring->nr_hwcur, lim = kring->nkr_num_slots - 1;
	u_int ft_i = 0;	/* start from 0 */

	for (; likely(j != end); j = unlikely(j == lim) ? 0 : j+1) {
		struct netmap_slot *slot = &ring->slot[j];
		char *buf = NMB(slot);
		int len = ft[ft_i].ft_len = slot->len;

		ft[ft_i].ft_flags = slot->flags;

		ND("flags is 0x%x", slot->flags);
		/* this slot goes into a list so initialize the link field */
		ft[ft_i].ft_next = NM_BDG_BATCH; /* equivalent to NULL */
		if (unlikely(len < 14))
			continue;
		buf = ft[ft_i].ft_buf = (slot->flags & NS_INDIRECT) ?
			*((void **)buf) : buf;
		prefetch(buf);
		if (unlikely(++ft_i == netmap_bridge))
			ft_i = nm_bdg_flush(ft, ft_i, na, ring_nr);
	}
	if (ft_i)
		ft_i = nm_bdg_flush(ft, ft_i, na, ring_nr);
	return j;
}


/*
 * Pass packets from nic to the bridge. Must be called with
 * proper locks on the source interface.
 * Note, no user process can access this NIC so we can ignore
 * the info in the 'ring'.
 */
static void
netmap_nic_to_bdg(struct ifnet *ifp, u_int ring_nr)
{
	struct netmap_adapter *na = NA(ifp);
	struct netmap_kring *kring = &na->rx_rings[ring_nr];
	struct netmap_ring *ring = kring->ring;
	int j, k, lim = kring->nkr_num_slots - 1;

	/* fetch packets that have arrived */
	na->nm_rxsync(ifp, ring_nr, 0);
	/* XXX we don't count reserved, but it should be 0 */
	j = kring->nr_hwcur;
	k = j + kring->nr_hwavail;
	if (k > lim)
		k -= lim + 1;
	if (k == j && netmap_verbose) {
		D("how strange, interrupt with no packets on %s",
			ifp->if_xname);
		return;
	}

	j = nm_bdg_preflush(na, ring_nr, kring, k);

	/* we consume everything, but we cannot update kring directly
	 * because the nic may have destroyed the info in the NIC ring.
	 * So we need to call rxsync again to restore it.
	 */
	ring->cur = j;
	ring->avail = 0;
	na->nm_rxsync(ifp, ring_nr, 0);
	return;
}


/*
 * Default functions to handle rx/tx interrupts
 * we have 4 cases:
 * 1 ring, single lock:
 *	lock(core); wake(i=0); unlock(core)
 * N rings, single lock:
 *	lock(core); wake(i); wake(N+1) unlock(core)
 * 1 ring, separate locks: (i=0)
 *	lock(i); wake(i); unlock(i)
 * N rings, separate locks:
 *	lock(i); wake(i); unlock(i); lock(core) wake(N+1) unlock(core)
 * work_done is non-null on the RX path.
 *
 * The 'q' argument also includes flag to tell whether the queue is
 * already locked on enter, and whether it should remain locked on exit.
 * This helps adapting to different defaults in drivers and OSes.
 */
int
netmap_rx_irq(struct ifnet *ifp, int q, int *work_done)
{
	struct netmap_adapter *na;
	struct netmap_kring *r;
	NM_SELINFO_T *main_wq;
	int locktype, unlocktype, nic_to_bridge, lock;

	if (!(ifp->if_capenable & IFCAP_NETMAP))
		return 0;

	lock = q & (NETMAP_LOCKED_ENTER | NETMAP_LOCKED_EXIT);
	q = q & NETMAP_RING_MASK;

	ND(5, "received %s queue %d", work_done ? "RX" : "TX" , q);
	na = NA(ifp);
	if (na->na_flags & NAF_SKIP_INTR) {
		ND("use regular interrupt");
		return 0;
	}

	if (work_done) { /* RX path */
		if (q >= na->num_rx_rings)
			return 0;	// not a physical queue
		r = na->rx_rings + q;
		r->nr_kflags |= NKR_PENDINTR;
		main_wq = (na->num_rx_rings > 1) ? &na->rx_si : NULL;
		/* set a flag if the NIC is attached to a VALE switch */
		nic_to_bridge = (na->na_bdg != NULL);
		locktype = NETMAP_RX_LOCK;
		unlocktype = NETMAP_RX_UNLOCK;
	} else { /* TX path */
		if (q >= na->num_tx_rings)
			return 0;	// not a physical queue
		r = na->tx_rings + q;
		main_wq = (na->num_tx_rings > 1) ? &na->tx_si : NULL;
		work_done = &q; /* dummy */
		nic_to_bridge = 0;
		locktype = NETMAP_TX_LOCK;
		unlocktype = NETMAP_TX_UNLOCK;
	}
	if (na->separate_locks) {
		if (!(lock & NETMAP_LOCKED_ENTER))
			na->nm_lock(ifp, locktype, q);
		/* If a NIC is attached to a bridge, flush packets
		 * (and no need to wakeup anyone). Otherwise, wakeup
		 * possible processes waiting for packets.
		 */
		if (nic_to_bridge)
			netmap_nic_to_bdg(ifp, q);
		else
			selwakeuppri(&r->si, PI_NET);
		na->nm_lock(ifp, unlocktype, q);
		if (main_wq && !nic_to_bridge) {
			na->nm_lock(ifp, NETMAP_CORE_LOCK, 0);
			selwakeuppri(main_wq, PI_NET);
			na->nm_lock(ifp, NETMAP_CORE_UNLOCK, 0);
		}
		/* lock the queue again if requested */
		if (lock & NETMAP_LOCKED_EXIT)
			na->nm_lock(ifp, locktype, q);
	} else {
		if (!(lock & NETMAP_LOCKED_ENTER))
			na->nm_lock(ifp, NETMAP_CORE_LOCK, 0);
		if (nic_to_bridge)
			netmap_nic_to_bdg(ifp, q);
		else {
			selwakeuppri(&r->si, PI_NET);
			if (main_wq)
				selwakeuppri(main_wq, PI_NET);
		}
		if (!(lock & NETMAP_LOCKED_EXIT))
			na->nm_lock(ifp, NETMAP_CORE_UNLOCK, 0);
	}
	*work_done = 1; /* do not fire napi again */
	return 1;
}


#ifdef linux	/* linux-specific routines */


/*
 * Remap linux arguments into the FreeBSD call.
 * - pwait is the poll table, passed as 'dev';
 *   If pwait == NULL someone else already woke up before. We can report
 *   events but they are filtered upstream.
 *   If pwait != NULL, then pwait->key contains the list of events.
 * - events is computed from pwait as above.
 * - file is passed as 'td';
 */
static u_int
linux_netmap_poll(struct file * file, struct poll_table_struct *pwait)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
	int events = POLLIN | POLLOUT; /* XXX maybe... */
#elif LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0)
	int events = pwait ? pwait->key : POLLIN | POLLOUT;
#else /* in 3.4.0 field 'key' was renamed to '_key' */
	int events = pwait ? pwait->_key : POLLIN | POLLOUT;
#endif
	return netmap_poll((void *)pwait, events, (void *)file);
}


static int
linux_netmap_mmap(struct file *f, struct vm_area_struct *vma)
{
	int lut_skip, i, j;
	int user_skip = 0;
	struct lut_entry *l_entry;
	int error = 0;
	unsigned long off, tomap;
	/*
	 * vma->vm_start: start of mapping user address space
	 * vma->vm_end: end of the mapping user address space
	 * vma->vm_pfoff: offset of first page in the device
	 */

	// XXX security checks

	error = netmap_get_memory(f->private_data);
	ND("get_memory returned %d", error);
	if (error)
	    return -error;

	off = vma->vm_pgoff << PAGE_SHIFT; /* offset in bytes */
	tomap = vma->vm_end - vma->vm_start;
	for (i = 0; i < NETMAP_POOLS_NR; i++) {  /* loop through obj_pools */
		const struct netmap_obj_pool *p = &nm_mem.pools[i];
		/*
		 * In each pool memory is allocated in clusters
		 * of size _clustsize, each containing clustentries
		 * entries. For each object k we already store the
		 * vtophys mapping in lut[k] so we use that, scanning
		 * the lut[] array in steps of clustentries,
		 * and we map each cluster (not individual pages,
		 * it would be overkill -- XXX slow ? 20130415).
		 */

		/*
		 * We interpret vm_pgoff as an offset into the whole
		 * netmap memory, as if all clusters where contiguous.
		 */
		for (lut_skip = 0, j = 0; j < p->_numclusters; j++, lut_skip += p->clustentries) {
			unsigned long paddr, mapsize;
			if (p->_clustsize <= off) {
				off -= p->_clustsize;
				continue;
			}
			l_entry = &p->lut[lut_skip]; /* first obj in the cluster */
			paddr = l_entry->paddr + off;
			mapsize = p->_clustsize - off;
			off = 0;
			if (mapsize > tomap)
				mapsize = tomap;
			ND("remap_pfn_range(%lx, %lx, %lx)",
				vma->vm_start + user_skip,
				paddr >> PAGE_SHIFT, mapsize);
			if (remap_pfn_range(vma, vma->vm_start + user_skip,
					paddr >> PAGE_SHIFT, mapsize,
					vma->vm_page_prot))
				return -EAGAIN; // XXX check return value
			user_skip += mapsize;
			tomap -= mapsize;
			if (tomap == 0)
				goto done;
		}
	}
done:

	return 0;
}


static netdev_tx_t
linux_netmap_start(struct sk_buff *skb, struct net_device *dev)
{
	netmap_start(dev, skb);
	return (NETDEV_TX_OK);
}


#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,37)	// XXX was 38
#define LIN_IOCTL_NAME	.ioctl
int
linux_netmap_ioctl(struct inode *inode, struct file *file, u_int cmd, u_long data /* arg */)
#else
#define LIN_IOCTL_NAME	.unlocked_ioctl
long
linux_netmap_ioctl(struct file *file, u_int cmd, u_long data /* arg */)
#endif
{
	int ret;
	struct nmreq nmr;
	bzero(&nmr, sizeof(nmr));

	if (data && copy_from_user(&nmr, (void *)data, sizeof(nmr) ) != 0)
		return -EFAULT;
	ret = netmap_ioctl(NULL, cmd, (caddr_t)&nmr, 0, (void *)file);
	if (data && copy_to_user((void*)data, &nmr, sizeof(nmr) ) != 0)
		return -EFAULT;
	return -ret;
}


static int
netmap_release(struct inode *inode, struct file *file)
{
	(void)inode;	/* UNUSED */
	if (file->private_data)
		netmap_dtor(file->private_data);
	return (0);
}


static int
linux_netmap_open(struct inode *inode, struct file *file)
{
	struct netmap_priv_d *priv;
	(void)inode;	/* UNUSED */

	priv = malloc(sizeof(struct netmap_priv_d), M_DEVBUF,
			      M_NOWAIT | M_ZERO);
	if (priv == NULL)
		return -ENOMEM;

	file->private_data = priv;

	return (0);
}


static struct file_operations netmap_fops = {
    .owner = THIS_MODULE,
    .open = linux_netmap_open,
    .mmap = linux_netmap_mmap,
    LIN_IOCTL_NAME = linux_netmap_ioctl,
    .poll = linux_netmap_poll,
    .release = netmap_release,
};


static struct miscdevice netmap_cdevsw = {	/* same name as FreeBSD */
	MISC_DYNAMIC_MINOR,
	"netmap",
	&netmap_fops,
};

static int netmap_init(void);
static void netmap_fini(void);


/* Errors have negative values on linux */
static int linux_netmap_init(void)
{
	return -netmap_init();
}

module_init(linux_netmap_init);
module_exit(netmap_fini);
/* export certain symbols to other modules */
EXPORT_SYMBOL(netmap_attach);		// driver attach routines
EXPORT_SYMBOL(netmap_detach);		// driver detach routines
EXPORT_SYMBOL(netmap_ring_reinit);	// ring init on error
EXPORT_SYMBOL(netmap_buffer_lut);
EXPORT_SYMBOL(netmap_total_buffers);	// index check
EXPORT_SYMBOL(netmap_buffer_base);
EXPORT_SYMBOL(netmap_reset);		// ring init routines
EXPORT_SYMBOL(netmap_buf_size);
EXPORT_SYMBOL(netmap_rx_irq);		// default irq handler
EXPORT_SYMBOL(netmap_no_pendintr);	// XXX mitigation - should go away
EXPORT_SYMBOL(netmap_bdg_ctl);		// bridge configuration routine
EXPORT_SYMBOL(netmap_bdg_learning);	// the default lookup function


MODULE_AUTHOR("http://info.iet.unipi.it/~luigi/netmap/");
MODULE_DESCRIPTION("The netmap packet I/O framework");
MODULE_LICENSE("Dual BSD/GPL"); /* the code here is all BSD. */

#else /* __FreeBSD__ */


static struct cdevsw netmap_cdevsw = {
	.d_version = D_VERSION,
	.d_name = "netmap",
	.d_open = netmap_open,
	.d_mmap = netmap_mmap,
	.d_mmap_single = netmap_mmap_single,
	.d_ioctl = netmap_ioctl,
	.d_poll = netmap_poll,
	.d_close = netmap_close,
};
#endif /* __FreeBSD__ */

#ifdef NM_BRIDGE
/*
 *---- support for virtual bridge -----
 */

/* ----- FreeBSD if_bridge hash function ------- */

/*
 * The following hash function is adapted from "Hash Functions" by Bob Jenkins
 * ("Algorithm Alley", Dr. Dobbs Journal, September 1997).
 *
 * http://www.burtleburtle.net/bob/hash/spooky.html
 */
#define mix(a, b, c)                                                    \
do {                                                                    \
        a -= b; a -= c; a ^= (c >> 13);                                 \
        b -= c; b -= a; b ^= (a << 8);                                  \
        c -= a; c -= b; c ^= (b >> 13);                                 \
        a -= b; a -= c; a ^= (c >> 12);                                 \
        b -= c; b -= a; b ^= (a << 16);                                 \
        c -= a; c -= b; c ^= (b >> 5);                                  \
        a -= b; a -= c; a ^= (c >> 3);                                  \
        b -= c; b -= a; b ^= (a << 10);                                 \
        c -= a; c -= b; c ^= (b >> 15);                                 \
} while (/*CONSTCOND*/0)

static __inline uint32_t
nm_bridge_rthash(const uint8_t *addr)
{
        uint32_t a = 0x9e3779b9, b = 0x9e3779b9, c = 0; // hask key

        b += addr[5] << 8;
        b += addr[4];
        a += addr[3] << 24;
        a += addr[2] << 16;
        a += addr[1] << 8;
        a += addr[0];

        mix(a, b, c);
#define BRIDGE_RTHASH_MASK	(NM_BDG_HASH-1)
        return (c & BRIDGE_RTHASH_MASK);
}

#undef mix


static int
bdg_netmap_reg(struct ifnet *ifp, int onoff)
{
	// struct nm_bridge *b = NA(ifp)->na_bdg;

	/* the interface is already attached to the bridge,
	 * so we only need to toggle IFCAP_NETMAP.
	 * Locking is not necessary (we are already under
	 * NMA_LOCK, and the port is not in use during this call).
	 */
	/* BDG_WLOCK(b); */
	if (onoff) {
		ifp->if_capenable |= IFCAP_NETMAP;
	} else {
		ifp->if_capenable &= ~IFCAP_NETMAP;
	}
	/* BDG_WUNLOCK(b); */
	return 0;
}


/*
 * Lookup function for a learning bridge.
 * Update the hash table with the source address,
 * and then returns the destination port index, and the
 * ring in *dst_ring (at the moment, always use ring 0)
 */
u_int
netmap_bdg_learning(char *buf, u_int len, uint8_t *dst_ring,
		struct netmap_adapter *na)
{
	struct nm_hash_ent *ht = na->na_bdg->ht;
	uint32_t sh, dh;
	u_int dst, mysrc = na->bdg_port;
	uint64_t smac, dmac;

	dmac = le64toh(*(uint64_t *)(buf)) & 0xffffffffffff;
	smac = le64toh(*(uint64_t *)(buf + 4));
	smac >>= 16;

	/*
	 * The hash is somewhat expensive, there might be some
	 * worthwhile optimizations here.
	 */
	if ((buf[6] & 1) == 0) { /* valid src */
		uint8_t *s = buf+6;
		sh = nm_bridge_rthash(buf+6); // XXX hash of source
		/* update source port forwarding entry */
		ht[sh].mac = smac;	/* XXX expire ? */
		ht[sh].ports = mysrc;
		if (netmap_verbose)
		    D("src %02x:%02x:%02x:%02x:%02x:%02x on port %d",
			s[0], s[1], s[2], s[3], s[4], s[5], mysrc);
	}
	dst = NM_BDG_BROADCAST;
	if ((buf[0] & 1) == 0) { /* unicast */
		dh = nm_bridge_rthash(buf); // XXX hash of dst
		if (ht[dh].mac == dmac) {	/* found dst */
			dst = ht[dh].ports;
		}
		/* XXX otherwise return NM_BDG_UNKNOWN ? */
	}
	*dst_ring = 0;
	return dst;
}


/*
 * This flush routine supports only unicast and broadcast but a large
 * number of ports, and lets us replace the learn and dispatch functions.
 */
int
nm_bdg_flush(struct nm_bdg_fwd *ft, int n, struct netmap_adapter *na,
		u_int ring_nr)
{
	struct nm_bdg_q *dst_ents, *brddst;
	uint16_t num_dsts = 0, *dsts;
	struct nm_bridge *b = na->na_bdg;
	u_int i, me = na->bdg_port;

	dst_ents = (struct nm_bdg_q *)(ft + NM_BDG_BATCH);
	dsts = (uint16_t *)(dst_ents + NM_BDG_MAXPORTS * NM_BDG_MAXRINGS + 1);

	BDG_RLOCK(b);

	/* first pass: find a destination */
	for (i = 0; likely(i < n); i++) {
		uint8_t *buf = ft[i].ft_buf;
		uint8_t dst_ring = ring_nr;
		uint16_t dst_port, d_i;
		struct nm_bdg_q *d;

		dst_port = b->nm_bdg_lookup(buf, ft[i].ft_len, &dst_ring, na);
		if (dst_port == NM_BDG_NOPORT) {
			continue; /* this packet is identified to be dropped */
		} else if (unlikely(dst_port > NM_BDG_MAXPORTS)) {
			continue;
		} else if (dst_port == NM_BDG_BROADCAST) {
			dst_ring = 0; /* broadcasts always go to ring 0 */
		} else if (unlikely(dst_port == me ||
		    !BDG_GET_VAR(b->bdg_ports[dst_port]))) {
			continue;
		}

		/* get a position in the scratch pad */
		d_i = dst_port * NM_BDG_MAXRINGS + dst_ring;
		d = dst_ents + d_i;
		if (d->bq_head == NM_BDG_BATCH) { /* new destination */
			d->bq_head = d->bq_tail = i;
			/* remember this position to be scanned later */
			if (dst_port != NM_BDG_BROADCAST)
				dsts[num_dsts++] = d_i;
		} else {
			ft[d->bq_tail].ft_next = i;
			d->bq_tail = i;
		}
	}

	/* if there is a broadcast, set ring 0 of all ports to be scanned
	 * XXX This would be optimized by recording the highest index of active
	 * ports.
	 */
	brddst = dst_ents + NM_BDG_BROADCAST * NM_BDG_MAXRINGS;
	if (brddst->bq_head != NM_BDG_BATCH) {
		for (i = 0; likely(i < NM_BDG_MAXPORTS); i++) {
			uint16_t d_i = i * NM_BDG_MAXRINGS;
			if (unlikely(i == me) || !BDG_GET_VAR(b->bdg_ports[i]))
				continue;
			else if (dst_ents[d_i].bq_head == NM_BDG_BATCH)
				dsts[num_dsts++] = d_i;
		}
	}

	/* second pass: scan destinations (XXX will be modular somehow) */
	for (i = 0; i < num_dsts; i++) {
		struct ifnet *dst_ifp;
		struct netmap_adapter *dst_na;
		struct netmap_kring *kring;
		struct netmap_ring *ring;
		u_int dst_nr, is_vp, lim, j, sent = 0, d_i, next, brd_next;
		int howmany, retry = netmap_txsync_retry;
		struct nm_bdg_q *d;

		d_i = dsts[i];
		d = dst_ents + d_i;
		dst_na = BDG_GET_VAR(b->bdg_ports[d_i/NM_BDG_MAXRINGS]);
		/* protect from the lookup function returning an inactive
		 * destination port
		 */
		if (unlikely(dst_na == NULL))
			continue;
		else if (dst_na->na_flags & NAF_SW_ONLY)
			continue;
		dst_ifp = dst_na->ifp;
		/*
		 * The interface may be in !netmap mode in two cases:
		 * - when na is attached but not activated yet;
		 * - when na is being deactivated but is still attached.
		 */
		if (unlikely(!(dst_ifp->if_capenable & IFCAP_NETMAP)))
			continue;

		/* there is at least one either unicast or broadcast packet */
		brd_next = brddst->bq_head;
		next = d->bq_head;

		is_vp = nma_is_vp(dst_na);
		dst_nr = d_i & (NM_BDG_MAXRINGS-1);
		if (is_vp) { /* virtual port */
			if (dst_nr >= dst_na->num_rx_rings)
				dst_nr = dst_nr % dst_na->num_rx_rings;
			kring = &dst_na->rx_rings[dst_nr];
			ring = kring->ring;
			lim = kring->nkr_num_slots - 1;
			dst_na->nm_lock(dst_ifp, NETMAP_RX_LOCK, dst_nr);
			j = kring->nr_hwcur + kring->nr_hwavail;
			if (j > lim)
				j -= kring->nkr_num_slots;
			howmany = lim - kring->nr_hwavail;
		} else { /* hw or sw adapter */
			if (dst_nr >= dst_na->num_tx_rings)
				dst_nr = dst_nr % dst_na->num_tx_rings;
			kring = &dst_na->tx_rings[dst_nr];
			ring = kring->ring;
			lim = kring->nkr_num_slots - 1;
			dst_na->nm_lock(dst_ifp, NETMAP_TX_LOCK, dst_nr);
retry:
			dst_na->nm_txsync(dst_ifp, dst_nr, 0);
			/* see nm_bdg_flush() */
			j = kring->nr_hwcur;
			howmany = kring->nr_hwavail;
		}
		while (howmany-- > 0) {
			struct netmap_slot *slot;
			struct nm_bdg_fwd *ft_p;

			/* our 'NULL' is always higher than valid indexes
			 * so we never dereference it if the other list
			 * has packets (and if both are NULL we never
			 * get here).
			 */
			if (next < brd_next) {
				ft_p = ft + next;
				next = ft_p->ft_next;
				ND("j %d uni %d next %d %d",
					j, ft_p - ft, next, brd_next);
			} else { /* insert broadcast */
				ft_p = ft + brd_next;
				brd_next = ft_p->ft_next;
				ND("j %d brd %d next %d %d",
					j, ft_p - ft, next, brd_next);
			}
			slot = &ring->slot[j];
			ND("send %d %d bytes at %s:%d", i, ft_p->ft_len, dst_ifp->if_xname, j);
		    if (ft_p->ft_flags & NS_INDIRECT) {
			ND("copying from INDIRECT source");
			copyin(ft_p->ft_buf, NMB(slot),
				(ft_p->ft_len + 63) & ~63);
		    } else {
			pkt_copy(ft_p->ft_buf, NMB(slot), ft_p->ft_len);
		    }
			slot->len = ft_p->ft_len;
			j = unlikely(j == lim) ? 0: j + 1; /* XXX to be macro-ed */
			sent++;
			/* are we done ? */
			if (next == NM_BDG_BATCH && brd_next == NM_BDG_BATCH)
				break;
		}
		if (netmap_verbose && (howmany < 0))
			D("rx ring full on %s", dst_ifp->if_xname);
		if (is_vp) {
			if (sent) {
				kring->nr_hwavail += sent;
				selwakeuppri(&kring->si, PI_NET);
			}
			dst_na->nm_lock(dst_ifp, NETMAP_RX_UNLOCK, dst_nr);
		} else {
			if (sent) {
				ring->avail -= sent;
				ring->cur = j;
				dst_na->nm_txsync(dst_ifp, dst_nr, 0);
			}
			/* retry to send more packets */
			if (nma_is_hw(dst_na) && howmany < 0 && retry--)
				goto retry;
			dst_na->nm_lock(dst_ifp, NETMAP_TX_UNLOCK, dst_nr);
		}
		/* NM_BDG_BATCH means 'no packet' */
		d->bq_head = d->bq_tail = NM_BDG_BATCH; /* cleanup */
	}
	brddst->bq_head = brddst->bq_tail = NM_BDG_BATCH; /* cleanup */
	BDG_RUNLOCK(b);
	return 0;
}


/*
 * main dispatch routine
 */
static int
bdg_netmap_txsync(struct ifnet *ifp, u_int ring_nr, int do_lock)
{
	struct netmap_adapter *na = NA(ifp);
	struct netmap_kring *kring = &na->tx_rings[ring_nr];
	struct netmap_ring *ring = kring->ring;
	int i, j, k, lim = kring->nkr_num_slots - 1;

	k = ring->cur;
	if (k > lim)
		return netmap_ring_reinit(kring);
	if (do_lock)
		na->nm_lock(ifp, NETMAP_TX_LOCK, ring_nr);

	if (netmap_bridge <= 0) { /* testing only */
		j = k; // used all
		goto done;
	}
	if (netmap_bridge > NM_BDG_BATCH)
		netmap_bridge = NM_BDG_BATCH;

	j = nm_bdg_preflush(na, ring_nr, kring, k);
	i = k - j;
	if (i < 0)
		i += kring->nkr_num_slots;
	kring->nr_hwavail = kring->nkr_num_slots - 1 - i;
	if (j != k)
		D("early break at %d/ %d, avail %d", j, k, kring->nr_hwavail);

done:
	kring->nr_hwcur = j;
	ring->avail = kring->nr_hwavail;
	if (do_lock)
		na->nm_lock(ifp, NETMAP_TX_UNLOCK, ring_nr);

	if (netmap_verbose)
		D("%s ring %d lock %d", ifp->if_xname, ring_nr, do_lock);
	return 0;
}


static int
bdg_netmap_rxsync(struct ifnet *ifp, u_int ring_nr, int do_lock)
{
	struct netmap_adapter *na = NA(ifp);
	struct netmap_kring *kring = &na->rx_rings[ring_nr];
	struct netmap_ring *ring = kring->ring;
	u_int j, lim = kring->nkr_num_slots - 1;
	u_int k = ring->cur, resvd = ring->reserved;
	int n;

	ND("%s ring %d lock %d avail %d",
		ifp->if_xname, ring_nr, do_lock, kring->nr_hwavail);

	if (k > lim)
		return netmap_ring_reinit(kring);
	if (do_lock)
		na->nm_lock(ifp, NETMAP_RX_LOCK, ring_nr);

	/* skip past packets that userspace has released */
	j = kring->nr_hwcur;    /* netmap ring index */
	if (resvd > 0) {
		if (resvd + ring->avail >= lim + 1) {
			D("XXX invalid reserve/avail %d %d", resvd, ring->avail);
			ring->reserved = resvd = 0; // XXX panic...
		}
		k = (k >= resvd) ? k - resvd : k + lim + 1 - resvd;
	}

	if (j != k) { /* userspace has released some packets. */
		n = k - j;
		if (n < 0)
			n += kring->nkr_num_slots;
		ND("userspace releases %d packets", n);
                for (n = 0; likely(j != k); n++) {
                        struct netmap_slot *slot = &ring->slot[j];
                        void *addr = NMB(slot);

                        if (addr == netmap_buffer_base) { /* bad buf */
                                if (do_lock)
                                        na->nm_lock(ifp, NETMAP_RX_UNLOCK, ring_nr);
                                return netmap_ring_reinit(kring);
                        }
			/* decrease refcount for buffer */

			slot->flags &= ~NS_BUF_CHANGED;
                        j = unlikely(j == lim) ? 0 : j + 1;
                }
                kring->nr_hwavail -= n;
                kring->nr_hwcur = k;
        }
        /* tell userspace that there are new packets */
        ring->avail = kring->nr_hwavail - resvd;

	if (do_lock)
		na->nm_lock(ifp, NETMAP_RX_UNLOCK, ring_nr);
	return 0;
}


static void
bdg_netmap_attach(struct netmap_adapter *arg)
{
	struct netmap_adapter na;

	ND("attaching virtual bridge");
	bzero(&na, sizeof(na));

	na.ifp = arg->ifp;
	na.separate_locks = 1;
	na.num_tx_rings = arg->num_tx_rings;
	na.num_rx_rings = arg->num_rx_rings;
	na.num_tx_desc = NM_BRIDGE_RINGSIZE;
	na.num_rx_desc = NM_BRIDGE_RINGSIZE;
	na.nm_txsync = bdg_netmap_txsync;
	na.nm_rxsync = bdg_netmap_rxsync;
	na.nm_register = bdg_netmap_reg;
	netmap_attach(&na, na.num_tx_rings);
}

#endif /* NM_BRIDGE */

static struct cdev *netmap_dev; /* /dev/netmap character device. */


/*
 * Module loader.
 *
 * Create the /dev/netmap device and initialize all global
 * variables.
 *
 * Return 0 on success, errno on failure.
 */
static int
netmap_init(void)
{
	int error;

	error = netmap_memory_init();
	if (error != 0) {
		printf("netmap: unable to initialize the memory allocator.\n");
		return (error);
	}
	printf("netmap: loaded module\n");
	netmap_dev = make_dev(&netmap_cdevsw, 0, UID_ROOT, GID_WHEEL, 0660,
			      "netmap");

#ifdef NM_BRIDGE
	{
	int i;
	mtx_init(&netmap_bridge_mutex, "netmap_bridge_mutex",
		MTX_NETWORK_LOCK, MTX_DEF);
	bzero(nm_bridges, sizeof(struct nm_bridge) * NM_BRIDGES); /* safety */
	for (i = 0; i < NM_BRIDGES; i++)
		rw_init(&nm_bridges[i].bdg_lock, "bdg lock");
	}
#endif
	return (error);
}


/*
 * Module unloader.
 *
 * Free all the memory, and destroy the ``/dev/netmap`` device.
 */
static void
netmap_fini(void)
{
	destroy_dev(netmap_dev);
	netmap_memory_fini();
	printf("netmap: unloaded module.\n");
}


#ifdef __FreeBSD__
/*
 * Kernel entry point.
 *
 * Initialize/finalize the module and return.
 *
 * Return 0 on success, errno on failure.
 */
static int
netmap_loader(__unused struct module *module, int event, __unused void *arg)
{
	int error = 0;

	switch (event) {
	case MOD_LOAD:
		error = netmap_init();
		break;

	case MOD_UNLOAD:
		netmap_fini();
		break;

	default:
		error = EOPNOTSUPP;
		break;
	}

	return (error);
}


DEV_MODULE(netmap, netmap_loader, NULL);
#endif /* __FreeBSD__ */
