/*-
 * Copyright (c) 1989, 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
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
 *	@(#)nfs_vfsops.c	8.12 (Berkeley) 5/20/95
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");


#include "opt_bootp.h"
#include "opt_nfsroot.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/jail.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/vnode.h>
#include <sys/signalvar.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/uma.h>

#include <net/if.h>
#include <net/route.h>
#include <net/vnet.h>

#include <netinet/in.h>

#include <rpc/rpc.h>

#include <nfs/nfsproto.h>
#include <nfsclient/nfs.h>
#include <nfsclient/nfsnode.h>
#include <nfsclient/nfsmount.h>
#include <nfs/xdr_subs.h>
#include <nfsclient/nfsm_subs.h>
#include <nfs/nfsdiskless.h>

FEATURE(nfsclient, "NFS client");

MALLOC_DEFINE(M_NFSREQ, "nfsclient_req", "NFS request header");
MALLOC_DEFINE(M_NFSBIGFH, "nfsclient_bigfh", "NFS version 3 file handle");
MALLOC_DEFINE(M_NFSDIROFF, "nfsclient_diroff", "NFS directory offset data");
MALLOC_DEFINE(M_NFSHASH, "nfsclient_hash", "NFS hash tables");
MALLOC_DEFINE(M_NFSDIRECTIO, "nfsclient_directio", "NFS Direct IO async write state");

uma_zone_t nfsmount_zone;

struct nfsstats	nfsstats;

SYSCTL_NODE(_vfs, OID_AUTO, oldnfs, CTLFLAG_RW, 0, "Old NFS filesystem");
SYSCTL_STRUCT(_vfs_oldnfs, NFS_NFSSTATS, nfsstats, CTLFLAG_RW,
	&nfsstats, nfsstats, "S,nfsstats");
static int nfs_ip_paranoia = 1;
SYSCTL_INT(_vfs_oldnfs, OID_AUTO, nfs_ip_paranoia, CTLFLAG_RW,
    &nfs_ip_paranoia, 0,
    "Disallow accepting replies from IPs which differ from those sent");
#ifdef NFS_DEBUG
int nfs_debug;
SYSCTL_INT(_vfs_oldnfs, OID_AUTO, debug, CTLFLAG_RW, &nfs_debug, 0,
    "Toggle debug flag");
#endif
static int nfs_tprintf_initial_delay = NFS_TPRINTF_INITIAL_DELAY;
SYSCTL_INT(_vfs_oldnfs, NFS_TPRINTF_INITIAL_DELAY,
    downdelayinitial, CTLFLAG_RW, &nfs_tprintf_initial_delay, 0,
    "Delay before printing \"nfs server not responding\" messages");
/* how long between console messages "nfs server foo not responding" */
static int nfs_tprintf_delay = NFS_TPRINTF_DELAY;
SYSCTL_INT(_vfs_oldnfs, NFS_TPRINTF_DELAY,
    downdelayinterval, CTLFLAG_RW, &nfs_tprintf_delay, 0,
    "Delay between printing \"nfs server not responding\" messages");

static void	nfs_decode_args(struct mount *mp, struct nfsmount *nmp,
		    struct nfs_args *argp, const char *hostname);
static int	mountnfs(struct nfs_args *, struct mount *,
		    struct sockaddr *, char *, struct vnode **,
		    struct ucred *cred, int, int);
static void	nfs_getnlminfo(struct vnode *, uint8_t *, size_t *,
		    struct sockaddr_storage *, int *, off_t *,
		    struct timeval *);
static vfs_mount_t nfs_mount;
static vfs_cmount_t nfs_cmount;
static vfs_unmount_t nfs_unmount;
static vfs_root_t nfs_root;
static vfs_statfs_t nfs_statfs;
static vfs_sync_t nfs_sync;
static vfs_sysctl_t nfs_sysctl;

static int	fake_wchan;

/*
 * nfs vfs operations.
 */
static struct vfsops nfs_vfsops = {
	.vfs_init =		nfs_init,
	.vfs_mount =		nfs_mount,
	.vfs_cmount =		nfs_cmount,
	.vfs_root =		nfs_root,
	.vfs_statfs =		nfs_statfs,
	.vfs_sync =		nfs_sync,
	.vfs_uninit =		nfs_uninit,
	.vfs_unmount =		nfs_unmount,
	.vfs_sysctl =		nfs_sysctl,
};
VFS_SET(nfs_vfsops, oldnfs, VFCF_NETWORK);

/* So that loader and kldload(2) can find us, wherever we are.. */
MODULE_VERSION(oldnfs, 1);
MODULE_DEPEND(oldnfs, krpc, 1, 1, 1);
#ifdef KGSSAPI
MODULE_DEPEND(oldnfs, kgssapi, 1, 1, 1);
#endif
MODULE_DEPEND(oldnfs, nfs_common, 1, 1, 1);
MODULE_DEPEND(oldnfs, nfslock, 1, 1, 1);

static struct nfs_rpcops nfs_rpcops = {
	nfs_readrpc,
	nfs_writerpc,
	nfs_writebp,
	nfs_readlinkrpc,
	nfs_invaldir,
	nfs_commit,
};

/*
 * This structure is now defined in sys/nfs/nfs_diskless.c so that it
 * can be shared by both NFS clients. It is declared here so that it
 * will be defined for kernels built without NFS_ROOT, although it
 * isn't used in that case.
 */
#ifndef NFS_ROOT
struct nfs_diskless	nfs_diskless = { { { 0 } } };
struct nfsv3_diskless	nfsv3_diskless = { { { 0 } } };
int			nfs_diskless_valid = 0;
#endif

SYSCTL_INT(_vfs_oldnfs, OID_AUTO, diskless_valid, CTLFLAG_RD,
    &nfs_diskless_valid, 0,
    "Has the diskless struct been filled correctly");

SYSCTL_STRING(_vfs_oldnfs, OID_AUTO, diskless_rootpath, CTLFLAG_RD,
    nfsv3_diskless.root_hostnam, 0, "Path to nfs root");

SYSCTL_OPAQUE(_vfs_oldnfs, OID_AUTO, diskless_rootaddr, CTLFLAG_RD,
    &nfsv3_diskless.root_saddr, sizeof nfsv3_diskless.root_saddr,
    "%Ssockaddr_in", "Diskless root nfs address");


void		nfsargs_ntoh(struct nfs_args *);
static int	nfs_mountdiskless(char *,
		    struct sockaddr_in *, struct nfs_args *,
		    struct thread *, struct vnode **, struct mount *);
static void	nfs_convert_diskless(void);
static void	nfs_convert_oargs(struct nfs_args *args,
		    struct onfs_args *oargs);

int
nfs_iosize(struct nfsmount *nmp)
{
	int iosize;

	/*
	 * Calculate the size used for io buffers.  Use the larger
	 * of the two sizes to minimise nfs requests but make sure
	 * that it is at least one VM page to avoid wasting buffer
	 * space.
	 */
	iosize = imax(nmp->nm_rsize, nmp->nm_wsize);
	iosize = imax(iosize, PAGE_SIZE);
	return (iosize);
}

static void
nfs_convert_oargs(struct nfs_args *args, struct onfs_args *oargs)
{

	args->version = NFS_ARGSVERSION;
	args->addr = oargs->addr;
	args->addrlen = oargs->addrlen;
	args->sotype = oargs->sotype;
	args->proto = oargs->proto;
	args->fh = oargs->fh;
	args->fhsize = oargs->fhsize;
	args->flags = oargs->flags;
	args->wsize = oargs->wsize;
	args->rsize = oargs->rsize;
	args->readdirsize = oargs->readdirsize;
	args->timeo = oargs->timeo;
	args->retrans = oargs->retrans;
	args->maxgrouplist = oargs->maxgrouplist;
	args->readahead = oargs->readahead;
	args->deadthresh = oargs->deadthresh;
	args->hostname = oargs->hostname;
}

static void
nfs_convert_diskless(void)
{

	bcopy(&nfs_diskless.myif, &nfsv3_diskless.myif,
		sizeof(struct ifaliasreq));
	bcopy(&nfs_diskless.mygateway, &nfsv3_diskless.mygateway,
		sizeof(struct sockaddr_in));
	nfs_convert_oargs(&nfsv3_diskless.root_args,&nfs_diskless.root_args);
	if (nfsv3_diskless.root_args.flags & NFSMNT_NFSV3) {
		nfsv3_diskless.root_fhsize = NFSX_V3FH;
		bcopy(nfs_diskless.root_fh, nfsv3_diskless.root_fh, NFSX_V3FH);
	} else {
		nfsv3_diskless.root_fhsize = NFSX_V2FH;
		bcopy(nfs_diskless.root_fh, nfsv3_diskless.root_fh, NFSX_V2FH);
	}
	bcopy(&nfs_diskless.root_saddr,&nfsv3_diskless.root_saddr,
		sizeof(struct sockaddr_in));
	bcopy(nfs_diskless.root_hostnam, nfsv3_diskless.root_hostnam, MNAMELEN);
	nfsv3_diskless.root_time = nfs_diskless.root_time;
	bcopy(nfs_diskless.my_hostnam, nfsv3_diskless.my_hostnam,
		MAXHOSTNAMELEN);
	nfs_diskless_valid = 3;
}

/*
 * nfs statfs call
 */
static int
nfs_statfs(struct mount *mp, struct statfs *sbp)
{
	struct vnode *vp;
	struct thread *td;
	struct nfs_statfs *sfp;
	caddr_t bpos, dpos;
	struct nfsmount *nmp = VFSTONFS(mp);
	int error = 0, v3 = (nmp->nm_flag & NFSMNT_NFSV3), retattr;
	struct mbuf *mreq, *mrep, *md, *mb;
	struct nfsnode *np;
	u_quad_t tquad;

	td = curthread;
#ifndef nolint
	sfp = NULL;
#endif
	error = vfs_busy(mp, MBF_NOWAIT);
	if (error)
		return (error);
	error = nfs_nget(mp, (nfsfh_t *)nmp->nm_fh, nmp->nm_fhsize, &np, LK_EXCLUSIVE);
	if (error) {
		vfs_unbusy(mp);
		return (error);
	}
	vp = NFSTOV(np);
	mtx_lock(&nmp->nm_mtx);
	if (v3 && (nmp->nm_state & NFSSTA_GOTFSINFO) == 0) {
		mtx_unlock(&nmp->nm_mtx);		
		(void)nfs_fsinfo(nmp, vp, td->td_ucred, td);
	} else
		mtx_unlock(&nmp->nm_mtx);
	nfsstats.rpccnt[NFSPROC_FSSTAT]++;
	mreq = nfsm_reqhead(vp, NFSPROC_FSSTAT, NFSX_FH(v3));
	mb = mreq;
	bpos = mtod(mb, caddr_t);
	nfsm_fhtom(vp, v3);
	nfsm_request(vp, NFSPROC_FSSTAT, td, td->td_ucred);
	if (v3)
		nfsm_postop_attr(vp, retattr);
	if (error) {
		if (mrep != NULL)
			m_freem(mrep);
		goto nfsmout;
	}
	sfp = nfsm_dissect(struct nfs_statfs *, NFSX_STATFS(v3));
	mtx_lock(&nmp->nm_mtx);
	sbp->f_iosize = nfs_iosize(nmp);
	mtx_unlock(&nmp->nm_mtx);
	if (v3) {
		sbp->f_bsize = NFS_FABLKSIZE;
		tquad = fxdr_hyper(&sfp->sf_tbytes);
		sbp->f_blocks = tquad / NFS_FABLKSIZE;
		tquad = fxdr_hyper(&sfp->sf_fbytes);
		sbp->f_bfree = tquad / NFS_FABLKSIZE;
		tquad = fxdr_hyper(&sfp->sf_abytes);
		sbp->f_bavail = tquad / NFS_FABLKSIZE;
		sbp->f_files = (fxdr_unsigned(int32_t,
		    sfp->sf_tfiles.nfsuquad[1]) & 0x7fffffff);
		sbp->f_ffree = (fxdr_unsigned(int32_t,
		    sfp->sf_ffiles.nfsuquad[1]) & 0x7fffffff);
	} else {
		sbp->f_bsize = fxdr_unsigned(int32_t, sfp->sf_bsize);
		sbp->f_blocks = fxdr_unsigned(int32_t, sfp->sf_blocks);
		sbp->f_bfree = fxdr_unsigned(int32_t, sfp->sf_bfree);
		sbp->f_bavail = fxdr_unsigned(int32_t, sfp->sf_bavail);
		sbp->f_files = 0;
		sbp->f_ffree = 0;
	}
	m_freem(mrep);
nfsmout:
	vput(vp);
	vfs_unbusy(mp);
	return (error);
}

/*
 * nfs version 3 fsinfo rpc call
 */
int
nfs_fsinfo(struct nfsmount *nmp, struct vnode *vp, struct ucred *cred,
    struct thread *td)
{
	struct nfsv3_fsinfo *fsp;
	u_int32_t pref, max;
	caddr_t bpos, dpos;
	int error = 0, retattr;
	struct mbuf *mreq, *mrep, *md, *mb;
	u_int64_t maxfsize;
	
	nfsstats.rpccnt[NFSPROC_FSINFO]++;
	mreq = nfsm_reqhead(vp, NFSPROC_FSINFO, NFSX_FH(1));
	mb = mreq;
	bpos = mtod(mb, caddr_t);
	nfsm_fhtom(vp, 1);
	nfsm_request(vp, NFSPROC_FSINFO, td, cred);
	nfsm_postop_attr(vp, retattr);
	if (!error) {
		fsp = nfsm_dissect(struct nfsv3_fsinfo *, NFSX_V3FSINFO);
		pref = fxdr_unsigned(u_int32_t, fsp->fs_wtpref);
		mtx_lock(&nmp->nm_mtx);
		if (pref < nmp->nm_wsize && pref >= NFS_FABLKSIZE)
			nmp->nm_wsize = (pref + NFS_FABLKSIZE - 1) &
				~(NFS_FABLKSIZE - 1);
		max = fxdr_unsigned(u_int32_t, fsp->fs_wtmax);
		if (max < nmp->nm_wsize && max > 0) {
			nmp->nm_wsize = max & ~(NFS_FABLKSIZE - 1);
			if (nmp->nm_wsize == 0)
				nmp->nm_wsize = max;
		}
		pref = fxdr_unsigned(u_int32_t, fsp->fs_rtpref);
		if (pref < nmp->nm_rsize && pref >= NFS_FABLKSIZE)
			nmp->nm_rsize = (pref + NFS_FABLKSIZE - 1) &
				~(NFS_FABLKSIZE - 1);
		max = fxdr_unsigned(u_int32_t, fsp->fs_rtmax);
		if (max < nmp->nm_rsize && max > 0) {
			nmp->nm_rsize = max & ~(NFS_FABLKSIZE - 1);
			if (nmp->nm_rsize == 0)
				nmp->nm_rsize = max;
		}
		pref = fxdr_unsigned(u_int32_t, fsp->fs_dtpref);
		if (pref < nmp->nm_readdirsize && pref >= NFS_DIRBLKSIZ)
			nmp->nm_readdirsize = (pref + NFS_DIRBLKSIZ - 1) &
				~(NFS_DIRBLKSIZ - 1);
		if (max < nmp->nm_readdirsize && max > 0) {
			nmp->nm_readdirsize = max & ~(NFS_DIRBLKSIZ - 1);
			if (nmp->nm_readdirsize == 0)
				nmp->nm_readdirsize = max;
		}
		maxfsize = fxdr_hyper(&fsp->fs_maxfilesize);
		if (maxfsize > 0 && maxfsize < nmp->nm_maxfilesize)
			nmp->nm_maxfilesize = maxfsize;
		nmp->nm_mountp->mnt_stat.f_iosize = nfs_iosize(nmp);
		nmp->nm_state |= NFSSTA_GOTFSINFO;
		mtx_unlock(&nmp->nm_mtx);
	}
	m_freem(mrep);
nfsmout:
	return (error);
}

/*
 * Mount a remote root fs via. nfs. This depends on the info in the
 * nfs_diskless structure that has been filled in properly by some primary
 * bootstrap.
 * It goes something like this:
 * - do enough of "ifconfig" by calling ifioctl() so that the system
 *   can talk to the server
 * - If nfs_diskless.mygateway is filled in, use that address as
 *   a default gateway.
 * - build the rootfs mount point and call mountnfs() to do the rest.
 *
 * It is assumed to be safe to read, modify, and write the nfsv3_diskless
 * structure, as well as other global NFS client variables here, as
 * nfs_mountroot() will be called once in the boot before any other NFS
 * client activity occurs.
 */
int
nfs_mountroot(struct mount *mp)
{
	struct thread *td = curthread;
	struct nfsv3_diskless *nd = &nfsv3_diskless;
	struct socket *so;
	struct vnode *vp;
	struct ifreq ir;
	int error;
	u_long l;
	char buf[128];
	char *cp;


#if defined(BOOTP_NFSROOT) && defined(BOOTP)
	bootpc_init();		/* use bootp to get nfs_diskless filled in */
#elif defined(NFS_ROOT)
	nfs_setup_diskless();
#endif

	if (nfs_diskless_valid == 0) {
		return (-1);
	}
	if (nfs_diskless_valid == 1)
		nfs_convert_diskless();

	/*
	 * XXX splnet, so networks will receive...
	 */
	splnet();

	/*
	 * Do enough of ifconfig(8) so that the critical net interface can
	 * talk to the server.
	 */
	error = socreate(nd->myif.ifra_addr.sa_family, &so, nd->root_args.sotype, 0,
	    td->td_ucred, td);
	if (error)
		panic("nfs_mountroot: socreate(%04x): %d",
			nd->myif.ifra_addr.sa_family, error);

#if 0 /* XXX Bad idea */
	/*
	 * We might not have been told the right interface, so we pass
	 * over the first ten interfaces of the same kind, until we get
	 * one of them configured.
	 */

	for (i = strlen(nd->myif.ifra_name) - 1;
		nd->myif.ifra_name[i] >= '0' &&
		nd->myif.ifra_name[i] <= '9';
		nd->myif.ifra_name[i] ++) {
		error = ifioctl(so, SIOCAIFADDR, (caddr_t)&nd->myif, td);
		if(!error)
			break;
	}
#endif

	error = ifioctl(so, SIOCAIFADDR, (caddr_t)&nd->myif, td);
	if (error)
		panic("nfs_mountroot: SIOCAIFADDR: %d", error);

	if ((cp = getenv("boot.netif.mtu")) != NULL) {
		ir.ifr_mtu = strtol(cp, NULL, 10);
		bcopy(nd->myif.ifra_name, ir.ifr_name, IFNAMSIZ);
		freeenv(cp);
		error = ifioctl(so, SIOCSIFMTU, (caddr_t)&ir, td);
		if (error)
			printf("nfs_mountroot: SIOCSIFMTU: %d", error);
	}
	soclose(so);

	/*
	 * If the gateway field is filled in, set it as the default route.
	 * Note that pxeboot will set a default route of 0 if the route
	 * is not set by the DHCP server.  Check also for a value of 0
	 * to avoid panicking inappropriately in that situation.
	 */
	if (nd->mygateway.sin_len != 0 &&
	    nd->mygateway.sin_addr.s_addr != 0) {
		struct sockaddr_in mask, sin;

		bzero((caddr_t)&mask, sizeof(mask));
		sin = mask;
		sin.sin_family = AF_INET;
		sin.sin_len = sizeof(sin);
                /* XXX MRT use table 0 for this sort of thing */
		CURVNET_SET(TD_TO_VNET(td));
		error = rtrequest_fib(RTM_ADD, (struct sockaddr *)&sin,
		    (struct sockaddr *)&nd->mygateway,
		    (struct sockaddr *)&mask,
		    RTF_UP | RTF_GATEWAY, NULL, RT_DEFAULT_FIB);
		CURVNET_RESTORE();
		if (error)
			panic("nfs_mountroot: RTM_ADD: %d", error);
	}

	/*
	 * Create the rootfs mount point.
	 */
	nd->root_args.fh = nd->root_fh;
	nd->root_args.fhsize = nd->root_fhsize;
	l = ntohl(nd->root_saddr.sin_addr.s_addr);
	snprintf(buf, sizeof(buf), "%ld.%ld.%ld.%ld:%s",
		(l >> 24) & 0xff, (l >> 16) & 0xff,
		(l >>  8) & 0xff, (l >>  0) & 0xff, nd->root_hostnam);
	printf("NFS ROOT: %s\n", buf);
	nd->root_args.hostname = buf;
	if ((error = nfs_mountdiskless(buf,
	    &nd->root_saddr, &nd->root_args, td, &vp, mp)) != 0) {
		return (error);
	}

	/*
	 * This is not really an nfs issue, but it is much easier to
	 * set hostname here and then let the "/etc/rc.xxx" files
	 * mount the right /var based upon its preset value.
	 */
	mtx_lock(&prison0.pr_mtx);
	strlcpy(prison0.pr_hostname, nd->my_hostnam,
	    sizeof (prison0.pr_hostname));
	mtx_unlock(&prison0.pr_mtx);
	inittodr(ntohl(nd->root_time));
	return (0);
}

/*
 * Internal version of mount system call for diskless setup.
 */
static int
nfs_mountdiskless(char *path,
    struct sockaddr_in *sin, struct nfs_args *args, struct thread *td,
    struct vnode **vpp, struct mount *mp)
{
	struct sockaddr *nam;
	int error;

	nam = sodupsockaddr((struct sockaddr *)sin, M_WAITOK);
	if ((error = mountnfs(args, mp, nam, path, vpp, td->td_ucred,
	    NFS_DEFAULT_NAMETIMEO, NFS_DEFAULT_NEGNAMETIMEO)) != 0) {
		printf("nfs_mountroot: mount %s on /: %d\n", path, error);
		return (error);
	}
	return (0);
}

static int
nfs_sec_name_to_num(char *sec)
{
	if (!strcmp(sec, "krb5"))
		return (RPCSEC_GSS_KRB5);
	if (!strcmp(sec, "krb5i"))
		return (RPCSEC_GSS_KRB5I);
	if (!strcmp(sec, "krb5p"))
		return (RPCSEC_GSS_KRB5P);
	if (!strcmp(sec, "sys"))
		return (AUTH_SYS);
	/*
	 * Userland should validate the string but we will try and
	 * cope with unexpected values.
	 */
	return (AUTH_SYS);
}

static void
nfs_decode_args(struct mount *mp, struct nfsmount *nmp, struct nfs_args *argp,
	const char *hostname)
{
	int s;
	int adjsock;
	int maxio;
	char *p;
	char *secname;
	char *principal;

	s = splnet();

	/*
	 * Set read-only flag if requested; otherwise, clear it if this is
	 * an update.  If this is not an update, then either the read-only
	 * flag is already clear, or this is a root mount and it was set
	 * intentionally at some previous point.
	 */
	if (vfs_getopt(mp->mnt_optnew, "ro", NULL, NULL) == 0) {
		MNT_ILOCK(mp);
		mp->mnt_flag |= MNT_RDONLY;
		MNT_IUNLOCK(mp);
	} else if (mp->mnt_flag & MNT_UPDATE) {
		MNT_ILOCK(mp);
		mp->mnt_flag &= ~MNT_RDONLY;
		MNT_IUNLOCK(mp);
	}

	/*
	 * Silently clear NFSMNT_NOCONN if it's a TCP mount, it makes
	 * no sense in that context.  Also, set up appropriate retransmit
	 * and soft timeout behavior.
	 */
	if (argp->sotype == SOCK_STREAM) {
		nmp->nm_flag &= ~NFSMNT_NOCONN;
		nmp->nm_flag |= NFSMNT_DUMBTIMR;
		nmp->nm_timeo = NFS_MAXTIMEO;
		nmp->nm_retry = NFS_RETRANS_TCP;
	}

	/* Also clear RDIRPLUS if not NFSv3, it crashes some servers */
	if ((argp->flags & NFSMNT_NFSV3) == 0)
		nmp->nm_flag &= ~NFSMNT_RDIRPLUS;

	/* Re-bind if rsrvd port requested and wasn't on one */
	adjsock = !(nmp->nm_flag & NFSMNT_RESVPORT)
		  && (argp->flags & NFSMNT_RESVPORT);
	/* Also re-bind if we're switching to/from a connected UDP socket */
	adjsock |= ((nmp->nm_flag & NFSMNT_NOCONN) !=
		    (argp->flags & NFSMNT_NOCONN));

	/* Update flags atomically.  Don't change the lock bits. */
	nmp->nm_flag = argp->flags | nmp->nm_flag;
	splx(s);

	if ((argp->flags & NFSMNT_TIMEO) && argp->timeo > 0) {
		nmp->nm_timeo = (argp->timeo * NFS_HZ + 5) / 10;
		if (nmp->nm_timeo < NFS_MINTIMEO)
			nmp->nm_timeo = NFS_MINTIMEO;
		else if (nmp->nm_timeo > NFS_MAXTIMEO)
			nmp->nm_timeo = NFS_MAXTIMEO;
	}

	if ((argp->flags & NFSMNT_RETRANS) && argp->retrans > 1) {
		nmp->nm_retry = argp->retrans;
		if (nmp->nm_retry > NFS_MAXREXMIT)
			nmp->nm_retry = NFS_MAXREXMIT;
	}

	if (argp->flags & NFSMNT_NFSV3) {
		if (argp->sotype == SOCK_DGRAM)
			maxio = NFS_MAXDGRAMDATA;
		else
			maxio = NFS_MAXDATA;
	} else
		maxio = NFS_V2MAXDATA;

	if ((argp->flags & NFSMNT_WSIZE) && argp->wsize > 0) {
		nmp->nm_wsize = argp->wsize;
		/* Round down to multiple of blocksize */
		nmp->nm_wsize &= ~(NFS_FABLKSIZE - 1);
		if (nmp->nm_wsize <= 0)
			nmp->nm_wsize = NFS_FABLKSIZE;
	}
	if (nmp->nm_wsize > maxio)
		nmp->nm_wsize = maxio;
	if (nmp->nm_wsize > MAXBSIZE)
		nmp->nm_wsize = MAXBSIZE;

	if ((argp->flags & NFSMNT_RSIZE) && argp->rsize > 0) {
		nmp->nm_rsize = argp->rsize;
		/* Round down to multiple of blocksize */
		nmp->nm_rsize &= ~(NFS_FABLKSIZE - 1);
		if (nmp->nm_rsize <= 0)
			nmp->nm_rsize = NFS_FABLKSIZE;
	}
	if (nmp->nm_rsize > maxio)
		nmp->nm_rsize = maxio;
	if (nmp->nm_rsize > MAXBSIZE)
		nmp->nm_rsize = MAXBSIZE;

	if ((argp->flags & NFSMNT_READDIRSIZE) && argp->readdirsize > 0) {
		nmp->nm_readdirsize = argp->readdirsize;
	}
	if (nmp->nm_readdirsize > maxio)
		nmp->nm_readdirsize = maxio;
	if (nmp->nm_readdirsize > nmp->nm_rsize)
		nmp->nm_readdirsize = nmp->nm_rsize;

	if ((argp->flags & NFSMNT_ACREGMIN) && argp->acregmin >= 0)
		nmp->nm_acregmin = argp->acregmin;
	else
		nmp->nm_acregmin = NFS_MINATTRTIMO;
	if ((argp->flags & NFSMNT_ACREGMAX) && argp->acregmax >= 0)
		nmp->nm_acregmax = argp->acregmax;
	else
		nmp->nm_acregmax = NFS_MAXATTRTIMO;
	if ((argp->flags & NFSMNT_ACDIRMIN) && argp->acdirmin >= 0)
		nmp->nm_acdirmin = argp->acdirmin;
	else
		nmp->nm_acdirmin = NFS_MINDIRATTRTIMO;
	if ((argp->flags & NFSMNT_ACDIRMAX) && argp->acdirmax >= 0)
		nmp->nm_acdirmax = argp->acdirmax;
	else
		nmp->nm_acdirmax = NFS_MAXDIRATTRTIMO;
	if (nmp->nm_acdirmin > nmp->nm_acdirmax)
		nmp->nm_acdirmin = nmp->nm_acdirmax;
	if (nmp->nm_acregmin > nmp->nm_acregmax)
		nmp->nm_acregmin = nmp->nm_acregmax;

	if ((argp->flags & NFSMNT_MAXGRPS) && argp->maxgrouplist >= 0) {
		if (argp->maxgrouplist <= NFS_MAXGRPS)
			nmp->nm_numgrps = argp->maxgrouplist;
		else
			nmp->nm_numgrps = NFS_MAXGRPS;
	}
	if ((argp->flags & NFSMNT_READAHEAD) && argp->readahead >= 0) {
		if (argp->readahead <= NFS_MAXRAHEAD)
			nmp->nm_readahead = argp->readahead;
		else
			nmp->nm_readahead = NFS_MAXRAHEAD;
	}
	if ((argp->flags & NFSMNT_WCOMMITSIZE) && argp->wcommitsize >= 0) {
		if (argp->wcommitsize < nmp->nm_wsize)
			nmp->nm_wcommitsize = nmp->nm_wsize;
		else
			nmp->nm_wcommitsize = argp->wcommitsize;
	}
	if ((argp->flags & NFSMNT_DEADTHRESH) && argp->deadthresh >= 0) {
		if (argp->deadthresh <= NFS_MAXDEADTHRESH)
			nmp->nm_deadthresh = argp->deadthresh;
		else
			nmp->nm_deadthresh = NFS_MAXDEADTHRESH;
	}

	adjsock |= ((nmp->nm_sotype != argp->sotype) ||
		    (nmp->nm_soproto != argp->proto));
	nmp->nm_sotype = argp->sotype;
	nmp->nm_soproto = argp->proto;

	if (nmp->nm_client && adjsock) {
		nfs_safedisconnect(nmp);
		if (nmp->nm_sotype == SOCK_DGRAM)
			while (nfs_connect(nmp)) {
				printf("nfs_args: retrying connect\n");
				(void) tsleep(&fake_wchan, PSOCK, "nfscon", hz);
			}
	}

	if (hostname) {
		strlcpy(nmp->nm_hostname, hostname,
		    sizeof(nmp->nm_hostname));
		p = strchr(nmp->nm_hostname, ':');
		if (p)
			*p = '\0';
	}

	if (vfs_getopt(mp->mnt_optnew, "sec",
		(void **) &secname, NULL) == 0) {
		nmp->nm_secflavor = nfs_sec_name_to_num(secname);
	} else {
		nmp->nm_secflavor = AUTH_SYS;
	}

	if (vfs_getopt(mp->mnt_optnew, "principal",
		(void **) &principal, NULL) == 0) {
		strlcpy(nmp->nm_principal, principal,
		    sizeof(nmp->nm_principal));
	} else {
		snprintf(nmp->nm_principal, sizeof(nmp->nm_principal),
		    "nfs@%s", nmp->nm_hostname);
	}
}

static const char *nfs_opts[] = { "from", "nfs_args",
    "noatime", "noexec", "suiddir", "nosuid", "nosymfollow", "union",
    "noclusterr", "noclusterw", "multilabel", "acls", "force", "update",
    "async", "dumbtimer", "noconn", "nolockd", "intr", "rdirplus", "resvport",
    "readahead", "readdirsize", "soft", "hard", "mntudp", "tcp", "udp",
    "wsize", "rsize", "retrans", "acregmin", "acregmax", "acdirmin",
    "acdirmax", "deadthresh", "hostname", "timeout", "addr", "fh", "nfsv3",
    "sec", "maxgroups", "principal", "negnametimeo", "nocto", "wcommitsize",
    "nametimeo",
    NULL };

/*
 * VFS Operations.
 *
 * mount system call
 * It seems a bit dumb to copyinstr() the host and path here and then
 * bcopy() them in mountnfs(), but I wanted to detect errors before
 * doing the sockargs() call because sockargs() allocates an mbuf and
 * an error after that means that I have to release the mbuf.
 */
/* ARGSUSED */
static int
nfs_mount(struct mount *mp)
{
	struct nfs_args args = {
	    .version = NFS_ARGSVERSION,
	    .addr = NULL,
	    .addrlen = sizeof (struct sockaddr_in),
	    .sotype = SOCK_STREAM,
	    .proto = 0,
	    .fh = NULL,
	    .fhsize = 0,
	    .flags = NFSMNT_RESVPORT,
	    .wsize = NFS_WSIZE,
	    .rsize = NFS_RSIZE,
	    .readdirsize = NFS_READDIRSIZE,
	    .timeo = 10,
	    .retrans = NFS_RETRANS,
	    .maxgrouplist = NFS_MAXGRPS,
	    .readahead = NFS_DEFRAHEAD,
	    .wcommitsize = 0,			/* was: NQ_DEFLEASE */
	    .deadthresh = NFS_MAXDEADTHRESH,	/* was: NQ_DEADTHRESH */
	    .hostname = NULL,
	    /* args version 4 */
	    .acregmin = NFS_MINATTRTIMO,
	    .acregmax = NFS_MAXATTRTIMO,
	    .acdirmin = NFS_MINDIRATTRTIMO,
	    .acdirmax = NFS_MAXDIRATTRTIMO,
	};
	int error, ret, has_nfs_args_opt;
	int has_addr_opt, has_fh_opt, has_hostname_opt;
	struct sockaddr *nam;
	struct vnode *vp;
	char hst[MNAMELEN];
	size_t len;
	u_char nfh[NFSX_V3FHMAX];
	char *opt;
	int nametimeo = NFS_DEFAULT_NAMETIMEO;
	int negnametimeo = NFS_DEFAULT_NEGNAMETIMEO;

	has_nfs_args_opt = 0;
	has_addr_opt = 0;
	has_fh_opt = 0;
	has_hostname_opt = 0;

	if (vfs_filteropt(mp->mnt_optnew, nfs_opts)) {
		error = EINVAL;
		goto out;
	}

	if ((mp->mnt_flag & (MNT_ROOTFS | MNT_UPDATE)) == MNT_ROOTFS) {
		error = nfs_mountroot(mp);
		goto out;
	}

	/*
	 * The old mount_nfs program passed the struct nfs_args
	 * from userspace to kernel.  The new mount_nfs program
	 * passes string options via nmount() from userspace to kernel
	 * and we populate the struct nfs_args in the kernel.
	 */
	if (vfs_getopt(mp->mnt_optnew, "nfs_args", NULL, NULL) == 0) {
		error = vfs_copyopt(mp->mnt_optnew, "nfs_args", &args,
		    sizeof args);
		if (error)
			goto out;

		if (args.version != NFS_ARGSVERSION) {
			error = EPROGMISMATCH;
			goto out;
		}
		has_nfs_args_opt = 1;
	}

	if (vfs_getopt(mp->mnt_optnew, "dumbtimer", NULL, NULL) == 0)
		args.flags |= NFSMNT_DUMBTIMR;
	if (vfs_getopt(mp->mnt_optnew, "noconn", NULL, NULL) == 0)
		args.flags |= NFSMNT_NOCONN;
	if (vfs_getopt(mp->mnt_optnew, "conn", NULL, NULL) == 0)
		args.flags |= NFSMNT_NOCONN;
	if (vfs_getopt(mp->mnt_optnew, "nolockd", NULL, NULL) == 0)
		args.flags |= NFSMNT_NOLOCKD;
	if (vfs_getopt(mp->mnt_optnew, "lockd", NULL, NULL) == 0)
		args.flags &= ~NFSMNT_NOLOCKD;
	if (vfs_getopt(mp->mnt_optnew, "intr", NULL, NULL) == 0)
		args.flags |= NFSMNT_INT;
	if (vfs_getopt(mp->mnt_optnew, "rdirplus", NULL, NULL) == 0)
		args.flags |= NFSMNT_RDIRPLUS;
	if (vfs_getopt(mp->mnt_optnew, "resvport", NULL, NULL) == 0)
		args.flags |= NFSMNT_RESVPORT;
	if (vfs_getopt(mp->mnt_optnew, "noresvport", NULL, NULL) == 0)
		args.flags &= ~NFSMNT_RESVPORT;
	if (vfs_getopt(mp->mnt_optnew, "soft", NULL, NULL) == 0)
		args.flags |= NFSMNT_SOFT;
	if (vfs_getopt(mp->mnt_optnew, "hard", NULL, NULL) == 0)
		args.flags &= ~NFSMNT_SOFT;
	if (vfs_getopt(mp->mnt_optnew, "mntudp", NULL, NULL) == 0)
		args.sotype = SOCK_DGRAM;
	if (vfs_getopt(mp->mnt_optnew, "udp", NULL, NULL) == 0)
		args.sotype = SOCK_DGRAM;
	if (vfs_getopt(mp->mnt_optnew, "tcp", NULL, NULL) == 0)
		args.sotype = SOCK_STREAM;
	if (vfs_getopt(mp->mnt_optnew, "nfsv3", NULL, NULL) == 0)
		args.flags |= NFSMNT_NFSV3;
	if (vfs_getopt(mp->mnt_optnew, "nocto", NULL, NULL) == 0)
		args.flags |= NFSMNT_NOCTO;
	if (vfs_getopt(mp->mnt_optnew, "readdirsize", (void **)&opt, NULL) == 0) {
		if (opt == NULL) { 
			vfs_mount_error(mp, "illegal readdirsize");
			error = EINVAL;
			goto out;
		}
		ret = sscanf(opt, "%d", &args.readdirsize);
		if (ret != 1 || args.readdirsize <= 0) {
			vfs_mount_error(mp, "illegal readdirsize: %s",
			    opt);
			error = EINVAL;
			goto out;
		}
		args.flags |= NFSMNT_READDIRSIZE;
	}
	if (vfs_getopt(mp->mnt_optnew, "readahead", (void **)&opt, NULL) == 0) {
		if (opt == NULL) { 
			vfs_mount_error(mp, "illegal readahead");
			error = EINVAL;
			goto out;
		}
		ret = sscanf(opt, "%d", &args.readahead);
		if (ret != 1 || args.readahead <= 0) {
			vfs_mount_error(mp, "illegal readahead: %s",
			    opt);
			error = EINVAL;
			goto out;
		}
		args.flags |= NFSMNT_READAHEAD;
	}
	if (vfs_getopt(mp->mnt_optnew, "wsize", (void **)&opt, NULL) == 0) {
		if (opt == NULL) { 
			vfs_mount_error(mp, "illegal wsize");
			error = EINVAL;
			goto out;
		}
		ret = sscanf(opt, "%d", &args.wsize);
		if (ret != 1 || args.wsize <= 0) {
			vfs_mount_error(mp, "illegal wsize: %s",
			    opt);
			error = EINVAL;
			goto out;
		}
		args.flags |= NFSMNT_WSIZE;
	}
	if (vfs_getopt(mp->mnt_optnew, "rsize", (void **)&opt, NULL) == 0) {
		if (opt == NULL) { 
			vfs_mount_error(mp, "illegal rsize");
			error = EINVAL;
			goto out;
		}
		ret = sscanf(opt, "%d", &args.rsize);
		if (ret != 1 || args.rsize <= 0) {
			vfs_mount_error(mp, "illegal wsize: %s",
			    opt);
			error = EINVAL;
			goto out;
		}
		args.flags |= NFSMNT_RSIZE;
	}
	if (vfs_getopt(mp->mnt_optnew, "retrans", (void **)&opt, NULL) == 0) {
		if (opt == NULL) { 
			vfs_mount_error(mp, "illegal retrans");
			error = EINVAL;
			goto out;
		}
		ret = sscanf(opt, "%d", &args.retrans);
		if (ret != 1 || args.retrans <= 0) {
			vfs_mount_error(mp, "illegal retrans: %s",
			    opt);
			error = EINVAL;
			goto out;
		}
		args.flags |= NFSMNT_RETRANS;
	}
	if (vfs_getopt(mp->mnt_optnew, "acregmin", (void **)&opt, NULL) == 0) {
		ret = sscanf(opt, "%d", &args.acregmin);
		if (ret != 1 || args.acregmin < 0) {
			vfs_mount_error(mp, "illegal acregmin: %s",
			    opt);
			error = EINVAL;
			goto out;
		}
		args.flags |= NFSMNT_ACREGMIN;
	}
	if (vfs_getopt(mp->mnt_optnew, "acregmax", (void **)&opt, NULL) == 0) {
		ret = sscanf(opt, "%d", &args.acregmax);
		if (ret != 1 || args.acregmax < 0) {
			vfs_mount_error(mp, "illegal acregmax: %s",
			    opt);
			error = EINVAL;
			goto out;
		}
		args.flags |= NFSMNT_ACREGMAX;
	}
	if (vfs_getopt(mp->mnt_optnew, "acdirmin", (void **)&opt, NULL) == 0) {
		ret = sscanf(opt, "%d", &args.acdirmin);
		if (ret != 1 || args.acdirmin < 0) {
			vfs_mount_error(mp, "illegal acdirmin: %s",
			    opt);
			error = EINVAL;
			goto out;
		}
		args.flags |= NFSMNT_ACDIRMIN;
	}
	if (vfs_getopt(mp->mnt_optnew, "acdirmax", (void **)&opt, NULL) == 0) {
		ret = sscanf(opt, "%d", &args.acdirmax);
		if (ret != 1 || args.acdirmax < 0) {
			vfs_mount_error(mp, "illegal acdirmax: %s",
			    opt);
			error = EINVAL;
			goto out;
		}
		args.flags |= NFSMNT_ACDIRMAX;
	}
	if (vfs_getopt(mp->mnt_optnew, "wcommitsize", (void **)&opt, NULL) == 0) {
		ret = sscanf(opt, "%d", &args.wcommitsize);
		if (ret != 1 || args.wcommitsize < 0) {
			vfs_mount_error(mp, "illegal wcommitsize: %s", opt);
			error = EINVAL;
			goto out;
		}
		args.flags |= NFSMNT_WCOMMITSIZE;
	}
	if (vfs_getopt(mp->mnt_optnew, "deadthresh", (void **)&opt, NULL) == 0) {
		ret = sscanf(opt, "%d", &args.deadthresh);
		if (ret != 1 || args.deadthresh <= 0) {
			vfs_mount_error(mp, "illegal deadthresh: %s",
			    opt);
			error = EINVAL;
			goto out;
		}
		args.flags |= NFSMNT_DEADTHRESH;
	}
	if (vfs_getopt(mp->mnt_optnew, "timeout", (void **)&opt, NULL) == 0) {
		ret = sscanf(opt, "%d", &args.timeo);
		if (ret != 1 || args.timeo <= 0) {
			vfs_mount_error(mp, "illegal timeout: %s",
			    opt);
			error = EINVAL;
			goto out;
		}
		args.flags |= NFSMNT_TIMEO;
	}
	if (vfs_getopt(mp->mnt_optnew, "maxgroups", (void **)&opt, NULL) == 0) {
		ret = sscanf(opt, "%d", &args.maxgrouplist);
		if (ret != 1 || args.maxgrouplist <= 0) {
			vfs_mount_error(mp, "illegal maxgroups: %s",
			    opt);
			error = EINVAL;
			goto out;
		}
		args.flags |= NFSMNT_MAXGRPS;
	}
	if (vfs_getopt(mp->mnt_optnew, "nametimeo", (void **)&opt, NULL) == 0) {
		ret = sscanf(opt, "%d", &nametimeo);
		if (ret != 1 || nametimeo < 0) {
			vfs_mount_error(mp, "illegal nametimeo: %s", opt);
			error = EINVAL;
			goto out;
		}
	}
	if (vfs_getopt(mp->mnt_optnew, "negnametimeo", (void **)&opt, NULL)
	    == 0) {
		ret = sscanf(opt, "%d", &negnametimeo);
		if (ret != 1 || negnametimeo < 0) {
			vfs_mount_error(mp, "illegal negnametimeo: %s",
			    opt);
			error = EINVAL;
			goto out;
		}
	}
	if (vfs_getopt(mp->mnt_optnew, "addr", (void **)&args.addr,
		&args.addrlen) == 0) {
		has_addr_opt = 1;
		if (args.addrlen > SOCK_MAXADDRLEN) {
			error = ENAMETOOLONG;
			goto out;
		}
		nam = malloc(args.addrlen, M_SONAME,
		    M_WAITOK);
		bcopy(args.addr, nam, args.addrlen);
		nam->sa_len = args.addrlen;
	}
	if (vfs_getopt(mp->mnt_optnew, "fh", (void **)&args.fh,
		&args.fhsize) == 0) {
		has_fh_opt = 1;
	}
	if (vfs_getopt(mp->mnt_optnew, "hostname", (void **)&args.hostname,
		NULL) == 0) {
		has_hostname_opt = 1;
	}
	if (args.hostname == NULL) {
		vfs_mount_error(mp, "Invalid hostname");
		error = EINVAL;
		goto out;
	}
	if (args.fhsize < 0 || args.fhsize > NFSX_V3FHMAX) {
		vfs_mount_error(mp, "Bad file handle");
		error = EINVAL;
		goto out;
	}

	if (mp->mnt_flag & MNT_UPDATE) {
		struct nfsmount *nmp = VFSTONFS(mp);

		if (nmp == NULL) {
			error = EIO;
			goto out;
		}

		/*
		 * If a change from TCP->UDP is done and there are thread(s)
		 * that have I/O RPC(s) in progress with a tranfer size
		 * greater than NFS_MAXDGRAMDATA, those thread(s) will be
		 * hung, retrying the RPC(s) forever. Usually these threads
		 * will be seen doing an uninterruptible sleep on wait channel
		 * "newnfsreq" (truncated to "newnfsre" by procstat).
		 */
		if (args.sotype == SOCK_DGRAM && nmp->nm_sotype == SOCK_STREAM)
			tprintf(curthread->td_proc, LOG_WARNING,
	"Warning: mount -u that changes TCP->UDP can result in hung threads\n");

		/*
		 * When doing an update, we can't change from or to
		 * v3, switch lockd strategies or change cookie translation
		 */
		args.flags = (args.flags &
		    ~(NFSMNT_NFSV3 | NFSMNT_NOLOCKD /*|NFSMNT_XLATECOOKIE*/)) |
		    (nmp->nm_flag &
			(NFSMNT_NFSV3 | NFSMNT_NOLOCKD /*|NFSMNT_XLATECOOKIE*/));
		nfs_decode_args(mp, nmp, &args, NULL);
		goto out;
	}

	/*
	 * Make the nfs_ip_paranoia sysctl serve as the default connection
	 * or no-connection mode for those protocols that support 
	 * no-connection mode (the flag will be cleared later for protocols
	 * that do not support no-connection mode).  This will allow a client
	 * to receive replies from a different IP then the request was
	 * sent to.  Note: default value for nfs_ip_paranoia is 1 (paranoid),
	 * not 0.
	 */
	if (nfs_ip_paranoia == 0)
		args.flags |= NFSMNT_NOCONN;

	if (has_nfs_args_opt) {
		/*
		 * In the 'nfs_args' case, the pointers in the args
		 * structure are in userland - we copy them in here.
		 */
		if (!has_fh_opt) {
			error = copyin((caddr_t)args.fh, (caddr_t)nfh,
			    args.fhsize);
			if (error) {
				goto out;
			}
			args.fh = nfh;
		}
		if (!has_hostname_opt) {
			error = copyinstr(args.hostname, hst, MNAMELEN-1, &len);
			if (error) {
				goto out;
			}
			bzero(&hst[len], MNAMELEN - len);
			args.hostname = hst;
		}
		if (!has_addr_opt) {
			/* sockargs() call must be after above copyin() calls */
			error = getsockaddr(&nam, (caddr_t)args.addr,
			    args.addrlen);
			if (error) {
				goto out;
			}
		}
	} else if (has_addr_opt == 0) {
		vfs_mount_error(mp, "No server address");
		error = EINVAL;
		goto out;
	}
	error = mountnfs(&args, mp, nam, args.hostname, &vp,
	    curthread->td_ucred, nametimeo, negnametimeo);
out:
	if (!error) {
		MNT_ILOCK(mp);
		mp->mnt_kern_flag |= (MNTK_MPSAFE|MNTK_LOOKUP_SHARED);
		MNT_IUNLOCK(mp);
	}
	return (error);
}


/*
 * VFS Operations.
 *
 * mount system call
 * It seems a bit dumb to copyinstr() the host and path here and then
 * bcopy() them in mountnfs(), but I wanted to detect errors before
 * doing the sockargs() call because sockargs() allocates an mbuf and
 * an error after that means that I have to release the mbuf.
 */
/* ARGSUSED */
static int
nfs_cmount(struct mntarg *ma, void *data, uint64_t flags)
{
	int error;
	struct nfs_args args;

	error = copyin(data, &args, sizeof (struct nfs_args));
	if (error)
		return error;

	ma = mount_arg(ma, "nfs_args", &args, sizeof args);

	error = kernel_mount(ma, flags);
	return (error);
}

/*
 * Common code for mount and mountroot
 */
static int
mountnfs(struct nfs_args *argp, struct mount *mp, struct sockaddr *nam,
    char *hst, struct vnode **vpp, struct ucred *cred, int nametimeo,
    int negnametimeo)
{
	struct nfsmount *nmp;
	struct nfsnode *np;
	int error;
	struct vattr attrs;

	if (mp->mnt_flag & MNT_UPDATE) {
		nmp = VFSTONFS(mp);
		printf("%s: MNT_UPDATE is no longer handled here\n", __func__);
		free(nam, M_SONAME);
		return (0);
	} else {
		nmp = uma_zalloc(nfsmount_zone, M_WAITOK);
		bzero((caddr_t)nmp, sizeof (struct nfsmount));
		TAILQ_INIT(&nmp->nm_bufq);
		mp->mnt_data = nmp;
		nmp->nm_getinfo = nfs_getnlminfo;
		nmp->nm_vinvalbuf = nfs_vinvalbuf;
	}
	vfs_getnewfsid(mp);
	nmp->nm_mountp = mp;
	mtx_init(&nmp->nm_mtx, "NFSmount lock", NULL, MTX_DEF);			

	/*
	 * V2 can only handle 32 bit filesizes.  A 4GB-1 limit may be too
	 * high, depending on whether we end up with negative offsets in
	 * the client or server somewhere.  2GB-1 may be safer.
	 *
	 * For V3, nfs_fsinfo will adjust this as necessary.  Assume maximum
	 * that we can handle until we find out otherwise.
	 */
	if ((argp->flags & NFSMNT_NFSV3) == 0)
		nmp->nm_maxfilesize = 0xffffffffLL;
	else
		nmp->nm_maxfilesize = OFF_MAX;

	nmp->nm_timeo = NFS_TIMEO;
	nmp->nm_retry = NFS_RETRANS;
	if ((argp->flags & NFSMNT_NFSV3) && argp->sotype == SOCK_STREAM) {
		nmp->nm_wsize = nmp->nm_rsize = NFS_MAXDATA;
	} else {
		nmp->nm_wsize = NFS_WSIZE;
		nmp->nm_rsize = NFS_RSIZE;
	}
	nmp->nm_wcommitsize = hibufspace / (desiredvnodes / 1000);
	nmp->nm_readdirsize = NFS_READDIRSIZE;
	nmp->nm_numgrps = NFS_MAXGRPS;
	nmp->nm_readahead = NFS_DEFRAHEAD;
	nmp->nm_deadthresh = NFS_MAXDEADTHRESH;
	nmp->nm_nametimeo = nametimeo;
	nmp->nm_negnametimeo = negnametimeo;
	nmp->nm_tprintf_delay = nfs_tprintf_delay;
	if (nmp->nm_tprintf_delay < 0)
		nmp->nm_tprintf_delay = 0;
	nmp->nm_tprintf_initial_delay = nfs_tprintf_initial_delay;
	if (nmp->nm_tprintf_initial_delay < 0)
		nmp->nm_tprintf_initial_delay = 0;
	nmp->nm_fhsize = argp->fhsize;
	bcopy((caddr_t)argp->fh, (caddr_t)nmp->nm_fh, argp->fhsize);
	bcopy(hst, mp->mnt_stat.f_mntfromname, MNAMELEN);
	nmp->nm_nam = nam;
	/* Set up the sockets and per-host congestion */
	nmp->nm_sotype = argp->sotype;
	nmp->nm_soproto = argp->proto;
	nmp->nm_rpcops = &nfs_rpcops;

	nfs_decode_args(mp, nmp, argp, hst);

	/*
	 * For Connection based sockets (TCP,...) defer the connect until
	 * the first request, in case the server is not responding.
	 */
	if (nmp->nm_sotype == SOCK_DGRAM &&
		(error = nfs_connect(nmp)))
		goto bad;

	/*
	 * This is silly, but it has to be set so that vinifod() works.
	 * We do not want to do an nfs_statfs() here since we can get
	 * stuck on a dead server and we are holding a lock on the mount
	 * point.
	 */
	mtx_lock(&nmp->nm_mtx);
	mp->mnt_stat.f_iosize = nfs_iosize(nmp);
	mtx_unlock(&nmp->nm_mtx);
	/*
	 * A reference count is needed on the nfsnode representing the
	 * remote root.  If this object is not persistent, then backward
	 * traversals of the mount point (i.e. "..") will not work if
	 * the nfsnode gets flushed out of the cache. Ufs does not have
	 * this problem, because one can identify root inodes by their
	 * number == ROOTINO (2).
	 */
	error = nfs_nget(mp, (nfsfh_t *)nmp->nm_fh, nmp->nm_fhsize, &np, LK_EXCLUSIVE);
	if (error)
		goto bad;
	*vpp = NFSTOV(np);

	/*
	 * Get file attributes and transfer parameters for the
	 * mountpoint.  This has the side effect of filling in
	 * (*vpp)->v_type with the correct value.
	 */
	if (argp->flags & NFSMNT_NFSV3)
		nfs_fsinfo(nmp, *vpp, curthread->td_ucred, curthread);
	else
		VOP_GETATTR(*vpp, &attrs, curthread->td_ucred);

	/*
	 * Lose the lock but keep the ref.
	 */
	VOP_UNLOCK(*vpp, 0);

	return (0);
bad:
	nfs_disconnect(nmp);
	mtx_destroy(&nmp->nm_mtx);
	uma_zfree(nfsmount_zone, nmp);
	free(nam, M_SONAME);
	return (error);
}

/*
 * unmount system call
 */
static int
nfs_unmount(struct mount *mp, int mntflags)
{
	struct nfsmount *nmp;
	int error, flags = 0;

	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;
	nmp = VFSTONFS(mp);
	/*
	 * Goes something like this..
	 * - Call vflush() to clear out vnodes for this filesystem
	 * - Close the socket
	 * - Free up the data structures
	 */
	/* In the forced case, cancel any outstanding requests. */
	if (flags & FORCECLOSE) {
		error = nfs_nmcancelreqs(nmp);
		if (error)
			goto out;
	}
	/* We hold 1 extra ref on the root vnode; see comment in mountnfs(). */
	error = vflush(mp, 1, flags, curthread);
	if (error)
		goto out;

	/*
	 * We are now committed to the unmount.
	 */
	nfs_disconnect(nmp);
	free(nmp->nm_nam, M_SONAME);

	mtx_destroy(&nmp->nm_mtx);
	uma_zfree(nfsmount_zone, nmp);
out:
	return (error);
}

/*
 * Return root of a filesystem
 */
static int
nfs_root(struct mount *mp, int flags, struct vnode **vpp)
{
	struct vnode *vp;
	struct nfsmount *nmp;
	struct nfsnode *np;
	int error;

	nmp = VFSTONFS(mp);
	error = nfs_nget(mp, (nfsfh_t *)nmp->nm_fh, nmp->nm_fhsize, &np, flags);
	if (error)
		return error;
	vp = NFSTOV(np);
	/*
	 * Get transfer parameters and attributes for root vnode once.
	 */
	mtx_lock(&nmp->nm_mtx);
	if ((nmp->nm_state & NFSSTA_GOTFSINFO) == 0 &&
	    (nmp->nm_flag & NFSMNT_NFSV3)) {
		mtx_unlock(&nmp->nm_mtx);
		nfs_fsinfo(nmp, vp, curthread->td_ucred, curthread);
	} else 
		mtx_unlock(&nmp->nm_mtx);
	if (vp->v_type == VNON)
	    vp->v_type = VDIR;
	vp->v_vflag |= VV_ROOT;
	*vpp = vp;
	return (0);
}

/*
 * Flush out the buffer cache
 */
/* ARGSUSED */
static int
nfs_sync(struct mount *mp, int waitfor)
{
	struct vnode *vp, *mvp;
	struct thread *td;
	int error, allerror = 0;

	td = curthread;

	MNT_ILOCK(mp);
	/*
	 * If a forced dismount is in progress, return from here so that
	 * the umount(2) syscall doesn't get stuck in VFS_SYNC() before
	 * calling VFS_UNMOUNT().
	 */
	if ((mp->mnt_kern_flag & MNTK_UNMOUNTF) != 0) {
		MNT_IUNLOCK(mp);
		return (EBADF);
	}
	MNT_IUNLOCK(mp);

	/*
	 * Force stale buffer cache information to be flushed.
	 */
loop:
	MNT_VNODE_FOREACH_ALL(vp, mp, mvp) {
		/* XXX Racy bv_cnt check. */
		if (VOP_ISLOCKED(vp) || vp->v_bufobj.bo_dirty.bv_cnt == 0 ||
		    waitfor == MNT_LAZY) {
			VI_UNLOCK(vp);
			continue;
		}
		if (vget(vp, LK_EXCLUSIVE | LK_INTERLOCK, td)) {
			MNT_VNODE_FOREACH_ALL_ABORT(mp, mvp);
			goto loop;
		}
		error = VOP_FSYNC(vp, waitfor, td);
		if (error)
			allerror = error;
		VOP_UNLOCK(vp, 0);
		vrele(vp);
	}
	return (allerror);
}

static int
nfs_sysctl(struct mount *mp, fsctlop_t op, struct sysctl_req *req)
{
	struct nfsmount *nmp = VFSTONFS(mp);
	struct vfsquery vq;
	int error;

	bzero(&vq, sizeof(vq));
	switch (op) {
#if 0
	case VFS_CTL_NOLOCKS:
		val = (nmp->nm_flag & NFSMNT_NOLOCKS) ? 1 : 0;
 		if (req->oldptr != NULL) {
 			error = SYSCTL_OUT(req, &val, sizeof(val));
 			if (error)
 				return (error);
 		}
 		if (req->newptr != NULL) {
 			error = SYSCTL_IN(req, &val, sizeof(val));
 			if (error)
 				return (error);
			if (val)
				nmp->nm_flag |= NFSMNT_NOLOCKS;
			else
				nmp->nm_flag &= ~NFSMNT_NOLOCKS;
 		}
		break;
#endif
	case VFS_CTL_QUERY:
		mtx_lock(&nmp->nm_mtx);
		if (nmp->nm_state & NFSSTA_TIMEO)
			vq.vq_flags |= VQ_NOTRESP;
		mtx_unlock(&nmp->nm_mtx);
#if 0
		if (!(nmp->nm_flag & NFSMNT_NOLOCKS) &&
		    (nmp->nm_state & NFSSTA_LOCKTIMEO))
			vq.vq_flags |= VQ_NOTRESPLOCK;
#endif
		error = SYSCTL_OUT(req, &vq, sizeof(vq));
		break;
 	case VFS_CTL_TIMEO:
 		if (req->oldptr != NULL) {
 			error = SYSCTL_OUT(req, &nmp->nm_tprintf_initial_delay,
 			    sizeof(nmp->nm_tprintf_initial_delay));
 			if (error)
 				return (error);
 		}
 		if (req->newptr != NULL) {
			error = vfs_suser(mp, req->td);
			if (error)
				return (error);
 			error = SYSCTL_IN(req, &nmp->nm_tprintf_initial_delay,
 			    sizeof(nmp->nm_tprintf_initial_delay));
 			if (error)
 				return (error);
 			if (nmp->nm_tprintf_initial_delay < 0)
 				nmp->nm_tprintf_initial_delay = 0;
 		}
		break;
	default:
		return (ENOTSUP);
	}
	return (0);
}

/*
 * Extract the information needed by the nlm from the nfs vnode.
 */
static void
nfs_getnlminfo(struct vnode *vp, uint8_t *fhp, size_t *fhlenp,
    struct sockaddr_storage *sp, int *is_v3p, off_t *sizep,
    struct timeval *timeop)
{
	struct nfsmount *nmp;
	struct nfsnode *np = VTONFS(vp);

	nmp = VFSTONFS(vp->v_mount);
	if (fhlenp != NULL)
		*fhlenp = (size_t)np->n_fhsize;
	if (fhp != NULL)
		bcopy(np->n_fhp, fhp, np->n_fhsize);
	if (sp != NULL)
		bcopy(nmp->nm_nam, sp, min(nmp->nm_nam->sa_len, sizeof(*sp)));
	if (is_v3p != NULL)
		*is_v3p = NFS_ISV3(vp);
	if (sizep != NULL)
		*sizep = np->n_size;
	if (timeop != NULL) {
		timeop->tv_sec = nmp->nm_timeo / NFS_HZ;
		timeop->tv_usec = (nmp->nm_timeo % NFS_HZ) * (1000000 / NFS_HZ);
	}
}

