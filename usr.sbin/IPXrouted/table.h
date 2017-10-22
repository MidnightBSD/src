/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Copyright (c) 1995 John Hay.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	@(#)table.h	5.1 (Berkeley) 6/4/85 (routed/table.h)
 *
 *	@(#)table.h	8.1 (Berkeley) 6/5/93
 *
 * $FreeBSD$
 */

/*
 * Routing table management daemon.
 */

/*
 * Routing table structure; differs a bit from kernel tables.
 *
 * Note: the union below must agree in the first 4 members
 * so the ioctl's will work.
 */
struct rthash {
	struct	rt_entry *rt_forw;
	struct	rt_entry *rt_back;
};

#ifdef RTM_ADD
#define rtentry ortentry
#endif

struct rt_entry {
	struct	rt_entry *rt_forw;
	struct	rt_entry *rt_back;
	union {
		struct	rtentry rtu_rt;
		struct  rtuentry {
			u_long	rtu_hash;
			struct	sockaddr rtu_dst;
			struct	sockaddr rtu_router;
			short	rtu_rtflags; /* used by old rtioctl */
			short	rtu_wasted; /* XXX routed does it this way. */
			int	rtu_flags;
			int	rtu_state;
			int	rtu_timer;
			int	rtu_metric;
			int	rtu_ticks;
			struct	interface *rtu_ifp;
		} rtu_entry;
	} rt_rtu;
	struct	rt_entry *rt_clone;
};

#define	rt_rt		rt_rtu.rtu_entry		/* pass to ioctl */
#define	rt_hash		rt_rtu.rtu_entry.rtu_hash	/* for net or host */
#define	rt_dst		rt_rtu.rtu_entry.rtu_dst	/* match value */
#define	rt_router	rt_rtu.rtu_entry.rtu_router	/* who to forward to */
#define	rt_flags	rt_rtu.rtu_entry.rtu_flags	/* kernel flags */
#define	rt_timer	rt_rtu.rtu_entry.rtu_timer	/* for invalidation */
#define	rt_state	rt_rtu.rtu_entry.rtu_state	/* see below */
#define	rt_metric	rt_rtu.rtu_entry.rtu_metric	/* cost of route */
#define	rt_ticks	rt_rtu.rtu_entry.rtu_ticks	/* time of route */
#define	rt_ifp		rt_rtu.rtu_entry.rtu_ifp	/* interface to take */

#define	ROUTEHASHSIZ	128		/* must be a power of 2 */
#define	ROUTEHASHMASK	(ROUTEHASHSIZ - 1)

/*
 * "State" of routing table entry.
 */
#define	RTS_CHANGED	0x1		/* route has been altered recently */
#define	RTS_PASSIVE	IFF_PASSIVE	/* don't time out route */
#define	RTS_INTERFACE	IFF_INTERFACE	/* route is for network interface */
#define	RTS_REMOTE	IFF_REMOTE	/* route is for ``remote'' entity */

extern struct	rthash nethash[ROUTEHASHSIZ];
struct	rt_entry *rtlookup(struct sockaddr *);
struct	rt_entry *rtfind(struct sockaddr *);
void	rtadd(struct sockaddr *, struct sockaddr *, short, short, int);
void	rtadd_clone(struct rt_entry *, struct sockaddr *, struct sockaddr *, 
		    short, short, int);
void	rtchange(struct rt_entry *, struct sockaddr *, short, short);
void	rtdelete(struct rt_entry *);
int	rtioctl(int, struct rtuentry *);
void	rtinit(void);

