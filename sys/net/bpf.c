/*-
 * Copyright (c) 1990, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from the Stanford/CMU enet packet filter,
 * (net/enet.c) distributed as part of 4.3BSD, and code contributed
 * to Berkeley by Steven McCanne and Van Jacobson both of Lawrence
 * Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      @(#)bpf.c	8.4 (Berkeley) 1/9/95
 *
 * $FreeBSD: release/7.0.0/sys/net/bpf.c 174854 2007-12-22 06:32:46Z cvs2svn $
 */

#include "opt_bpf.h"
#include "opt_mac.h"
#include "opt_netgraph.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/time.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/filio.h>
#include <sys/sockio.h>
#include <sys/ttycom.h>
#include <sys/uio.h>

#include <sys/event.h>
#include <sys/file.h>
#include <sys/poll.h>
#include <sys/proc.h>

#include <sys/socket.h>

#include <net/if.h>
#include <net/bpf.h>
#ifdef BPF_JITTER
#include <net/bpf_jitter.h>
#endif
#include <net/bpfdesc.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>

#include <net80211/ieee80211_freebsd.h>

#include <security/mac/mac_framework.h>

static MALLOC_DEFINE(M_BPF, "BPF", "BPF data");

#if defined(DEV_BPF) || defined(NETGRAPH_BPF)

#define PRINET  26			/* interruptible */

#define	M_SKIP_BPF	M_SKIP_FIREWALL

/*
 * bpf_iflist is a list of BPF interface structures, each corresponding to a
 * specific DLT.  The same network interface might have several BPF interface
 * structures registered by different layers in the stack (i.e., 802.11
 * frames, ethernet frames, etc).
 */
static LIST_HEAD(, bpf_if)	bpf_iflist;
static struct mtx	bpf_mtx;		/* bpf global lock */
static int		bpf_bpfd_cnt;

static void	bpf_allocbufs(struct bpf_d *);
static void	bpf_attachd(struct bpf_d *, struct bpf_if *);
static void	bpf_detachd(struct bpf_d *);
static void	bpf_freed(struct bpf_d *);
static void	bpf_mcopy(const void *, void *, size_t);
static int	bpf_movein(struct uio *, int, struct ifnet *, struct mbuf **,
		    struct sockaddr *, int *, struct bpf_insn *);
static int	bpf_setif(struct bpf_d *, struct ifreq *);
static void	bpf_timed_out(void *);
static __inline void
		bpf_wakeup(struct bpf_d *);
static void	catchpacket(struct bpf_d *, u_char *, u_int,
		    u_int, void (*)(const void *, void *, size_t),
		    struct timeval *);
static void	reset_d(struct bpf_d *);
static int	 bpf_setf(struct bpf_d *, struct bpf_program *, u_long cmd);
static int	bpf_getdltlist(struct bpf_d *, struct bpf_dltlist *);
static int	bpf_setdlt(struct bpf_d *, u_int);
static void	filt_bpfdetach(struct knote *);
static int	filt_bpfread(struct knote *, long);
static void	bpf_drvinit(void *);
static void	bpf_clone(void *, struct ucred *, char *, int, struct cdev **);
static int	bpf_stats_sysctl(SYSCTL_HANDLER_ARGS);

SYSCTL_NODE(_net, OID_AUTO, bpf, CTLFLAG_RW, 0, "bpf sysctl");
static int bpf_bufsize = 4096;
SYSCTL_INT(_net_bpf, OID_AUTO, bufsize, CTLFLAG_RW,
    &bpf_bufsize, 0, "Default bpf buffer size");
static int bpf_maxbufsize = BPF_MAXBUFSIZE;
SYSCTL_INT(_net_bpf, OID_AUTO, maxbufsize, CTLFLAG_RW,
    &bpf_maxbufsize, 0, "Maximum bpf buffer size");
static int bpf_maxinsns = BPF_MAXINSNS;
SYSCTL_INT(_net_bpf, OID_AUTO, maxinsns, CTLFLAG_RW,
    &bpf_maxinsns, 0, "Maximum bpf program instructions");
SYSCTL_NODE(_net_bpf, OID_AUTO, stats, CTLFLAG_RW,
    bpf_stats_sysctl, "bpf statistics portal");

static	d_open_t	bpfopen;
static	d_close_t	bpfclose;
static	d_read_t	bpfread;
static	d_write_t	bpfwrite;
static	d_ioctl_t	bpfioctl;
static	d_poll_t	bpfpoll;
static	d_kqfilter_t	bpfkqfilter;

static struct cdevsw bpf_cdevsw = {
	.d_version =	D_VERSION,
	.d_open =	bpfopen,
	.d_close =	bpfclose,
	.d_read =	bpfread,
	.d_write =	bpfwrite,
	.d_ioctl =	bpfioctl,
	.d_poll =	bpfpoll,
	.d_name =	"bpf",
	.d_kqfilter =	bpfkqfilter,
};

static struct filterops bpfread_filtops =
	{ 1, NULL, filt_bpfdetach, filt_bpfread };

static int
bpf_movein(struct uio *uio, int linktype, struct ifnet *ifp, struct mbuf **mp,
    struct sockaddr *sockp, int *hdrlen, struct bpf_insn *wfilter)
{
	const struct ieee80211_bpf_params *p;
	struct ether_header *eh;
	struct mbuf *m;
	int error;
	int len;
	int hlen;
	int slen;

	/*
	 * Build a sockaddr based on the data link layer type.
	 * We do this at this level because the ethernet header
	 * is copied directly into the data field of the sockaddr.
	 * In the case of SLIP, there is no header and the packet
	 * is forwarded as is.
	 * Also, we are careful to leave room at the front of the mbuf
	 * for the link level header.
	 */
	switch (linktype) {

	case DLT_SLIP:
		sockp->sa_family = AF_INET;
		hlen = 0;
		break;

	case DLT_EN10MB:
		sockp->sa_family = AF_UNSPEC;
		/* XXX Would MAXLINKHDR be better? */
		hlen = ETHER_HDR_LEN;
		break;

	case DLT_FDDI:
		sockp->sa_family = AF_IMPLINK;
		hlen = 0;
		break;

	case DLT_RAW:
		sockp->sa_family = AF_UNSPEC;
		hlen = 0;
		break;

	case DLT_NULL:
		/*
		 * null interface types require a 4 byte pseudo header which
		 * corresponds to the address family of the packet.
		 */
		sockp->sa_family = AF_UNSPEC;
		hlen = 4;
		break;

	case DLT_ATM_RFC1483:
		/*
		 * en atm driver requires 4-byte atm pseudo header.
		 * though it isn't standard, vpi:vci needs to be
		 * specified anyway.
		 */
		sockp->sa_family = AF_UNSPEC;
		hlen = 12;	/* XXX 4(ATM_PH) + 3(LLC) + 5(SNAP) */
		break;

	case DLT_PPP:
		sockp->sa_family = AF_UNSPEC;
		hlen = 4;	/* This should match PPP_HDRLEN */
		break;

	case DLT_IEEE802_11:		/* IEEE 802.11 wireless */
		sockp->sa_family = AF_IEEE80211;
		hlen = 0;
		break;

	case DLT_IEEE802_11_RADIO:	/* IEEE 802.11 wireless w/ phy params */
		sockp->sa_family = AF_IEEE80211;
		sockp->sa_len = 12;	/* XXX != 0 */
		hlen = sizeof(struct ieee80211_bpf_params);
		break;

	default:
		return (EIO);
	}

	len = uio->uio_resid;

	if (len - hlen > ifp->if_mtu)
		return (EMSGSIZE);

	if ((unsigned)len > MCLBYTES)
		return (EIO);

	if (len > MHLEN) {
		m = m_getcl(M_TRYWAIT, MT_DATA, M_PKTHDR);
	} else {
		MGETHDR(m, M_TRYWAIT, MT_DATA);
	}
	if (m == NULL)
		return (ENOBUFS);
	m->m_pkthdr.len = m->m_len = len;
	m->m_pkthdr.rcvif = NULL;
	*mp = m;

	if (m->m_len < hlen) {
		error = EPERM;
		goto bad;
	}

	error = uiomove(mtod(m, u_char *), len, uio);
	if (error)
		goto bad;

	slen = bpf_filter(wfilter, mtod(m, u_char *), len, len);
	if (slen == 0) {
		error = EPERM;
		goto bad;
	}

	/* Check for multicast destination */
	switch (linktype) {
	case DLT_EN10MB:
		eh = mtod(m, struct ether_header *);
		if (ETHER_IS_MULTICAST(eh->ether_dhost)) {
			if (bcmp(ifp->if_broadcastaddr, eh->ether_dhost,
			    ETHER_ADDR_LEN) == 0)
				m->m_flags |= M_BCAST;
			else
				m->m_flags |= M_MCAST;
		}
		break;
	}

	/*
	 * Make room for link header, and copy it to sockaddr
	 */
	if (hlen != 0) {
		if (sockp->sa_family == AF_IEEE80211) {
			/*
			 * Collect true length from the parameter header
			 * NB: sockp is known to be zero'd so if we do a
			 *     short copy unspecified parameters will be
			 *     zero.
			 * NB: packet may not be aligned after stripping
			 *     bpf params
			 * XXX check ibp_vers
			 */
			p = mtod(m, const struct ieee80211_bpf_params *);
			hlen = p->ibp_len;
			if (hlen > sizeof(sockp->sa_data)) {
				error = EINVAL;
				goto bad;
			}
		}
		bcopy(m->m_data, sockp->sa_data, hlen);
	}
	*hdrlen = hlen;

	return (0);
bad:
	m_freem(m);
	return (error);
}

/*
 * Attach file to the bpf interface, i.e. make d listen on bp.
 */
static void
bpf_attachd(struct bpf_d *d, struct bpf_if *bp)
{
	/*
	 * Point d at bp, and add d to the interface's list of listeners.
	 * Finally, point the driver's bpf cookie at the interface so
	 * it will divert packets to bpf.
	 */
	BPFIF_LOCK(bp);
	d->bd_bif = bp;
	LIST_INSERT_HEAD(&bp->bif_dlist, d, bd_next);

	bpf_bpfd_cnt++;
	BPFIF_UNLOCK(bp);
}

/*
 * Detach a file from its interface.
 */
static void
bpf_detachd(struct bpf_d *d)
{
	int error;
	struct bpf_if *bp;
	struct ifnet *ifp;

	bp = d->bd_bif;
	BPFIF_LOCK(bp);
	BPFD_LOCK(d);
	ifp = d->bd_bif->bif_ifp;

	/*
	 * Remove d from the interface's descriptor list.
	 */
	LIST_REMOVE(d, bd_next);

	bpf_bpfd_cnt--;
	d->bd_bif = NULL;
	BPFD_UNLOCK(d);
	BPFIF_UNLOCK(bp);

	/*
	 * Check if this descriptor had requested promiscuous mode.
	 * If so, turn it off.
	 */
	if (d->bd_promisc) {
		d->bd_promisc = 0;
		error = ifpromisc(ifp, 0);
		if (error != 0 && error != ENXIO) {
			/*
			 * ENXIO can happen if a pccard is unplugged
			 * Something is really wrong if we were able to put
			 * the driver into promiscuous mode, but can't
			 * take it out.
			 */
			if_printf(bp->bif_ifp,
				"bpf_detach: ifpromisc failed (%d)\n", error);
		}
	}
}

/*
 * Open ethernet device.  Returns ENXIO for illegal minor device number,
 * EBUSY if file is open by another process.
 */
/* ARGSUSED */
static	int
bpfopen(struct cdev *dev, int flags, int fmt, struct thread *td)
{
	struct bpf_d *d;

	mtx_lock(&bpf_mtx);
	d = dev->si_drv1;
	/*
	 * Each minor can be opened by only one process.  If the requested
	 * minor is in use, return EBUSY.
	 */
	if (d != NULL) {
		mtx_unlock(&bpf_mtx);
		return (EBUSY);
	}
	dev->si_drv1 = (struct bpf_d *)~0;	/* mark device in use */
	mtx_unlock(&bpf_mtx);

	if ((dev->si_flags & SI_NAMED) == 0)
		make_dev(&bpf_cdevsw, minor(dev), UID_ROOT, GID_WHEEL, 0600,
		    "bpf%d", dev2unit(dev));
	MALLOC(d, struct bpf_d *, sizeof(*d), M_BPF, M_WAITOK | M_ZERO);
	dev->si_drv1 = d;
	d->bd_bufsize = bpf_bufsize;
	d->bd_sig = SIGIO;
	d->bd_direction = BPF_D_INOUT;
	d->bd_pid = td->td_proc->p_pid;
#ifdef MAC
	mac_init_bpfdesc(d);
	mac_create_bpfdesc(td->td_ucred, d);
#endif
	mtx_init(&d->bd_mtx, devtoname(dev), "bpf cdev lock", MTX_DEF);
	callout_init(&d->bd_callout, CALLOUT_MPSAFE);
	knlist_init(&d->bd_sel.si_note, &d->bd_mtx, NULL, NULL, NULL);

	return (0);
}

/*
 * Close the descriptor by detaching it from its interface,
 * deallocating its buffers, and marking it free.
 */
/* ARGSUSED */
static	int
bpfclose(struct cdev *dev, int flags, int fmt, struct thread *td)
{
	struct bpf_d *d = dev->si_drv1;

	BPFD_LOCK(d);
	if (d->bd_state == BPF_WAITING)
		callout_stop(&d->bd_callout);
	d->bd_state = BPF_IDLE;
	BPFD_UNLOCK(d);
	funsetown(&d->bd_sigio);
	mtx_lock(&bpf_mtx);
	if (d->bd_bif)
		bpf_detachd(d);
	mtx_unlock(&bpf_mtx);
	selwakeuppri(&d->bd_sel, PRINET);
#ifdef MAC
	mac_destroy_bpfdesc(d);
#endif /* MAC */
	knlist_destroy(&d->bd_sel.si_note);
	bpf_freed(d);
	dev->si_drv1 = NULL;
	free(d, M_BPF);

	return (0);
}


/*
 * Rotate the packet buffers in descriptor d.  Move the store buffer
 * into the hold slot, and the free buffer into the store slot.
 * Zero the length of the new store buffer.
 */
#define ROTATE_BUFFERS(d) \
	(d)->bd_hbuf = (d)->bd_sbuf; \
	(d)->bd_hlen = (d)->bd_slen; \
	(d)->bd_sbuf = (d)->bd_fbuf; \
	(d)->bd_slen = 0; \
	(d)->bd_fbuf = NULL;
/*
 *  bpfread - read next chunk of packets from buffers
 */
static	int
bpfread(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct bpf_d *d = dev->si_drv1;
	int timed_out;
	int error;

	/*
	 * Restrict application to use a buffer the same size as
	 * as kernel buffers.
	 */
	if (uio->uio_resid != d->bd_bufsize)
		return (EINVAL);

	BPFD_LOCK(d);
	d->bd_pid = curthread->td_proc->p_pid;
	if (d->bd_state == BPF_WAITING)
		callout_stop(&d->bd_callout);
	timed_out = (d->bd_state == BPF_TIMED_OUT);
	d->bd_state = BPF_IDLE;
	/*
	 * If the hold buffer is empty, then do a timed sleep, which
	 * ends when the timeout expires or when enough packets
	 * have arrived to fill the store buffer.
	 */
	while (d->bd_hbuf == NULL) {
		if ((d->bd_immediate || timed_out) && d->bd_slen != 0) {
			/*
			 * A packet(s) either arrived since the previous
			 * read or arrived while we were asleep.
			 * Rotate the buffers and return what's here.
			 */
			ROTATE_BUFFERS(d);
			break;
		}

		/*
		 * No data is available, check to see if the bpf device
		 * is still pointed at a real interface.  If not, return
		 * ENXIO so that the userland process knows to rebind
		 * it before using it again.
		 */
		if (d->bd_bif == NULL) {
			BPFD_UNLOCK(d);
			return (ENXIO);
		}

		if (ioflag & O_NONBLOCK) {
			BPFD_UNLOCK(d);
			return (EWOULDBLOCK);
		}
		error = msleep(d, &d->bd_mtx, PRINET|PCATCH,
		     "bpf", d->bd_rtout);
		if (error == EINTR || error == ERESTART) {
			BPFD_UNLOCK(d);
			return (error);
		}
		if (error == EWOULDBLOCK) {
			/*
			 * On a timeout, return what's in the buffer,
			 * which may be nothing.  If there is something
			 * in the store buffer, we can rotate the buffers.
			 */
			if (d->bd_hbuf)
				/*
				 * We filled up the buffer in between
				 * getting the timeout and arriving
				 * here, so we don't need to rotate.
				 */
				break;

			if (d->bd_slen == 0) {
				BPFD_UNLOCK(d);
				return (0);
			}
			ROTATE_BUFFERS(d);
			break;
		}
	}
	/*
	 * At this point, we know we have something in the hold slot.
	 */
	BPFD_UNLOCK(d);

	/*
	 * Move data from hold buffer into user space.
	 * We know the entire buffer is transferred since
	 * we checked above that the read buffer is bpf_bufsize bytes.
	 */
	error = uiomove(d->bd_hbuf, d->bd_hlen, uio);

	BPFD_LOCK(d);
	d->bd_fbuf = d->bd_hbuf;
	d->bd_hbuf = NULL;
	d->bd_hlen = 0;
	BPFD_UNLOCK(d);

	return (error);
}


/*
 * If there are processes sleeping on this descriptor, wake them up.
 */
static __inline void
bpf_wakeup(struct bpf_d *d)
{

	BPFD_LOCK_ASSERT(d);
	if (d->bd_state == BPF_WAITING) {
		callout_stop(&d->bd_callout);
		d->bd_state = BPF_IDLE;
	}
	wakeup(d);
	if (d->bd_async && d->bd_sig && d->bd_sigio)
		pgsigio(&d->bd_sigio, d->bd_sig, 0);

	selwakeuppri(&d->bd_sel, PRINET);
	KNOTE_LOCKED(&d->bd_sel.si_note, 0);
}

static void
bpf_timed_out(void *arg)
{
	struct bpf_d *d = (struct bpf_d *)arg;

	BPFD_LOCK(d);
	if (d->bd_state == BPF_WAITING) {
		d->bd_state = BPF_TIMED_OUT;
		if (d->bd_slen != 0)
			bpf_wakeup(d);
	}
	BPFD_UNLOCK(d);
}

static int
bpfwrite(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct bpf_d *d = dev->si_drv1;
	struct ifnet *ifp;
	struct mbuf *m, *mc;
	struct sockaddr dst;
	int error, hlen;

	d->bd_pid = curthread->td_proc->p_pid;
	if (d->bd_bif == NULL)
		return (ENXIO);

	ifp = d->bd_bif->bif_ifp;

	if ((ifp->if_flags & IFF_UP) == 0)
		return (ENETDOWN);

	if (uio->uio_resid == 0)
		return (0);

	bzero(&dst, sizeof(dst));
	m = NULL;
	hlen = 0;
	error = bpf_movein(uio, (int)d->bd_bif->bif_dlt, ifp,
	    &m, &dst, &hlen, d->bd_wfilter);
	if (error)
		return (error);

	if (d->bd_hdrcmplt)
		dst.sa_family = pseudo_AF_HDRCMPLT;

	if (d->bd_feedback) {
		mc = m_dup(m, M_DONTWAIT);
		if (mc != NULL)
			mc->m_pkthdr.rcvif = ifp;
		/* XXX Do not return the same packet twice. */
		if (d->bd_direction == BPF_D_INOUT)
			m->m_flags |= M_SKIP_BPF;
	} else
		mc = NULL;

	m->m_pkthdr.len -= hlen;
	m->m_len -= hlen;
	m->m_data += hlen;	/* XXX */

#ifdef MAC
	BPFD_LOCK(d);
	mac_create_mbuf_from_bpfdesc(d, m);
	if (mc != NULL)
		mac_create_mbuf_from_bpfdesc(d, mc);
	BPFD_UNLOCK(d);
#endif

	error = (*ifp->if_output)(ifp, m, &dst, NULL);

	if (mc != NULL) {
		if (error == 0)
			(*ifp->if_input)(ifp, mc);
		else
			m_freem(mc);
	}

	return (error);
}

/*
 * Reset a descriptor by flushing its packet buffer and clearing the
 * receive and drop counts.
 */
static void
reset_d(struct bpf_d *d)
{

	mtx_assert(&d->bd_mtx, MA_OWNED);
	if (d->bd_hbuf) {
		/* Free the hold buffer. */
		d->bd_fbuf = d->bd_hbuf;
		d->bd_hbuf = NULL;
	}
	d->bd_slen = 0;
	d->bd_hlen = 0;
	d->bd_rcount = 0;
	d->bd_dcount = 0;
	d->bd_fcount = 0;
}

/*
 *  FIONREAD		Check for read packet available.
 *  SIOCGIFADDR		Get interface address - convenient hook to driver.
 *  BIOCGBLEN		Get buffer len [for read()].
 *  BIOCSETF		Set ethernet read filter.
 *  BIOCSETWF		Set ethernet write filter.
 *  BIOCFLUSH		Flush read packet buffer.
 *  BIOCPROMISC		Put interface into promiscuous mode.
 *  BIOCGDLT		Get link layer type.
 *  BIOCGETIF		Get interface name.
 *  BIOCSETIF		Set interface.
 *  BIOCSRTIMEOUT	Set read timeout.
 *  BIOCGRTIMEOUT	Get read timeout.
 *  BIOCGSTATS		Get packet stats.
 *  BIOCIMMEDIATE	Set immediate mode.
 *  BIOCVERSION		Get filter language version.
 *  BIOCGHDRCMPLT	Get "header already complete" flag
 *  BIOCSHDRCMPLT	Set "header already complete" flag
 *  BIOCGDIRECTION	Get packet direction flag
 *  BIOCSDIRECTION	Set packet direction flag
 *  BIOCLOCK		Set "locked" flag
 *  BIOCFEEDBACK	Set packet feedback mode.
 */
/* ARGSUSED */
static	int
bpfioctl(struct cdev *dev, u_long cmd, caddr_t addr, int flags,
    struct thread *td)
{
	struct bpf_d *d = dev->si_drv1;
	int error = 0;

	/* 
	 * Refresh PID associated with this descriptor.
	 */
	BPFD_LOCK(d);
	d->bd_pid = td->td_proc->p_pid;
	if (d->bd_state == BPF_WAITING)
		callout_stop(&d->bd_callout);
	d->bd_state = BPF_IDLE;
	BPFD_UNLOCK(d);

	if (d->bd_locked == 1) {
		switch (cmd) {
		case BIOCGBLEN:
		case BIOCFLUSH:
		case BIOCGDLT:
		case BIOCGDLTLIST: 
		case BIOCGETIF:
		case BIOCGRTIMEOUT:
		case BIOCGSTATS:
		case BIOCVERSION:
		case BIOCGRSIG:
		case BIOCGHDRCMPLT:
		case BIOCFEEDBACK:
		case FIONREAD:
		case BIOCLOCK:
		case BIOCSRTIMEOUT:
		case BIOCIMMEDIATE:
		case TIOCGPGRP:
			break;
		default:
			return (EPERM);
		}
	}
	switch (cmd) {

	default:
		error = EINVAL;
		break;

	/*
	 * Check for read packet available.
	 */
	case FIONREAD:
		{
			int n;

			BPFD_LOCK(d);
			n = d->bd_slen;
			if (d->bd_hbuf)
				n += d->bd_hlen;
			BPFD_UNLOCK(d);

			*(int *)addr = n;
			break;
		}

	case SIOCGIFADDR:
		{
			struct ifnet *ifp;

			if (d->bd_bif == NULL)
				error = EINVAL;
			else {
				ifp = d->bd_bif->bif_ifp;
				error = (*ifp->if_ioctl)(ifp, cmd, addr);
			}
			break;
		}

	/*
	 * Get buffer len [for read()].
	 */
	case BIOCGBLEN:
		*(u_int *)addr = d->bd_bufsize;
		break;

	/*
	 * Set buffer length.
	 */
	case BIOCSBLEN:
		if (d->bd_bif != NULL)
			error = EINVAL;
		else {
			u_int size = *(u_int *)addr;

			if (size > bpf_maxbufsize)
				*(u_int *)addr = size = bpf_maxbufsize;
			else if (size < BPF_MINBUFSIZE)
				*(u_int *)addr = size = BPF_MINBUFSIZE;
			d->bd_bufsize = size;
		}
		break;

	/*
	 * Set link layer read filter.
	 */
	case BIOCSETF:
	case BIOCSETWF:
		error = bpf_setf(d, (struct bpf_program *)addr, cmd);
		break;

	/*
	 * Flush read packet buffer.
	 */
	case BIOCFLUSH:
		BPFD_LOCK(d);
		reset_d(d);
		BPFD_UNLOCK(d);
		break;

	/*
	 * Put interface into promiscuous mode.
	 */
	case BIOCPROMISC:
		if (d->bd_bif == NULL) {
			/*
			 * No interface attached yet.
			 */
			error = EINVAL;
			break;
		}
		if (d->bd_promisc == 0) {
			error = ifpromisc(d->bd_bif->bif_ifp, 1);
			if (error == 0)
				d->bd_promisc = 1;
		}
		break;

	/*
	 * Get current data link type.
	 */
	case BIOCGDLT:
		if (d->bd_bif == NULL)
			error = EINVAL;
		else
			*(u_int *)addr = d->bd_bif->bif_dlt;
		break;

	/*
	 * Get a list of supported data link types.
	 */
	case BIOCGDLTLIST:
		if (d->bd_bif == NULL)
			error = EINVAL;
		else
			error = bpf_getdltlist(d, (struct bpf_dltlist *)addr);
		break;

	/*
	 * Set data link type.
	 */
	case BIOCSDLT:
		if (d->bd_bif == NULL)
			error = EINVAL;
		else
			error = bpf_setdlt(d, *(u_int *)addr);
		break;

	/*
	 * Get interface name.
	 */
	case BIOCGETIF:
		if (d->bd_bif == NULL)
			error = EINVAL;
		else {
			struct ifnet *const ifp = d->bd_bif->bif_ifp;
			struct ifreq *const ifr = (struct ifreq *)addr;

			strlcpy(ifr->ifr_name, ifp->if_xname,
			    sizeof(ifr->ifr_name));
		}
		break;

	/*
	 * Set interface.
	 */
	case BIOCSETIF:
		error = bpf_setif(d, (struct ifreq *)addr);
		break;

	/*
	 * Set read timeout.
	 */
	case BIOCSRTIMEOUT:
		{
			struct timeval *tv = (struct timeval *)addr;

			/*
			 * Subtract 1 tick from tvtohz() since this isn't
			 * a one-shot timer.
			 */
			if ((error = itimerfix(tv)) == 0)
				d->bd_rtout = tvtohz(tv) - 1;
			break;
		}

	/*
	 * Get read timeout.
	 */
	case BIOCGRTIMEOUT:
		{
			struct timeval *tv = (struct timeval *)addr;

			tv->tv_sec = d->bd_rtout / hz;
			tv->tv_usec = (d->bd_rtout % hz) * tick;
			break;
		}

	/*
	 * Get packet stats.
	 */
	case BIOCGSTATS:
		{
			struct bpf_stat *bs = (struct bpf_stat *)addr;

			bs->bs_recv = d->bd_rcount;
			bs->bs_drop = d->bd_dcount;
			break;
		}

	/*
	 * Set immediate mode.
	 */
	case BIOCIMMEDIATE:
		d->bd_immediate = *(u_int *)addr;
		break;

	case BIOCVERSION:
		{
			struct bpf_version *bv = (struct bpf_version *)addr;

			bv->bv_major = BPF_MAJOR_VERSION;
			bv->bv_minor = BPF_MINOR_VERSION;
			break;
		}

	/*
	 * Get "header already complete" flag
	 */
	case BIOCGHDRCMPLT:
		*(u_int *)addr = d->bd_hdrcmplt;
		break;

	/*
	 * Set "header already complete" flag
	 */
	case BIOCSHDRCMPLT:
		d->bd_hdrcmplt = *(u_int *)addr ? 1 : 0;
		break;

	/*
	 * Get packet direction flag
	 */
	case BIOCGDIRECTION:
		*(u_int *)addr = d->bd_direction;
		break;

	/*
	 * Set packet direction flag
	 */
	case BIOCSDIRECTION:
		{
			u_int	direction;

			direction = *(u_int *)addr;
			switch (direction) {
			case BPF_D_IN:
			case BPF_D_INOUT:
			case BPF_D_OUT:
				d->bd_direction = direction;
				break;
			default:
				error = EINVAL;
			}
		}
		break;

	case BIOCFEEDBACK:
		d->bd_feedback = *(u_int *)addr;
		break;

	case BIOCLOCK:
		d->bd_locked = 1;
		break;

	case FIONBIO:		/* Non-blocking I/O */
		break;

	case FIOASYNC:		/* Send signal on receive packets */
		d->bd_async = *(int *)addr;
		break;

	case FIOSETOWN:
		error = fsetown(*(int *)addr, &d->bd_sigio);
		break;

	case FIOGETOWN:
		*(int *)addr = fgetown(&d->bd_sigio);
		break;

	/* This is deprecated, FIOSETOWN should be used instead. */
	case TIOCSPGRP:
		error = fsetown(-(*(int *)addr), &d->bd_sigio);
		break;

	/* This is deprecated, FIOGETOWN should be used instead. */
	case TIOCGPGRP:
		*(int *)addr = -fgetown(&d->bd_sigio);
		break;

	case BIOCSRSIG:		/* Set receive signal */
		{
			u_int sig;

			sig = *(u_int *)addr;

			if (sig >= NSIG)
				error = EINVAL;
			else
				d->bd_sig = sig;
			break;
		}
	case BIOCGRSIG:
		*(u_int *)addr = d->bd_sig;
		break;
	}
	return (error);
}

/*
 * Set d's packet filter program to fp.  If this file already has a filter,
 * free it and replace it.  Returns EINVAL for bogus requests.
 */
static int
bpf_setf(struct bpf_d *d, struct bpf_program *fp, u_long cmd)
{
	struct bpf_insn *fcode, *old;
	u_int wfilter, flen, size;
#ifdef BPF_JITTER
	bpf_jit_filter *ofunc;
#endif

	if (cmd == BIOCSETWF) {
		old = d->bd_wfilter;
		wfilter = 1;
#ifdef BPF_JITTER
		ofunc = NULL;
#endif
	} else {
		wfilter = 0;
		old = d->bd_rfilter;
#ifdef BPF_JITTER
		ofunc = d->bd_bfilter;
#endif
	}
	if (fp->bf_insns == NULL) {
		if (fp->bf_len != 0)
			return (EINVAL);
		BPFD_LOCK(d);
		if (wfilter)
			d->bd_wfilter = NULL;
		else {
			d->bd_rfilter = NULL;
#ifdef BPF_JITTER
			d->bd_bfilter = NULL;
#endif
		}
		reset_d(d);
		BPFD_UNLOCK(d);
		if (old != NULL)
			free((caddr_t)old, M_BPF);
#ifdef BPF_JITTER
		if (ofunc != NULL)
			bpf_destroy_jit_filter(ofunc);
#endif
		return (0);
	}
	flen = fp->bf_len;
	if (flen > bpf_maxinsns)
		return (EINVAL);

	size = flen * sizeof(*fp->bf_insns);
	fcode = (struct bpf_insn *)malloc(size, M_BPF, M_WAITOK);
	if (copyin((caddr_t)fp->bf_insns, (caddr_t)fcode, size) == 0 &&
	    bpf_validate(fcode, (int)flen)) {
		BPFD_LOCK(d);
		if (wfilter)
			d->bd_wfilter = fcode;
		else {
			d->bd_rfilter = fcode;
#ifdef BPF_JITTER
			d->bd_bfilter = bpf_jitter(fcode, flen);
#endif
		}
		reset_d(d);
		BPFD_UNLOCK(d);
		if (old != NULL)
			free((caddr_t)old, M_BPF);
#ifdef BPF_JITTER
		if (ofunc != NULL)
			bpf_destroy_jit_filter(ofunc);
#endif

		return (0);
	}
	free((caddr_t)fcode, M_BPF);
	return (EINVAL);
}

/*
 * Detach a file from its current interface (if attached at all) and attach
 * to the interface indicated by the name stored in ifr.
 * Return an errno or 0.
 */
static int
bpf_setif(struct bpf_d *d, struct ifreq *ifr)
{
	struct bpf_if *bp;
	struct ifnet *theywant;

	theywant = ifunit(ifr->ifr_name);
	if (theywant == NULL || theywant->if_bpf == NULL)
		return (ENXIO);

	bp = theywant->if_bpf;
	/*
	 * Allocate the packet buffers if we need to.
	 * If we're already attached to requested interface,
	 * just flush the buffer.
	 */
	if (d->bd_sbuf == NULL)
		bpf_allocbufs(d);
	if (bp != d->bd_bif) {
		if (d->bd_bif)
			/*
			 * Detach if attached to something else.
			 */
			bpf_detachd(d);

		bpf_attachd(d, bp);
	}
	BPFD_LOCK(d);
	reset_d(d);
	BPFD_UNLOCK(d);
	return (0);
}

/*
 * Support for select() and poll() system calls
 *
 * Return true iff the specific operation will not block indefinitely.
 * Otherwise, return false but make a note that a selwakeup() must be done.
 */
static int
bpfpoll(struct cdev *dev, int events, struct thread *td)
{
	struct bpf_d *d;
	int revents;

	d = dev->si_drv1;
	if (d->bd_bif == NULL)
		return (ENXIO);

	/*
	 * Refresh PID associated with this descriptor.
	 */
	revents = events & (POLLOUT | POLLWRNORM);
	BPFD_LOCK(d);
	d->bd_pid = td->td_proc->p_pid;
	if (events & (POLLIN | POLLRDNORM)) {
		if (bpf_ready(d))
			revents |= events & (POLLIN | POLLRDNORM);
		else {
			selrecord(td, &d->bd_sel);
			/* Start the read timeout if necessary. */
			if (d->bd_rtout > 0 && d->bd_state == BPF_IDLE) {
				callout_reset(&d->bd_callout, d->bd_rtout,
				    bpf_timed_out, d);
				d->bd_state = BPF_WAITING;
			}
		}
	}
	BPFD_UNLOCK(d);
	return (revents);
}

/*
 * Support for kevent() system call.  Register EVFILT_READ filters and
 * reject all others.
 */
int
bpfkqfilter(struct cdev *dev, struct knote *kn)
{
	struct bpf_d *d = (struct bpf_d *)dev->si_drv1;

	if (kn->kn_filter != EVFILT_READ)
		return (1);

	/* 
	 * Refresh PID associated with this descriptor.
	 */
	BPFD_LOCK(d);
	d->bd_pid = curthread->td_proc->p_pid;
	kn->kn_fop = &bpfread_filtops;
	kn->kn_hook = d;
	knlist_add(&d->bd_sel.si_note, kn, 1);
	BPFD_UNLOCK(d);

	return (0);
}

static void
filt_bpfdetach(struct knote *kn)
{
	struct bpf_d *d = (struct bpf_d *)kn->kn_hook;

	knlist_remove(&d->bd_sel.si_note, kn, 0);
}

static int
filt_bpfread(struct knote *kn, long hint)
{
	struct bpf_d *d = (struct bpf_d *)kn->kn_hook;
	int ready;

	BPFD_LOCK_ASSERT(d);
	ready = bpf_ready(d);
	if (ready) {
		kn->kn_data = d->bd_slen;
		if (d->bd_hbuf)
			kn->kn_data += d->bd_hlen;
	}
	else if (d->bd_rtout > 0 && d->bd_state == BPF_IDLE) {
		callout_reset(&d->bd_callout, d->bd_rtout,
		    bpf_timed_out, d);
		d->bd_state = BPF_WAITING;
	}

	return (ready);
}

/*
 * Incoming linkage from device drivers.  Process the packet pkt, of length
 * pktlen, which is stored in a contiguous buffer.  The packet is parsed
 * by each process' filter, and if accepted, stashed into the corresponding
 * buffer.
 */
void
bpf_tap(struct bpf_if *bp, u_char *pkt, u_int pktlen)
{
	struct bpf_d *d;
	u_int slen;
	int gottime;
	struct timeval tv;

	gottime = 0;
	BPFIF_LOCK(bp);
	LIST_FOREACH(d, &bp->bif_dlist, bd_next) {
		BPFD_LOCK(d);
		++d->bd_rcount;
#ifdef BPF_JITTER
		if (bpf_jitter_enable != 0 && d->bd_bfilter != NULL)
			slen = (*(d->bd_bfilter->func))(pkt, pktlen, pktlen);
		else
#endif
		slen = bpf_filter(d->bd_rfilter, pkt, pktlen, pktlen);
		if (slen != 0) {
			d->bd_fcount++;
			if (!gottime) {
				microtime(&tv);
				gottime = 1;
			}
#ifdef MAC
			if (mac_check_bpfdesc_receive(d, bp->bif_ifp) == 0)
#endif
				catchpacket(d, pkt, pktlen, slen, bcopy, &tv);
		}
		BPFD_UNLOCK(d);
	}
	BPFIF_UNLOCK(bp);
}

/*
 * Copy data from an mbuf chain into a buffer.  This code is derived
 * from m_copydata in sys/uipc_mbuf.c.
 */
static void
bpf_mcopy(const void *src_arg, void *dst_arg, size_t len)
{
	const struct mbuf *m;
	u_int count;
	u_char *dst;

	m = src_arg;
	dst = dst_arg;
	while (len > 0) {
		if (m == NULL)
			panic("bpf_mcopy");
		count = min(m->m_len, len);
		bcopy(mtod(m, void *), dst, count);
		m = m->m_next;
		dst += count;
		len -= count;
	}
}

#define	BPF_CHECK_DIRECTION(d, m) \
	if (((d)->bd_direction == BPF_D_IN && (m)->m_pkthdr.rcvif == NULL) || \
	    ((d)->bd_direction == BPF_D_OUT && (m)->m_pkthdr.rcvif != NULL))

/*
 * Incoming linkage from device drivers, when packet is in an mbuf chain.
 */
void
bpf_mtap(struct bpf_if *bp, struct mbuf *m)
{
	struct bpf_d *d;
	u_int pktlen, slen;
	int gottime;
	struct timeval tv;

	if (m->m_flags & M_SKIP_BPF) {
		m->m_flags &= ~M_SKIP_BPF;
		return;
	}

	gottime = 0;

	pktlen = m_length(m, NULL);

	BPFIF_LOCK(bp);
	LIST_FOREACH(d, &bp->bif_dlist, bd_next) {
		BPF_CHECK_DIRECTION(d, m)
			continue;
		BPFD_LOCK(d);
		++d->bd_rcount;
#ifdef BPF_JITTER
		/* XXX We cannot handle multiple mbufs. */
		if (bpf_jitter_enable != 0 && d->bd_bfilter != NULL &&
		    m->m_next == NULL)
			slen = (*(d->bd_bfilter->func))(mtod(m, u_char *),
			    pktlen, pktlen);
		else
#endif
		slen = bpf_filter(d->bd_rfilter, (u_char *)m, pktlen, 0);
		if (slen != 0) {
			d->bd_fcount++;
			if (!gottime) {
				microtime(&tv);
				gottime = 1;
			}
#ifdef MAC
			if (mac_check_bpfdesc_receive(d, bp->bif_ifp) == 0)
#endif
				catchpacket(d, (u_char *)m, pktlen, slen,
				    bpf_mcopy, &tv);
		}
		BPFD_UNLOCK(d);
	}
	BPFIF_UNLOCK(bp);
}

/*
 * Incoming linkage from device drivers, when packet is in
 * an mbuf chain and to be prepended by a contiguous header.
 */
void
bpf_mtap2(struct bpf_if *bp, void *data, u_int dlen, struct mbuf *m)
{
	struct mbuf mb;
	struct bpf_d *d;
	u_int pktlen, slen;
	int gottime;
	struct timeval tv;

	if (m->m_flags & M_SKIP_BPF) {
		m->m_flags &= ~M_SKIP_BPF;
		return;
	}

	gottime = 0;

	pktlen = m_length(m, NULL);
	/*
	 * Craft on-stack mbuf suitable for passing to bpf_filter.
	 * Note that we cut corners here; we only setup what's
	 * absolutely needed--this mbuf should never go anywhere else.
	 */
	mb.m_next = m;
	mb.m_data = data;
	mb.m_len = dlen;
	pktlen += dlen;

	BPFIF_LOCK(bp);
	LIST_FOREACH(d, &bp->bif_dlist, bd_next) {
		BPF_CHECK_DIRECTION(d, m)
			continue;
		BPFD_LOCK(d);
		++d->bd_rcount;
		slen = bpf_filter(d->bd_rfilter, (u_char *)&mb, pktlen, 0);
		if (slen != 0) {
			d->bd_fcount++;
			if (!gottime) {
				microtime(&tv);
				gottime = 1;
			}
#ifdef MAC
			if (mac_check_bpfdesc_receive(d, bp->bif_ifp) == 0)
#endif
				catchpacket(d, (u_char *)&mb, pktlen, slen,
				    bpf_mcopy, &tv);
		}
		BPFD_UNLOCK(d);
	}
	BPFIF_UNLOCK(bp);
}

#undef	BPF_CHECK_DIRECTION

/*
 * Move the packet data from interface memory (pkt) into the
 * store buffer.  "cpfn" is the routine called to do the actual data
 * transfer.  bcopy is passed in to copy contiguous chunks, while
 * bpf_mcopy is passed in to copy mbuf chains.  In the latter case,
 * pkt is really an mbuf.
 */
static void
catchpacket(struct bpf_d *d, u_char *pkt, u_int pktlen, u_int snaplen,
    void (*cpfn)(const void *, void *, size_t), struct timeval *tv)
{
	struct bpf_hdr *hp;
	int totlen, curlen;
	int hdrlen = d->bd_bif->bif_hdrlen;
	int do_wakeup = 0;

	BPFD_LOCK_ASSERT(d);
	/*
	 * Figure out how many bytes to move.  If the packet is
	 * greater or equal to the snapshot length, transfer that
	 * much.  Otherwise, transfer the whole packet (unless
	 * we hit the buffer size limit).
	 */
	totlen = hdrlen + min(snaplen, pktlen);
	if (totlen > d->bd_bufsize)
		totlen = d->bd_bufsize;

	/*
	 * Round up the end of the previous packet to the next longword.
	 */
	curlen = BPF_WORDALIGN(d->bd_slen);
	if (curlen + totlen > d->bd_bufsize) {
		/*
		 * This packet will overflow the storage buffer.
		 * Rotate the buffers if we can, then wakeup any
		 * pending reads.
		 */
		if (d->bd_fbuf == NULL) {
			/*
			 * We haven't completed the previous read yet,
			 * so drop the packet.
			 */
			++d->bd_dcount;
			return;
		}
		ROTATE_BUFFERS(d);
		do_wakeup = 1;
		curlen = 0;
	}
	else if (d->bd_immediate || d->bd_state == BPF_TIMED_OUT)
		/*
		 * Immediate mode is set, or the read timeout has
		 * already expired during a select call.  A packet
		 * arrived, so the reader should be woken up.
		 */
		do_wakeup = 1;

	/*
	 * Append the bpf header.
	 */
	hp = (struct bpf_hdr *)(d->bd_sbuf + curlen);
	hp->bh_tstamp = *tv;
	hp->bh_datalen = pktlen;
	hp->bh_hdrlen = hdrlen;
	/*
	 * Copy the packet data into the store buffer and update its length.
	 */
	(*cpfn)(pkt, (u_char *)hp + hdrlen, (hp->bh_caplen = totlen - hdrlen));
	d->bd_slen = curlen + totlen;

	if (do_wakeup)
		bpf_wakeup(d);
}

/*
 * Initialize all nonzero fields of a descriptor.
 */
static void
bpf_allocbufs(struct bpf_d *d)
{

	KASSERT(d->bd_fbuf == NULL, ("bpf_allocbufs: bd_fbuf != NULL"));
	KASSERT(d->bd_sbuf == NULL, ("bpf_allocbufs: bd_sbuf != NULL"));
	KASSERT(d->bd_hbuf == NULL, ("bpf_allocbufs: bd_hbuf != NULL"));

	d->bd_fbuf = (caddr_t)malloc(d->bd_bufsize, M_BPF, M_WAITOK);
	d->bd_sbuf = (caddr_t)malloc(d->bd_bufsize, M_BPF, M_WAITOK);
	d->bd_slen = 0;
	d->bd_hlen = 0;
}

/*
 * Free buffers currently in use by a descriptor.
 * Called on close.
 */
static void
bpf_freed(struct bpf_d *d)
{
	/*
	 * We don't need to lock out interrupts since this descriptor has
	 * been detached from its interface and it yet hasn't been marked
	 * free.
	 */
	if (d->bd_sbuf != NULL) {
		free(d->bd_sbuf, M_BPF);
		if (d->bd_hbuf != NULL)
			free(d->bd_hbuf, M_BPF);
		if (d->bd_fbuf != NULL)
			free(d->bd_fbuf, M_BPF);
	}
	if (d->bd_rfilter) {
		free((caddr_t)d->bd_rfilter, M_BPF);
#ifdef BPF_JITTER
		bpf_destroy_jit_filter(d->bd_bfilter);
#endif
	}
	if (d->bd_wfilter)
		free((caddr_t)d->bd_wfilter, M_BPF);
	mtx_destroy(&d->bd_mtx);
}

/*
 * Attach an interface to bpf.  dlt is the link layer type; hdrlen is the
 * fixed size of the link header (variable length headers not yet supported).
 */
void
bpfattach(struct ifnet *ifp, u_int dlt, u_int hdrlen)
{

	bpfattach2(ifp, dlt, hdrlen, &ifp->if_bpf);
}

/*
 * Attach an interface to bpf.  ifp is a pointer to the structure
 * defining the interface to be attached, dlt is the link layer type,
 * and hdrlen is the fixed size of the link header (variable length
 * headers are not yet supporrted).
 */
void
bpfattach2(struct ifnet *ifp, u_int dlt, u_int hdrlen, struct bpf_if **driverp)
{
	struct bpf_if *bp;

	bp = malloc(sizeof(*bp), M_BPF, M_NOWAIT | M_ZERO);
	if (bp == NULL)
		panic("bpfattach");

	LIST_INIT(&bp->bif_dlist);
	bp->bif_ifp = ifp;
	bp->bif_dlt = dlt;
	mtx_init(&bp->bif_mtx, "bpf interface lock", NULL, MTX_DEF);
	KASSERT(*driverp == NULL, ("bpfattach2: driverp already initialized"));
	*driverp = bp;

	mtx_lock(&bpf_mtx);
	LIST_INSERT_HEAD(&bpf_iflist, bp, bif_next);
	mtx_unlock(&bpf_mtx);

	/*
	 * Compute the length of the bpf header.  This is not necessarily
	 * equal to SIZEOF_BPF_HDR because we want to insert spacing such
	 * that the network layer header begins on a longword boundary (for
	 * performance reasons and to alleviate alignment restrictions).
	 */
	bp->bif_hdrlen = BPF_WORDALIGN(hdrlen + SIZEOF_BPF_HDR) - hdrlen;

	if (bootverbose)
		if_printf(ifp, "bpf attached\n");
}

/*
 * Detach bpf from an interface.  This involves detaching each descriptor
 * associated with the interface, and leaving bd_bif NULL.  Notify each
 * descriptor as it's detached so that any sleepers wake up and get
 * ENXIO.
 */
void
bpfdetach(struct ifnet *ifp)
{
	struct bpf_if	*bp;
	struct bpf_d	*d;

	/* Locate BPF interface information */
	mtx_lock(&bpf_mtx);
	LIST_FOREACH(bp, &bpf_iflist, bif_next) {
		if (ifp == bp->bif_ifp)
			break;
	}

	/* Interface wasn't attached */
	if ((bp == NULL) || (bp->bif_ifp == NULL)) {
		mtx_unlock(&bpf_mtx);
		printf("bpfdetach: %s was not attached\n", ifp->if_xname);
		return;
	}

	LIST_REMOVE(bp, bif_next);
	mtx_unlock(&bpf_mtx);

	while ((d = LIST_FIRST(&bp->bif_dlist)) != NULL) {
		bpf_detachd(d);
		BPFD_LOCK(d);
		bpf_wakeup(d);
		BPFD_UNLOCK(d);
	}

	mtx_destroy(&bp->bif_mtx);
	free(bp, M_BPF);
}

/*
 * Get a list of available data link type of the interface.
 */
static int
bpf_getdltlist(struct bpf_d *d, struct bpf_dltlist *bfl)
{
	int n, error;
	struct ifnet *ifp;
	struct bpf_if *bp;

	ifp = d->bd_bif->bif_ifp;
	n = 0;
	error = 0;
	mtx_lock(&bpf_mtx);
	LIST_FOREACH(bp, &bpf_iflist, bif_next) {
		if (bp->bif_ifp != ifp)
			continue;
		if (bfl->bfl_list != NULL) {
			if (n >= bfl->bfl_len) {
				mtx_unlock(&bpf_mtx);
				return (ENOMEM);
			}
			error = copyout(&bp->bif_dlt,
			    bfl->bfl_list + n, sizeof(u_int));
		}
		n++;
	}
	mtx_unlock(&bpf_mtx);
	bfl->bfl_len = n;
	return (error);
}

/*
 * Set the data link type of a BPF instance.
 */
static int
bpf_setdlt(struct bpf_d *d, u_int dlt)
{
	int error, opromisc;
	struct ifnet *ifp;
	struct bpf_if *bp;

	if (d->bd_bif->bif_dlt == dlt)
		return (0);
	ifp = d->bd_bif->bif_ifp;
	mtx_lock(&bpf_mtx);
	LIST_FOREACH(bp, &bpf_iflist, bif_next) {
		if (bp->bif_ifp == ifp && bp->bif_dlt == dlt)
			break;
	}
	mtx_unlock(&bpf_mtx);
	if (bp != NULL) {
		opromisc = d->bd_promisc;
		bpf_detachd(d);
		bpf_attachd(d, bp);
		BPFD_LOCK(d);
		reset_d(d);
		BPFD_UNLOCK(d);
		if (opromisc) {
			error = ifpromisc(bp->bif_ifp, 1);
			if (error)
				if_printf(bp->bif_ifp,
					"bpf_setdlt: ifpromisc failed (%d)\n",
					error);
			else
				d->bd_promisc = 1;
		}
	}
	return (bp == NULL ? EINVAL : 0);
}

static void
bpf_clone(void *arg, struct ucred *cred, char *name, int namelen,
    struct cdev **dev)
{
	int u;

	if (*dev != NULL)
		return;
	if (dev_stdclone(name, NULL, "bpf", &u) != 1)
		return;
	*dev = make_dev(&bpf_cdevsw, unit2minor(u), UID_ROOT, GID_WHEEL, 0600,
	    "bpf%d", u);
	dev_ref(*dev);
	(*dev)->si_flags |= SI_CHEAPCLONE;
	return;
}

static void
bpf_drvinit(void *unused)
{

	mtx_init(&bpf_mtx, "bpf global lock", NULL, MTX_DEF);
	LIST_INIT(&bpf_iflist);
	EVENTHANDLER_REGISTER(dev_clone, bpf_clone, 0, 1000);
}

static void
bpfstats_fill_xbpf(struct xbpf_d *d, struct bpf_d *bd)
{

	bzero(d, sizeof(*d));
	BPFD_LOCK_ASSERT(bd);
	d->bd_immediate = bd->bd_immediate;
	d->bd_promisc = bd->bd_promisc;
	d->bd_hdrcmplt = bd->bd_hdrcmplt;
	d->bd_direction = bd->bd_direction;
	d->bd_feedback = bd->bd_feedback;
	d->bd_async = bd->bd_async;
	d->bd_rcount = bd->bd_rcount;
	d->bd_dcount = bd->bd_dcount;
	d->bd_fcount = bd->bd_fcount;
	d->bd_sig = bd->bd_sig;
	d->bd_slen = bd->bd_slen;
	d->bd_hlen = bd->bd_hlen;
	d->bd_bufsize = bd->bd_bufsize;
	d->bd_pid = bd->bd_pid;
	strlcpy(d->bd_ifname,
	    bd->bd_bif->bif_ifp->if_xname, IFNAMSIZ);
	d->bd_locked = bd->bd_locked;
}

static int
bpf_stats_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct xbpf_d *xbdbuf, *xbd;
	int index, error;
	struct bpf_if *bp;
	struct bpf_d *bd;

	/*
	 * XXX This is not technically correct. It is possible for non
	 * privileged users to open bpf devices. It would make sense
	 * if the users who opened the devices were able to retrieve
	 * the statistics for them, too.
	 */
	error = priv_check(req->td, PRIV_NET_BPF);
	if (error)
		return (error);
	if (req->oldptr == NULL)
		return (SYSCTL_OUT(req, 0, bpf_bpfd_cnt * sizeof(*xbd)));
	if (bpf_bpfd_cnt == 0)
		return (SYSCTL_OUT(req, 0, 0));
	xbdbuf = malloc(req->oldlen, M_BPF, M_WAITOK);
	mtx_lock(&bpf_mtx);
	if (req->oldlen < (bpf_bpfd_cnt * sizeof(*xbd))) {
		mtx_unlock(&bpf_mtx);
		free(xbdbuf, M_BPF);
		return (ENOMEM);
	}
	index = 0;
	LIST_FOREACH(bp, &bpf_iflist, bif_next) {
		BPFIF_LOCK(bp);
		LIST_FOREACH(bd, &bp->bif_dlist, bd_next) {
			xbd = &xbdbuf[index++];
			BPFD_LOCK(bd);
			bpfstats_fill_xbpf(xbd, bd);
			BPFD_UNLOCK(bd);
		}
		BPFIF_UNLOCK(bp);
	}
	mtx_unlock(&bpf_mtx);
	error = SYSCTL_OUT(req, xbdbuf, index * sizeof(*xbd));
	free(xbdbuf, M_BPF);
	return (error);
}

SYSINIT(bpfdev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE,bpf_drvinit,NULL)

#else /* !DEV_BPF && !NETGRAPH_BPF */
/*
 * NOP stubs to allow bpf-using drivers to load and function.
 *
 * A 'better' implementation would allow the core bpf functionality
 * to be loaded at runtime.
 */
static struct bpf_if bp_null;

void
bpf_tap(struct bpf_if *bp, u_char *pkt, u_int pktlen)
{
}

void
bpf_mtap(struct bpf_if *bp, struct mbuf *m)
{
}

void
bpf_mtap2(struct bpf_if *bp, void *d, u_int l, struct mbuf *m)
{
}

void
bpfattach(struct ifnet *ifp, u_int dlt, u_int hdrlen)
{

	bpfattach2(ifp, dlt, hdrlen, &ifp->if_bpf);
}

void
bpfattach2(struct ifnet *ifp, u_int dlt, u_int hdrlen, struct bpf_if **driverp)
{

	*driverp = &bp_null;
}

void
bpfdetach(struct ifnet *ifp)
{
}

u_int
bpf_filter(const struct bpf_insn *pc, u_char *p, u_int wirelen, u_int buflen)
{
	return -1;	/* "no filter" behaviour */
}

int
bpf_validate(const struct bpf_insn *f, int len)
{
	return 0;		/* false */
}

#endif /* !DEV_BPF && !NETGRAPH_BPF */
