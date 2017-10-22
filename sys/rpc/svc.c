/*	$NetBSD: svc.c,v 1.21 2000/07/06 03:10:35 christos Exp $	*/

/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 *
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 *
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 *
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 *
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char *sccsid2 = "@(#)svc.c 1.44 88/02/08 Copyr 1984 Sun Micro";
static char *sccsid = "@(#)svc.c	2.4 88/08/11 4.0 RPCSRC";
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * svc.c, Server-side remote procedure call interface.
 *
 * There are two sets of procedures here.  The xprt routines are
 * for handling transport handles.  The svc routines handle the
 * list of service routines.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 */

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/socketvar.h>
#include <sys/systm.h>
#include <sys/ucred.h>

#include <rpc/rpc.h>
#include <rpc/rpcb_clnt.h>
#include <rpc/replay.h>

#include <rpc/rpc_com.h>

#define SVC_VERSQUIET 0x0001		/* keep quiet about vers mismatch */
#define version_keepquiet(xp) (SVC_EXT(xp)->xp_flags & SVC_VERSQUIET)

static struct svc_callout *svc_find(SVCPOOL *pool, rpcprog_t, rpcvers_t,
    char *);
static void svc_new_thread(SVCPOOL *pool);
static void xprt_unregister_locked(SVCXPRT *xprt);

/* ***************  SVCXPRT related stuff **************** */

static int svcpool_minthread_sysctl(SYSCTL_HANDLER_ARGS);
static int svcpool_maxthread_sysctl(SYSCTL_HANDLER_ARGS);

SVCPOOL*
svcpool_create(const char *name, struct sysctl_oid_list *sysctl_base)
{
	SVCPOOL *pool;

	pool = malloc(sizeof(SVCPOOL), M_RPC, M_WAITOK|M_ZERO);
	
	mtx_init(&pool->sp_lock, "sp_lock", NULL, MTX_DEF);
	pool->sp_name = name;
	pool->sp_state = SVCPOOL_INIT;
	pool->sp_proc = NULL;
	TAILQ_INIT(&pool->sp_xlist);
	TAILQ_INIT(&pool->sp_active);
	TAILQ_INIT(&pool->sp_callouts);
	LIST_INIT(&pool->sp_threads);
	LIST_INIT(&pool->sp_idlethreads);
	pool->sp_minthreads = 1;
	pool->sp_maxthreads = 1;
	pool->sp_threadcount = 0;

	/*
	 * Don't use more than a quarter of mbuf clusters or more than
	 * 45Mb buffering requests.
	 */
	pool->sp_space_high = nmbclusters * MCLBYTES / 4;
	if (pool->sp_space_high > 45 << 20)
		pool->sp_space_high = 45 << 20;
	pool->sp_space_low = 2 * pool->sp_space_high / 3;

	sysctl_ctx_init(&pool->sp_sysctl);
	if (sysctl_base) {
		SYSCTL_ADD_PROC(&pool->sp_sysctl, sysctl_base, OID_AUTO,
		    "minthreads", CTLTYPE_INT | CTLFLAG_RW,
		    pool, 0, svcpool_minthread_sysctl, "I", "");
		SYSCTL_ADD_PROC(&pool->sp_sysctl, sysctl_base, OID_AUTO,
		    "maxthreads", CTLTYPE_INT | CTLFLAG_RW,
		    pool, 0, svcpool_maxthread_sysctl, "I", "");
		SYSCTL_ADD_INT(&pool->sp_sysctl, sysctl_base, OID_AUTO,
		    "threads", CTLFLAG_RD, &pool->sp_threadcount, 0, "");

		SYSCTL_ADD_UINT(&pool->sp_sysctl, sysctl_base, OID_AUTO,
		    "request_space_used", CTLFLAG_RD,
		    &pool->sp_space_used, 0,
		    "Space in parsed but not handled requests.");

		SYSCTL_ADD_UINT(&pool->sp_sysctl, sysctl_base, OID_AUTO,
		    "request_space_used_highest", CTLFLAG_RD,
		    &pool->sp_space_used_highest, 0,
		    "Highest space used since reboot.");

		SYSCTL_ADD_UINT(&pool->sp_sysctl, sysctl_base, OID_AUTO,
		    "request_space_high", CTLFLAG_RW,
		    &pool->sp_space_high, 0,
		    "Maximum space in parsed but not handled requests.");

		SYSCTL_ADD_UINT(&pool->sp_sysctl, sysctl_base, OID_AUTO,
		    "request_space_low", CTLFLAG_RW,
		    &pool->sp_space_low, 0,
		    "Low water mark for request space.");

		SYSCTL_ADD_INT(&pool->sp_sysctl, sysctl_base, OID_AUTO,
		    "request_space_throttled", CTLFLAG_RD,
		    &pool->sp_space_throttled, 0,
		    "Whether nfs requests are currently throttled");

		SYSCTL_ADD_INT(&pool->sp_sysctl, sysctl_base, OID_AUTO,
		    "request_space_throttle_count", CTLFLAG_RD,
		    &pool->sp_space_throttle_count, 0,
		    "Count of times throttling based on request space has occurred");
	}

	return pool;
}

void
svcpool_destroy(SVCPOOL *pool)
{
	SVCXPRT *xprt, *nxprt;
	struct svc_callout *s;
	struct svcxprt_list cleanup;

	TAILQ_INIT(&cleanup);
	mtx_lock(&pool->sp_lock);

	while (TAILQ_FIRST(&pool->sp_xlist)) {
		xprt = TAILQ_FIRST(&pool->sp_xlist);
		xprt_unregister_locked(xprt);
		TAILQ_INSERT_TAIL(&cleanup, xprt, xp_link);
	}

	while (TAILQ_FIRST(&pool->sp_callouts)) {
		s = TAILQ_FIRST(&pool->sp_callouts);
		mtx_unlock(&pool->sp_lock);
		svc_unreg(pool, s->sc_prog, s->sc_vers);
		mtx_lock(&pool->sp_lock);
	}
	mtx_unlock(&pool->sp_lock);

	TAILQ_FOREACH_SAFE(xprt, &cleanup, xp_link, nxprt) {
		SVC_RELEASE(xprt);
	}

	mtx_destroy(&pool->sp_lock);

	if (pool->sp_rcache)
		replay_freecache(pool->sp_rcache);

	sysctl_ctx_free(&pool->sp_sysctl);
	free(pool, M_RPC);
}

static bool_t
svcpool_active(SVCPOOL *pool)
{
	enum svcpool_state state = pool->sp_state;

	if (state == SVCPOOL_INIT || state == SVCPOOL_CLOSING)
		return (FALSE);
	return (TRUE);
}

/*
 * Sysctl handler to set the minimum thread count on a pool
 */
static int
svcpool_minthread_sysctl(SYSCTL_HANDLER_ARGS)
{
	SVCPOOL *pool;
	int newminthreads, error, n;

	pool = oidp->oid_arg1;
	newminthreads = pool->sp_minthreads;
	error = sysctl_handle_int(oidp, &newminthreads, 0, req);
	if (error == 0 && newminthreads != pool->sp_minthreads) {
		if (newminthreads > pool->sp_maxthreads)
			return (EINVAL);
		mtx_lock(&pool->sp_lock);
		if (newminthreads > pool->sp_minthreads
		    && svcpool_active(pool)) {
			/*
			 * If the pool is running and we are
			 * increasing, create some more threads now.
			 */
			n = newminthreads - pool->sp_threadcount;
			if (n > 0) {
				mtx_unlock(&pool->sp_lock);
				while (n--)
					svc_new_thread(pool);
				mtx_lock(&pool->sp_lock);
			}
		}
		pool->sp_minthreads = newminthreads;
		mtx_unlock(&pool->sp_lock);
	}
	return (error);
}

/*
 * Sysctl handler to set the maximum thread count on a pool
 */
static int
svcpool_maxthread_sysctl(SYSCTL_HANDLER_ARGS)
{
	SVCPOOL *pool;
	SVCTHREAD *st;
	int newmaxthreads, error;

	pool = oidp->oid_arg1;
	newmaxthreads = pool->sp_maxthreads;
	error = sysctl_handle_int(oidp, &newmaxthreads, 0, req);
	if (error == 0 && newmaxthreads != pool->sp_maxthreads) {
		if (newmaxthreads < pool->sp_minthreads)
			return (EINVAL);
		mtx_lock(&pool->sp_lock);
		if (newmaxthreads < pool->sp_maxthreads
		    && svcpool_active(pool)) {
			/*
			 * If the pool is running and we are
			 * decreasing, wake up some idle threads to
			 * encourage them to exit.
			 */
			LIST_FOREACH(st, &pool->sp_idlethreads, st_ilink)
				cv_signal(&st->st_cond);
		}
		pool->sp_maxthreads = newmaxthreads;
		mtx_unlock(&pool->sp_lock);
	}
	return (error);
}

/*
 * Activate a transport handle.
 */
void
xprt_register(SVCXPRT *xprt)
{
	SVCPOOL *pool = xprt->xp_pool;

	SVC_ACQUIRE(xprt);
	mtx_lock(&pool->sp_lock);
	xprt->xp_registered = TRUE;
	xprt->xp_active = FALSE;
	TAILQ_INSERT_TAIL(&pool->sp_xlist, xprt, xp_link);
	mtx_unlock(&pool->sp_lock);
}

/*
 * De-activate a transport handle. Note: the locked version doesn't
 * release the transport - caller must do that after dropping the pool
 * lock.
 */
static void
xprt_unregister_locked(SVCXPRT *xprt)
{
	SVCPOOL *pool = xprt->xp_pool;

	KASSERT(xprt->xp_registered == TRUE,
	    ("xprt_unregister_locked: not registered"));
	if (xprt->xp_active) {
		TAILQ_REMOVE(&pool->sp_active, xprt, xp_alink);
		xprt->xp_active = FALSE;
	}
	TAILQ_REMOVE(&pool->sp_xlist, xprt, xp_link);
	xprt->xp_registered = FALSE;
}

void
xprt_unregister(SVCXPRT *xprt)
{
	SVCPOOL *pool = xprt->xp_pool;

	mtx_lock(&pool->sp_lock);
	if (xprt->xp_registered == FALSE) {
		/* Already unregistered by another thread */
		mtx_unlock(&pool->sp_lock);
		return;
	}
	xprt_unregister_locked(xprt);
	mtx_unlock(&pool->sp_lock);

	SVC_RELEASE(xprt);
}

static void
xprt_assignthread(SVCXPRT *xprt)
{
	SVCPOOL *pool = xprt->xp_pool;
	SVCTHREAD *st;

	/*
	 * Attempt to assign a service thread to this
	 * transport.
	 */
	LIST_FOREACH(st, &pool->sp_idlethreads, st_ilink) {
		if (st->st_xprt == NULL && STAILQ_EMPTY(&st->st_reqs))
			break;
	}
	if (st) {
		SVC_ACQUIRE(xprt);
		xprt->xp_thread = st;
		st->st_xprt = xprt;
		cv_signal(&st->st_cond);
	} else {
		/*
		 * See if we can create a new thread. The
		 * actual thread creation happens in
		 * svc_run_internal because our locking state
		 * is poorly defined (we are typically called
		 * from a socket upcall). Don't create more
		 * than one thread per second.
		 */
		if (pool->sp_state == SVCPOOL_ACTIVE
		    && pool->sp_lastcreatetime < time_uptime
		    && pool->sp_threadcount < pool->sp_maxthreads) {
			pool->sp_state = SVCPOOL_THREADWANTED;
		}
	}
}

void
xprt_active(SVCXPRT *xprt)
{
	SVCPOOL *pool = xprt->xp_pool;

	mtx_lock(&pool->sp_lock);

	if (!xprt->xp_registered) {
		/*
		 * Race with xprt_unregister - we lose.
		 */
		mtx_unlock(&pool->sp_lock);
		return;
	}

	if (!xprt->xp_active) {
		TAILQ_INSERT_TAIL(&pool->sp_active, xprt, xp_alink);
		xprt->xp_active = TRUE;
		xprt_assignthread(xprt);
	}

	mtx_unlock(&pool->sp_lock);
}

void
xprt_inactive_locked(SVCXPRT *xprt)
{
	SVCPOOL *pool = xprt->xp_pool;

	if (xprt->xp_active) {
		TAILQ_REMOVE(&pool->sp_active, xprt, xp_alink);
		xprt->xp_active = FALSE;
	}
}

void
xprt_inactive(SVCXPRT *xprt)
{
	SVCPOOL *pool = xprt->xp_pool;

	mtx_lock(&pool->sp_lock);
	xprt_inactive_locked(xprt);
	mtx_unlock(&pool->sp_lock);
}

/*
 * Add a service program to the callout list.
 * The dispatch routine will be called when a rpc request for this
 * program number comes in.
 */
bool_t
svc_reg(SVCXPRT *xprt, const rpcprog_t prog, const rpcvers_t vers,
    void (*dispatch)(struct svc_req *, SVCXPRT *),
    const struct netconfig *nconf)
{
	SVCPOOL *pool = xprt->xp_pool;
	struct svc_callout *s;
	char *netid = NULL;
	int flag = 0;

/* VARIABLES PROTECTED BY svc_lock: s, svc_head */

	if (xprt->xp_netid) {
		netid = strdup(xprt->xp_netid, M_RPC);
		flag = 1;
	} else if (nconf && nconf->nc_netid) {
		netid = strdup(nconf->nc_netid, M_RPC);
		flag = 1;
	} /* must have been created with svc_raw_create */
	if ((netid == NULL) && (flag == 1)) {
		return (FALSE);
	}

	mtx_lock(&pool->sp_lock);
	if ((s = svc_find(pool, prog, vers, netid)) != NULL) {
		if (netid)
			free(netid, M_RPC);
		if (s->sc_dispatch == dispatch)
			goto rpcb_it; /* he is registering another xptr */
		mtx_unlock(&pool->sp_lock);
		return (FALSE);
	}
	s = malloc(sizeof (struct svc_callout), M_RPC, M_NOWAIT);
	if (s == NULL) {
		if (netid)
			free(netid, M_RPC);
		mtx_unlock(&pool->sp_lock);
		return (FALSE);
	}

	s->sc_prog = prog;
	s->sc_vers = vers;
	s->sc_dispatch = dispatch;
	s->sc_netid = netid;
	TAILQ_INSERT_TAIL(&pool->sp_callouts, s, sc_link);

	if ((xprt->xp_netid == NULL) && (flag == 1) && netid)
		((SVCXPRT *) xprt)->xp_netid = strdup(netid, M_RPC);

rpcb_it:
	mtx_unlock(&pool->sp_lock);
	/* now register the information with the local binder service */
	if (nconf) {
		bool_t dummy;
		struct netconfig tnc;
		struct netbuf nb;
		tnc = *nconf;
		nb.buf = &xprt->xp_ltaddr;
		nb.len = xprt->xp_ltaddr.ss_len;
		dummy = rpcb_set(prog, vers, &tnc, &nb);
		return (dummy);
	}
	return (TRUE);
}

/*
 * Remove a service program from the callout list.
 */
void
svc_unreg(SVCPOOL *pool, const rpcprog_t prog, const rpcvers_t vers)
{
	struct svc_callout *s;

	/* unregister the information anyway */
	(void) rpcb_unset(prog, vers, NULL);
	mtx_lock(&pool->sp_lock);
	while ((s = svc_find(pool, prog, vers, NULL)) != NULL) {
		TAILQ_REMOVE(&pool->sp_callouts, s, sc_link);
		if (s->sc_netid)
			mem_free(s->sc_netid, sizeof (s->sc_netid) + 1);
		mem_free(s, sizeof (struct svc_callout));
	}
	mtx_unlock(&pool->sp_lock);
}

/* ********************** CALLOUT list related stuff ************* */

/*
 * Search the callout list for a program number, return the callout
 * struct.
 */
static struct svc_callout *
svc_find(SVCPOOL *pool, rpcprog_t prog, rpcvers_t vers, char *netid)
{
	struct svc_callout *s;

	mtx_assert(&pool->sp_lock, MA_OWNED);
	TAILQ_FOREACH(s, &pool->sp_callouts, sc_link) {
		if (s->sc_prog == prog && s->sc_vers == vers
		    && (netid == NULL || s->sc_netid == NULL ||
			strcmp(netid, s->sc_netid) == 0))
			break;
	}

	return (s);
}

/* ******************* REPLY GENERATION ROUTINES  ************ */

static bool_t
svc_sendreply_common(struct svc_req *rqstp, struct rpc_msg *rply,
    struct mbuf *body)
{
	SVCXPRT *xprt = rqstp->rq_xprt;
	bool_t ok;

	if (rqstp->rq_args) {
		m_freem(rqstp->rq_args);
		rqstp->rq_args = NULL;
	}

	if (xprt->xp_pool->sp_rcache)
		replay_setreply(xprt->xp_pool->sp_rcache,
		    rply, svc_getrpccaller(rqstp), body);

	if (!SVCAUTH_WRAP(&rqstp->rq_auth, &body))
		return (FALSE);

	ok = SVC_REPLY(xprt, rply, rqstp->rq_addr, body); 
	if (rqstp->rq_addr) {
		free(rqstp->rq_addr, M_SONAME);
		rqstp->rq_addr = NULL;
	}

	return (ok);
}

/*
 * Send a reply to an rpc request
 */
bool_t
svc_sendreply(struct svc_req *rqstp, xdrproc_t xdr_results, void * xdr_location)
{
	struct rpc_msg rply; 
	struct mbuf *m;
	XDR xdrs;
	bool_t ok;

	rply.rm_xid = rqstp->rq_xid;
	rply.rm_direction = REPLY;  
	rply.rm_reply.rp_stat = MSG_ACCEPTED; 
	rply.acpted_rply.ar_verf = rqstp->rq_verf; 
	rply.acpted_rply.ar_stat = SUCCESS;
	rply.acpted_rply.ar_results.where = NULL;
	rply.acpted_rply.ar_results.proc = (xdrproc_t) xdr_void;

	MGET(m, M_WAIT, MT_DATA);
	MCLGET(m, M_WAIT);
	m->m_len = 0;
	xdrmbuf_create(&xdrs, m, XDR_ENCODE);
	ok = xdr_results(&xdrs, xdr_location);
	XDR_DESTROY(&xdrs);

	if (ok) {
		return (svc_sendreply_common(rqstp, &rply, m));
	} else {
		m_freem(m);
		return (FALSE);
	}
}

bool_t
svc_sendreply_mbuf(struct svc_req *rqstp, struct mbuf *m)
{
	struct rpc_msg rply; 

	rply.rm_xid = rqstp->rq_xid;
	rply.rm_direction = REPLY;  
	rply.rm_reply.rp_stat = MSG_ACCEPTED; 
	rply.acpted_rply.ar_verf = rqstp->rq_verf; 
	rply.acpted_rply.ar_stat = SUCCESS;
	rply.acpted_rply.ar_results.where = NULL;
	rply.acpted_rply.ar_results.proc = (xdrproc_t) xdr_void;

	return (svc_sendreply_common(rqstp, &rply, m));
}

/*
 * No procedure error reply
 */
void
svcerr_noproc(struct svc_req *rqstp)
{
	SVCXPRT *xprt = rqstp->rq_xprt;
	struct rpc_msg rply;

	rply.rm_xid = rqstp->rq_xid;
	rply.rm_direction = REPLY;
	rply.rm_reply.rp_stat = MSG_ACCEPTED;
	rply.acpted_rply.ar_verf = rqstp->rq_verf;
	rply.acpted_rply.ar_stat = PROC_UNAVAIL;

	if (xprt->xp_pool->sp_rcache)
		replay_setreply(xprt->xp_pool->sp_rcache,
		    &rply, svc_getrpccaller(rqstp), NULL);

	svc_sendreply_common(rqstp, &rply, NULL);
}

/*
 * Can't decode args error reply
 */
void
svcerr_decode(struct svc_req *rqstp)
{
	SVCXPRT *xprt = rqstp->rq_xprt;
	struct rpc_msg rply; 

	rply.rm_xid = rqstp->rq_xid;
	rply.rm_direction = REPLY; 
	rply.rm_reply.rp_stat = MSG_ACCEPTED; 
	rply.acpted_rply.ar_verf = rqstp->rq_verf;
	rply.acpted_rply.ar_stat = GARBAGE_ARGS;

	if (xprt->xp_pool->sp_rcache)
		replay_setreply(xprt->xp_pool->sp_rcache,
		    &rply, (struct sockaddr *) &xprt->xp_rtaddr, NULL);

	svc_sendreply_common(rqstp, &rply, NULL);
}

/*
 * Some system error
 */
void
svcerr_systemerr(struct svc_req *rqstp)
{
	SVCXPRT *xprt = rqstp->rq_xprt;
	struct rpc_msg rply; 

	rply.rm_xid = rqstp->rq_xid;
	rply.rm_direction = REPLY; 
	rply.rm_reply.rp_stat = MSG_ACCEPTED; 
	rply.acpted_rply.ar_verf = rqstp->rq_verf;
	rply.acpted_rply.ar_stat = SYSTEM_ERR;

	if (xprt->xp_pool->sp_rcache)
		replay_setreply(xprt->xp_pool->sp_rcache,
		    &rply, svc_getrpccaller(rqstp), NULL);

	svc_sendreply_common(rqstp, &rply, NULL);
}

/*
 * Authentication error reply
 */
void
svcerr_auth(struct svc_req *rqstp, enum auth_stat why)
{
	SVCXPRT *xprt = rqstp->rq_xprt;
	struct rpc_msg rply;

	rply.rm_xid = rqstp->rq_xid;
	rply.rm_direction = REPLY;
	rply.rm_reply.rp_stat = MSG_DENIED;
	rply.rjcted_rply.rj_stat = AUTH_ERROR;
	rply.rjcted_rply.rj_why = why;

	if (xprt->xp_pool->sp_rcache)
		replay_setreply(xprt->xp_pool->sp_rcache,
		    &rply, svc_getrpccaller(rqstp), NULL);

	svc_sendreply_common(rqstp, &rply, NULL);
}

/*
 * Auth too weak error reply
 */
void
svcerr_weakauth(struct svc_req *rqstp)
{

	svcerr_auth(rqstp, AUTH_TOOWEAK);
}

/*
 * Program unavailable error reply
 */
void 
svcerr_noprog(struct svc_req *rqstp)
{
	SVCXPRT *xprt = rqstp->rq_xprt;
	struct rpc_msg rply;  

	rply.rm_xid = rqstp->rq_xid;
	rply.rm_direction = REPLY;   
	rply.rm_reply.rp_stat = MSG_ACCEPTED;  
	rply.acpted_rply.ar_verf = rqstp->rq_verf;  
	rply.acpted_rply.ar_stat = PROG_UNAVAIL;

	if (xprt->xp_pool->sp_rcache)
		replay_setreply(xprt->xp_pool->sp_rcache,
		    &rply, svc_getrpccaller(rqstp), NULL);

	svc_sendreply_common(rqstp, &rply, NULL);
}

/*
 * Program version mismatch error reply
 */
void  
svcerr_progvers(struct svc_req *rqstp, rpcvers_t low_vers, rpcvers_t high_vers)
{
	SVCXPRT *xprt = rqstp->rq_xprt;
	struct rpc_msg rply;

	rply.rm_xid = rqstp->rq_xid;
	rply.rm_direction = REPLY;
	rply.rm_reply.rp_stat = MSG_ACCEPTED;
	rply.acpted_rply.ar_verf = rqstp->rq_verf;
	rply.acpted_rply.ar_stat = PROG_MISMATCH;
	rply.acpted_rply.ar_vers.low = (uint32_t)low_vers;
	rply.acpted_rply.ar_vers.high = (uint32_t)high_vers;

	if (xprt->xp_pool->sp_rcache)
		replay_setreply(xprt->xp_pool->sp_rcache,
		    &rply, svc_getrpccaller(rqstp), NULL);

	svc_sendreply_common(rqstp, &rply, NULL);
}

/*
 * Allocate a new server transport structure. All fields are
 * initialized to zero and xp_p3 is initialized to point at an
 * extension structure to hold various flags and authentication
 * parameters.
 */
SVCXPRT *
svc_xprt_alloc()
{
	SVCXPRT *xprt;
	SVCXPRT_EXT *ext;

	xprt = mem_alloc(sizeof(SVCXPRT));
	memset(xprt, 0, sizeof(SVCXPRT));
	ext = mem_alloc(sizeof(SVCXPRT_EXT));
	memset(ext, 0, sizeof(SVCXPRT_EXT));
	xprt->xp_p3 = ext;
	refcount_init(&xprt->xp_refs, 1);

	return (xprt);
}

/*
 * Free a server transport structure.
 */
void
svc_xprt_free(xprt)
	SVCXPRT *xprt;
{

	mem_free(xprt->xp_p3, sizeof(SVCXPRT_EXT));
	mem_free(xprt, sizeof(SVCXPRT));
}

/* ******************* SERVER INPUT STUFF ******************* */

/*
 * Read RPC requests from a transport and queue them to be
 * executed. We handle authentication and replay cache replies here.
 * Actually dispatching the RPC is deferred till svc_executereq.
 */
static enum xprt_stat
svc_getreq(SVCXPRT *xprt, struct svc_req **rqstp_ret)
{
	SVCPOOL *pool = xprt->xp_pool;
	struct svc_req *r;
	struct rpc_msg msg;
	struct mbuf *args;
	enum xprt_stat stat;

	/* now receive msgs from xprtprt (support batch calls) */
	r = malloc(sizeof(*r), M_RPC, M_WAITOK|M_ZERO);

	msg.rm_call.cb_cred.oa_base = r->rq_credarea;
	msg.rm_call.cb_verf.oa_base = &r->rq_credarea[MAX_AUTH_BYTES];
	r->rq_clntcred = &r->rq_credarea[2*MAX_AUTH_BYTES];
	if (SVC_RECV(xprt, &msg, &r->rq_addr, &args)) {
		enum auth_stat why;

		/*
		 * Handle replays and authenticate before queuing the
		 * request to be executed.
		 */
		SVC_ACQUIRE(xprt);
		r->rq_xprt = xprt;
		if (pool->sp_rcache) {
			struct rpc_msg repmsg;
			struct mbuf *repbody;
			enum replay_state rs;
			rs = replay_find(pool->sp_rcache, &msg,
			    svc_getrpccaller(r), &repmsg, &repbody);
			switch (rs) {
			case RS_NEW:
				break;
			case RS_DONE:
				SVC_REPLY(xprt, &repmsg, r->rq_addr,
				    repbody);
				if (r->rq_addr) {
					free(r->rq_addr, M_SONAME);
					r->rq_addr = NULL;
				}
				m_freem(args);
				goto call_done;

			default:
				m_freem(args);
				goto call_done;
			}
		}

		r->rq_xid = msg.rm_xid;
		r->rq_prog = msg.rm_call.cb_prog;
		r->rq_vers = msg.rm_call.cb_vers;
		r->rq_proc = msg.rm_call.cb_proc;
		r->rq_size = sizeof(*r) + m_length(args, NULL);
		r->rq_args = args;
		if ((why = _authenticate(r, &msg)) != AUTH_OK) {
			/*
			 * RPCSEC_GSS uses this return code
			 * for requests that form part of its
			 * context establishment protocol and
			 * should not be dispatched to the
			 * application.
			 */
			if (why != RPCSEC_GSS_NODISPATCH)
				svcerr_auth(r, why);
			goto call_done;
		}

		if (!SVCAUTH_UNWRAP(&r->rq_auth, &r->rq_args)) {
			svcerr_decode(r);
			goto call_done;
		}

		/*
		 * Everything checks out, return request to caller.
		 */
		*rqstp_ret = r;
		r = NULL;
	}
call_done:
	if (r) {
		svc_freereq(r);
		r = NULL;
	}
	if ((stat = SVC_STAT(xprt)) == XPRT_DIED) {
		xprt_unregister(xprt);
	}

	return (stat);
}

static void
svc_executereq(struct svc_req *rqstp)
{
	SVCXPRT *xprt = rqstp->rq_xprt;
	SVCPOOL *pool = xprt->xp_pool;
	int prog_found;
	rpcvers_t low_vers;
	rpcvers_t high_vers;
	struct svc_callout *s;

	/* now match message with a registered service*/
	prog_found = FALSE;
	low_vers = (rpcvers_t) -1L;
	high_vers = (rpcvers_t) 0L;
	TAILQ_FOREACH(s, &pool->sp_callouts, sc_link) {
		if (s->sc_prog == rqstp->rq_prog) {
			if (s->sc_vers == rqstp->rq_vers) {
				/*
				 * We hand ownership of r to the
				 * dispatch method - they must call
				 * svc_freereq.
				 */
				(*s->sc_dispatch)(rqstp, xprt);
				return;
			}  /* found correct version */
			prog_found = TRUE;
			if (s->sc_vers < low_vers)
				low_vers = s->sc_vers;
			if (s->sc_vers > high_vers)
				high_vers = s->sc_vers;
		}   /* found correct program */
	}

	/*
	 * if we got here, the program or version
	 * is not served ...
	 */
	if (prog_found)
		svcerr_progvers(rqstp, low_vers, high_vers);
	else
		svcerr_noprog(rqstp);

	svc_freereq(rqstp);
}

static void
svc_checkidle(SVCPOOL *pool)
{
	SVCXPRT *xprt, *nxprt;
	time_t timo;
	struct svcxprt_list cleanup;

	TAILQ_INIT(&cleanup);
	TAILQ_FOREACH_SAFE(xprt, &pool->sp_xlist, xp_link, nxprt) {
		/*
		 * Only some transports have idle timers. Don't time
		 * something out which is just waking up.
		 */
		if (!xprt->xp_idletimeout || xprt->xp_thread)
			continue;

		timo = xprt->xp_lastactive + xprt->xp_idletimeout;
		if (time_uptime > timo) {
			xprt_unregister_locked(xprt);
			TAILQ_INSERT_TAIL(&cleanup, xprt, xp_link);
		}
	}

	mtx_unlock(&pool->sp_lock);
	TAILQ_FOREACH_SAFE(xprt, &cleanup, xp_link, nxprt) {
		SVC_RELEASE(xprt);
	}
	mtx_lock(&pool->sp_lock);

}

static void
svc_assign_waiting_sockets(SVCPOOL *pool)
{
	SVCXPRT *xprt;

	TAILQ_FOREACH(xprt, &pool->sp_active, xp_alink) {
		if (!xprt->xp_thread) {
			xprt_assignthread(xprt);
		}
	}
}

static bool_t
svc_request_space_available(SVCPOOL *pool)
{

	mtx_assert(&pool->sp_lock, MA_OWNED);

	if (pool->sp_space_throttled) {
		/*
		 * Below the low-water yet? If so, assign any waiting sockets.
		 */
		if (pool->sp_space_used < pool->sp_space_low) {
			pool->sp_space_throttled = FALSE;
			svc_assign_waiting_sockets(pool);
			return TRUE;
		}
		
		return FALSE;
	} else {
		if (pool->sp_space_used
		    >= pool->sp_space_high) {
			pool->sp_space_throttled = TRUE;
			pool->sp_space_throttle_count++;
			return FALSE;
		}

		return TRUE;
	}
}

static void
svc_run_internal(SVCPOOL *pool, bool_t ismaster)
{
	SVCTHREAD *st, *stpref;
	SVCXPRT *xprt;
	enum xprt_stat stat;
	struct svc_req *rqstp;
	int error;

	st = mem_alloc(sizeof(*st));
	st->st_xprt = NULL;
	STAILQ_INIT(&st->st_reqs);
	cv_init(&st->st_cond, "rpcsvc");

	mtx_lock(&pool->sp_lock);
	LIST_INSERT_HEAD(&pool->sp_threads, st, st_link);

	/*
	 * If we are a new thread which was spawned to cope with
	 * increased load, set the state back to SVCPOOL_ACTIVE.
	 */
	if (pool->sp_state == SVCPOOL_THREADSTARTING)
		pool->sp_state = SVCPOOL_ACTIVE;

	while (pool->sp_state != SVCPOOL_CLOSING) {
		/*
		 * Check for idle transports once per second.
		 */
		if (time_uptime > pool->sp_lastidlecheck) {
			pool->sp_lastidlecheck = time_uptime;
			svc_checkidle(pool);
		}

		xprt = st->st_xprt;
		if (!xprt && STAILQ_EMPTY(&st->st_reqs)) {
			/*
			 * Enforce maxthreads count.
			 */
			if (pool->sp_threadcount > pool->sp_maxthreads)
				break;

			/*
			 * Before sleeping, see if we can find an
			 * active transport which isn't being serviced
			 * by a thread.
			 */
			if (svc_request_space_available(pool)) {
				TAILQ_FOREACH(xprt, &pool->sp_active,
				    xp_alink) {
					if (!xprt->xp_thread) {
						SVC_ACQUIRE(xprt);
						xprt->xp_thread = st;
						st->st_xprt = xprt;
						break;
					}
				}
			}
			if (st->st_xprt)
				continue;

			LIST_INSERT_HEAD(&pool->sp_idlethreads, st, st_ilink);
			error = cv_timedwait_sig(&st->st_cond, &pool->sp_lock,
				5 * hz);
			LIST_REMOVE(st, st_ilink);

			/*
			 * Reduce worker thread count when idle.
			 */
			if (error == EWOULDBLOCK) {
				if (!ismaster
				    && (pool->sp_threadcount
					> pool->sp_minthreads)
					&& !st->st_xprt
					&& STAILQ_EMPTY(&st->st_reqs))
					break;
			}
			if (error == EWOULDBLOCK)
				continue;
			if (error) {
				if (pool->sp_state != SVCPOOL_CLOSING) {
					mtx_unlock(&pool->sp_lock);
					svc_exit(pool);
					mtx_lock(&pool->sp_lock);
				}
				break;
			}

			if (pool->sp_state == SVCPOOL_THREADWANTED) {
				pool->sp_state = SVCPOOL_THREADSTARTING;
				pool->sp_lastcreatetime = time_uptime;
				mtx_unlock(&pool->sp_lock);
				svc_new_thread(pool);
				mtx_lock(&pool->sp_lock);
			}
			continue;
		}

		if (xprt) {
			/*
			 * Drain the transport socket and queue up any
			 * RPCs.
			 */
			xprt->xp_lastactive = time_uptime;
			stat = XPRT_IDLE;
			do {
				if (!svc_request_space_available(pool))
					break;
				rqstp = NULL;
				mtx_unlock(&pool->sp_lock);
				stat = svc_getreq(xprt, &rqstp);
				mtx_lock(&pool->sp_lock);
				if (rqstp) {
					/*
					 * See if the application has
					 * a preference for some other
					 * thread.
					 */
					stpref = st;
					if (pool->sp_assign)
						stpref = pool->sp_assign(st,
						    rqstp);
					
					pool->sp_space_used +=
						rqstp->rq_size;
					if (pool->sp_space_used
					    > pool->sp_space_used_highest)
						pool->sp_space_used_highest =
							pool->sp_space_used;
					rqstp->rq_thread = stpref;
					STAILQ_INSERT_TAIL(&stpref->st_reqs,
					    rqstp, rq_link);
					stpref->st_reqcount++;

					/*
					 * If we assigned the request
					 * to another thread, make
					 * sure its awake and continue
					 * reading from the
					 * socket. Otherwise, try to
					 * find some other thread to
					 * read from the socket and
					 * execute the request
					 * immediately.
					 */
					if (stpref != st) {
						cv_signal(&stpref->st_cond);
						continue;
					} else {
						break;
					}
				}
			} while (stat == XPRT_MOREREQS
			    && pool->sp_state != SVCPOOL_CLOSING);
		       
			/*
			 * Move this transport to the end of the
			 * active list to ensure fairness when
			 * multiple transports are active. If this was
			 * the last queued request, svc_getreq will
			 * end up calling xprt_inactive to remove from
			 * the active list.
			 */
			xprt->xp_thread = NULL;
			st->st_xprt = NULL;
			if (xprt->xp_active) {
				xprt_assignthread(xprt);
				TAILQ_REMOVE(&pool->sp_active, xprt, xp_alink);
				TAILQ_INSERT_TAIL(&pool->sp_active, xprt,
				    xp_alink);
			}
			mtx_unlock(&pool->sp_lock);
			SVC_RELEASE(xprt);
			mtx_lock(&pool->sp_lock);
		}

		/*
		 * Execute what we have queued.
		 */
		while ((rqstp = STAILQ_FIRST(&st->st_reqs)) != NULL) {
			size_t sz = rqstp->rq_size;
			mtx_unlock(&pool->sp_lock);
			svc_executereq(rqstp);
			mtx_lock(&pool->sp_lock);
			pool->sp_space_used -= sz;
		}
	}

	if (st->st_xprt) {
		xprt = st->st_xprt;
		st->st_xprt = NULL;
		SVC_RELEASE(xprt);
	}

	KASSERT(STAILQ_EMPTY(&st->st_reqs), ("stray reqs on exit"));
	LIST_REMOVE(st, st_link);
	pool->sp_threadcount--;

	mtx_unlock(&pool->sp_lock);

	cv_destroy(&st->st_cond);
	mem_free(st, sizeof(*st));

	if (!ismaster)
		wakeup(pool);
}

static void
svc_thread_start(void *arg)
{

	svc_run_internal((SVCPOOL *) arg, FALSE);
	kthread_exit();
}

static void
svc_new_thread(SVCPOOL *pool)
{
	struct thread *td;

	pool->sp_threadcount++;
	kthread_add(svc_thread_start, pool,
	    pool->sp_proc, &td, 0, 0,
	    "%s: service", pool->sp_name);
}

void
svc_run(SVCPOOL *pool)
{
	int i;
	struct proc *p;
	struct thread *td;

	p = curproc;
	td = curthread;
	snprintf(td->td_name, sizeof(td->td_name),
	    "%s: master", pool->sp_name);
	pool->sp_state = SVCPOOL_ACTIVE;
	pool->sp_proc = p;
	pool->sp_lastcreatetime = time_uptime;
	pool->sp_threadcount = 1;

	for (i = 1; i < pool->sp_minthreads; i++) {
		svc_new_thread(pool);
	}

	svc_run_internal(pool, TRUE);

	mtx_lock(&pool->sp_lock);
	while (pool->sp_threadcount > 0)
		msleep(pool, &pool->sp_lock, 0, "svcexit", 0);
	mtx_unlock(&pool->sp_lock);
}

void
svc_exit(SVCPOOL *pool)
{
	SVCTHREAD *st;

	mtx_lock(&pool->sp_lock);

	pool->sp_state = SVCPOOL_CLOSING;
	LIST_FOREACH(st, &pool->sp_idlethreads, st_ilink)
		cv_signal(&st->st_cond);

	mtx_unlock(&pool->sp_lock);
}

bool_t
svc_getargs(struct svc_req *rqstp, xdrproc_t xargs, void *args)
{
	struct mbuf *m;
	XDR xdrs;
	bool_t stat;

	m = rqstp->rq_args;
	rqstp->rq_args = NULL;

	xdrmbuf_create(&xdrs, m, XDR_DECODE);
	stat = xargs(&xdrs, args);
	XDR_DESTROY(&xdrs);

	return (stat);
}

bool_t
svc_freeargs(struct svc_req *rqstp, xdrproc_t xargs, void *args)
{
	XDR xdrs;

	if (rqstp->rq_addr) {
		free(rqstp->rq_addr, M_SONAME);
		rqstp->rq_addr = NULL;
	}

	xdrs.x_op = XDR_FREE;
	return (xargs(&xdrs, args));
}

void
svc_freereq(struct svc_req *rqstp)
{
	SVCTHREAD *st;
	SVCXPRT *xprt;
	SVCPOOL *pool;

	st = rqstp->rq_thread;
	xprt = rqstp->rq_xprt;
	if (xprt)
		pool = xprt->xp_pool;
	else
		pool = NULL;
	if (st) {
		mtx_lock(&pool->sp_lock);
		KASSERT(rqstp == STAILQ_FIRST(&st->st_reqs),
		    ("Freeing request out of order"));
		STAILQ_REMOVE_HEAD(&st->st_reqs, rq_link);
		st->st_reqcount--;
		if (pool->sp_done)
			pool->sp_done(st, rqstp);
		mtx_unlock(&pool->sp_lock);
	}

	if (rqstp->rq_auth.svc_ah_ops)
		SVCAUTH_RELEASE(&rqstp->rq_auth);

	if (rqstp->rq_xprt) {
		SVC_RELEASE(rqstp->rq_xprt);
	}

	if (rqstp->rq_addr)
		free(rqstp->rq_addr, M_SONAME);

	if (rqstp->rq_args)
		m_freem(rqstp->rq_args);

	free(rqstp, M_RPC);
}
