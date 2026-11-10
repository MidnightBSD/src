/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Lucas Holt
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

/*
 * MidnightBSD-only header.  It is not part of FreeBSD's OFED and must be
 * preserved across OFED re-syncs from FreeBSD.
 *
 * MNBSD-KPI: FreeBSD 14 extended the ifnet accessor API (IfAPI) and the
 * OFED stack and RDMA drivers were converted to it.  MidnightBSD's ifnet
 * KPI is at the FreeBSD 13 level: struct ifnet is still public, but a few
 * accessors and the interface/address iterators are missing.  Provide them
 * here, for OFED and its consumers only, so the FreeBSD code can stay
 * verbatim.  Everything is static inline and renamed through macros so no
 * symbol leaks into the kernel namespace.  Remove this header once sys/net
 * provides these interfaces.
 */

#ifndef _RDMA_IB_MNBSD_IFAPI_H_
#define	_RDMA_IB_MNBSD_IFAPI_H_

#ifdef __MidnightBSD__

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/epoch.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/vnet.h>

#ifndef if_getindex
#define	if_getindex(ifp)		((ifp)->if_index)
#endif
#ifndef if_getdunit
#define	if_getdunit(ifp)		((ifp)->if_dunit)
#endif
#ifndef if_getvnet
#define	if_getvnet(ifp)			((ifp)->if_vnet)
#endif
#ifndef if_getaddrlen
#define	if_getaddrlen(ifp)		((ifp)->if_addrlen)
#endif
#ifndef if_getpcp
#define	if_getpcp(ifp)			((ifp)->if_pcp)
#endif
#ifndef if_getvlantrunk
#define	if_getvlantrunk(ifp)		((ifp)->if_vlantrunk)
#endif
#ifndef if_setbroadcastaddr
#define	if_setbroadcastaddr(ifp, addr)	((ifp)->if_broadcastaddr = (addr))
#endif

typedef u_int	ib_mnbsd_if_addr_cb_t(void *, struct ifaddr *, u_int);
typedef int	(*ib_mnbsd_if_foreach_cb_t)(if_t, void *);

struct ib_mnbsd_if_iter {
	void	*context[4];
};

#define	if_addr_cb_t		ib_mnbsd_if_addr_cb_t
#define	if_foreach_cb_t		ib_mnbsd_if_foreach_cb_t
#define	if_iter			ib_mnbsd_if_iter
#define	if_foreach_addr_type	ib_mnbsd_if_foreach_addr_type
#define	if_foreach		ib_mnbsd_if_foreach
#define	if_iter_start		ib_mnbsd_if_iter_start
#define	if_iter_next		ib_mnbsd_if_iter_next
#define	if_iter_finish		ib_mnbsd_if_iter_finish
#define	if_transmit(ifp, m)		ib_mnbsd_if_transmit(ifp, m)
#define	if_init(ifp, ctx)		ib_mnbsd_if_init(ifp, ctx)
#define	if_resolvemulti(ifp, srcs, dst)					\
	ib_mnbsd_if_resolvemulti(ifp, srcs, dst)

/*
 * if_transmit() and if_init() share their names with struct ifnet
 * members; the member calls below are parenthesized so the function-like
 * macros above do not expand them.
 */
static inline int
ib_mnbsd_if_transmit(if_t ifp, struct mbuf *m)
{

	return ((ifp->if_transmit)(ifp, m));
}

static inline void
ib_mnbsd_if_init(if_t ifp, void *ctx)
{

	(ifp->if_init)(ctx);
}

/* Resolve a multicast address to a link layer address; as above. */
static inline int
ib_mnbsd_if_resolvemulti(if_t ifp, struct sockaddr **srcs,
    struct sockaddr *dst)
{

	if (ifp->if_resolvemulti == NULL)
		return (EOPNOTSUPP);
	return ((ifp->if_resolvemulti)(ifp, srcs, dst));
}

/* Call cb for each address of family type on ifp; sum of its returns. */
static inline u_int
ib_mnbsd_if_foreach_addr_type(if_t ifp, int type, ib_mnbsd_if_addr_cb_t cb,
    void *cb_arg)
{
	struct epoch_tracker et;
	struct ifaddr *ifa;
	u_int count;

	MPASS(cb != NULL);

	count = 0;
	NET_EPOCH_ENTER(et);
	CK_STAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
		if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != type)
			continue;
		count += (*cb)(cb_arg, ifa, count);
	}
	NET_EPOCH_EXIT(et);

	return (count);
}

/* Call cb for each interface in the current vnet; stop on error. */
static inline int
ib_mnbsd_if_foreach(ib_mnbsd_if_foreach_cb_t cb, void *cb_arg)
{
	if_t ifp;
	int error;

	NET_EPOCH_ASSERT();
	MPASS(cb != NULL);

	error = 0;
	CK_STAILQ_FOREACH(ifp, &V_ifnet, if_link) {
		error = cb(ifp, cb_arg);
		if (error != 0)
			break;
	}

	return (error);
}

/* Interface iterator; the caller must be in the net epoch. */
static inline if_t
ib_mnbsd_if_iter_start(struct ib_mnbsd_if_iter *iter)
{
	if_t ifp;

	NET_EPOCH_ASSERT();
	bzero(iter, sizeof(*iter));
	ifp = CK_STAILQ_FIRST(&V_ifnet);
	if (ifp != NULL)
		iter->context[0] = CK_STAILQ_NEXT(ifp, if_link);
	return (ifp);
}

static inline if_t
ib_mnbsd_if_iter_next(struct ib_mnbsd_if_iter *iter)
{
	if_t cur_ifp;

	cur_ifp = iter->context[0];
	if (cur_ifp != NULL)
		iter->context[0] = CK_STAILQ_NEXT(cur_ifp, if_link);
	return (cur_ifp);
}

static inline void
ib_mnbsd_if_iter_finish(struct ib_mnbsd_if_iter *iter __unused)
{
}

#endif /* __MidnightBSD__ */

#endif /* _RDMA_IB_MNBSD_IFAPI_H_ */
