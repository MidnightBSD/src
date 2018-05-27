/* $MidnightBSD$ */
/*
 * Copyright (C) 2011-2014 Matteo Landi, Luigi Rizzo. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
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
 * $FreeBSD: stable/10/sys/dev/netmap/netmap.c 281706 2015-04-18 21:22:26Z rpaulo $
 *
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
 *

		SYNCHRONIZATION (USER)

The netmap rings and data structures may be shared among multiple
user threads or even independent processes.
Any synchronization among those threads/processes is delegated
to the threads themselves. Only one thread at a time can be in
a system call on the same netmap ring. The OS does not enforce
this and only guarantees against system crashes in case of
invalid usage.

		LOCKING (INTERNAL)

Within the kernel, access to the netmap rings is protected as follows:

- a spinlock on each ring, to handle producer/consumer races on
  RX rings attached to the host stack (against multiple host
  threads writing from the host stack to the same ring),
  and on 'destination' rings attached to a VALE switch
  (i.e. RX rings in VALE ports, and TX rings in NIC/host ports)
  protecting multiple active senders for the same destination)

- an atomic variable to guarantee that there is at most one
  instance of *_*xsync() on the ring at any time.
  For rings connected to user file
  descriptors, an atomic_test_and_set() protects this, and the
  lock on the ring is not actually used.
  For NIC RX rings connected to a VALE switch, an atomic_test_and_set()
  is also used to prevent multiple executions (the driver might indeed
  already guarantee this).
  For NIC TX rings connected to a VALE switch, the lock arbitrates
  access to the queue (both when allocating buffers and when pushing
  them out).

- *xsync() should be protected against initializations of the card.
  On FreeBSD most devices have the reset routine protected by
  a RING lock (ixgbe, igb, em) or core lock (re). lem is missing
  the RING protection on rx_reset(), this should be added.

  On linux there is an external lock on the tx path, which probably
  also arbitrates access to the reset routine. XXX to be revised

- a per-interface core_lock protecting access from the host stack
  while interfaces may be detached from netmap mode.
  XXX there should be no need for this lock if we detach the interfaces
  only while they are down.


--- VALE SWITCH ---

NMG_LOCK() serializes all modifications to switches and ports.
A switch cannot be deleted until all ports are gone.

For each switch, an SX lock (RWlock on linux) protects
deletion of ports. When configuring or deleting a new port, the
lock is acquired in exclusive mode (after holding NMG_LOCK).
When forwarding, the lock is acquired in shared mode (without NMG_LOCK).
The lock is held throughout the entire forwarding cycle,
during which the thread may incur in a page fault.
Hence it is important that sleepable shared locks are used.

On the rx ring, the per-port lock is grabbed initially to reserve
a number of slot in the ring, then the lock is released,
packets are copied from source to destination, and then
the lock is acquired again and the receive ring is updated.
(A similar thing is done on the tx ring for NIC and host stack
ports attached to the switch)

 */


/* --- internals ----
 *
 * Roadmap to the code that implements the above.
 *
 * > 1. a process/thread issues one or more open() on /dev/netmap, to create
 * >    select()able file descriptor on which events are reported.
 *
 *  	Internally, we allocate a netmap_priv_d structure, that will be
 *  	initialized on ioctl(NIOCREGIF).
 *
 *      os-specific:
 *  	    FreeBSD: netmap_open (netmap_freebsd.c). The priv is
 *  		     per-thread.
 *  	    linux:   linux_netmap_open (netmap_linux.c). The priv is
 *  		     per-open.
 *
 * > 2. on each descriptor, the process issues an ioctl() to identify
 * >    the interface that should report events to the file descriptor.
 *
 * 	Implemented by netmap_ioctl(), NIOCREGIF case, with nmr->nr_cmd==0.
 * 	Most important things happen in netmap_get_na() and
 * 	netmap_do_regif(), called from there. Additional details can be
 * 	found in the comments above those functions.
 *
 * 	In all cases, this action creates/takes-a-reference-to a
 * 	netmap_*_adapter describing the port, and allocates a netmap_if
 * 	and all necessary netmap rings, filling them with netmap buffers.
 *
 *      In this phase, the sync callbacks for each ring are set (these are used
 *      in steps 5 and 6 below).  The callbacks depend on the type of adapter.
 *      The adapter creation/initialization code puts them in the
 * 	netmap_adapter (fields na->nm_txsync and na->nm_rxsync).  Then, they
 * 	are copied from there to the netmap_kring's during netmap_do_regif(), by
 * 	the nm_krings_create() callback.  All the nm_krings_create callbacks
 * 	actually call netmap_krings_create() to perform this and the other
 * 	common stuff. netmap_krings_create() also takes care of the host rings,
 * 	if needed, by setting their sync callbacks appropriately.
 *
 * 	Additional actions depend on the kind of netmap_adapter that has been
 * 	registered:
 *
 * 	- netmap_hw_adapter:  	     [netmap.c]
 * 	     This is a system netdev/ifp with native netmap support.
 * 	     The ifp is detached from the host stack by redirecting:
 * 	       - transmissions (from the network stack) to netmap_transmit()
 * 	       - receive notifications to the nm_notify() callback for
 * 	         this adapter. The callback is normally netmap_notify(), unless
 * 	         the ifp is attached to a bridge using bwrap, in which case it
 * 	         is netmap_bwrap_intr_notify().
 *
 * 	- netmap_generic_adapter:      [netmap_generic.c]
 * 	      A system netdev/ifp without native netmap support.
 *
 * 	(the decision about native/non native support is taken in
 * 	 netmap_get_hw_na(), called by netmap_get_na())
 *
 * 	- netmap_vp_adapter 		[netmap_vale.c]
 * 	      Returned by netmap_get_bdg_na().
 * 	      This is a persistent or ephemeral VALE port. Ephemeral ports
 * 	      are created on the fly if they don't already exist, and are
 * 	      always attached to a bridge.
 * 	      Persistent VALE ports must must be created seperately, and i
 * 	      then attached like normal NICs. The NIOCREGIF we are examining
 * 	      will find them only if they had previosly been created and
 * 	      attached (see VALE_CTL below).
 *
 * 	- netmap_pipe_adapter 	      [netmap_pipe.c]
 * 	      Returned by netmap_get_pipe_na().
 * 	      Both pipe ends are created, if they didn't already exist.
 *
 * 	- netmap_monitor_adapter      [netmap_monitor.c]
 * 	      Returned by netmap_get_monitor_na().
 * 	      If successful, the nm_sync callbacks of the monitored adapter
 * 	      will be intercepted by the returned monitor.
 *
 * 	- netmap_bwrap_adapter	      [netmap_vale.c]
 * 	      Cannot be obtained in this way, see VALE_CTL below
 *
 *
 * 	os-specific:
 * 	    linux: we first go through linux_netmap_ioctl() to
 * 	           adapt the FreeBSD interface to the linux one.
 *
 *
 * > 3. on each descriptor, the process issues an mmap() request to
 * >    map the shared memory region within the process' address space.
 * >    The list of interesting queues is indicated by a location in
 * >    the shared memory region.
 *
 *      os-specific:
 *  	    FreeBSD: netmap_mmap_single (netmap_freebsd.c).
 *  	    linux:   linux_netmap_mmap (netmap_linux.c).
 *
 * > 4. using the functions in the netmap(4) userspace API, a process
 * >    can look up the occupation state of a queue, access memory buffers,
 * >    and retrieve received packets or enqueue packets to transmit.
 *
 * 	these actions do not involve the kernel.
 *
 * > 5. using some ioctl()s the process can synchronize the userspace view
 * >    of the queue with the actual status in the kernel. This includes both
 * >    receiving the notification of new packets, and transmitting new
 * >    packets on the output interface.
 *
 * 	These are implemented in netmap_ioctl(), NIOCTXSYNC and NIOCRXSYNC
 * 	cases. They invoke the nm_sync callbacks on the netmap_kring
 * 	structures, as initialized in step 2 and maybe later modified
 * 	by a monitor. Monitors, however, will always call the original
 * 	callback before doing anything else.
 *
 *
 * > 6. select() or poll() can be used to wait for events on individual
 * >    transmit or receive queues (or all queues for a given interface).
 *
 * 	Implemented in netmap_poll(). This will call the same nm_sync()
 * 	callbacks as in step 5 above.
 *
 * 	os-specific:
 * 		linux: we first go through linux_netmap_poll() to adapt
 * 		       the FreeBSD interface to the linux one.
 *
 *
 *  ----  VALE_CTL -----
 *
 *  VALE switches are controlled by issuing a NIOCREGIF with a non-null
 *  nr_cmd in the nmreq structure. These subcommands are handled by
 *  netmap_bdg_ctl() in netmap_vale.c. Persistent VALE ports are created
 *  and destroyed by issuing the NETMAP_BDG_NEWIF and NETMAP_BDG_DELIF
 *  subcommands, respectively.
 *
 *  Any network interface known to the system (including a persistent VALE
 *  port) can be attached to a VALE switch by issuing the
 *  NETMAP_BDG_ATTACH subcommand. After the attachment, persistent VALE ports
 *  look exactly like ephemeral VALE ports (as created in step 2 above).  The
 *  attachment of other interfaces, instead, requires the creation of a
 *  netmap_bwrap_adapter.  Moreover, the attached interface must be put in
 *  netmap mode. This may require the creation of a netmap_generic_adapter if
 *  we have no native support for the interface, or if generic adapters have
 *  been forced by sysctl.
 *
 *  Both persistent VALE ports and bwraps are handled by netmap_get_bdg_na(),
 *  called by nm_bdg_ctl_attach(), and discriminated by the nm_bdg_attach()
 *  callback.  In the case of the bwrap, the callback creates the
 *  netmap_bwrap_adapter.  The initialization of the bwrap is then
 *  completed by calling netmap_do_regif() on it, in the nm_bdg_ctl()
 *  callback (netmap_bwrap_bdg_ctl in netmap_vale.c).
 *  A generic adapter for the wrapped ifp will be created if needed, when
 *  netmap_get_bdg_na() calls netmap_get_hw_na().
 *
 *
 *  ---- DATAPATHS -----
 *
 *              -= SYSTEM DEVICE WITH NATIVE SUPPORT =-
 *
 *    na == NA(ifp) == netmap_hw_adapter created in DEVICE_netmap_attach()
 *
 *    - tx from netmap userspace:
 *	 concurrently:
 *           1) ioctl(NIOCTXSYNC)/netmap_poll() in process context
 *                kring->nm_sync() == DEVICE_netmap_txsync()
 *           2) device interrupt handler
 *                na->nm_notify()  == netmap_notify()
 *    - rx from netmap userspace:
 *       concurrently:
 *           1) ioctl(NIOCRXSYNC)/netmap_poll() in process context
 *                kring->nm_sync() == DEVICE_netmap_rxsync()
 *           2) device interrupt handler
 *                na->nm_notify()  == netmap_notify()
 *    - tx from host stack
 *       concurrently:
 *           1) host stack
 *                netmap_transmit()
 *                  na->nm_notify  == netmap_notify()
 *           2) ioctl(NIOCRXSYNC)/netmap_poll() in process context
 *                kring->nm_sync() == netmap_rxsync_from_host_compat
 *                  netmap_rxsync_from_host(na, NULL, NULL)
 *    - tx to host stack
 *           ioctl(NIOCTXSYNC)/netmap_poll() in process context
 *             kring->nm_sync() == netmap_txsync_to_host_compat
 *               netmap_txsync_to_host(na)
 *                 NM_SEND_UP()
 *                   FreeBSD: na->if_input() == ?? XXX
 *                   linux: netif_rx() with NM_MAGIC_PRIORITY_RX
 *
 *
 *
 *               -= SYSTEM DEVICE WITH GENERIC SUPPORT =-
 *
 *
 *
 *                           -= VALE PORT =-
 *
 *
 *
 *                           -= NETMAP PIPE =-
 *
 *
 *
 *  -= SYSTEM DEVICE WITH NATIVE SUPPORT, CONNECTED TO VALE, NO HOST RINGS =-
 *
 *
 *
 *  -= SYSTEM DEVICE WITH NATIVE SUPPORT, CONNECTED TO VALE, WITH HOST RINGS =-
 *
 *
 *
 *  -= SYSTEM DEVICE WITH GENERIC SUPPORT, CONNECTED TO VALE, NO HOST RINGS =-
 *
 *
 *
 *  -= SYSTEM DEVICE WITH GENERIC SUPPORT, CONNECTED TO VALE, WITH HOST RINGS =-
 *
 *
 *
 */

/*
 * OS-specific code that is used only within this file.
 * Other OS-specific code that must be accessed by drivers
 * is present in netmap_kern.h
 */

#if defined(__FreeBSD__)
#include <sys/cdefs.h> /* prerequisite */
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/param.h>	/* defines used in kernel.h */
#include <sys/kernel.h>	/* types used in module initialization */
#include <sys/conf.h>	/* cdevsw struct, UID, GID */
#include <sys/filio.h>	/* FIONBIO */
#include <sys/sockio.h>
#include <sys/socketvar.h>	/* struct socket */
#include <sys/malloc.h>
#include <sys/poll.h>
#include <sys/rwlock.h>
#include <sys/socket.h> /* sockaddrs */
#include <sys/selinfo.h>
#include <sys/sysctl.h>
#include <sys/jail.h>
#include <net/vnet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/bpf.h>		/* BIOCIMMEDIATE */
#include <machine/bus.h>	/* bus_dmamap_* */
#include <sys/endian.h>
#include <sys/refcount.h>


/* reduce conditional code */
// linux API, use for the knlist in FreeBSD
/* use a private mutex for the knlist */
#define init_waitqueue_head(x) do {			\
	struct mtx *m = &(x)->m;			\
	mtx_init(m, "nm_kn_lock", NULL, MTX_DEF);	\
	knlist_init_mtx(&(x)->si.si_note, m);		\
    } while (0)

#define OS_selrecord(a, b)	selrecord(a, &((b)->si))
#define OS_selwakeup(a, b)	freebsd_selwakeup(a, b)

#elif defined(linux)

#include "bsd_glue.h"



#elif defined(__APPLE__)

#warning OSX support is only partial
#include "osx_glue.h"

#else

#error	Unsupported platform

#endif /* unsupported */

/*
 * common headers
 */
#include <net/netmap.h>
#include <dev/netmap/netmap_kern.h>
#include <dev/netmap/netmap_mem2.h>


MALLOC_DEFINE(M_NETMAP, "netmap", "Network memory map");

/*
 * The following variables are used by the drivers and replicate
 * fields in the global memory pool. They only refer to buffers
 * used by physical interfaces.
 */
u_int netmap_total_buffers;
u_int netmap_buf_size;
char *netmap_buffer_base;	/* also address of an invalid buffer */

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

int netmap_adaptive_io = 0;
SYSCTL_INT(_dev_netmap, OID_AUTO, adaptive_io, CTLFLAG_RW,
    &netmap_adaptive_io, 0 , "Adaptive I/O on paravirt");

int netmap_flags = 0;	/* debug flags */
int netmap_fwd = 0;	/* force transparent mode */
int netmap_mmap_unreg = 0; /* allow mmap of unregistered fds */

/*
 * netmap_admode selects the netmap mode to use.
 * Invalid values are reset to NETMAP_ADMODE_BEST
 */
enum { NETMAP_ADMODE_BEST = 0,	/* use native, fallback to generic */
	NETMAP_ADMODE_NATIVE,	/* either native or none */
	NETMAP_ADMODE_GENERIC,	/* force generic */
	NETMAP_ADMODE_LAST };
static int netmap_admode = NETMAP_ADMODE_BEST;

int netmap_generic_mit = 100*1000;   /* Generic mitigation interval in nanoseconds. */
int netmap_generic_ringsize = 1024;   /* Generic ringsize. */
int netmap_generic_rings = 1;   /* number of queues in generic. */

SYSCTL_INT(_dev_netmap, OID_AUTO, flags, CTLFLAG_RW, &netmap_flags, 0 , "");
SYSCTL_INT(_dev_netmap, OID_AUTO, fwd, CTLFLAG_RW, &netmap_fwd, 0 , "");
SYSCTL_INT(_dev_netmap, OID_AUTO, mmap_unreg, CTLFLAG_RW, &netmap_mmap_unreg, 0, "");
SYSCTL_INT(_dev_netmap, OID_AUTO, admode, CTLFLAG_RW, &netmap_admode, 0 , "");
SYSCTL_INT(_dev_netmap, OID_AUTO, generic_mit, CTLFLAG_RW, &netmap_generic_mit, 0 , "");
SYSCTL_INT(_dev_netmap, OID_AUTO, generic_ringsize, CTLFLAG_RW, &netmap_generic_ringsize, 0 , "");
SYSCTL_INT(_dev_netmap, OID_AUTO, generic_rings, CTLFLAG_RW, &netmap_generic_rings, 0 , "");

NMG_LOCK_T	netmap_global_lock;


static void
nm_kr_get(struct netmap_kring *kr)
{
	while (NM_ATOMIC_TEST_AND_SET(&kr->nr_busy))
		tsleep(kr, 0, "NM_KR_GET", 4);
}


/*
 * mark the ring as stopped, and run through the locks
 * to make sure other users get to see it.
 */
static void
netmap_disable_ring(struct netmap_kring *kr)
{
	kr->nkr_stopped = 1;
	nm_kr_get(kr);
	mtx_lock(&kr->q_lock);
	mtx_unlock(&kr->q_lock);
	nm_kr_put(kr);
}

/* stop or enable a single tx ring */
void
netmap_set_txring(struct netmap_adapter *na, u_int ring_id, int stopped)
{
	if (stopped)
		netmap_disable_ring(na->tx_rings + ring_id);
	else
		na->tx_rings[ring_id].nkr_stopped = 0;
	/* nofify that the stopped state has changed. This is currently
	 *only used by bwrap to propagate the state to its own krings.
	 * (see netmap_bwrap_intr_notify).
	 */
	na->nm_notify(na, ring_id, NR_TX, NAF_DISABLE_NOTIFY);
}

/* stop or enable a single rx ring */
void
netmap_set_rxring(struct netmap_adapter *na, u_int ring_id, int stopped)
{
	if (stopped)
		netmap_disable_ring(na->rx_rings + ring_id);
	else
		na->rx_rings[ring_id].nkr_stopped = 0;
	/* nofify that the stopped state has changed. This is currently
	 *only used by bwrap to propagate the state to its own krings.
	 * (see netmap_bwrap_intr_notify).
	 */
	na->nm_notify(na, ring_id, NR_RX, NAF_DISABLE_NOTIFY);
}


/* stop or enable all the rings of na */
void
netmap_set_all_rings(struct netmap_adapter *na, int stopped)
{
	int i;
	u_int ntx, nrx;

	if (!nm_netmap_on(na))
		return;

	ntx = netmap_real_tx_rings(na);
	nrx = netmap_real_rx_rings(na);

	for (i = 0; i < ntx; i++) {
		netmap_set_txring(na, i, stopped);
	}

	for (i = 0; i < nrx; i++) {
		netmap_set_rxring(na, i, stopped);
	}
}

/*
 * Convenience function used in drivers.  Waits for current txsync()s/rxsync()s
 * to finish and prevents any new one from starting.  Call this before turning
 * netmap mode off, or before removing the harware rings (e.g., on module
 * onload).  As a rule of thumb for linux drivers, this should be placed near
 * each napi_disable().
 */
void
netmap_disable_all_rings(struct ifnet *ifp)
{
	netmap_set_all_rings(NA(ifp), 1 /* stopped */);
}

/*
 * Convenience function used in drivers.  Re-enables rxsync and txsync on the
 * adapter's rings In linux drivers, this should be placed near each
 * napi_enable().
 */
void
netmap_enable_all_rings(struct ifnet *ifp)
{
	netmap_set_all_rings(NA(ifp), 0 /* enabled */);
}


/*
 * generic bound_checking function
 */
u_int
nm_bound_var(u_int *v, u_int dflt, u_int lo, u_int hi, const char *msg)
{
	u_int oldv = *v;
	const char *op = NULL;

	if (dflt < lo)
		dflt = lo;
	if (dflt > hi)
		dflt = hi;
	if (oldv < lo) {
		*v = dflt;
		op = "Bump";
	} else if (oldv > hi) {
		*v = hi;
		op = "Clamp";
	}
	if (op && msg)
		printf("%s %s to %d (was %d)\n", op, msg, *v, oldv);
	return *v;
}


/*
 * packet-dump function, user-supplied or static buffer.
 * The destination buffer must be at least 30+4*len
 */
const char *
nm_dump_buf(char *p, int len, int lim, char *dst)
{
	static char _dst[8192];
	int i, j, i0;
	static char hex[] ="0123456789abcdef";
	char *o;	/* output position */

#define P_HI(x)	hex[((x) & 0xf0)>>4]
#define P_LO(x)	hex[((x) & 0xf)]
#define P_C(x)	((x) >= 0x20 && (x) <= 0x7e ? (x) : '.')
	if (!dst)
		dst = _dst;
	if (lim <= 0 || lim > len)
		lim = len;
	o = dst;
	sprintf(o, "buf 0x%p len %d lim %d\n", p, len, lim);
	o += strlen(o);
	/* hexdump routine */
	for (i = 0; i < lim; ) {
		sprintf(o, "%5d: ", i);
		o += strlen(o);
		memset(o, ' ', 48);
		i0 = i;
		for (j=0; j < 16 && i < lim; i++, j++) {
			o[j*3] = P_HI(p[i]);
			o[j*3+1] = P_LO(p[i]);
		}
		i = i0;
		for (j=0; j < 16 && i < lim; i++, j++)
			o[j + 48] = P_C(p[i]);
		o[j+48] = '\n';
		o += j+49;
	}
	*o = '\0';
#undef P_HI
#undef P_LO
#undef P_C
	return dst;
}


/*
 * Fetch configuration from the device, to cope with dynamic
 * reconfigurations after loading the module.
 */
/* call with NMG_LOCK held */
int
netmap_update_config(struct netmap_adapter *na)
{
	u_int txr, txd, rxr, rxd;

	txr = txd = rxr = rxd = 0;
	if (na->nm_config == NULL ||
	    na->nm_config(na, &txr, &txd, &rxr, &rxd)) {
		/* take whatever we had at init time */
		txr = na->num_tx_rings;
		txd = na->num_tx_desc;
		rxr = na->num_rx_rings;
		rxd = na->num_rx_desc;
	}

	if (na->num_tx_rings == txr && na->num_tx_desc == txd &&
	    na->num_rx_rings == rxr && na->num_rx_desc == rxd)
		return 0; /* nothing changed */
	if (netmap_verbose || na->active_fds > 0) {
		D("stored config %s: txring %d x %d, rxring %d x %d",
			na->name,
			na->num_tx_rings, na->num_tx_desc,
			na->num_rx_rings, na->num_rx_desc);
		D("new config %s: txring %d x %d, rxring %d x %d",
			na->name, txr, txd, rxr, rxd);
	}
	if (na->active_fds == 0) {
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

/* kring->nm_sync callback for the host tx ring */
static int
netmap_txsync_to_host_compat(struct netmap_kring *kring, int flags)
{
	(void)flags; /* unused */
	netmap_txsync_to_host(kring->na);
	return 0;
}

/* kring->nm_sync callback for the host rx ring */
static int
netmap_rxsync_from_host_compat(struct netmap_kring *kring, int flags)
{
	(void)flags; /* unused */
	netmap_rxsync_from_host(kring->na, NULL, NULL);
	return 0;
}



/* create the krings array and initialize the fields common to all adapters.
 * The array layout is this:
 *
 *                    +----------+
 * na->tx_rings ----->|          | \
 *                    |          |  } na->num_tx_ring
 *                    |          | /
 *                    +----------+
 *                    |          |    host tx kring
 * na->rx_rings ----> +----------+
 *                    |          | \
 *                    |          |  } na->num_rx_rings
 *                    |          | /
 *                    +----------+
 *                    |          |    host rx kring
 *                    +----------+
 * na->tailroom ----->|          | \
 *                    |          |  } tailroom bytes
 *                    |          | /
 *                    +----------+
 *
 * Note: for compatibility, host krings are created even when not needed.
 * The tailroom space is currently used by vale ports for allocating leases.
 */
/* call with NMG_LOCK held */
int
netmap_krings_create(struct netmap_adapter *na, u_int tailroom)
{
	u_int i, len, ndesc;
	struct netmap_kring *kring;
	u_int ntx, nrx;

	/* account for the (possibly fake) host rings */
	ntx = na->num_tx_rings + 1;
	nrx = na->num_rx_rings + 1;

	len = (ntx + nrx) * sizeof(struct netmap_kring) + tailroom;

	na->tx_rings = malloc((size_t)len, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (na->tx_rings == NULL) {
		D("Cannot allocate krings");
		return ENOMEM;
	}
	na->rx_rings = na->tx_rings + ntx;

	/*
	 * All fields in krings are 0 except the one initialized below.
	 * but better be explicit on important kring fields.
	 */
	ndesc = na->num_tx_desc;
	for (i = 0; i < ntx; i++) { /* Transmit rings */
		kring = &na->tx_rings[i];
		bzero(kring, sizeof(*kring));
		kring->na = na;
		kring->ring_id = i;
		kring->nkr_num_slots = ndesc;
		if (i < na->num_tx_rings) {
			kring->nm_sync = na->nm_txsync;
		} else if (i == na->num_tx_rings) {
			kring->nm_sync = netmap_txsync_to_host_compat;
		}
		/*
		 * IMPORTANT: Always keep one slot empty.
		 */
		kring->rhead = kring->rcur = kring->nr_hwcur = 0;
		kring->rtail = kring->nr_hwtail = ndesc - 1;
		snprintf(kring->name, sizeof(kring->name) - 1, "%s TX%d", na->name, i);
		ND("ktx %s h %d c %d t %d",
			kring->name, kring->rhead, kring->rcur, kring->rtail);
		mtx_init(&kring->q_lock, "nm_txq_lock", NULL, MTX_DEF);
		init_waitqueue_head(&kring->si);
	}

	ndesc = na->num_rx_desc;
	for (i = 0; i < nrx; i++) { /* Receive rings */
		kring = &na->rx_rings[i];
		bzero(kring, sizeof(*kring));
		kring->na = na;
		kring->ring_id = i;
		kring->nkr_num_slots = ndesc;
		if (i < na->num_rx_rings) {
			kring->nm_sync = na->nm_rxsync;
		} else if (i == na->num_rx_rings) {
			kring->nm_sync = netmap_rxsync_from_host_compat;
		}
		kring->rhead = kring->rcur = kring->nr_hwcur = 0;
		kring->rtail = kring->nr_hwtail = 0;
		snprintf(kring->name, sizeof(kring->name) - 1, "%s RX%d", na->name, i);
		ND("krx %s h %d c %d t %d",
			kring->name, kring->rhead, kring->rcur, kring->rtail);
		mtx_init(&kring->q_lock, "nm_rxq_lock", NULL, MTX_DEF);
		init_waitqueue_head(&kring->si);
	}
	init_waitqueue_head(&na->tx_si);
	init_waitqueue_head(&na->rx_si);

	na->tailroom = na->rx_rings + nrx;

	return 0;
}


#ifdef __FreeBSD__
static void
netmap_knlist_destroy(NM_SELINFO_T *si)
{
	/* XXX kqueue(9) needed; these will mirror knlist_init. */
	knlist_delete(&si->si.si_note, curthread, 0 /* not locked */ );
	knlist_destroy(&si->si.si_note);
	/* now we don't need the mutex anymore */
	mtx_destroy(&si->m);
}
#endif /* __FreeBSD__ */


/* undo the actions performed by netmap_krings_create */
/* call with NMG_LOCK held */
void
netmap_krings_delete(struct netmap_adapter *na)
{
	struct netmap_kring *kring = na->tx_rings;

	/* we rely on the krings layout described above */
	for ( ; kring != na->tailroom; kring++) {
		mtx_destroy(&kring->q_lock);
		netmap_knlist_destroy(&kring->si);
	}
	free(na->tx_rings, M_DEVBUF);
	na->tx_rings = na->rx_rings = na->tailroom = NULL;
}


/*
 * Destructor for NIC ports. They also have an mbuf queue
 * on the rings connected to the host so we need to purge
 * them first.
 */
/* call with NMG_LOCK held */
static void
netmap_hw_krings_delete(struct netmap_adapter *na)
{
	struct mbq *q = &na->rx_rings[na->num_rx_rings].rx_queue;

	ND("destroy sw mbq with len %d", mbq_len(q));
	mbq_purge(q);
	mbq_safe_destroy(q);
	netmap_krings_delete(na);
}


/* create a new netmap_if for a newly registered fd.
 * If this is the first registration of the adapter,
 * also create the netmap rings and their in-kernel view,
 * the netmap krings.
 */
/* call with NMG_LOCK held */
static struct netmap_if*
netmap_if_new(struct netmap_adapter *na)
{
	struct netmap_if *nifp;

	if (netmap_update_config(na)) {
		/* configuration mismatch, report and fail */
		return NULL;
	}

	if (na->active_fds)	/* already registered */
		goto final;

	/* create and init the krings arrays.
	 * Depending on the adapter, this may also create
	 * the netmap rings themselves
	 */
	if (na->nm_krings_create(na))
		return NULL;

	/* create all missing netmap rings */
	if (netmap_mem_rings_create(na))
		goto cleanup;

final:

	/* in all cases, create a new netmap if */
	nifp = netmap_mem_if_new(na);
	if (nifp == NULL)
		goto cleanup;

	return (nifp);

cleanup:

	if (na->active_fds == 0) {
		netmap_mem_rings_delete(na);
		na->nm_krings_delete(na);
	}

	return NULL;
}


/* grab a reference to the memory allocator, if we don't have one already.  The
 * reference is taken from the netmap_adapter registered with the priv.
 */
/* call with NMG_LOCK held */
static int
netmap_get_memory_locked(struct netmap_priv_d* p)
{
	struct netmap_mem_d *nmd;
	int error = 0;

	if (p->np_na == NULL) {
		if (!netmap_mmap_unreg)
			return ENODEV;
		/* for compatibility with older versions of the API
 		 * we use the global allocator when no interface has been
 		 * registered
 		 */
		nmd = &nm_mem;
	} else {
		nmd = p->np_na->nm_mem;
	}
	if (p->np_mref == NULL) {
		error = netmap_mem_finalize(nmd, p->np_na);
		if (!error)
			p->np_mref = nmd;
	} else if (p->np_mref != nmd) {
		/* a virtual port has been registered, but previous
 		 * syscalls already used the global allocator.
 		 * We cannot continue
 		 */
		error = ENODEV;
	}
	return error;
}


/* call with NMG_LOCK *not* held */
int
netmap_get_memory(struct netmap_priv_d* p)
{
	int error;
	NMG_LOCK();
	error = netmap_get_memory_locked(p);
	NMG_UNLOCK();
	return error;
}


/* call with NMG_LOCK held */
static int
netmap_have_memory_locked(struct netmap_priv_d* p)
{
	return p->np_mref != NULL;
}


/* call with NMG_LOCK held */
static void
netmap_drop_memory_locked(struct netmap_priv_d* p)
{
	if (p->np_mref) {
		netmap_mem_deref(p->np_mref, p->np_na);
		p->np_mref = NULL;
	}
}


/*
 * Call nm_register(ifp,0) to stop netmap mode on the interface and
 * revert to normal operation.
 * The second argument is the nifp to work on. In some cases it is
 * not attached yet to the netmap_priv_d so we need to pass it as
 * a separate argument.
 */
/* call with NMG_LOCK held */
static void
netmap_do_unregif(struct netmap_priv_d *priv, struct netmap_if *nifp)
{
	struct netmap_adapter *na = priv->np_na;

	NMG_LOCK_ASSERT();
	na->active_fds--;
	if (na->active_fds <= 0) {	/* last instance */

		if (netmap_verbose)
			D("deleting last instance for %s", na->name);
		/*
		 * (TO CHECK) This function is only called
		 * when the last reference to this file descriptor goes
		 * away. This means we cannot have any pending poll()
		 * or interrupt routine operating on the structure.
		 * XXX The file may be closed in a thread while
		 * another thread is using it.
		 * Linux keeps the file opened until the last reference
		 * by any outstanding ioctl/poll or mmap is gone.
		 * FreeBSD does not track mmap()s (but we do) and
		 * wakes up any sleeping poll(). Need to check what
		 * happens if the close() occurs while a concurrent
		 * syscall is running.
		 */
		na->nm_register(na, 0); /* off, clear flags */
		/* Wake up any sleeping threads. netmap_poll will
		 * then return POLLERR
		 * XXX The wake up now must happen during *_down(), when
		 * we order all activities to stop. -gl
		 */
		netmap_knlist_destroy(&na->tx_si);
		netmap_knlist_destroy(&na->rx_si);

		/* delete rings and buffers */
		netmap_mem_rings_delete(na);
		na->nm_krings_delete(na);
	}
	/* delete the nifp */
	netmap_mem_if_delete(na, nifp);
}

/* call with NMG_LOCK held */
static __inline int
nm_tx_si_user(struct netmap_priv_d *priv)
{
	return (priv->np_na != NULL &&
		(priv->np_txqlast - priv->np_txqfirst > 1));
}

/* call with NMG_LOCK held */
static __inline int
nm_rx_si_user(struct netmap_priv_d *priv)
{
	return (priv->np_na != NULL &&
		(priv->np_rxqlast - priv->np_rxqfirst > 1));
}


/*
 * Destructor of the netmap_priv_d, called when the fd has
 * no active open() and mmap(). Also called in error paths.
 *
 * returns 1 if this is the last instance and we can free priv
 */
/* call with NMG_LOCK held */
int
netmap_dtor_locked(struct netmap_priv_d *priv)
{
	struct netmap_adapter *na = priv->np_na;

#ifdef __FreeBSD__
	/*
	 * np_refcount is the number of active mmaps on
	 * this file descriptor
	 */
	if (--priv->np_refcount > 0) {
		return 0;
	}
#endif /* __FreeBSD__ */
	if (!na) {
	    return 1; //XXX is it correct?
	}
	netmap_do_unregif(priv, priv->np_nifp);
	priv->np_nifp = NULL;
	netmap_drop_memory_locked(priv);
	if (priv->np_na) {
		if (nm_tx_si_user(priv))
			na->tx_si_users--;
		if (nm_rx_si_user(priv))
			na->rx_si_users--;
		netmap_adapter_put(na);
		priv->np_na = NULL;
	}
	return 1;
}


/* call with NMG_LOCK *not* held */
void
netmap_dtor(void *data)
{
	struct netmap_priv_d *priv = data;
	int last_instance;

	NMG_LOCK();
	last_instance = netmap_dtor_locked(priv);
	NMG_UNLOCK();
	if (last_instance) {
		bzero(priv, sizeof(*priv));	/* for safety */
		free(priv, M_DEVBUF);
	}
}




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
 * We do not need to lock because the queue is private.
 */
static void
netmap_send_up(struct ifnet *dst, struct mbq *q)
{
	struct mbuf *m;

	/* send packets up, outside the lock */
	while ((m = mbq_dequeue(q)) != NULL) {
		if (netmap_verbose & NM_VERB_HOST)
			D("sending up pkt %p size %d", m, MBUF_LEN(m));
		NM_SEND_UP(dst, m);
	}
	mbq_destroy(q);
}


/*
 * put a copy of the buffers marked NS_FORWARD into an mbuf chain.
 * Take packets from hwcur to ring->head marked NS_FORWARD (or forced)
 * and pass them up. Drop remaining packets in the unlikely event
 * of an mbuf shortage.
 */
static void
netmap_grab_packets(struct netmap_kring *kring, struct mbq *q, int force)
{
	u_int const lim = kring->nkr_num_slots - 1;
	u_int const head = kring->ring->head;
	u_int n;
	struct netmap_adapter *na = kring->na;

	for (n = kring->nr_hwcur; n != head; n = nm_next(n, lim)) {
		struct mbuf *m;
		struct netmap_slot *slot = &kring->ring->slot[n];

		if ((slot->flags & NS_FORWARD) == 0 && !force)
			continue;
		if (slot->len < 14 || slot->len > NETMAP_BUF_SIZE(na)) {
			RD(5, "bad pkt at %d len %d", n, slot->len);
			continue;
		}
		slot->flags &= ~NS_FORWARD; // XXX needed ?
		/* XXX TODO: adapt to the case of a multisegment packet */
		m = m_devget(NMB(na, slot), slot->len, 0, na->ifp, NULL);

		if (m == NULL)
			break;
		mbq_enqueue(q, m);
	}
}


/*
 * Send to the NIC rings packets marked NS_FORWARD between
 * kring->nr_hwcur and kring->rhead
 * Called under kring->rx_queue.lock on the sw rx ring,
 */
static u_int
netmap_sw_to_nic(struct netmap_adapter *na)
{
	struct netmap_kring *kring = &na->rx_rings[na->num_rx_rings];
	struct netmap_slot *rxslot = kring->ring->slot;
	u_int i, rxcur = kring->nr_hwcur;
	u_int const head = kring->rhead;
	u_int const src_lim = kring->nkr_num_slots - 1;
	u_int sent = 0;

	/* scan rings to find space, then fill as much as possible */
	for (i = 0; i < na->num_tx_rings; i++) {
		struct netmap_kring *kdst = &na->tx_rings[i];
		struct netmap_ring *rdst = kdst->ring;
		u_int const dst_lim = kdst->nkr_num_slots - 1;

		/* XXX do we trust ring or kring->rcur,rtail ? */
		for (; rxcur != head && !nm_ring_empty(rdst);
		     rxcur = nm_next(rxcur, src_lim) ) {
			struct netmap_slot *src, *dst, tmp;
			u_int dst_cur = rdst->cur;

			src = &rxslot[rxcur];
			if ((src->flags & NS_FORWARD) == 0 && !netmap_fwd)
				continue;

			sent++;

			dst = &rdst->slot[dst_cur];

			tmp = *src;

			src->buf_idx = dst->buf_idx;
			src->flags = NS_BUF_CHANGED;

			dst->buf_idx = tmp.buf_idx;
			dst->len = tmp.len;
			dst->flags = NS_BUF_CHANGED;

			rdst->cur = nm_next(dst_cur, dst_lim);
		}
		/* if (sent) XXX txsync ? */
	}
	return sent;
}


/*
 * netmap_txsync_to_host() passes packets up. We are called from a
 * system call in user process context, and the only contention
 * can be among multiple user threads erroneously calling
 * this routine concurrently.
 */
void
netmap_txsync_to_host(struct netmap_adapter *na)
{
	struct netmap_kring *kring = &na->tx_rings[na->num_tx_rings];
	struct netmap_ring *ring = kring->ring;
	u_int const lim = kring->nkr_num_slots - 1;
	u_int const head = kring->rhead;
	struct mbq q;

	/* Take packets from hwcur to head and pass them up.
	 * force head = cur since netmap_grab_packets() stops at head
	 * In case of no buffers we give up. At the end of the loop,
	 * the queue is drained in all cases.
	 */
	mbq_init(&q);
	ring->cur = head;
	netmap_grab_packets(kring, &q, 1 /* force */);
	ND("have %d pkts in queue", mbq_len(&q));
	kring->nr_hwcur = head;
	kring->nr_hwtail = head + lim;
	if (kring->nr_hwtail > lim)
		kring->nr_hwtail -= lim + 1;
	nm_txsync_finalize(kring);

	netmap_send_up(na->ifp, &q);
}


/*
 * rxsync backend for packets coming from the host stack.
 * They have been put in kring->rx_queue by netmap_transmit().
 * We protect access to the kring using kring->rx_queue.lock
 *
 * This routine also does the selrecord if called from the poll handler
 * (we know because td != NULL).
 *
 * NOTE: on linux, selrecord() is defined as a macro and uses pwait
 *     as an additional hidden argument.
 * returns the number of packets delivered to tx queues in
 * transparent mode, or a negative value if error
 */
int
netmap_rxsync_from_host(struct netmap_adapter *na, struct thread *td, void *pwait)
{
	struct netmap_kring *kring = &na->rx_rings[na->num_rx_rings];
	struct netmap_ring *ring = kring->ring;
	u_int nm_i, n;
	u_int const lim = kring->nkr_num_slots - 1;
	u_int const head = kring->rhead;
	int ret = 0;
	struct mbq *q = &kring->rx_queue;

	(void)pwait;	/* disable unused warnings */
	(void)td;

	mbq_lock(q);

	/* First part: import newly received packets */
	n = mbq_len(q);
	if (n) { /* grab packets from the queue */
		struct mbuf *m;
		uint32_t stop_i;

		nm_i = kring->nr_hwtail;
		stop_i = nm_prev(nm_i, lim);
		while ( nm_i != stop_i && (m = mbq_dequeue(q)) != NULL ) {
			int len = MBUF_LEN(m);
			struct netmap_slot *slot = &ring->slot[nm_i];

			m_copydata(m, 0, len, NMB(na, slot));
			ND("nm %d len %d", nm_i, len);
			if (netmap_verbose)
                                D("%s", nm_dump_buf(NMB(na, slot),len, 128, NULL));

			slot->len = len;
			slot->flags = kring->nkr_slot_flags;
			nm_i = nm_next(nm_i, lim);
			m_freem(m);
		}
		kring->nr_hwtail = nm_i;
	}

	/*
	 * Second part: skip past packets that userspace has released.
	 */
	nm_i = kring->nr_hwcur;
	if (nm_i != head) { /* something was released */
		if (netmap_fwd || kring->ring->flags & NR_FORWARD)
			ret = netmap_sw_to_nic(na);
		kring->nr_hwcur = head;
	}

	nm_rxsync_finalize(kring);

	/* access copies of cur,tail in the kring */
	if (kring->rcur == kring->rtail && td) /* no bufs available */
		OS_selrecord(td, &kring->si);

	mbq_unlock(q);
	return ret;
}


/* Get a netmap adapter for the port.
 *
 * If it is possible to satisfy the request, return 0
 * with *na containing the netmap adapter found.
 * Otherwise return an error code, with *na containing NULL.
 *
 * When the port is attached to a bridge, we always return
 * EBUSY.
 * Otherwise, if the port is already bound to a file descriptor,
 * then we unconditionally return the existing adapter into *na.
 * In all the other cases, we return (into *na) either native,
 * generic or NULL, according to the following table:
 *
 *					native_support
 * active_fds   dev.netmap.admode         YES     NO
 * -------------------------------------------------------
 *    >0              *                 NA(ifp) NA(ifp)
 *
 *     0        NETMAP_ADMODE_BEST      NATIVE  GENERIC
 *     0        NETMAP_ADMODE_NATIVE    NATIVE   NULL
 *     0        NETMAP_ADMODE_GENERIC   GENERIC GENERIC
 *
 */

int
netmap_get_hw_na(struct ifnet *ifp, struct netmap_adapter **na)
{
	/* generic support */
	int i = netmap_admode;	/* Take a snapshot. */
	int error = 0;
	struct netmap_adapter *prev_na;
	struct netmap_generic_adapter *gna;

	*na = NULL; /* default */

	/* reset in case of invalid value */
	if (i < NETMAP_ADMODE_BEST || i >= NETMAP_ADMODE_LAST)
		i = netmap_admode = NETMAP_ADMODE_BEST;

	if (NETMAP_CAPABLE(ifp)) {
		prev_na = NA(ifp);
		/* If an adapter already exists, return it if
		 * there are active file descriptors or if
		 * netmap is not forced to use generic
		 * adapters.
		 */
		if (NETMAP_OWNED_BY_ANY(prev_na)
			|| i != NETMAP_ADMODE_GENERIC
			|| prev_na->na_flags & NAF_FORCE_NATIVE
#ifdef WITH_PIPES
			/* ugly, but we cannot allow an adapter switch
			 * if some pipe is referring to this one
			 */
			|| prev_na->na_next_pipe > 0
#endif
		) {
			*na = prev_na;
			return 0;
		}
	}

	/* If there isn't native support and netmap is not allowed
	 * to use generic adapters, we cannot satisfy the request.
	 */
	if (!NETMAP_CAPABLE(ifp) && i == NETMAP_ADMODE_NATIVE)
		return EOPNOTSUPP;

	/* Otherwise, create a generic adapter and return it,
	 * saving the previously used netmap adapter, if any.
	 *
	 * Note that here 'prev_na', if not NULL, MUST be a
	 * native adapter, and CANNOT be a generic one. This is
	 * true because generic adapters are created on demand, and
	 * destroyed when not used anymore. Therefore, if the adapter
	 * currently attached to an interface 'ifp' is generic, it
	 * must be that
	 * (NA(ifp)->active_fds > 0 || NETMAP_OWNED_BY_KERN(NA(ifp))).
	 * Consequently, if NA(ifp) is generic, we will enter one of
	 * the branches above. This ensures that we never override
	 * a generic adapter with another generic adapter.
	 */
	prev_na = NA(ifp);
	error = generic_netmap_attach(ifp);
	if (error)
		return error;

	*na = NA(ifp);
	gna = (struct netmap_generic_adapter*)NA(ifp);
	gna->prev = prev_na; /* save old na */
	if (prev_na != NULL) {
		ifunit_ref(ifp->if_xname);
		// XXX add a refcount ?
		netmap_adapter_get(prev_na);
	}
	ND("Created generic NA %p (prev %p)", gna, gna->prev);

	return 0;
}


/*
 * MUST BE CALLED UNDER NMG_LOCK()
 *
 * Get a refcounted reference to a netmap adapter attached
 * to the interface specified by nmr.
 * This is always called in the execution of an ioctl().
 *
 * Return ENXIO if the interface specified by the request does
 * not exist, ENOTSUP if netmap is not supported by the interface,
 * EBUSY if the interface is already attached to a bridge,
 * EINVAL if parameters are invalid, ENOMEM if needed resources
 * could not be allocated.
 * If successful, hold a reference to the netmap adapter.
 *
 * No reference is kept on the real interface, which may then
 * disappear at any time.
 */
int
netmap_get_na(struct nmreq *nmr, struct netmap_adapter **na, int create)
{
	struct ifnet *ifp = NULL;
	int error = 0;
	struct netmap_adapter *ret = NULL;

	*na = NULL;     /* default return value */

	NMG_LOCK_ASSERT();

	/* we cascade through all possibile types of netmap adapter.
	 * All netmap_get_*_na() functions return an error and an na,
	 * with the following combinations:
	 *
	 * error    na
	 *   0	   NULL		type doesn't match
	 *  !0	   NULL		type matches, but na creation/lookup failed
	 *   0	  !NULL		type matches and na created/found
	 *  !0    !NULL		impossible
	 */

	/* try to see if this is a monitor port */
	error = netmap_get_monitor_na(nmr, na, create);
	if (error || *na != NULL)
		return error;

	/* try to see if this is a pipe port */
	error = netmap_get_pipe_na(nmr, na, create);
	if (error || *na != NULL)
		return error;

	/* try to see if this is a bridge port */
	error = netmap_get_bdg_na(nmr, na, create);
	if (error)
		return error;

	if (*na != NULL) /* valid match in netmap_get_bdg_na() */
		goto pipes;

	/*
	 * This must be a hardware na, lookup the name in the system.
	 * Note that by hardware we actually mean "it shows up in ifconfig".
	 * This may still be a tap, a veth/epair, or even a
	 * persistent VALE port.
	 */
	ifp = ifunit_ref(nmr->nr_name);
	if (ifp == NULL) {
	        return ENXIO;
	}

	error = netmap_get_hw_na(ifp, &ret);
	if (error)
		goto out;

	*na = ret;
	netmap_adapter_get(ret);

pipes:
	/*
	 * If we are opening a pipe whose parent was not in netmap mode,
	 * we have to allocate the pipe array now.
	 * XXX get rid of this clumsiness (2014-03-15)
	 */
	error = netmap_pipe_alloc(*na, nmr);

out:
	if (error && ret != NULL)
		netmap_adapter_put(ret);

	if (ifp)
		if_rele(ifp); /* allow live unloading of drivers modules */

	return error;
}


/*
 * validate parameters on entry for *_txsync()
 * Returns ring->cur if ok, or something >= kring->nkr_num_slots
 * in case of error.
 *
 * rhead, rcur and rtail=hwtail are stored from previous round.
 * hwcur is the next packet to send to the ring.
 *
 * We want
 *    hwcur <= *rhead <= head <= cur <= tail = *rtail <= hwtail
 *
 * hwcur, rhead, rtail and hwtail are reliable
 */
u_int
nm_txsync_prologue(struct netmap_kring *kring)
{
	struct netmap_ring *ring = kring->ring;
	u_int head = ring->head; /* read only once */
	u_int cur = ring->cur; /* read only once */
	u_int n = kring->nkr_num_slots;

	ND(5, "%s kcur %d ktail %d head %d cur %d tail %d",
		kring->name,
		kring->nr_hwcur, kring->nr_hwtail,
		ring->head, ring->cur, ring->tail);
#if 1 /* kernel sanity checks; but we can trust the kring. */
	if (kring->nr_hwcur >= n || kring->rhead >= n ||
	    kring->rtail >= n ||  kring->nr_hwtail >= n)
		goto error;
#endif /* kernel sanity checks */
	/*
	 * user sanity checks. We only use 'cur',
	 * A, B, ... are possible positions for cur:
	 *
	 *  0    A  cur   B  tail  C  n-1
	 *  0    D  tail  E  cur   F  n-1
	 *
	 * B, F, D are valid. A, C, E are wrong
	 */
	if (kring->rtail >= kring->rhead) {
		/* want rhead <= head <= rtail */
		if (head < kring->rhead || head > kring->rtail)
			goto error;
		/* and also head <= cur <= rtail */
		if (cur < head || cur > kring->rtail)
			goto error;
	} else { /* here rtail < rhead */
		/* we need head outside rtail .. rhead */
		if (head > kring->rtail && head < kring->rhead)
			goto error;

		/* two cases now: head <= rtail or head >= rhead  */
		if (head <= kring->rtail) {
			/* want head <= cur <= rtail */
			if (cur < head || cur > kring->rtail)
				goto error;
		} else { /* head >= rhead */
			/* cur must be outside rtail..head */
			if (cur > kring->rtail && cur < head)
				goto error;
		}
	}
	if (ring->tail != kring->rtail) {
		RD(5, "tail overwritten was %d need %d",
			ring->tail, kring->rtail);
		ring->tail = kring->rtail;
	}
	kring->rhead = head;
	kring->rcur = cur;
	return head;

error:
	RD(5, "%s kring error: hwcur %d rcur %d hwtail %d cur %d tail %d",
		kring->name,
		kring->nr_hwcur,
		kring->rcur, kring->nr_hwtail,
		cur, ring->tail);
	return n;
}


/*
 * validate parameters on entry for *_rxsync()
 * Returns ring->head if ok, kring->nkr_num_slots on error.
 *
 * For a valid configuration,
 * hwcur <= head <= cur <= tail <= hwtail
 *
 * We only consider head and cur.
 * hwcur and hwtail are reliable.
 *
 */
u_int
nm_rxsync_prologue(struct netmap_kring *kring)
{
	struct netmap_ring *ring = kring->ring;
	uint32_t const n = kring->nkr_num_slots;
	uint32_t head, cur;

	ND("%s kc %d kt %d h %d c %d t %d",
		kring->name,
		kring->nr_hwcur, kring->nr_hwtail,
		ring->head, ring->cur, ring->tail);
	/*
	 * Before storing the new values, we should check they do not
	 * move backwards. However:
	 * - head is not an issue because the previous value is hwcur;
	 * - cur could in principle go back, however it does not matter
	 *   because we are processing a brand new rxsync()
	 */
	cur = kring->rcur = ring->cur;	/* read only once */
	head = kring->rhead = ring->head;	/* read only once */
#if 1 /* kernel sanity checks */
	if (kring->nr_hwcur >= n || kring->nr_hwtail >= n)
		goto error;
#endif /* kernel sanity checks */
	/* user sanity checks */
	if (kring->nr_hwtail >= kring->nr_hwcur) {
		/* want hwcur <= rhead <= hwtail */
		if (head < kring->nr_hwcur || head > kring->nr_hwtail)
			goto error;
		/* and also rhead <= rcur <= hwtail */
		if (cur < head || cur > kring->nr_hwtail)
			goto error;
	} else {
		/* we need rhead outside hwtail..hwcur */
		if (head < kring->nr_hwcur && head > kring->nr_hwtail)
			goto error;
		/* two cases now: head <= hwtail or head >= hwcur  */
		if (head <= kring->nr_hwtail) {
			/* want head <= cur <= hwtail */
			if (cur < head || cur > kring->nr_hwtail)
				goto error;
		} else {
			/* cur must be outside hwtail..head */
			if (cur < head && cur > kring->nr_hwtail)
				goto error;
		}
	}
	if (ring->tail != kring->rtail) {
		RD(5, "%s tail overwritten was %d need %d",
			kring->name,
			ring->tail, kring->rtail);
		ring->tail = kring->rtail;
	}
	return head;

error:
	RD(5, "kring error: hwcur %d rcur %d hwtail %d head %d cur %d tail %d",
		kring->nr_hwcur,
		kring->rcur, kring->nr_hwtail,
		kring->rhead, kring->rcur, ring->tail);
	return n;
}


/*
 * Error routine called when txsync/rxsync detects an error.
 * Can't do much more than resetting head =cur = hwcur, tail = hwtail
 * Return 1 on reinit.
 *
 * This routine is only called by the upper half of the kernel.
 * It only reads hwcur (which is changed only by the upper half, too)
 * and hwtail (which may be changed by the lower half, but only on
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

	// XXX KASSERT nm_kr_tryget
	RD(10, "called for %s", kring->name);
	// XXX probably wrong to trust userspace
	kring->rhead = ring->head;
	kring->rcur  = ring->cur;
	kring->rtail = ring->tail;

	if (ring->cur > lim)
		errors++;
	if (ring->head > lim)
		errors++;
	if (ring->tail > lim)
		errors++;
	for (i = 0; i <= lim; i++) {
		u_int idx = ring->slot[i].buf_idx;
		u_int len = ring->slot[i].len;
		if (idx < 2 || idx >= netmap_total_buffers) {
			RD(5, "bad index at slot %d idx %d len %d ", i, idx, len);
			ring->slot[i].buf_idx = 0;
			ring->slot[i].len = 0;
		} else if (len > NETMAP_BUF_SIZE(kring->na)) {
			ring->slot[i].len = 0;
			RD(5, "bad len at slot %d idx %d len %d", i, idx, len);
		}
	}
	if (errors) {
		RD(10, "total %d errors", errors);
		RD(10, "%s reinit, cur %d -> %d tail %d -> %d",
			kring->name,
			ring->cur, kring->nr_hwcur,
			ring->tail, kring->nr_hwtail);
		ring->head = kring->rhead = kring->nr_hwcur;
		ring->cur  = kring->rcur  = kring->nr_hwcur;
		ring->tail = kring->rtail = kring->nr_hwtail;
	}
	return (errors ? 1 : 0);
}

/* interpret the ringid and flags fields of an nmreq, by translating them
 * into a pair of intervals of ring indices:
 *
 * [priv->np_txqfirst, priv->np_txqlast) and
 * [priv->np_rxqfirst, priv->np_rxqlast)
 *
 */
int
netmap_interp_ringid(struct netmap_priv_d *priv, uint16_t ringid, uint32_t flags)
{
	struct netmap_adapter *na = priv->np_na;
	u_int j, i = ringid & NETMAP_RING_MASK;
	u_int reg = flags & NR_REG_MASK;

	if (reg == NR_REG_DEFAULT) {
		/* convert from old ringid to flags */
		if (ringid & NETMAP_SW_RING) {
			reg = NR_REG_SW;
		} else if (ringid & NETMAP_HW_RING) {
			reg = NR_REG_ONE_NIC;
		} else {
			reg = NR_REG_ALL_NIC;
		}
		D("deprecated API, old ringid 0x%x -> ringid %x reg %d", ringid, i, reg);
	}
	switch (reg) {
	case NR_REG_ALL_NIC:
	case NR_REG_PIPE_MASTER:
	case NR_REG_PIPE_SLAVE:
		priv->np_txqfirst = 0;
		priv->np_txqlast = na->num_tx_rings;
		priv->np_rxqfirst = 0;
		priv->np_rxqlast = na->num_rx_rings;
		ND("%s %d %d", "ALL/PIPE",
			priv->np_rxqfirst, priv->np_rxqlast);
		break;
	case NR_REG_SW:
	case NR_REG_NIC_SW:
		if (!(na->na_flags & NAF_HOST_RINGS)) {
			D("host rings not supported");
			return EINVAL;
		}
		priv->np_txqfirst = (reg == NR_REG_SW ?
			na->num_tx_rings : 0);
		priv->np_txqlast = na->num_tx_rings + 1;
		priv->np_rxqfirst = (reg == NR_REG_SW ?
			na->num_rx_rings : 0);
		priv->np_rxqlast = na->num_rx_rings + 1;
		ND("%s %d %d", reg == NR_REG_SW ? "SW" : "NIC+SW",
			priv->np_rxqfirst, priv->np_rxqlast);
		break;
	case NR_REG_ONE_NIC:
		if (i >= na->num_tx_rings && i >= na->num_rx_rings) {
			D("invalid ring id %d", i);
			return EINVAL;
		}
		/* if not enough rings, use the first one */
		j = i;
		if (j >= na->num_tx_rings)
			j = 0;
		priv->np_txqfirst = j;
		priv->np_txqlast = j + 1;
		j = i;
		if (j >= na->num_rx_rings)
			j = 0;
		priv->np_rxqfirst = j;
		priv->np_rxqlast = j + 1;
		break;
	default:
		D("invalid regif type %d", reg);
		return EINVAL;
	}
	priv->np_flags = (flags & ~NR_REG_MASK) | reg;

	if (netmap_verbose) {
		D("%s: tx [%d,%d) rx [%d,%d) id %d",
			na->name,
			priv->np_txqfirst,
			priv->np_txqlast,
			priv->np_rxqfirst,
			priv->np_rxqlast,
			i);
	}
	return 0;
}


/*
 * Set the ring ID. For devices with a single queue, a request
 * for all rings is the same as a single ring.
 */
static int
netmap_set_ringid(struct netmap_priv_d *priv, uint16_t ringid, uint32_t flags)
{
	struct netmap_adapter *na = priv->np_na;
	int error;

	error = netmap_interp_ringid(priv, ringid, flags);
	if (error) {
		return error;
	}

	priv->np_txpoll = (ringid & NETMAP_NO_TX_POLL) ? 0 : 1;

	/* optimization: count the users registered for more than
	 * one ring, which are the ones sleeping on the global queue.
	 * The default netmap_notify() callback will then
	 * avoid signaling the global queue if nobody is using it
	 */
	if (nm_tx_si_user(priv))
		na->tx_si_users++;
	if (nm_rx_si_user(priv))
		na->rx_si_users++;
	return 0;
}

/*
 * possibly move the interface to netmap-mode.
 * If success it returns a pointer to netmap_if, otherwise NULL.
 * This must be called with NMG_LOCK held.
 *
 * The following na callbacks are called in the process:
 *
 * na->nm_config()			[by netmap_update_config]
 * (get current number and size of rings)
 *
 *  	We have a generic one for linux (netmap_linux_config).
 *  	The bwrap has to override this, since it has to forward
 *  	the request to the wrapped adapter (netmap_bwrap_config).
 *
 *    	XXX netmap_if_new calls this again (2014-03-15)
 *
 * na->nm_krings_create()		[by netmap_if_new]
 * (create and init the krings array)
 *
 * 	One of the following:
 *
 *	* netmap_hw_krings_create, 			(hw ports)
 *		creates the standard layout for the krings
 * 		and adds the mbq (used for the host rings).
 *
 * 	* netmap_vp_krings_create			(VALE ports)
 * 		add leases and scratchpads
 *
 * 	* netmap_pipe_krings_create			(pipes)
 * 		create the krings and rings of both ends and
 * 		cross-link them
 *
 *      * netmap_monitor_krings_create 			(monitors)
 *      	avoid allocating the mbq
 *
 *      * netmap_bwrap_krings_create			(bwraps)
 *      	create both the brap krings array,
 *      	the krings array of the wrapped adapter, and
 *      	(if needed) the fake array for the host adapter
 *
 * na->nm_register(, 1)
 * (put the adapter in netmap mode)
 *
 * 	This may be one of the following:
 * 	(XXX these should be either all *_register or all *_reg 2014-03-15)
 *
 * 	* netmap_hw_register				(hw ports)
 * 		checks that the ifp is still there, then calls
 * 		the hardware specific callback;
 *
 * 	* netmap_vp_reg					(VALE ports)
 *		If the port is connected to a bridge,
 *		set the NAF_NETMAP_ON flag under the
 *		bridge write lock.
 *
 *	* netmap_pipe_reg				(pipes)
 *		inform the other pipe end that it is no
 *		longer responsibile for the lifetime of this
 *		pipe end
 *
 *	* netmap_monitor_reg				(monitors)
 *		intercept the sync callbacks of the monitored
 *		rings
 *
 *	* netmap_bwrap_register				(bwraps)
 *		cross-link the bwrap and hwna rings,
 *		forward the request to the hwna, override
 *		the hwna notify callback (to get the frames
 *		coming from outside go through the bridge).
 *
 * XXX maybe netmap_if_new() should be merged with this (2014-03-15).
 *
 */
struct netmap_if *
netmap_do_regif(struct netmap_priv_d *priv, struct netmap_adapter *na,
	uint16_t ringid, uint32_t flags, int *err)
{
	struct netmap_if *nifp = NULL;
	int error, need_mem = 0;

	NMG_LOCK_ASSERT();
	/* ring configuration may have changed, fetch from the card */
	netmap_update_config(na);
	priv->np_na = na;     /* store the reference */
	error = netmap_set_ringid(priv, ringid, flags);
	if (error)
		goto out;
	/* ensure allocators are ready */
	need_mem = !netmap_have_memory_locked(priv);
	if (need_mem) {
		error = netmap_get_memory_locked(priv);
		ND("get_memory returned %d", error);
		if (error)
			goto out;
	}
	/* Allocate a netmap_if and, if necessary, all the netmap_ring's */
	nifp = netmap_if_new(na);
	if (nifp == NULL) { /* allocation failed */
		error = ENOMEM;
		goto out;
	}
	na->active_fds++;
	if (!nm_netmap_on(na)) {
		/* Netmap not active, set the card in netmap mode
		 * and make it use the shared buffers.
		 */
		/* cache the allocator info in the na */
		na->na_lut = netmap_mem_get_lut(na->nm_mem);
		ND("%p->na_lut == %p", na, na->na_lut);
		na->na_lut_objtotal = netmap_mem_get_buftotal(na->nm_mem);
		na->na_lut_objsize = netmap_mem_get_bufsize(na->nm_mem);
		error = na->nm_register(na, 1); /* mode on */
		if (error) {
			netmap_do_unregif(priv, nifp);
			nifp = NULL;
		}
	}
out:
	*err = error;
	if (error) {
		/* we should drop the allocator, but only
		 * if we were the ones who grabbed it
		 */
		if (need_mem)
			netmap_drop_memory_locked(priv);
		priv->np_na = NULL;
	}
	if (nifp != NULL) {
		/*
		 * advertise that the interface is ready bt setting ni_nifp.
		 * The barrier is needed because readers (poll and *SYNC)
		 * check for priv->np_nifp != NULL without locking
		 */
		wmb(); /* make sure previous writes are visible to all CPUs */
		priv->np_nifp = nifp;
	}
	return nifp;
}



/*
 * ioctl(2) support for the "netmap" device.
 *
 * Following a list of accepted commands:
 * - NIOCGINFO
 * - SIOCGIFADDR	just for convenience
 * - NIOCREGIF
 * - NIOCTXSYNC
 * - NIOCRXSYNC
 *
 * Return 0 on success, errno otherwise.
 */
int
netmap_ioctl(struct cdev *dev, u_long cmd, caddr_t data,
	int fflag, struct thread *td)
{
	struct netmap_priv_d *priv = NULL;
	struct nmreq *nmr = (struct nmreq *) data;
	struct netmap_adapter *na = NULL;
	int error;
	u_int i, qfirst, qlast;
	struct netmap_if *nifp;
	struct netmap_kring *krings;

	(void)dev;	/* UNUSED */
	(void)fflag;	/* UNUSED */

	if (cmd == NIOCGINFO || cmd == NIOCREGIF) {
		/* truncate name */
		nmr->nr_name[sizeof(nmr->nr_name) - 1] = '\0';
		if (nmr->nr_version != NETMAP_API) {
			D("API mismatch for %s got %d need %d",
				nmr->nr_name,
				nmr->nr_version, NETMAP_API);
			nmr->nr_version = NETMAP_API;
		}
		if (nmr->nr_version < NETMAP_MIN_API ||
		    nmr->nr_version > NETMAP_MAX_API) {
			return EINVAL;
		}
	}
	CURVNET_SET(TD_TO_VNET(td));

	error = devfs_get_cdevpriv((void **)&priv);
	if (error) {
		CURVNET_RESTORE();
		/* XXX ENOENT should be impossible, since the priv
		 * is now created in the open */
		return (error == ENOENT ? ENXIO : error);
	}

	switch (cmd) {
	case NIOCGINFO:		/* return capabilities etc */
		if (nmr->nr_cmd == NETMAP_BDG_LIST) {
			error = netmap_bdg_ctl(nmr, NULL);
			break;
		}

		NMG_LOCK();
		do {
			/* memsize is always valid */
			struct netmap_mem_d *nmd = &nm_mem;
			u_int memflags;

			if (nmr->nr_name[0] != '\0') {
				/* get a refcount */
				error = netmap_get_na(nmr, &na, 1 /* create */);
				if (error)
					break;
				nmd = na->nm_mem; /* get memory allocator */
			}

			error = netmap_mem_get_info(nmd, &nmr->nr_memsize, &memflags,
				&nmr->nr_arg2);
			if (error)
				break;
			if (na == NULL) /* only memory info */
				break;
			nmr->nr_offset = 0;
			nmr->nr_rx_slots = nmr->nr_tx_slots = 0;
			netmap_update_config(na);
			nmr->nr_rx_rings = na->num_rx_rings;
			nmr->nr_tx_rings = na->num_tx_rings;
			nmr->nr_rx_slots = na->num_rx_desc;
			nmr->nr_tx_slots = na->num_tx_desc;
			netmap_adapter_put(na);
		} while (0);
		NMG_UNLOCK();
		break;

	case NIOCREGIF:
		/* possibly attach/detach NIC and VALE switch */
		i = nmr->nr_cmd;
		if (i == NETMAP_BDG_ATTACH || i == NETMAP_BDG_DETACH
				|| i == NETMAP_BDG_VNET_HDR
				|| i == NETMAP_BDG_NEWIF
				|| i == NETMAP_BDG_DELIF) {
			error = netmap_bdg_ctl(nmr, NULL);
			break;
		} else if (i != 0) {
			D("nr_cmd must be 0 not %d", i);
			error = EINVAL;
			break;
		}

		/* protect access to priv from concurrent NIOCREGIF */
		NMG_LOCK();
		do {
			u_int memflags;

			if (priv->np_na != NULL) {	/* thread already registered */
				error = EBUSY;
				break;
			}
			/* find the interface and a reference */
			error = netmap_get_na(nmr, &na, 1 /* create */); /* keep reference */
			if (error)
				break;
			if (NETMAP_OWNED_BY_KERN(na)) {
				netmap_adapter_put(na);
				error = EBUSY;
				break;
			}
			nifp = netmap_do_regif(priv, na, nmr->nr_ringid, nmr->nr_flags, &error);
			if (!nifp) {    /* reg. failed, release priv and ref */
				netmap_adapter_put(na);
				priv->np_nifp = NULL;
				break;
			}
			priv->np_td = td; // XXX kqueue, debugging only

			/* return the offset of the netmap_if object */
			nmr->nr_rx_rings = na->num_rx_rings;
			nmr->nr_tx_rings = na->num_tx_rings;
			nmr->nr_rx_slots = na->num_rx_desc;
			nmr->nr_tx_slots = na->num_tx_desc;
			error = netmap_mem_get_info(na->nm_mem, &nmr->nr_memsize, &memflags,
				&nmr->nr_arg2);
			if (error) {
				netmap_adapter_put(na);
				break;
			}
			if (memflags & NETMAP_MEM_PRIVATE) {
				*(uint32_t *)(uintptr_t)&nifp->ni_flags |= NI_PRIV_MEM;
			}
			priv->np_txsi = (priv->np_txqlast - priv->np_txqfirst > 1) ?
				&na->tx_si : &na->tx_rings[priv->np_txqfirst].si;
			priv->np_rxsi = (priv->np_rxqlast - priv->np_rxqfirst > 1) ?
				&na->rx_si : &na->rx_rings[priv->np_rxqfirst].si;

			if (nmr->nr_arg3) {
				D("requested %d extra buffers", nmr->nr_arg3);
				nmr->nr_arg3 = netmap_extra_alloc(na,
					&nifp->ni_bufs_head, nmr->nr_arg3);
				D("got %d extra buffers", nmr->nr_arg3);
			}
			nmr->nr_offset = netmap_mem_if_offset(na->nm_mem, nifp);
		} while (0);
		NMG_UNLOCK();
		break;

	case NIOCTXSYNC:
	case NIOCRXSYNC:
		nifp = priv->np_nifp;

		if (nifp == NULL) {
			error = ENXIO;
			break;
		}
		mb(); /* make sure following reads are not from cache */

		na = priv->np_na;      /* we have a reference */

		if (na == NULL) {
			D("Internal error: nifp != NULL && na == NULL");
			error = ENXIO;
			break;
		}

		if (!nm_netmap_on(na)) {
			error = ENXIO;
			break;
		}

		if (cmd == NIOCTXSYNC) {
			krings = na->tx_rings;
			qfirst = priv->np_txqfirst;
			qlast = priv->np_txqlast;
		} else {
			krings = na->rx_rings;
			qfirst = priv->np_rxqfirst;
			qlast = priv->np_rxqlast;
		}

		for (i = qfirst; i < qlast; i++) {
			struct netmap_kring *kring = krings + i;
			if (nm_kr_tryget(kring)) {
				error = EBUSY;
				goto out;
			}
			if (cmd == NIOCTXSYNC) {
				if (netmap_verbose & NM_VERB_TXSYNC)
					D("pre txsync ring %d cur %d hwcur %d",
					    i, kring->ring->cur,
					    kring->nr_hwcur);
				if (nm_txsync_prologue(kring) >= kring->nkr_num_slots) {
					netmap_ring_reinit(kring);
				} else {
					kring->nm_sync(kring, NAF_FORCE_RECLAIM);
				}
				if (netmap_verbose & NM_VERB_TXSYNC)
					D("post txsync ring %d cur %d hwcur %d",
					    i, kring->ring->cur,
					    kring->nr_hwcur);
			} else {
				kring->nm_sync(kring, NAF_FORCE_READ);
				microtime(&na->rx_rings[i].ring->ts);
			}
			nm_kr_put(kring);
		}

		break;

	case NIOCCONFIG:
		error = netmap_bdg_config(nmr);
		break;
#ifdef __FreeBSD__
	case FIONBIO:
	case FIOASYNC:
		ND("FIONBIO/FIOASYNC are no-ops");
		break;

	case BIOCIMMEDIATE:
	case BIOCGHDRCMPLT:
	case BIOCSHDRCMPLT:
	case BIOCSSEESENT:
		D("ignore BIOCIMMEDIATE/BIOCSHDRCMPLT/BIOCSHDRCMPLT/BIOCSSEESENT");
		break;

	default:	/* allow device-specific ioctls */
	    {
		struct ifnet *ifp = ifunit_ref(nmr->nr_name);
		if (ifp == NULL) {
			error = ENXIO;
		} else {
			struct socket so;

			bzero(&so, sizeof(so));
			so.so_vnet = ifp->if_vnet;
			// so->so_proto not null.
			error = ifioctl(&so, cmd, data, td);
			if_rele(ifp);
		}
		break;
	    }

#else /* linux */
	default:
		error = EOPNOTSUPP;
#endif /* linux */
	}
out:

	CURVNET_RESTORE();
	return (error);
}


/*
 * select(2) and poll(2) handlers for the "netmap" device.
 *
 * Can be called for one or more queues.
 * Return true the event mask corresponding to ready events.
 * If there are no ready events, do a selrecord on either individual
 * selinfo or on the global one.
 * Device-dependent parts (locking and sync of tx/rx rings)
 * are done through callbacks.
 *
 * On linux, arguments are really pwait, the poll table, and 'td' is struct file *
 * The first one is remapped to pwait as selrecord() uses the name as an
 * hidden argument.
 */
int
netmap_poll(struct cdev *dev, int events, struct thread *td)
{
	struct netmap_priv_d *priv = NULL;
	struct netmap_adapter *na;
	struct netmap_kring *kring;
	u_int i, check_all_tx, check_all_rx, want_tx, want_rx, revents = 0;
	struct mbq q;		/* packets from hw queues to host stack */
	void *pwait = dev;	/* linux compatibility */
	int is_kevent = 0;

	/*
	 * In order to avoid nested locks, we need to "double check"
	 * txsync and rxsync if we decide to do a selrecord().
	 * retry_tx (and retry_rx, later) prevent looping forever.
	 */
	int retry_tx = 1, retry_rx = 1;

	(void)pwait;
	mbq_init(&q);

	/*
	 * XXX kevent has curthread->tp_fop == NULL,
	 * so devfs_get_cdevpriv() fails. We circumvent this by passing
	 * priv as the first argument, which is also useful to avoid
	 * the selrecord() which are not necessary in that case.
	 */
	if (devfs_get_cdevpriv((void **)&priv) != 0) {
		is_kevent = 1;
		if (netmap_verbose)
			D("called from kevent");
		priv = (struct netmap_priv_d *)dev;
	}
	if (priv == NULL)
		return POLLERR;

	if (priv->np_nifp == NULL) {
		D("No if registered");
		return POLLERR;
	}
	rmb(); /* make sure following reads are not from cache */

	na = priv->np_na;

	if (!nm_netmap_on(na))
		return POLLERR;

	if (netmap_verbose & 0x8000)
		D("device %s events 0x%x", na->name, events);
	want_tx = events & (POLLOUT | POLLWRNORM);
	want_rx = events & (POLLIN | POLLRDNORM);


	/*
	 * check_all_{tx|rx} are set if the card has more than one queue AND
	 * the file descriptor is bound to all of them. If so, we sleep on
	 * the "global" selinfo, otherwise we sleep on individual selinfo
	 * (FreeBSD only allows two selinfo's per file descriptor).
	 * The interrupt routine in the driver wake one or the other
	 * (or both) depending on which clients are active.
	 *
	 * rxsync() is only called if we run out of buffers on a POLLIN.
	 * txsync() is called if we run out of buffers on POLLOUT, or
	 * there are pending packets to send. The latter can be disabled
	 * passing NETMAP_NO_TX_POLL in the NIOCREG call.
	 */
	check_all_tx = nm_tx_si_user(priv);
	check_all_rx = nm_rx_si_user(priv);

	/*
	 * We start with a lock free round which is cheap if we have
	 * slots available. If this fails, then lock and call the sync
	 * routines.
	 */
	for (i = priv->np_rxqfirst; want_rx && i < priv->np_rxqlast; i++) {
		kring = &na->rx_rings[i];
		/* XXX compare ring->cur and kring->tail */
		if (!nm_ring_empty(kring->ring)) {
			revents |= want_rx;
			want_rx = 0;	/* also breaks the loop */
		}
	}
	for (i = priv->np_txqfirst; want_tx && i < priv->np_txqlast; i++) {
		kring = &na->tx_rings[i];
		/* XXX compare ring->cur and kring->tail */
		if (!nm_ring_empty(kring->ring)) {
			revents |= want_tx;
			want_tx = 0;	/* also breaks the loop */
		}
	}

	/*
	 * If we want to push packets out (priv->np_txpoll) or
	 * want_tx is still set, we must issue txsync calls
	 * (on all rings, to avoid that the tx rings stall).
	 * XXX should also check cur != hwcur on the tx rings.
	 * Fortunately, normal tx mode has np_txpoll set.
	 */
	if (priv->np_txpoll || want_tx) {
		/*
		 * The first round checks if anyone is ready, if not
		 * do a selrecord and another round to handle races.
		 * want_tx goes to 0 if any space is found, and is
		 * used to skip rings with no pending transmissions.
		 */
flush_tx:
		for (i = priv->np_txqfirst; i < priv->np_txqlast; i++) {
			int found = 0;

			kring = &na->tx_rings[i];
			if (!want_tx && kring->ring->cur == kring->nr_hwcur)
				continue;
			/* only one thread does txsync */
			if (nm_kr_tryget(kring)) {
				/* either busy or stopped
				 * XXX if the ring is stopped, sleeping would
				 * be better. In current code, however, we only
				 * stop the rings for brief intervals (2014-03-14)
				 */
				if (netmap_verbose)
					RD(2, "%p lost race on txring %d, ok",
					    priv, i);
				continue;
			}
			if (nm_txsync_prologue(kring) >= kring->nkr_num_slots) {
				netmap_ring_reinit(kring);
				revents |= POLLERR;
			} else {
				if (kring->nm_sync(kring, 0))
					revents |= POLLERR;
			}

			/*
			 * If we found new slots, notify potential
			 * listeners on the same ring.
			 * Since we just did a txsync, look at the copies
			 * of cur,tail in the kring.
			 */
			found = kring->rcur != kring->rtail;
			nm_kr_put(kring);
			if (found) { /* notify other listeners */
				revents |= want_tx;
				want_tx = 0;
				na->nm_notify(na, i, NR_TX, 0);
			}
		}
		if (want_tx && retry_tx && !is_kevent) {
			OS_selrecord(td, check_all_tx ?
			    &na->tx_si : &na->tx_rings[priv->np_txqfirst].si);
			retry_tx = 0;
			goto flush_tx;
		}
	}

	/*
	 * If want_rx is still set scan receive rings.
	 * Do it on all rings because otherwise we starve.
	 */
	if (want_rx) {
		int send_down = 0; /* transparent mode */
		/* two rounds here for race avoidance */
do_retry_rx:
		for (i = priv->np_rxqfirst; i < priv->np_rxqlast; i++) {
			int found = 0;

			kring = &na->rx_rings[i];

			if (nm_kr_tryget(kring)) {
				if (netmap_verbose)
					RD(2, "%p lost race on rxring %d, ok",
					    priv, i);
				continue;
			}

			/*
			 * transparent mode support: collect packets
			 * from the rxring(s).
			 * XXX NR_FORWARD should only be read on
			 * physical or NIC ports
			 */
			if (netmap_fwd ||kring->ring->flags & NR_FORWARD) {
				ND(10, "forwarding some buffers up %d to %d",
				    kring->nr_hwcur, kring->ring->cur);
				netmap_grab_packets(kring, &q, netmap_fwd);
			}

			if (kring->nm_sync(kring, 0))
				revents |= POLLERR;
			if (netmap_no_timestamp == 0 ||
					kring->ring->flags & NR_TIMESTAMP) {
				microtime(&kring->ring->ts);
			}
			/* after an rxsync we can use kring->rcur, rtail */
			found = kring->rcur != kring->rtail;
			nm_kr_put(kring);
			if (found) {
				revents |= want_rx;
				retry_rx = 0;
				na->nm_notify(na, i, NR_RX, 0);
			}
		}

		/* transparent mode XXX only during first pass ? */
		if (na->na_flags & NAF_HOST_RINGS) {
			kring = &na->rx_rings[na->num_rx_rings];
			if (check_all_rx
			    && (netmap_fwd || kring->ring->flags & NR_FORWARD)) {
				/* XXX fix to use kring fields */
				if (nm_ring_empty(kring->ring))
					send_down = netmap_rxsync_from_host(na, td, dev);
				if (!nm_ring_empty(kring->ring))
					revents |= want_rx;
			}
		}

		if (retry_rx && !is_kevent)
			OS_selrecord(td, check_all_rx ?
			    &na->rx_si : &na->rx_rings[priv->np_rxqfirst].si);
		if (send_down > 0 || retry_rx) {
			retry_rx = 0;
			if (send_down)
				goto flush_tx; /* and retry_rx */
			else
				goto do_retry_rx;
		}
	}

	/*
	 * Transparent mode: marked bufs on rx rings between
	 * kring->nr_hwcur and ring->head
	 * are passed to the other endpoint.
	 *
	 * In this mode we also scan the sw rxring, which in
	 * turn passes packets up.
	 *
	 * XXX Transparent mode at the moment requires to bind all
 	 * rings to a single file descriptor.
	 */

	if (q.head && na->ifp != NULL)
		netmap_send_up(na->ifp, &q);

	return (revents);
}


/*-------------------- driver support routines -------------------*/

static int netmap_hw_krings_create(struct netmap_adapter *);

/* default notify callback */
static int
netmap_notify(struct netmap_adapter *na, u_int n_ring,
	enum txrx tx, int flags)
{
	struct netmap_kring *kring;

	if (tx == NR_TX) {
		kring = na->tx_rings + n_ring;
		OS_selwakeup(&kring->si, PI_NET);
		/* optimization: avoid a wake up on the global
		 * queue if nobody has registered for more
		 * than one ring
		 */
		if (na->tx_si_users > 0)
			OS_selwakeup(&na->tx_si, PI_NET);
	} else {
		kring = na->rx_rings + n_ring;
		OS_selwakeup(&kring->si, PI_NET);
		/* optimization: same as above */
		if (na->rx_si_users > 0)
			OS_selwakeup(&na->rx_si, PI_NET);
	}
	return 0;
}


/* called by all routines that create netmap_adapters.
 * Attach na to the ifp (if any) and provide defaults
 * for optional callbacks. Defaults assume that we
 * are creating an hardware netmap_adapter.
 */
int
netmap_attach_common(struct netmap_adapter *na)
{
	struct ifnet *ifp = na->ifp;

	if (na->num_tx_rings == 0 || na->num_rx_rings == 0) {
		D("%s: invalid rings tx %d rx %d",
			na->name, na->num_tx_rings, na->num_rx_rings);
		return EINVAL;
	}
	/* ifp is NULL for virtual adapters (bwrap, non-persistent VALE ports,
	 * pipes, monitors). For bwrap we actually have a non-null ifp for
	 * use by the external modules, but that is set after this
	 * function has been called.
	 * XXX this is ugly, maybe split this function in two (2014-03-14)
	 */
	if (ifp != NULL) {
		WNA(ifp) = na;

	/* the following is only needed for na that use the host port.
	 * XXX do we have something similar for linux ?
	 */
#ifdef __FreeBSD__
		na->if_input = ifp->if_input; /* for netmap_send_up */
#endif /* __FreeBSD__ */

		NETMAP_SET_CAPABLE(ifp);
	}
	if (na->nm_krings_create == NULL) {
		/* we assume that we have been called by a driver,
		 * since other port types all provide their own
		 * nm_krings_create
		 */
		na->nm_krings_create = netmap_hw_krings_create;
		na->nm_krings_delete = netmap_hw_krings_delete;
	}
	if (na->nm_notify == NULL)
		na->nm_notify = netmap_notify;
	na->active_fds = 0;

	if (na->nm_mem == NULL)
		/* use the global allocator */
		na->nm_mem = &nm_mem;
	if (na->nm_bdg_attach == NULL)
		/* no special nm_bdg_attach callback. On VALE
		 * attach, we need to interpose a bwrap
		 */
		na->nm_bdg_attach = netmap_bwrap_attach;
	return 0;
}


/* standard cleanup, called by all destructors */
void
netmap_detach_common(struct netmap_adapter *na)
{
	if (na->ifp != NULL)
		WNA(na->ifp) = NULL; /* XXX do we need this? */

	if (na->tx_rings) { /* XXX should not happen */
		D("freeing leftover tx_rings");
		na->nm_krings_delete(na);
	}
	netmap_pipe_dealloc(na);
	if (na->na_flags & NAF_MEM_OWNER)
		netmap_mem_private_delete(na->nm_mem);
	bzero(na, sizeof(*na));
	free(na, M_DEVBUF);
}

/* Wrapper for the register callback provided hardware drivers.
 * na->ifp == NULL means the the driver module has been
 * unloaded, so we cannot call into it.
 * Note that module unloading, in our patched linux drivers,
 * happens under NMG_LOCK and after having stopped all the
 * nic rings (see netmap_detach). This provides sufficient
 * protection for the other driver-provied callbacks
 * (i.e., nm_config and nm_*xsync), that therefore don't need
 * to wrapped.
 */
static int
netmap_hw_register(struct netmap_adapter *na, int onoff)
{
	struct netmap_hw_adapter *hwna =
		(struct netmap_hw_adapter*)na;

	if (na->ifp == NULL)
		return onoff ? ENXIO : 0;

	return hwna->nm_hw_register(na, onoff);
}


/*
 * Initialize a ``netmap_adapter`` object created by driver on attach.
 * We allocate a block of memory with room for a struct netmap_adapter
 * plus two sets of N+2 struct netmap_kring (where N is the number
 * of hardware rings):
 * krings	0..N-1	are for the hardware queues.
 * kring	N	is for the host stack queue
 * kring	N+1	is only used for the selinfo for all queues. // XXX still true ?
 * Return 0 on success, ENOMEM otherwise.
 */
int
netmap_attach(struct netmap_adapter *arg)
{
	struct netmap_hw_adapter *hwna = NULL;
	// XXX when is arg == NULL ?
	struct ifnet *ifp = arg ? arg->ifp : NULL;

	if (arg == NULL || ifp == NULL)
		goto fail;
	hwna = malloc(sizeof(*hwna), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (hwna == NULL)
		goto fail;
	hwna->up = *arg;
	hwna->up.na_flags |= NAF_HOST_RINGS;
	strncpy(hwna->up.name, ifp->if_xname, sizeof(hwna->up.name));
	hwna->nm_hw_register = hwna->up.nm_register;
	hwna->up.nm_register = netmap_hw_register;
	if (netmap_attach_common(&hwna->up)) {
		free(hwna, M_DEVBUF);
		goto fail;
	}
	netmap_adapter_get(&hwna->up);

#ifdef linux
	if (ifp->netdev_ops) {
		/* prepare a clone of the netdev ops */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 28)
		hwna->nm_ndo.ndo_start_xmit = ifp->netdev_ops;
#else
		hwna->nm_ndo = *ifp->netdev_ops;
#endif
	}
	hwna->nm_ndo.ndo_start_xmit = linux_netmap_start_xmit;
	if (ifp->ethtool_ops) {
		hwna->nm_eto = *ifp->ethtool_ops;
	}
	hwna->nm_eto.set_ringparam = linux_netmap_set_ringparam;
#ifdef ETHTOOL_SCHANNELS
	hwna->nm_eto.set_channels = linux_netmap_set_channels;
#endif
	if (arg->nm_config == NULL) {
		hwna->up.nm_config = netmap_linux_config;
	}
#endif /* linux */

#ifdef __FreeBSD__
	if_printf(ifp, "netmap queues/slots: TX %d/%d, RX %d/%d\n",
	    hwna->up.num_tx_rings, hwna->up.num_tx_desc,
	    hwna->up.num_rx_rings, hwna->up.num_rx_desc);
#else
	D("success for %s tx %d/%d rx %d/%d queues/slots",
		hwna->up.name,
		hwna->up.num_tx_rings, hwna->up.num_tx_desc,
		hwna->up.num_rx_rings, hwna->up.num_rx_desc
		);
#endif
	return 0;

fail:
	D("fail, arg %p ifp %p na %p", arg, ifp, hwna);
	if (ifp)
		netmap_detach(ifp);
	return (hwna ? EINVAL : ENOMEM);
}


void
NM_DBG(netmap_adapter_get)(struct netmap_adapter *na)
{
	if (!na) {
		return;
	}

	refcount_acquire(&na->na_refcount);
}


/* returns 1 iff the netmap_adapter is destroyed */
int
NM_DBG(netmap_adapter_put)(struct netmap_adapter *na)
{
	if (!na)
		return 1;

	if (!refcount_release(&na->na_refcount))
		return 0;

	if (na->nm_dtor)
		na->nm_dtor(na);

	netmap_detach_common(na);

	return 1;
}

/* nm_krings_create callback for all hardware native adapters */
int
netmap_hw_krings_create(struct netmap_adapter *na)
{
	int ret = netmap_krings_create(na, 0);
	if (ret == 0) {
		/* initialize the mbq for the sw rx ring */
		mbq_safe_init(&na->rx_rings[na->num_rx_rings].rx_queue);
		ND("initialized sw rx queue %d", na->num_rx_rings);
	}
	return ret;
}



/*
 * Called on module unload by the netmap-enabled drivers
 */
void
netmap_detach(struct ifnet *ifp)
{
	struct netmap_adapter *na = NA(ifp);

	if (!na)
		return;

	NMG_LOCK();
	netmap_disable_all_rings(ifp);
	if (!netmap_adapter_put(na)) {
		/* someone is still using the adapter,
		 * tell them that the interface is gone
		 */
		na->ifp = NULL;
		// XXX also clear NAF_NATIVE_ON ?
		na->na_flags &= ~NAF_NETMAP_ON;
		/* give them a chance to notice */
		netmap_enable_all_rings(ifp);
	}
	NMG_UNLOCK();
}


/*
 * Intercept packets from the network stack and pass them
 * to netmap as incoming packets on the 'software' ring.
 *
 * We only store packets in a bounded mbq and then copy them
 * in the relevant rxsync routine.
 *
 * We rely on the OS to make sure that the ifp and na do not go
 * away (typically the caller checks for IFF_DRV_RUNNING or the like).
 * In nm_register() or whenever there is a reinitialization,
 * we make sure to make the mode change visible here.
 */
int
netmap_transmit(struct ifnet *ifp, struct mbuf *m)
{
	struct netmap_adapter *na = NA(ifp);
	struct netmap_kring *kring;
	u_int len = MBUF_LEN(m);
	u_int error = ENOBUFS;
	struct mbq *q;
	int space;

	// XXX [Linux] we do not need this lock
	// if we follow the down/configure/up protocol -gl
	// mtx_lock(&na->core_lock);

	if (!nm_netmap_on(na)) {
		D("%s not in netmap mode anymore", na->name);
		error = ENXIO;
		goto done;
	}

	kring = &na->rx_rings[na->num_rx_rings];
	q = &kring->rx_queue;

	// XXX reconsider long packets if we handle fragments
	if (len > NETMAP_BUF_SIZE(na)) { /* too long for us */
		D("%s from_host, drop packet size %d > %d", na->name,
			len, NETMAP_BUF_SIZE(na));
		goto done;
	}

	/* protect against rxsync_from_host(), netmap_sw_to_nic()
	 * and maybe other instances of netmap_transmit (the latter
	 * not possible on Linux).
	 * Also avoid overflowing the queue.
	 */
	mbq_lock(q);

        space = kring->nr_hwtail - kring->nr_hwcur;
        if (space < 0)
                space += kring->nkr_num_slots;
	if (space + mbq_len(q) >= kring->nkr_num_slots - 1) { // XXX
		RD(10, "%s full hwcur %d hwtail %d qlen %d len %d m %p",
			na->name, kring->nr_hwcur, kring->nr_hwtail, mbq_len(q),
			len, m);
	} else {
		mbq_enqueue(q, m);
		ND(10, "%s %d bufs in queue len %d m %p",
			na->name, mbq_len(q), len, m);
		/* notify outside the lock */
		m = NULL;
		error = 0;
	}
	mbq_unlock(q);

done:
	if (m)
		m_freem(m);
	/* unconditionally wake up listeners */
	na->nm_notify(na, na->num_rx_rings, NR_RX, 0);
	/* this is normally netmap_notify(), but for nics
	 * connected to a bridge it is netmap_bwrap_intr_notify(),
	 * that possibly forwards the frames through the switch
	 */

	return (error);
}


/*
 * netmap_reset() is called by the driver routines when reinitializing
 * a ring. The driver is in charge of locking to protect the kring.
 * If native netmap mode is not set just return NULL.
 */
struct netmap_slot *
netmap_reset(struct netmap_adapter *na, enum txrx tx, u_int n,
	u_int new_cur)
{
	struct netmap_kring *kring;
	int new_hwofs, lim;

	if (!nm_native_on(na)) {
		ND("interface not in native netmap mode");
		return NULL;	/* nothing to reinitialize */
	}

	/* XXX note- in the new scheme, we are not guaranteed to be
	 * under lock (e.g. when called on a device reset).
	 * In this case, we should set a flag and do not trust too
	 * much the values. In practice: TODO
	 * - set a RESET flag somewhere in the kring
	 * - do the processing in a conservative way
	 * - let the *sync() fixup at the end.
	 */
	if (tx == NR_TX) {
		if (n >= na->num_tx_rings)
			return NULL;
		kring = na->tx_rings + n;
		// XXX check whether we should use hwcur or rcur
		new_hwofs = kring->nr_hwcur - new_cur;
	} else {
		if (n >= na->num_rx_rings)
			return NULL;
		kring = na->rx_rings + n;
		new_hwofs = kring->nr_hwtail - new_cur;
	}
	lim = kring->nkr_num_slots - 1;
	if (new_hwofs > lim)
		new_hwofs -= lim + 1;

	/* Always set the new offset value and realign the ring. */
	if (netmap_verbose)
	    D("%s %s%d hwofs %d -> %d, hwtail %d -> %d",
		na->name,
		tx == NR_TX ? "TX" : "RX", n,
		kring->nkr_hwofs, new_hwofs,
		kring->nr_hwtail,
		tx == NR_TX ? lim : kring->nr_hwtail);
	kring->nkr_hwofs = new_hwofs;
	if (tx == NR_TX) {
		kring->nr_hwtail = kring->nr_hwcur + lim;
		if (kring->nr_hwtail > lim)
			kring->nr_hwtail -= lim + 1;
	}

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
	 * Wakeup on the individual and global selwait
	 * We do the wakeup here, but the ring is not yet reconfigured.
	 * However, we are under lock so there are no races.
	 */
	na->nm_notify(na, n, tx, 0);
	return kring->ring->slot;
}


/*
 * Dispatch rx/tx interrupts to the netmap rings.
 *
 * "work_done" is non-null on the RX path, NULL for the TX path.
 * We rely on the OS to make sure that there is only one active
 * instance per queue, and that there is appropriate locking.
 *
 * The 'notify' routine depends on what the ring is attached to.
 * - for a netmap file descriptor, do a selwakeup on the individual
 *   waitqueue, plus one on the global one if needed
 *   (see netmap_notify)
 * - for a nic connected to a switch, call the proper forwarding routine
 *   (see netmap_bwrap_intr_notify)
 */
void
netmap_common_irq(struct ifnet *ifp, u_int q, u_int *work_done)
{
	struct netmap_adapter *na = NA(ifp);
	struct netmap_kring *kring;

	q &= NETMAP_RING_MASK;

	if (netmap_verbose) {
	        RD(5, "received %s queue %d", work_done ? "RX" : "TX" , q);
	}

	if (work_done) { /* RX path */
		if (q >= na->num_rx_rings)
			return;	// not a physical queue
		kring = na->rx_rings + q;
		kring->nr_kflags |= NKR_PENDINTR;	// XXX atomic ?
		na->nm_notify(na, q, NR_RX, 0);
		*work_done = 1; /* do not fire napi again */
	} else { /* TX path */
		if (q >= na->num_tx_rings)
			return;	// not a physical queue
		kring = na->tx_rings + q;
		na->nm_notify(na, q, NR_TX, 0);
	}
}


/*
 * Default functions to handle rx/tx interrupts from a physical device.
 * "work_done" is non-null on the RX path, NULL for the TX path.
 *
 * If the card is not in netmap mode, simply return 0,
 * so that the caller proceeds with regular processing.
 * Otherwise call netmap_common_irq() and return 1.
 *
 * If the card is connected to a netmap file descriptor,
 * do a selwakeup on the individual queue, plus one on the global one
 * if needed (multiqueue card _and_ there are multiqueue listeners),
 * and return 1.
 *
 * Finally, if called on rx from an interface connected to a switch,
 * calls the proper forwarding routine, and return 1.
 */
int
netmap_rx_irq(struct ifnet *ifp, u_int q, u_int *work_done)
{
	struct netmap_adapter *na = NA(ifp);

	/*
	 * XXX emulated netmap mode sets NAF_SKIP_INTR so
	 * we still use the regular driver even though the previous
	 * check fails. It is unclear whether we should use
	 * nm_native_on() here.
	 */
	if (!nm_netmap_on(na))
		return 0;

	if (na->na_flags & NAF_SKIP_INTR) {
		ND("use regular interrupt");
		return 0;
	}

	netmap_common_irq(ifp, q, work_done);
	return 1;
}


/*
 * Module loader and unloader
 *
 * netmap_init() creates the /dev/netmap device and initializes
 * all global variables. Returns 0 on success, errno on failure
 * (but there is no chance)
 *
 * netmap_fini() destroys everything.
 */

static struct cdev *netmap_dev; /* /dev/netmap character device. */
extern struct cdevsw netmap_cdevsw;


void
netmap_fini(void)
{
	// XXX destroy_bridges() ?
	if (netmap_dev)
		destroy_dev(netmap_dev);
	netmap_mem_fini();
	NMG_LOCK_DESTROY();
	printf("netmap: unloaded module.\n");
}


int
netmap_init(void)
{
	int error;

	NMG_LOCK_INIT();

	error = netmap_mem_init();
	if (error != 0)
		goto fail;
	/*
	 * MAKEDEV_ETERNAL_KLD avoids an expensive check on syscalls
	 * when the module is compiled in.
	 * XXX could use make_dev_credv() to get error number
	 */
	netmap_dev = make_dev_credf(MAKEDEV_ETERNAL_KLD,
		&netmap_cdevsw, 0, NULL, UID_ROOT, GID_WHEEL, 0600,
			      "netmap");
	if (!netmap_dev)
		goto fail;

	netmap_init_bridges();
#ifdef __FreeBSD__
	nm_vi_init_index();
#endif
	printf("netmap: loaded module\n");
	return (0);
fail:
	netmap_fini();
	return (EINVAL); /* may be incorrect */
}
