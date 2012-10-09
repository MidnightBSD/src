/*-
 * Copyright (c) 1989, 1993
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
 *	@(#)nfs_vnops.c	8.16 (Berkeley) 5/27/95
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * vnode op calls for Sun NFS version 2 and 3
 */

#include "opt_inet.h"
#include "opt_kdtrace.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/resourcevar.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/jail.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/namei.h>
#include <sys/socket.h>
#include <sys/vnode.h>
#include <sys/dirent.h>
#include <sys/fcntl.h>
#include <sys/lockf.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/signalvar.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_object.h>

#include <fs/fifofs/fifo.h>

#include <nfs/nfsproto.h>
#include <nfsclient/nfs.h>
#include <nfsclient/nfsnode.h>
#include <nfsclient/nfsmount.h>
#include <nfs/nfs_kdtrace.h>
#include <nfs/nfs_lock.h>
#include <nfs/xdr_subs.h>
#include <nfsclient/nfsm_subs.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_var.h>

#include <machine/stdarg.h>

#ifdef KDTRACE_HOOKS
#include <sys/dtrace_bsd.h>

dtrace_nfsclient_accesscache_flush_probe_func_t
    dtrace_nfsclient_accesscache_flush_done_probe;
uint32_t nfsclient_accesscache_flush_done_id;

dtrace_nfsclient_accesscache_get_probe_func_t
    dtrace_nfsclient_accesscache_get_hit_probe,
    dtrace_nfsclient_accesscache_get_miss_probe;
uint32_t nfsclient_accesscache_get_hit_id;
uint32_t nfsclient_accesscache_get_miss_id;

dtrace_nfsclient_accesscache_load_probe_func_t
    dtrace_nfsclient_accesscache_load_done_probe;
uint32_t nfsclient_accesscache_load_done_id;
#endif /* !KDTRACE_HOOKS */

/* Defs */
#define	TRUE	1
#define	FALSE	0

/*
 * Ifdef for FreeBSD-current merged buffer cache. It is unfortunate that these
 * calls are not in getblk() and brelse() so that they would not be necessary
 * here.
 */
#ifndef B_VMIO
#define vfs_busy_pages(bp, f)
#endif

static vop_read_t	nfsfifo_read;
static vop_write_t	nfsfifo_write;
static vop_close_t	nfsfifo_close;
static int	nfs_flush(struct vnode *, int, int);
static int	nfs_setattrrpc(struct vnode *, struct vattr *, struct ucred *);
static vop_lookup_t	nfs_lookup;
static vop_create_t	nfs_create;
static vop_mknod_t	nfs_mknod;
static vop_open_t	nfs_open;
static vop_close_t	nfs_close;
static vop_access_t	nfs_access;
static vop_getattr_t	nfs_getattr;
static vop_setattr_t	nfs_setattr;
static vop_read_t	nfs_read;
static vop_fsync_t	nfs_fsync;
static vop_remove_t	nfs_remove;
static vop_link_t	nfs_link;
static vop_rename_t	nfs_rename;
static vop_mkdir_t	nfs_mkdir;
static vop_rmdir_t	nfs_rmdir;
static vop_symlink_t	nfs_symlink;
static vop_readdir_t	nfs_readdir;
static vop_strategy_t	nfs_strategy;
static	int	nfs_lookitup(struct vnode *, const char *, int,
		    struct ucred *, struct thread *, struct nfsnode **);
static	int	nfs_sillyrename(struct vnode *, struct vnode *,
		    struct componentname *);
static vop_access_t	nfsspec_access;
static vop_readlink_t	nfs_readlink;
static vop_print_t	nfs_print;
static vop_advlock_t	nfs_advlock;
static vop_advlockasync_t nfs_advlockasync;

/*
 * Global vfs data structures for nfs
 */
struct vop_vector nfs_vnodeops = {
	.vop_default =		&default_vnodeops,
	.vop_access =		nfs_access,
	.vop_advlock =		nfs_advlock,
	.vop_advlockasync =	nfs_advlockasync,
	.vop_close =		nfs_close,
	.vop_create =		nfs_create,
	.vop_fsync =		nfs_fsync,
	.vop_getattr =		nfs_getattr,
	.vop_getpages =		nfs_getpages,
	.vop_putpages =		nfs_putpages,
	.vop_inactive =		nfs_inactive,
	.vop_link =		nfs_link,
	.vop_lookup =		nfs_lookup,
	.vop_mkdir =		nfs_mkdir,
	.vop_mknod =		nfs_mknod,
	.vop_open =		nfs_open,
	.vop_print =		nfs_print,
	.vop_read =		nfs_read,
	.vop_readdir =		nfs_readdir,
	.vop_readlink =		nfs_readlink,
	.vop_reclaim =		nfs_reclaim,
	.vop_remove =		nfs_remove,
	.vop_rename =		nfs_rename,
	.vop_rmdir =		nfs_rmdir,
	.vop_setattr =		nfs_setattr,
	.vop_strategy =		nfs_strategy,
	.vop_symlink =		nfs_symlink,
	.vop_write =		nfs_write,
};

struct vop_vector nfs_fifoops = {
	.vop_default =		&fifo_specops,
	.vop_access =		nfsspec_access,
	.vop_close =		nfsfifo_close,
	.vop_fsync =		nfs_fsync,
	.vop_getattr =		nfs_getattr,
	.vop_inactive =		nfs_inactive,
	.vop_print =		nfs_print,
	.vop_read =		nfsfifo_read,
	.vop_reclaim =		nfs_reclaim,
	.vop_setattr =		nfs_setattr,
	.vop_write =		nfsfifo_write,
};

static int	nfs_mknodrpc(struct vnode *dvp, struct vnode **vpp,
			     struct componentname *cnp, struct vattr *vap);
static int	nfs_removerpc(struct vnode *dvp, const char *name, int namelen,
			      struct ucred *cred, struct thread *td);
static int	nfs_renamerpc(struct vnode *fdvp, const char *fnameptr,
			      int fnamelen, struct vnode *tdvp,
			      const char *tnameptr, int tnamelen,
			      struct ucred *cred, struct thread *td);
static int	nfs_renameit(struct vnode *sdvp, struct componentname *scnp,
			     struct sillyrename *sp);

/*
 * Global variables
 */
struct mtx 	nfs_iod_mtx;
enum nfsiod_state nfs_iodwant[NFS_MAXASYNCDAEMON];
struct nfsmount *nfs_iodmount[NFS_MAXASYNCDAEMON];
int		 nfs_numasync = 0;
#define	DIRHDSIZ	(sizeof (struct dirent) - (MAXNAMLEN + 1))

SYSCTL_DECL(_vfs_oldnfs);

static int	nfsaccess_cache_timeout = NFS_MAXATTRTIMO;
SYSCTL_INT(_vfs_oldnfs, OID_AUTO, access_cache_timeout, CTLFLAG_RW,
	   &nfsaccess_cache_timeout, 0, "NFS ACCESS cache timeout");

static int	nfs_prime_access_cache = 0;
SYSCTL_INT(_vfs_oldnfs, OID_AUTO, prime_access_cache, CTLFLAG_RW,
	   &nfs_prime_access_cache, 0,
	   "Prime NFS ACCESS cache when fetching attributes");

static int	nfsv3_commit_on_close = 0;
SYSCTL_INT(_vfs_oldnfs, OID_AUTO, nfsv3_commit_on_close, CTLFLAG_RW,
	   &nfsv3_commit_on_close, 0, "write+commit on close, else only write");

static int	nfs_clean_pages_on_close = 1;
SYSCTL_INT(_vfs_oldnfs, OID_AUTO, clean_pages_on_close, CTLFLAG_RW,
	   &nfs_clean_pages_on_close, 0, "NFS clean dirty pages on close");

int nfs_directio_enable = 0;
SYSCTL_INT(_vfs_oldnfs, OID_AUTO, nfs_directio_enable, CTLFLAG_RW,
	   &nfs_directio_enable, 0, "Enable NFS directio");

/*
 * This sysctl allows other processes to mmap a file that has been opened
 * O_DIRECT by a process.  In general, having processes mmap the file while
 * Direct IO is in progress can lead to Data Inconsistencies.  But, we allow
 * this by default to prevent DoS attacks - to prevent a malicious user from
 * opening up files O_DIRECT preventing other users from mmap'ing these
 * files.  "Protected" environments where stricter consistency guarantees are
 * required can disable this knob.  The process that opened the file O_DIRECT
 * cannot mmap() the file, because mmap'ed IO on an O_DIRECT open() is not
 * meaningful.
 */
int nfs_directio_allow_mmap = 1;
SYSCTL_INT(_vfs_oldnfs, OID_AUTO, nfs_directio_allow_mmap, CTLFLAG_RW,
	   &nfs_directio_allow_mmap, 0, "Enable mmaped IO on file with O_DIRECT opens");

#if 0
SYSCTL_INT(_vfs_oldnfs, OID_AUTO, access_cache_hits, CTLFLAG_RD,
	   &nfsstats.accesscache_hits, 0, "NFS ACCESS cache hit count");

SYSCTL_INT(_vfs_oldnfs, OID_AUTO, access_cache_misses, CTLFLAG_RD,
	   &nfsstats.accesscache_misses, 0, "NFS ACCESS cache miss count");
#endif

#define	NFSV3ACCESS_ALL (NFSV3ACCESS_READ | NFSV3ACCESS_MODIFY		\
			 | NFSV3ACCESS_EXTEND | NFSV3ACCESS_EXECUTE	\
			 | NFSV3ACCESS_DELETE | NFSV3ACCESS_LOOKUP)

/*
 * SMP Locking Note :
 * The list of locks after the description of the lock is the ordering
 * of other locks acquired with the lock held.
 * np->n_mtx : Protects the fields in the nfsnode.
       VM Object Lock
       VI_MTX (acquired indirectly)
 * nmp->nm_mtx : Protects the fields in the nfsmount.
       rep->r_mtx
 * nfs_iod_mtx : Global lock, protects shared nfsiod state.
 * nfs_reqq_mtx : Global lock, protects the nfs_reqq list.
       nmp->nm_mtx
       rep->r_mtx
 * rep->r_mtx : Protects the fields in an nfsreq.
 */

static int
nfs3_access_otw(struct vnode *vp, int wmode, struct thread *td,
    struct ucred *cred, uint32_t *retmode)
{
	const int v3 = 1;
	u_int32_t *tl;
	int error = 0, attrflag, i, lrupos;

	struct mbuf *mreq, *mrep, *md, *mb;
	caddr_t bpos, dpos;
	u_int32_t rmode;
	struct nfsnode *np = VTONFS(vp);

	nfsstats.rpccnt[NFSPROC_ACCESS]++;
	mreq = nfsm_reqhead(vp, NFSPROC_ACCESS, NFSX_FH(v3) + NFSX_UNSIGNED);
	mb = mreq;
	bpos = mtod(mb, caddr_t);
	nfsm_fhtom(vp, v3);
	tl = nfsm_build(u_int32_t *, NFSX_UNSIGNED);
	*tl = txdr_unsigned(wmode);
	nfsm_request(vp, NFSPROC_ACCESS, td, cred);
	nfsm_postop_attr(vp, attrflag);
	if (!error) {
		lrupos = 0;
		tl = nfsm_dissect(u_int32_t *, NFSX_UNSIGNED);
		rmode = fxdr_unsigned(u_int32_t, *tl);
		mtx_lock(&np->n_mtx);
		for (i = 0; i < NFS_ACCESSCACHESIZE; i++) {
			if (np->n_accesscache[i].uid == cred->cr_uid) {
				np->n_accesscache[i].mode = rmode;
				np->n_accesscache[i].stamp = time_second;
				break;
			}
			if (i > 0 && np->n_accesscache[i].stamp <
			    np->n_accesscache[lrupos].stamp)
				lrupos = i;
		}
		if (i == NFS_ACCESSCACHESIZE) {
			np->n_accesscache[lrupos].uid = cred->cr_uid;
			np->n_accesscache[lrupos].mode = rmode;
			np->n_accesscache[lrupos].stamp = time_second;
		}
		mtx_unlock(&np->n_mtx);
		if (retmode != NULL)
			*retmode = rmode;
		KDTRACE_NFS_ACCESSCACHE_LOAD_DONE(vp, cred->cr_uid, rmode, 0);
	}
	m_freem(mrep);
nfsmout:
#ifdef KDTRACE_HOOKS
	if (error) {
		KDTRACE_NFS_ACCESSCACHE_LOAD_DONE(vp, cred->cr_uid, 0,
		    error);
	}
#endif
	return (error);
}

/*
 * nfs access vnode op.
 * For nfs version 2, just return ok. File accesses may fail later.
 * For nfs version 3, use the access rpc to check accessibility. If file modes
 * are changed on the server, accesses might still fail later.
 */
static int
nfs_access(struct vop_access_args *ap)
{
	struct vnode *vp = ap->a_vp;
	int error = 0, i, gotahit;
	u_int32_t mode, rmode, wmode;
	int v3 = NFS_ISV3(vp);
	struct nfsnode *np = VTONFS(vp);

	/*
	 * Disallow write attempts on filesystems mounted read-only;
	 * unless the file is a socket, fifo, or a block or character
	 * device resident on the filesystem.
	 */
	if ((ap->a_accmode & VWRITE) && (vp->v_mount->mnt_flag & MNT_RDONLY)) {
		switch (vp->v_type) {
		case VREG:
		case VDIR:
		case VLNK:
			return (EROFS);
		default:
			break;
		}
	}
	/*
	 * For nfs v3, check to see if we have done this recently, and if
	 * so return our cached result instead of making an ACCESS call.
	 * If not, do an access rpc, otherwise you are stuck emulating
	 * ufs_access() locally using the vattr. This may not be correct,
	 * since the server may apply other access criteria such as
	 * client uid-->server uid mapping that we do not know about.
	 */
	if (v3) {
		if (ap->a_accmode & VREAD)
			mode = NFSV3ACCESS_READ;
		else
			mode = 0;
		if (vp->v_type != VDIR) {
			if (ap->a_accmode & VWRITE)
				mode |= (NFSV3ACCESS_MODIFY | NFSV3ACCESS_EXTEND);
			if (ap->a_accmode & VEXEC)
				mode |= NFSV3ACCESS_EXECUTE;
		} else {
			if (ap->a_accmode & VWRITE)
				mode |= (NFSV3ACCESS_MODIFY | NFSV3ACCESS_EXTEND |
					 NFSV3ACCESS_DELETE);
			if (ap->a_accmode & VEXEC)
				mode |= NFSV3ACCESS_LOOKUP;
		}
		/* XXX safety belt, only make blanket request if caching */
		if (nfsaccess_cache_timeout > 0) {
			wmode = NFSV3ACCESS_READ | NFSV3ACCESS_MODIFY |
				NFSV3ACCESS_EXTEND | NFSV3ACCESS_EXECUTE |
				NFSV3ACCESS_DELETE | NFSV3ACCESS_LOOKUP;
		} else {
			wmode = mode;
		}

		/*
		 * Does our cached result allow us to give a definite yes to
		 * this request?
		 */
		gotahit = 0;
		mtx_lock(&np->n_mtx);
		for (i = 0; i < NFS_ACCESSCACHESIZE; i++) {
			if (ap->a_cred->cr_uid == np->n_accesscache[i].uid) {
				if (time_second < (np->n_accesscache[i].stamp +
				    nfsaccess_cache_timeout) &&
				    (np->n_accesscache[i].mode & mode) == mode) {
					nfsstats.accesscache_hits++;
					gotahit = 1;
				}
				break;
			}
		}
		mtx_unlock(&np->n_mtx);
#ifdef KDTRACE_HOOKS
		if (gotahit)
			KDTRACE_NFS_ACCESSCACHE_GET_HIT(vp,
			    ap->a_cred->cr_uid, mode);
		else
			KDTRACE_NFS_ACCESSCACHE_GET_MISS(vp,
			    ap->a_cred->cr_uid, mode);
#endif
		if (gotahit == 0) {
			/*
			 * Either a no, or a don't know.  Go to the wire.
			 */
			nfsstats.accesscache_misses++;
		        error = nfs3_access_otw(vp, wmode, ap->a_td, ap->a_cred,
			    &rmode);
			if (!error) {
				if ((rmode & mode) != mode)
					error = EACCES;
			}
		}
		return (error);
	} else {
		if ((error = nfsspec_access(ap)) != 0) {
			return (error);
		}
		/*
		 * Attempt to prevent a mapped root from accessing a file
		 * which it shouldn't.  We try to read a byte from the file
		 * if the user is root and the file is not zero length.
		 * After calling nfsspec_access, we should have the correct
		 * file size cached.
		 */
		mtx_lock(&np->n_mtx);
		if (ap->a_cred->cr_uid == 0 && (ap->a_accmode & VREAD)
		    && VTONFS(vp)->n_size > 0) {
			struct iovec aiov;
			struct uio auio;
			char buf[1];

			mtx_unlock(&np->n_mtx);
			aiov.iov_base = buf;
			aiov.iov_len = 1;
			auio.uio_iov = &aiov;
			auio.uio_iovcnt = 1;
			auio.uio_offset = 0;
			auio.uio_resid = 1;
			auio.uio_segflg = UIO_SYSSPACE;
			auio.uio_rw = UIO_READ;
			auio.uio_td = ap->a_td;

			if (vp->v_type == VREG)
				error = nfs_readrpc(vp, &auio, ap->a_cred);
			else if (vp->v_type == VDIR) {
				char* bp;
				bp = malloc(NFS_DIRBLKSIZ, M_TEMP, M_WAITOK);
				aiov.iov_base = bp;
				aiov.iov_len = auio.uio_resid = NFS_DIRBLKSIZ;
				error = nfs_readdirrpc(vp, &auio, ap->a_cred);
				free(bp, M_TEMP);
			} else if (vp->v_type == VLNK)
				error = nfs_readlinkrpc(vp, &auio, ap->a_cred);
			else
				error = EACCES;
		} else
			mtx_unlock(&np->n_mtx);
		return (error);
	}
}

int nfs_otw_getattr_avoid = 0;

/*
 * nfs open vnode op
 * Check to see if the type is ok
 * and that deletion is not in progress.
 * For paged in text files, you will need to flush the page cache
 * if consistency is lost.
 */
/* ARGSUSED */
static int
nfs_open(struct vop_open_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);
	struct vattr vattr;
	int error;
	int fmode = ap->a_mode;
	struct ucred *cred;

	if (vp->v_type != VREG && vp->v_type != VDIR && vp->v_type != VLNK)
		return (EOPNOTSUPP);

	/*
	 * Get a valid lease. If cached data is stale, flush it.
	 */
	mtx_lock(&np->n_mtx);
	if (np->n_flag & NMODIFIED) {
		mtx_unlock(&np->n_mtx);
		error = nfs_vinvalbuf(vp, V_SAVE, ap->a_td, 1);
		if (error == EINTR || error == EIO)
			return (error);
		mtx_lock(&np->n_mtx);
		np->n_attrstamp = 0;
		KDTRACE_NFS_ATTRCACHE_FLUSH_DONE(vp);
		if (vp->v_type == VDIR)
			np->n_direofoffset = 0;
		mtx_unlock(&np->n_mtx);
		error = VOP_GETATTR(vp, &vattr, ap->a_cred);
		if (error)
			return (error);
		mtx_lock(&np->n_mtx);
		np->n_mtime = vattr.va_mtime;
	} else {
		mtx_unlock(&np->n_mtx);
		error = VOP_GETATTR(vp, &vattr, ap->a_cred);
		if (error)
			return (error);
		mtx_lock(&np->n_mtx);
		if (NFS_TIMESPEC_COMPARE(&np->n_mtime, &vattr.va_mtime)) {
			if (vp->v_type == VDIR)
				np->n_direofoffset = 0;
			mtx_unlock(&np->n_mtx);
			error = nfs_vinvalbuf(vp, V_SAVE, ap->a_td, 1);
			if (error == EINTR || error == EIO) {
				return (error);
			}
			mtx_lock(&np->n_mtx);
			np->n_mtime = vattr.va_mtime;
		}
	}
	/*
	 * If the object has >= 1 O_DIRECT active opens, we disable caching.
	 */
	if (nfs_directio_enable && (fmode & O_DIRECT) && (vp->v_type == VREG)) {
		if (np->n_directio_opens == 0) {
			mtx_unlock(&np->n_mtx);
			error = nfs_vinvalbuf(vp, V_SAVE, ap->a_td, 1);
			if (error)
				return (error);
			mtx_lock(&np->n_mtx);
			np->n_flag |= NNONCACHE;
		}
		np->n_directio_opens++;
	}

	/*
	 * If this is an open for writing, capture a reference to the
	 * credentials, so they can be used by nfs_putpages(). Using
	 * these write credentials is preferable to the credentials of
	 * whatever thread happens to be doing the VOP_PUTPAGES() since
	 * the write RPCs are less likely to fail with EACCES.
	 */
	if ((fmode & FWRITE) != 0) {
		cred = np->n_writecred;
		np->n_writecred = crhold(ap->a_cred);
	} else
		cred = NULL;
	mtx_unlock(&np->n_mtx);
	if (cred != NULL)
		crfree(cred);
	vnode_create_vobject(vp, vattr.va_size, ap->a_td);
	return (0);
}

/*
 * nfs close vnode op
 * What an NFS client should do upon close after writing is a debatable issue.
 * Most NFS clients push delayed writes to the server upon close, basically for
 * two reasons:
 * 1 - So that any write errors may be reported back to the client process
 *     doing the close system call. By far the two most likely errors are
 *     NFSERR_NOSPC and NFSERR_DQUOT to indicate space allocation failure.
 * 2 - To put a worst case upper bound on cache inconsistency between
 *     multiple clients for the file.
 * There is also a consistency problem for Version 2 of the protocol w.r.t.
 * not being able to tell if other clients are writing a file concurrently,
 * since there is no way of knowing if the changed modify time in the reply
 * is only due to the write for this client.
 * (NFS Version 3 provides weak cache consistency data in the reply that
 *  should be sufficient to detect and handle this case.)
 *
 * The current code does the following:
 * for NFS Version 2 - play it safe and flush/invalidate all dirty buffers
 * for NFS Version 3 - flush dirty buffers to the server but don't invalidate
 *                     or commit them (this satisfies 1 and 2 except for the
 *                     case where the server crashes after this close but
 *                     before the commit RPC, which is felt to be "good
 *                     enough". Changing the last argument to nfs_flush() to
 *                     a 1 would force a commit operation, if it is felt a
 *                     commit is necessary now.
 */
/* ARGSUSED */
static int
nfs_close(struct vop_close_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);
	int error = 0;
	int fmode = ap->a_fflag;

	if (vp->v_type == VREG) {
	    /*
	     * Examine and clean dirty pages, regardless of NMODIFIED.
	     * This closes a major hole in close-to-open consistency.
	     * We want to push out all dirty pages (and buffers) on
	     * close, regardless of whether they were dirtied by
	     * mmap'ed writes or via write().
	     */
	    if (nfs_clean_pages_on_close && vp->v_object) {
		VM_OBJECT_LOCK(vp->v_object);
		vm_object_page_clean(vp->v_object, 0, 0, 0);
		VM_OBJECT_UNLOCK(vp->v_object);
	    }
	    mtx_lock(&np->n_mtx);
	    if (np->n_flag & NMODIFIED) {
		mtx_unlock(&np->n_mtx);
		if (NFS_ISV3(vp)) {
		    /*
		     * Under NFSv3 we have dirty buffers to dispose of.  We
		     * must flush them to the NFS server.  We have the option
		     * of waiting all the way through the commit rpc or just
		     * waiting for the initial write.  The default is to only
		     * wait through the initial write so the data is in the
		     * server's cache, which is roughly similar to the state
		     * a standard disk subsystem leaves the file in on close().
		     *
		     * We cannot clear the NMODIFIED bit in np->n_flag due to
		     * potential races with other processes, and certainly
		     * cannot clear it if we don't commit.
		     */
		    int cm = nfsv3_commit_on_close ? 1 : 0;
		    error = nfs_flush(vp, MNT_WAIT, cm);
		    /* np->n_flag &= ~NMODIFIED; */
		} else
		    error = nfs_vinvalbuf(vp, V_SAVE, ap->a_td, 1);
		mtx_lock(&np->n_mtx);
	    }
	    if (np->n_flag & NWRITEERR) {
		np->n_flag &= ~NWRITEERR;
		error = np->n_error;
	    }
	    mtx_unlock(&np->n_mtx);
	}
	if (nfs_directio_enable)
		KASSERT((np->n_directio_asyncwr == 0),
			("nfs_close: dirty unflushed (%d) directio buffers\n",
			 np->n_directio_asyncwr));
	if (nfs_directio_enable && (fmode & O_DIRECT) && (vp->v_type == VREG)) {
		mtx_lock(&np->n_mtx);
		KASSERT((np->n_directio_opens > 0), 
			("nfs_close: unexpectedly value (0) of n_directio_opens\n"));
		np->n_directio_opens--;
		if (np->n_directio_opens == 0)
			np->n_flag &= ~NNONCACHE;
		mtx_unlock(&np->n_mtx);
	}
	return (error);
}

/*
 * nfs getattr call from vfs.
 */
static int
nfs_getattr(struct vop_getattr_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);
	struct thread *td = curthread;
	struct vattr *vap = ap->a_vap;
	struct vattr vattr;
	caddr_t bpos, dpos;
	int error = 0;
	struct mbuf *mreq, *mrep, *md, *mb;
	int v3 = NFS_ISV3(vp);

	/*
	 * Update local times for special files.
	 */
	mtx_lock(&np->n_mtx);
	if (np->n_flag & (NACC | NUPD))
		np->n_flag |= NCHG;
	mtx_unlock(&np->n_mtx);
	/*
	 * First look in the cache.
	 */
	if (nfs_getattrcache(vp, &vattr) == 0)
		goto nfsmout;
	if (v3 && nfs_prime_access_cache && nfsaccess_cache_timeout > 0) {
		nfsstats.accesscache_misses++;
		nfs3_access_otw(vp, NFSV3ACCESS_ALL, td, ap->a_cred, NULL);
		if (nfs_getattrcache(vp, &vattr) == 0)
			goto nfsmout;
	}
	nfsstats.rpccnt[NFSPROC_GETATTR]++;
	mreq = nfsm_reqhead(vp, NFSPROC_GETATTR, NFSX_FH(v3));
	mb = mreq;
	bpos = mtod(mb, caddr_t);
	nfsm_fhtom(vp, v3);
	nfsm_request(vp, NFSPROC_GETATTR, td, ap->a_cred);
	if (!error) {
		nfsm_loadattr(vp, &vattr);
	}
	m_freem(mrep);
nfsmout:
	vap->va_type = vattr.va_type;
	vap->va_mode = vattr.va_mode;
	vap->va_nlink = vattr.va_nlink;
	vap->va_uid = vattr.va_uid;
	vap->va_gid = vattr.va_gid;
	vap->va_fsid = vattr.va_fsid;
	vap->va_fileid = vattr.va_fileid;
	vap->va_size = vattr.va_size;
	vap->va_blocksize = vattr.va_blocksize;
	vap->va_atime = vattr.va_atime;
	vap->va_mtime = vattr.va_mtime;
	vap->va_ctime = vattr.va_ctime;
	vap->va_gen = vattr.va_gen;
	vap->va_flags = vattr.va_flags;
	vap->va_rdev = vattr.va_rdev;
	vap->va_bytes = vattr.va_bytes;
	vap->va_filerev = vattr.va_filerev;

	return (error);
}

/*
 * nfs setattr call.
 */
static int
nfs_setattr(struct vop_setattr_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);
	struct vattr *vap = ap->a_vap;
	struct thread *td = curthread;
	int error = 0;
	u_quad_t tsize;

#ifndef nolint
	tsize = (u_quad_t)0;
#endif

	/*
	 * Setting of flags is not supported.
	 */
	if (vap->va_flags != VNOVAL)
		return (EOPNOTSUPP);

	/*
	 * Disallow write attempts if the filesystem is mounted read-only.
	 */
  	if ((vap->va_flags != VNOVAL || vap->va_uid != (uid_t)VNOVAL ||
	    vap->va_gid != (gid_t)VNOVAL || vap->va_atime.tv_sec != VNOVAL ||
	    vap->va_mtime.tv_sec != VNOVAL || vap->va_mode != (mode_t)VNOVAL) &&
	    (vp->v_mount->mnt_flag & MNT_RDONLY)) {
		error = EROFS;
		goto out;
	}
	if (vap->va_size != VNOVAL) {
 		switch (vp->v_type) {
 		case VDIR:
 			return (EISDIR);
 		case VCHR:
 		case VBLK:
 		case VSOCK:
 		case VFIFO:
			if (vap->va_mtime.tv_sec == VNOVAL &&
			    vap->va_atime.tv_sec == VNOVAL &&
			    vap->va_mode == (mode_t)VNOVAL &&
			    vap->va_uid == (uid_t)VNOVAL &&
			    vap->va_gid == (gid_t)VNOVAL)
				return (0);		
 			vap->va_size = VNOVAL;
 			break;
 		default:
			/*
			 * Disallow write attempts if the filesystem is
			 * mounted read-only.
			 */
			if (vp->v_mount->mnt_flag & MNT_RDONLY)
				return (EROFS);
			/*
			 *  We run vnode_pager_setsize() early (why?),
			 * we must set np->n_size now to avoid vinvalbuf
			 * V_SAVE races that might setsize a lower
			 * value.
			 */
			mtx_lock(&np->n_mtx);
			tsize = np->n_size;
			mtx_unlock(&np->n_mtx);
			error = nfs_meta_setsize(vp, ap->a_cred, td,
			    vap->va_size);
			mtx_lock(&np->n_mtx);
 			if (np->n_flag & NMODIFIED) {
			    tsize = np->n_size;
			    mtx_unlock(&np->n_mtx);
 			    if (vap->va_size == 0)
 				error = nfs_vinvalbuf(vp, 0, td, 1);
 			    else
 				error = nfs_vinvalbuf(vp, V_SAVE, td, 1);
 			    if (error) {
				vnode_pager_setsize(vp, tsize);
				goto out;
			    }
 			} else
			    mtx_unlock(&np->n_mtx);
			/*
			 * np->n_size has already been set to vap->va_size
			 * in nfs_meta_setsize(). We must set it again since
			 * nfs_loadattrcache() could be called through
			 * nfs_meta_setsize() and could modify np->n_size.
			 */
			mtx_lock(&np->n_mtx);
 			np->n_vattr.va_size = np->n_size = vap->va_size;
			mtx_unlock(&np->n_mtx);
  		};
  	} else {
		mtx_lock(&np->n_mtx);
		if ((vap->va_mtime.tv_sec != VNOVAL || vap->va_atime.tv_sec != VNOVAL) && 
		    (np->n_flag & NMODIFIED) && vp->v_type == VREG) {
			mtx_unlock(&np->n_mtx);
			if ((error = nfs_vinvalbuf(vp, V_SAVE, td, 1)) != 0 &&
			    (error == EINTR || error == EIO))
				return error;
		} else
			mtx_unlock(&np->n_mtx);
	}
	error = nfs_setattrrpc(vp, vap, ap->a_cred);
	if (error && vap->va_size != VNOVAL) {
		mtx_lock(&np->n_mtx);
		np->n_size = np->n_vattr.va_size = tsize;
		vnode_pager_setsize(vp, tsize);
		mtx_unlock(&np->n_mtx);
	}
out:
	return (error);
}

/*
 * Do an nfs setattr rpc.
 */
static int
nfs_setattrrpc(struct vnode *vp, struct vattr *vap, struct ucred *cred)
{
	struct nfsv2_sattr *sp;
	struct nfsnode *np = VTONFS(vp);
	caddr_t bpos, dpos;
	u_int32_t *tl;
	int error = 0, i, wccflag = NFSV3_WCCRATTR;
	struct mbuf *mreq, *mrep, *md, *mb;
	int v3 = NFS_ISV3(vp);

	nfsstats.rpccnt[NFSPROC_SETATTR]++;
	mreq = nfsm_reqhead(vp, NFSPROC_SETATTR, NFSX_FH(v3) + NFSX_SATTR(v3));
	mb = mreq;
	bpos = mtod(mb, caddr_t);
	nfsm_fhtom(vp, v3);
	if (v3) {
		nfsm_v3attrbuild(vap, TRUE);
		tl = nfsm_build(u_int32_t *, NFSX_UNSIGNED);
		*tl = nfs_false;
	} else {
		sp = nfsm_build(struct nfsv2_sattr *, NFSX_V2SATTR);
		if (vap->va_mode == (mode_t)VNOVAL)
			sp->sa_mode = nfs_xdrneg1;
		else
			sp->sa_mode = vtonfsv2_mode(vp->v_type, vap->va_mode);
		if (vap->va_uid == (uid_t)VNOVAL)
			sp->sa_uid = nfs_xdrneg1;
		else
			sp->sa_uid = txdr_unsigned(vap->va_uid);
		if (vap->va_gid == (gid_t)VNOVAL)
			sp->sa_gid = nfs_xdrneg1;
		else
			sp->sa_gid = txdr_unsigned(vap->va_gid);
		sp->sa_size = txdr_unsigned(vap->va_size);
		txdr_nfsv2time(&vap->va_atime, &sp->sa_atime);
		txdr_nfsv2time(&vap->va_mtime, &sp->sa_mtime);
	}
	nfsm_request(vp, NFSPROC_SETATTR, curthread, cred);
	if (v3) {
		mtx_lock(&np->n_mtx);
		for (i = 0; i < NFS_ACCESSCACHESIZE; i++)
			np->n_accesscache[i].stamp = 0;
		mtx_unlock(&np->n_mtx);
		KDTRACE_NFS_ACCESSCACHE_FLUSH_DONE(vp);
		nfsm_wcc_data(vp, wccflag);
	} else
		nfsm_loadattr(vp, NULL);
	m_freem(mrep);
nfsmout:
	return (error);
}

/*
 * nfs lookup call, one step at a time...
 * First look in cache
 * If not found, unlock the directory nfsnode and do the rpc
 */
static int
nfs_lookup(struct vop_lookup_args *ap)
{
	struct componentname *cnp = ap->a_cnp;
	struct vnode *dvp = ap->a_dvp;
	struct vnode **vpp = ap->a_vpp;
	struct mount *mp = dvp->v_mount;
	struct vattr dvattr, vattr;
	struct timespec nctime;
	int flags = cnp->cn_flags;
	struct vnode *newvp;
	struct nfsmount *nmp;
	caddr_t bpos, dpos;
	struct mbuf *mreq, *mrep, *md, *mb;
	long len;
	nfsfh_t *fhp;
	struct nfsnode *np, *newnp;
	int error = 0, attrflag, dattrflag, fhsize, ltype, ncticks;
	int v3 = NFS_ISV3(dvp);
	struct thread *td = cnp->cn_thread;

	*vpp = NULLVP;
	if ((flags & ISLASTCN) && (mp->mnt_flag & MNT_RDONLY) &&
	    (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME))
		return (EROFS);
	if (dvp->v_type != VDIR)
		return (ENOTDIR);
	nmp = VFSTONFS(mp);
	np = VTONFS(dvp);
	if ((error = VOP_ACCESS(dvp, VEXEC, cnp->cn_cred, td)) != 0) {
		*vpp = NULLVP;
		return (error);
	}
	error = cache_lookup_times(dvp, vpp, cnp, &nctime, &ncticks);
	if (error > 0 && error != ENOENT)
		return (error);
	if (error == -1) {
		/*
		 * Lookups of "." are special and always return the
		 * current directory.  cache_lookup() already handles
		 * associated locking bookkeeping, etc.
		 */
		if (cnp->cn_namelen == 1 && cnp->cn_nameptr[0] == '.') {
			/* XXX: Is this really correct? */
			if (cnp->cn_nameiop != LOOKUP &&
			    (flags & ISLASTCN))
				cnp->cn_flags |= SAVENAME;
			return (0);
		}

		/*
		 * We only accept a positive hit in the cache if the
		 * change time of the file matches our cached copy.
		 * Otherwise, we discard the cache entry and fallback
		 * to doing a lookup RPC.  We also only trust cache
		 * entries for less than nm_nametimeo seconds.
		 *
		 * To better handle stale file handles and attributes,
		 * clear the attribute cache of this node if it is a
		 * leaf component, part of an open() call, and not
		 * locally modified before fetching the attributes.
		 * This should allow stale file handles to be detected
		 * here where we can fall back to a LOOKUP RPC to
		 * recover rather than having nfs_open() detect the
		 * stale file handle and failing open(2) with ESTALE.
		 */
		newvp = *vpp;
		newnp = VTONFS(newvp);
		if (!(nmp->nm_flag & NFSMNT_NOCTO) &&
		    (flags & (ISLASTCN | ISOPEN)) == (ISLASTCN | ISOPEN) &&
		    !(newnp->n_flag & NMODIFIED)) {
			mtx_lock(&newnp->n_mtx);
			newnp->n_attrstamp = 0;
			KDTRACE_NFS_ATTRCACHE_FLUSH_DONE(newvp);
			mtx_unlock(&newnp->n_mtx);
		}
		if ((u_int)(ticks - ncticks) < (nmp->nm_nametimeo * hz) &&
		    VOP_GETATTR(newvp, &vattr, cnp->cn_cred) == 0 &&
		    timespeccmp(&vattr.va_ctime, &nctime, ==)) {
			nfsstats.lookupcache_hits++;
			if (cnp->cn_nameiop != LOOKUP &&
			    (flags & ISLASTCN))
				cnp->cn_flags |= SAVENAME;
			return (0);
		}
		cache_purge(newvp);
		if (dvp != newvp)
			vput(newvp);
		else 
			vrele(newvp);
		*vpp = NULLVP;
	} else if (error == ENOENT) {
		if (dvp->v_iflag & VI_DOOMED)
			return (ENOENT);
		/*
		 * We only accept a negative hit in the cache if the
		 * modification time of the parent directory matches
		 * the cached copy in the name cache entry.
		 * Otherwise, we discard all of the negative cache
		 * entries for this directory.  We also only trust
		 * negative cache entries for up to nm_negnametimeo
		 * seconds.
		 */
		if ((u_int)(ticks - ncticks) < (nmp->nm_negnametimeo * hz) &&
		    VOP_GETATTR(dvp, &vattr, cnp->cn_cred) == 0 &&
		    timespeccmp(&vattr.va_mtime, &nctime, ==)) {
			nfsstats.lookupcache_hits++;
			return (ENOENT);
		}
		cache_purge_negative(dvp);
	}

	attrflag = dattrflag = 0;
	error = 0;
	newvp = NULLVP;
	nfsstats.lookupcache_misses++;
	nfsstats.rpccnt[NFSPROC_LOOKUP]++;
	len = cnp->cn_namelen;
	mreq = nfsm_reqhead(dvp, NFSPROC_LOOKUP,
		NFSX_FH(v3) + NFSX_UNSIGNED + nfsm_rndup(len));
	mb = mreq;
	bpos = mtod(mb, caddr_t);
	nfsm_fhtom(dvp, v3);
	nfsm_strtom(cnp->cn_nameptr, len, NFS_MAXNAMLEN);
	nfsm_request(dvp, NFSPROC_LOOKUP, cnp->cn_thread, cnp->cn_cred);
	if (error) {
		if (v3) {
			nfsm_postop_attr_va(dvp, dattrflag, &vattr);
			m_freem(mrep);
		}
		goto nfsmout;
	}
	nfsm_getfh(fhp, fhsize, v3);

	/*
	 * Handle RENAME case...
	 */
	if (cnp->cn_nameiop == RENAME && (flags & ISLASTCN)) {
		if (NFS_CMPFH(np, fhp, fhsize)) {
			m_freem(mrep);
			return (EISDIR);
		}
		error = nfs_nget(mp, fhp, fhsize, &np, LK_EXCLUSIVE);
		if (error) {
			m_freem(mrep);
			return (error);
		}
		newvp = NFSTOV(np);
		if (v3) {
			nfsm_postop_attr(newvp, attrflag);
			nfsm_postop_attr(dvp, attrflag);
		} else
			nfsm_loadattr(newvp, NULL);
		*vpp = newvp;
		m_freem(mrep);
		cnp->cn_flags |= SAVENAME;
		return (0);
	}

	if (flags & ISDOTDOT) {
		ltype = VOP_ISLOCKED(dvp);
		error = vfs_busy(mp, MBF_NOWAIT);
		if (error != 0) {
			vfs_ref(mp);
			VOP_UNLOCK(dvp, 0);
			error = vfs_busy(mp, 0);
			vn_lock(dvp, ltype | LK_RETRY);
			vfs_rel(mp);
			if (error == 0 && (dvp->v_iflag & VI_DOOMED)) {
				vfs_unbusy(mp);
				error = ENOENT;
			}
			if (error != 0) {
				m_freem(mrep);
				return (error);
			}
		}
		VOP_UNLOCK(dvp, 0);
		error = nfs_nget(mp, fhp, fhsize, &np, cnp->cn_lkflags);
		if (error == 0)
			newvp = NFSTOV(np);
		vfs_unbusy(mp);
		if (newvp != dvp)
			vn_lock(dvp, ltype | LK_RETRY);
		if (dvp->v_iflag & VI_DOOMED) {
			if (error == 0) {
				if (newvp == dvp)
					vrele(newvp);
				else
					vput(newvp);
			}
			error = ENOENT;
		}
		if (error) {
			m_freem(mrep);
			return (error);
		}
	} else if (NFS_CMPFH(np, fhp, fhsize)) {
		VREF(dvp);
		newvp = dvp;
	} else {
		error = nfs_nget(mp, fhp, fhsize, &np, cnp->cn_lkflags);
		if (error) {
			m_freem(mrep);
			return (error);
		}
		newvp = NFSTOV(np);

		/*
		 * Flush the attribute cache when opening a leaf node
		 * to ensure that fresh attributes are fetched in
		 * nfs_open() if we are unable to fetch attributes
		 * from the LOOKUP reply.
		 */
		if ((flags & (ISLASTCN | ISOPEN)) == (ISLASTCN | ISOPEN) &&
		    !(np->n_flag & NMODIFIED)) {
			mtx_lock(&np->n_mtx);
			np->n_attrstamp = 0;
			KDTRACE_NFS_ATTRCACHE_FLUSH_DONE(newvp);
			mtx_unlock(&np->n_mtx);
		}
	}
	if (v3) {
		nfsm_postop_attr_va(newvp, attrflag, &vattr);
		nfsm_postop_attr_va(dvp, dattrflag, &dvattr);
	} else {
		nfsm_loadattr(newvp, &vattr);
		attrflag = 1;
	}
	if (cnp->cn_nameiop != LOOKUP && (flags & ISLASTCN))
		cnp->cn_flags |= SAVENAME;
	if ((cnp->cn_flags & MAKEENTRY) &&
	    (cnp->cn_nameiop != DELETE || !(flags & ISLASTCN)) &&
	    attrflag != 0 && (newvp->v_type != VDIR || dattrflag != 0))
		cache_enter_time(dvp, newvp, cnp, &vattr.va_ctime,
		    newvp->v_type != VDIR ? NULL : &dvattr.va_ctime);
	*vpp = newvp;
	m_freem(mrep);
nfsmout:
	if (error) {
		if (newvp != NULLVP) {
			vput(newvp);
			*vpp = NULLVP;
		}

		if (error != ENOENT)
			goto done;

		/* The requested file was not found. */
		if ((cnp->cn_nameiop == CREATE || cnp->cn_nameiop == RENAME) &&
		    (flags & ISLASTCN)) {
			/*
			 * XXX: UFS does a full VOP_ACCESS(dvp,
			 * VWRITE) here instead of just checking
			 * MNT_RDONLY.
			 */
			if (mp->mnt_flag & MNT_RDONLY)
				return (EROFS);
			cnp->cn_flags |= SAVENAME;
			return (EJUSTRETURN);
		}

		if ((cnp->cn_flags & MAKEENTRY) && cnp->cn_nameiop != CREATE &&
		    dattrflag) {
			/*
			 * Cache the modification time of the parent
			 * directory from the post-op attributes in
			 * the name cache entry.  The negative cache
			 * entry will be ignored once the directory
			 * has changed.  Don't bother adding the entry
			 * if the directory has already changed.
			 */
			mtx_lock(&np->n_mtx);
			if (timespeccmp(&np->n_vattr.va_mtime,
			    &vattr.va_mtime, ==)) {
				mtx_unlock(&np->n_mtx);
				cache_enter_time(dvp, NULL, cnp,
				    &vattr.va_mtime, NULL);
			} else
				mtx_unlock(&np->n_mtx);
		}
		return (ENOENT);
	}
done:
	return (error);
}

/*
 * nfs read call.
 * Just call nfs_bioread() to do the work.
 */
static int
nfs_read(struct vop_read_args *ap)
{
	struct vnode *vp = ap->a_vp;

	switch (vp->v_type) {
	case VREG:
		return (nfs_bioread(vp, ap->a_uio, ap->a_ioflag, ap->a_cred));
	case VDIR:
		return (EISDIR);
	default:
		return (EOPNOTSUPP);
	}
}

/*
 * nfs readlink call
 */
static int
nfs_readlink(struct vop_readlink_args *ap)
{
	struct vnode *vp = ap->a_vp;

	if (vp->v_type != VLNK)
		return (EINVAL);
	return (nfs_bioread(vp, ap->a_uio, 0, ap->a_cred));
}

/*
 * Do a readlink rpc.
 * Called by nfs_doio() from below the buffer cache.
 */
int
nfs_readlinkrpc(struct vnode *vp, struct uio *uiop, struct ucred *cred)
{
	caddr_t bpos, dpos;
	int error = 0, len, attrflag;
	struct mbuf *mreq, *mrep, *md, *mb;
	int v3 = NFS_ISV3(vp);

	nfsstats.rpccnt[NFSPROC_READLINK]++;
	mreq = nfsm_reqhead(vp, NFSPROC_READLINK, NFSX_FH(v3));
	mb = mreq;
	bpos = mtod(mb, caddr_t);
	nfsm_fhtom(vp, v3);
	nfsm_request(vp, NFSPROC_READLINK, uiop->uio_td, cred);
	if (v3)
		nfsm_postop_attr(vp, attrflag);
	if (!error) {
		nfsm_strsiz(len, NFS_MAXPATHLEN);
		if (len == NFS_MAXPATHLEN) {
			struct nfsnode *np = VTONFS(vp);
			mtx_lock(&np->n_mtx);
			if (np->n_size && np->n_size < NFS_MAXPATHLEN)
				len = np->n_size;
			mtx_unlock(&np->n_mtx);
		}
		nfsm_mtouio(uiop, len);
	}
	m_freem(mrep);
nfsmout:
	return (error);
}

/*
 * nfs read rpc call
 * Ditto above
 */
int
nfs_readrpc(struct vnode *vp, struct uio *uiop, struct ucred *cred)
{
	u_int32_t *tl;
	caddr_t bpos, dpos;
	struct mbuf *mreq, *mrep, *md, *mb;
	struct nfsmount *nmp;
	off_t end;
	int error = 0, len, retlen, tsiz, eof, attrflag;
	int v3 = NFS_ISV3(vp);
	int rsize;

#ifndef nolint
	eof = 0;
#endif
	nmp = VFSTONFS(vp->v_mount);
	tsiz = uiop->uio_resid;
	mtx_lock(&nmp->nm_mtx);
	end = uiop->uio_offset + tsiz;
	if (end > nmp->nm_maxfilesize || end < uiop->uio_offset) {
		mtx_unlock(&nmp->nm_mtx);
		return (EFBIG);
	}
	rsize = nmp->nm_rsize;
	mtx_unlock(&nmp->nm_mtx);
	while (tsiz > 0) {
		nfsstats.rpccnt[NFSPROC_READ]++;
		len = (tsiz > rsize) ? rsize : tsiz;
		mreq = nfsm_reqhead(vp, NFSPROC_READ, NFSX_FH(v3) + NFSX_UNSIGNED * 3);
		mb = mreq;
		bpos = mtod(mb, caddr_t);
		nfsm_fhtom(vp, v3);
		tl = nfsm_build(u_int32_t *, NFSX_UNSIGNED * 3);
		if (v3) {
			txdr_hyper(uiop->uio_offset, tl);
			*(tl + 2) = txdr_unsigned(len);
		} else {
			*tl++ = txdr_unsigned(uiop->uio_offset);
			*tl++ = txdr_unsigned(len);
			*tl = 0;
		}
		nfsm_request(vp, NFSPROC_READ, uiop->uio_td, cred);
		if (v3) {
			nfsm_postop_attr(vp, attrflag);
			if (error) {
				m_freem(mrep);
				goto nfsmout;
			}
			tl = nfsm_dissect(u_int32_t *, 2 * NFSX_UNSIGNED);
			eof = fxdr_unsigned(int, *(tl + 1));
		} else {
			nfsm_loadattr(vp, NULL);
		}
		nfsm_strsiz(retlen, rsize);
		nfsm_mtouio(uiop, retlen);
		m_freem(mrep);
		tsiz -= retlen;
		if (v3) {
			if (eof || retlen == 0) {
				tsiz = 0;
			}
		} else if (retlen < len) {
			tsiz = 0;
		}
	}
nfsmout:
	return (error);
}

/*
 * nfs write call
 */
int
nfs_writerpc(struct vnode *vp, struct uio *uiop, struct ucred *cred,
	     int *iomode, int *must_commit)
{
	u_int32_t *tl;
	int32_t backup;
	caddr_t bpos, dpos;
	struct mbuf *mreq, *mrep, *md, *mb;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	off_t end;
	int error = 0, len, tsiz, wccflag = NFSV3_WCCRATTR, rlen, commit;
	int v3 = NFS_ISV3(vp), committed = NFSV3WRITE_FILESYNC;
	int wsize;
	
	KASSERT(uiop->uio_iovcnt == 1, ("nfs: writerpc iovcnt > 1"));
	*must_commit = 0;
	tsiz = uiop->uio_resid;
	mtx_lock(&nmp->nm_mtx);
	end = uiop->uio_offset + tsiz;
	if (end > nmp->nm_maxfilesize || end < uiop->uio_offset) {
		mtx_unlock(&nmp->nm_mtx);		
		return (EFBIG);
	}
	wsize = nmp->nm_wsize;
	mtx_unlock(&nmp->nm_mtx);
	while (tsiz > 0) {
		nfsstats.rpccnt[NFSPROC_WRITE]++;
		len = (tsiz > wsize) ? wsize : tsiz;
		mreq = nfsm_reqhead(vp, NFSPROC_WRITE,
			NFSX_FH(v3) + 5 * NFSX_UNSIGNED + nfsm_rndup(len));
		mb = mreq;
		bpos = mtod(mb, caddr_t);
		nfsm_fhtom(vp, v3);
		if (v3) {
			tl = nfsm_build(u_int32_t *, 5 * NFSX_UNSIGNED);
			txdr_hyper(uiop->uio_offset, tl);
			tl += 2;
			*tl++ = txdr_unsigned(len);
			*tl++ = txdr_unsigned(*iomode);
			*tl = txdr_unsigned(len);
		} else {
			u_int32_t x;

			tl = nfsm_build(u_int32_t *, 4 * NFSX_UNSIGNED);
			/* Set both "begin" and "current" to non-garbage. */
			x = txdr_unsigned((u_int32_t)uiop->uio_offset);
			*tl++ = x;	/* "begin offset" */
			*tl++ = x;	/* "current offset" */
			x = txdr_unsigned(len);
			*tl++ = x;	/* total to this offset */
			*tl = x;	/* size of this write */
		}
		nfsm_uiotom(uiop, len);
		nfsm_request(vp, NFSPROC_WRITE, uiop->uio_td, cred);
		if (v3) {
			wccflag = NFSV3_WCCCHK;
			nfsm_wcc_data(vp, wccflag);
			if (!error) {
				tl = nfsm_dissect(u_int32_t *, 2 * NFSX_UNSIGNED
					+ NFSX_V3WRITEVERF);
				rlen = fxdr_unsigned(int, *tl++);
				if (rlen == 0) {
					error = NFSERR_IO;
					m_freem(mrep);
					break;
				} else if (rlen < len) {
					backup = len - rlen;
					uiop->uio_iov->iov_base =
					    (char *)uiop->uio_iov->iov_base -
					    backup;
					uiop->uio_iov->iov_len += backup;
					uiop->uio_offset -= backup;
					uiop->uio_resid += backup;
					len = rlen;
				}
				commit = fxdr_unsigned(int, *tl++);

				/*
				 * Return the lowest committment level
				 * obtained by any of the RPCs.
				 */
				if (committed == NFSV3WRITE_FILESYNC)
					committed = commit;
				else if (committed == NFSV3WRITE_DATASYNC &&
					commit == NFSV3WRITE_UNSTABLE)
					committed = commit;
				mtx_lock(&nmp->nm_mtx);
				if ((nmp->nm_state & NFSSTA_HASWRITEVERF) == 0){
				    bcopy((caddr_t)tl, (caddr_t)nmp->nm_verf,
					NFSX_V3WRITEVERF);
				    nmp->nm_state |= NFSSTA_HASWRITEVERF;
				} else if (bcmp((caddr_t)tl,
				    (caddr_t)nmp->nm_verf, NFSX_V3WRITEVERF)) {
				    *must_commit = 1;
				    bcopy((caddr_t)tl, (caddr_t)nmp->nm_verf,
					NFSX_V3WRITEVERF);
				}
				mtx_unlock(&nmp->nm_mtx);
			}
		} else {
			nfsm_loadattr(vp, NULL);
		}
		if (wccflag) {
			mtx_lock(&(VTONFS(vp))->n_mtx);
			VTONFS(vp)->n_mtime = VTONFS(vp)->n_vattr.va_mtime;
			mtx_unlock(&(VTONFS(vp))->n_mtx);
		}
		m_freem(mrep);
		if (error)
			break;
		tsiz -= len;
	}
nfsmout:
	if (DOINGASYNC(vp))
		committed = NFSV3WRITE_FILESYNC;
	*iomode = committed;
	if (error)
		uiop->uio_resid = tsiz;
	return (error);
}

/*
 * nfs mknod rpc
 * For NFS v2 this is a kludge. Use a create rpc but with the IFMT bits of the
 * mode set to specify the file type and the size field for rdev.
 */
static int
nfs_mknodrpc(struct vnode *dvp, struct vnode **vpp, struct componentname *cnp,
    struct vattr *vap)
{
	struct nfsv2_sattr *sp;
	u_int32_t *tl;
	struct vnode *newvp = NULL;
	struct nfsnode *np = NULL;
	struct vattr vattr;
	caddr_t bpos, dpos;
	int error = 0, wccflag = NFSV3_WCCRATTR, gotvp = 0;
	struct mbuf *mreq, *mrep, *md, *mb;
	u_int32_t rdev;
	int v3 = NFS_ISV3(dvp);

	if (vap->va_type == VCHR || vap->va_type == VBLK)
		rdev = txdr_unsigned(vap->va_rdev);
	else if (vap->va_type == VFIFO || vap->va_type == VSOCK)
		rdev = nfs_xdrneg1;
	else {
		return (EOPNOTSUPP);
	}
	if ((error = VOP_GETATTR(dvp, &vattr, cnp->cn_cred)) != 0)
		return (error);
	nfsstats.rpccnt[NFSPROC_MKNOD]++;
	mreq = nfsm_reqhead(dvp, NFSPROC_MKNOD, NFSX_FH(v3) + 4 * NFSX_UNSIGNED +
		+ nfsm_rndup(cnp->cn_namelen) + NFSX_SATTR(v3));
	mb = mreq;
	bpos = mtod(mb, caddr_t);
	nfsm_fhtom(dvp, v3);
	nfsm_strtom(cnp->cn_nameptr, cnp->cn_namelen, NFS_MAXNAMLEN);
	if (v3) {
		tl = nfsm_build(u_int32_t *, NFSX_UNSIGNED);
		*tl++ = vtonfsv3_type(vap->va_type);
		nfsm_v3attrbuild(vap, FALSE);
		if (vap->va_type == VCHR || vap->va_type == VBLK) {
			tl = nfsm_build(u_int32_t *, 2 * NFSX_UNSIGNED);
			*tl++ = txdr_unsigned(major(vap->va_rdev));
			*tl = txdr_unsigned(minor(vap->va_rdev));
		}
	} else {
		sp = nfsm_build(struct nfsv2_sattr *, NFSX_V2SATTR);
		sp->sa_mode = vtonfsv2_mode(vap->va_type, vap->va_mode);
		sp->sa_uid = nfs_xdrneg1;
		sp->sa_gid = nfs_xdrneg1;
		sp->sa_size = rdev;
		txdr_nfsv2time(&vap->va_atime, &sp->sa_atime);
		txdr_nfsv2time(&vap->va_mtime, &sp->sa_mtime);
	}
	nfsm_request(dvp, NFSPROC_MKNOD, cnp->cn_thread, cnp->cn_cred);
	if (!error) {
		nfsm_mtofh(dvp, newvp, v3, gotvp);
		if (!gotvp) {
			if (newvp) {
				vput(newvp);
				newvp = NULL;
			}
			error = nfs_lookitup(dvp, cnp->cn_nameptr,
			    cnp->cn_namelen, cnp->cn_cred, cnp->cn_thread, &np);
			if (!error)
				newvp = NFSTOV(np);
		}
	}
	if (v3)
		nfsm_wcc_data(dvp, wccflag);
	m_freem(mrep);
nfsmout:
	if (error) {
		if (newvp)
			vput(newvp);
	} else {
		*vpp = newvp;
	}
	mtx_lock(&(VTONFS(dvp))->n_mtx);
	VTONFS(dvp)->n_flag |= NMODIFIED;
	if (!wccflag) {
		VTONFS(dvp)->n_attrstamp = 0;
		KDTRACE_NFS_ATTRCACHE_FLUSH_DONE(dvp);
	}
	mtx_unlock(&(VTONFS(dvp))->n_mtx);
	return (error);
}

/*
 * nfs mknod vop
 * just call nfs_mknodrpc() to do the work.
 */
/* ARGSUSED */
static int
nfs_mknod(struct vop_mknod_args *ap)
{
	return (nfs_mknodrpc(ap->a_dvp, ap->a_vpp, ap->a_cnp, ap->a_vap));
}

static u_long create_verf;
/*
 * nfs file create call
 */
static int
nfs_create(struct vop_create_args *ap)
{
	struct vnode *dvp = ap->a_dvp;
	struct vattr *vap = ap->a_vap;
	struct componentname *cnp = ap->a_cnp;
	struct nfsv2_sattr *sp;
	u_int32_t *tl;
	struct nfsnode *np = NULL;
	struct vnode *newvp = NULL;
	caddr_t bpos, dpos;
	int error = 0, wccflag = NFSV3_WCCRATTR, gotvp = 0, fmode = 0;
	struct mbuf *mreq, *mrep, *md, *mb;
	struct vattr vattr;
	int v3 = NFS_ISV3(dvp);

	/*
	 * Oops, not for me..
	 */
	if (vap->va_type == VSOCK) {
		error = nfs_mknodrpc(dvp, ap->a_vpp, cnp, vap);
		return (error);
	}

	if ((error = VOP_GETATTR(dvp, &vattr, cnp->cn_cred)) != 0) {
		return (error);
	}
	if (vap->va_vaflags & VA_EXCLUSIVE)
		fmode |= O_EXCL;
again:
	nfsstats.rpccnt[NFSPROC_CREATE]++;
	mreq = nfsm_reqhead(dvp, NFSPROC_CREATE, NFSX_FH(v3) + 2 * NFSX_UNSIGNED +
		nfsm_rndup(cnp->cn_namelen) + NFSX_SATTR(v3));
	mb = mreq;
	bpos = mtod(mb, caddr_t);
	nfsm_fhtom(dvp, v3);
	nfsm_strtom(cnp->cn_nameptr, cnp->cn_namelen, NFS_MAXNAMLEN);
	if (v3) {
		tl = nfsm_build(u_int32_t *, NFSX_UNSIGNED);
		if (fmode & O_EXCL) {
			*tl = txdr_unsigned(NFSV3CREATE_EXCLUSIVE);
			tl = nfsm_build(u_int32_t *, NFSX_V3CREATEVERF);
#ifdef INET
			CURVNET_SET(CRED_TO_VNET(cnp->cn_cred));
			IN_IFADDR_RLOCK();
			if (!TAILQ_EMPTY(&V_in_ifaddrhead))
				*tl++ = IA_SIN(TAILQ_FIRST(&V_in_ifaddrhead))->sin_addr.s_addr;
			else
#endif
				*tl++ = create_verf;
#ifdef INET
			IN_IFADDR_RUNLOCK();
			CURVNET_RESTORE();
#endif
			*tl = ++create_verf;
		} else {
			*tl = txdr_unsigned(NFSV3CREATE_UNCHECKED);
			nfsm_v3attrbuild(vap, FALSE);
		}
	} else {
		sp = nfsm_build(struct nfsv2_sattr *, NFSX_V2SATTR);
		sp->sa_mode = vtonfsv2_mode(vap->va_type, vap->va_mode);
		sp->sa_uid = nfs_xdrneg1;
		sp->sa_gid = nfs_xdrneg1;
		sp->sa_size = 0;
		txdr_nfsv2time(&vap->va_atime, &sp->sa_atime);
		txdr_nfsv2time(&vap->va_mtime, &sp->sa_mtime);
	}
	nfsm_request(dvp, NFSPROC_CREATE, cnp->cn_thread, cnp->cn_cred);
	if (!error) {
		nfsm_mtofh(dvp, newvp, v3, gotvp);
		if (!gotvp) {
			if (newvp) {
				vput(newvp);
				newvp = NULL;
			}
			error = nfs_lookitup(dvp, cnp->cn_nameptr,
			    cnp->cn_namelen, cnp->cn_cred, cnp->cn_thread, &np);
			if (!error)
				newvp = NFSTOV(np);
		}
	}
	if (v3)
		nfsm_wcc_data(dvp, wccflag);
	m_freem(mrep);
nfsmout:
	if (error) {
		if (v3 && (fmode & O_EXCL) && error == NFSERR_NOTSUPP) {
			fmode &= ~O_EXCL;
			goto again;
		}
		if (newvp)
			vput(newvp);
	} else if (v3 && (fmode & O_EXCL)) {
		/*
		 * We are normally called with only a partially initialized
		 * VAP.  Since the NFSv3 spec says that server may use the
		 * file attributes to store the verifier, the spec requires
		 * us to do a SETATTR RPC. FreeBSD servers store the verifier
		 * in atime, but we can't really assume that all servers will
		 * so we ensure that our SETATTR sets both atime and mtime.
		 */
		if (vap->va_mtime.tv_sec == VNOVAL)
			vfs_timestamp(&vap->va_mtime);
		if (vap->va_atime.tv_sec == VNOVAL)
			vap->va_atime = vap->va_mtime;
		error = nfs_setattrrpc(newvp, vap, cnp->cn_cred);
		if (error)
			vput(newvp);
	}
	if (!error) {
		*ap->a_vpp = newvp;
	}
	mtx_lock(&(VTONFS(dvp))->n_mtx);
	VTONFS(dvp)->n_flag |= NMODIFIED;
	if (!wccflag) {
		VTONFS(dvp)->n_attrstamp = 0;
		KDTRACE_NFS_ATTRCACHE_FLUSH_DONE(dvp);
	}
	mtx_unlock(&(VTONFS(dvp))->n_mtx);
	return (error);
}

/*
 * nfs file remove call
 * To try and make nfs semantics closer to ufs semantics, a file that has
 * other processes using the vnode is renamed instead of removed and then
 * removed later on the last close.
 * - If v_usecount > 1
 *	  If a rename is not already in the works
 *	     call nfs_sillyrename() to set it up
 *     else
 *	  do the remove rpc
 */
static int
nfs_remove(struct vop_remove_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct vnode *dvp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
	struct nfsnode *np = VTONFS(vp);
	int error = 0;
	struct vattr vattr;

	KASSERT((cnp->cn_flags & HASBUF) != 0, ("nfs_remove: no name"));
	KASSERT(vrefcnt(vp) > 0, ("nfs_remove: bad v_usecount"));
	if (vp->v_type == VDIR)
		error = EPERM;
	else if (vrefcnt(vp) == 1 || (np->n_sillyrename &&
	    !VOP_GETATTR(vp, &vattr, cnp->cn_cred) && vattr.va_nlink > 1)) {
		/*
		 * Purge the name cache so that the chance of a lookup for
		 * the name succeeding while the remove is in progress is
		 * minimized. Without node locking it can still happen, such
		 * that an I/O op returns ESTALE, but since you get this if
		 * another host removes the file..
		 */
		cache_purge(vp);
		/*
		 * throw away biocache buffers, mainly to avoid
		 * unnecessary delayed writes later.
		 */
		error = nfs_vinvalbuf(vp, 0, cnp->cn_thread, 1);
		/* Do the rpc */
		if (error != EINTR && error != EIO)
			error = nfs_removerpc(dvp, cnp->cn_nameptr,
				cnp->cn_namelen, cnp->cn_cred, cnp->cn_thread);
		/*
		 * Kludge City: If the first reply to the remove rpc is lost..
		 *   the reply to the retransmitted request will be ENOENT
		 *   since the file was in fact removed
		 *   Therefore, we cheat and return success.
		 */
		if (error == ENOENT)
			error = 0;
	} else if (!np->n_sillyrename)
		error = nfs_sillyrename(dvp, vp, cnp);
	mtx_lock(&np->n_mtx);
	np->n_attrstamp = 0;
	mtx_unlock(&np->n_mtx);
	KDTRACE_NFS_ATTRCACHE_FLUSH_DONE(vp);
	return (error);
}

/*
 * nfs file remove rpc called from nfs_inactive
 */
int
nfs_removeit(struct sillyrename *sp)
{
	/*
	 * Make sure that the directory vnode is still valid.
	 * XXX we should lock sp->s_dvp here.
	 */
	if (sp->s_dvp->v_type == VBAD)
		return (0);
	return (nfs_removerpc(sp->s_dvp, sp->s_name, sp->s_namlen, sp->s_cred,
		NULL));
}

/*
 * Nfs remove rpc, called from nfs_remove() and nfs_removeit().
 */
static int
nfs_removerpc(struct vnode *dvp, const char *name, int namelen,
    struct ucred *cred, struct thread *td)
{
	caddr_t bpos, dpos;
	int error = 0, wccflag = NFSV3_WCCRATTR;
	struct mbuf *mreq, *mrep, *md, *mb;
	int v3 = NFS_ISV3(dvp);

	nfsstats.rpccnt[NFSPROC_REMOVE]++;
	mreq = nfsm_reqhead(dvp, NFSPROC_REMOVE,
		NFSX_FH(v3) + NFSX_UNSIGNED + nfsm_rndup(namelen));
	mb = mreq;
	bpos = mtod(mb, caddr_t);
	nfsm_fhtom(dvp, v3);
	nfsm_strtom(name, namelen, NFS_MAXNAMLEN);
	nfsm_request(dvp, NFSPROC_REMOVE, td, cred);
	if (v3)
		nfsm_wcc_data(dvp, wccflag);
	m_freem(mrep);
nfsmout:
	mtx_lock(&(VTONFS(dvp))->n_mtx);
	VTONFS(dvp)->n_flag |= NMODIFIED;
	if (!wccflag) {
		VTONFS(dvp)->n_attrstamp = 0;
		KDTRACE_NFS_ATTRCACHE_FLUSH_DONE(dvp);
	}
	mtx_unlock(&(VTONFS(dvp))->n_mtx);
	return (error);
}

/*
 * nfs file rename call
 */
static int
nfs_rename(struct vop_rename_args *ap)
{
	struct vnode *fvp = ap->a_fvp;
	struct vnode *tvp = ap->a_tvp;
	struct vnode *fdvp = ap->a_fdvp;
	struct vnode *tdvp = ap->a_tdvp;
	struct componentname *tcnp = ap->a_tcnp;
	struct componentname *fcnp = ap->a_fcnp;
	int error;

	KASSERT((tcnp->cn_flags & HASBUF) != 0 &&
	    (fcnp->cn_flags & HASBUF) != 0, ("nfs_rename: no name"));
	/* Check for cross-device rename */
	if ((fvp->v_mount != tdvp->v_mount) ||
	    (tvp && (fvp->v_mount != tvp->v_mount))) {
		error = EXDEV;
		goto out;
	}

	if (fvp == tvp) {
		nfs_printf("nfs_rename: fvp == tvp (can't happen)\n");
		error = 0;
		goto out;
	}
	if ((error = vn_lock(fvp, LK_EXCLUSIVE)) != 0)
		goto out;

	/*
	 * We have to flush B_DELWRI data prior to renaming
	 * the file.  If we don't, the delayed-write buffers
	 * can be flushed out later after the file has gone stale
	 * under NFSV3.  NFSV2 does not have this problem because
	 * ( as far as I can tell ) it flushes dirty buffers more
	 * often.
	 * 
	 * Skip the rename operation if the fsync fails, this can happen
	 * due to the server's volume being full, when we pushed out data
	 * that was written back to our cache earlier. Not checking for
	 * this condition can result in potential (silent) data loss.
	 */
	error = VOP_FSYNC(fvp, MNT_WAIT, fcnp->cn_thread);
	VOP_UNLOCK(fvp, 0);
	if (!error && tvp)
		error = VOP_FSYNC(tvp, MNT_WAIT, tcnp->cn_thread);
	if (error)
		goto out;

	/*
	 * If the tvp exists and is in use, sillyrename it before doing the
	 * rename of the new file over it.
	 * XXX Can't sillyrename a directory.
	 */
	if (tvp && vrefcnt(tvp) > 1 && !VTONFS(tvp)->n_sillyrename &&
		tvp->v_type != VDIR && !nfs_sillyrename(tdvp, tvp, tcnp)) {
		vput(tvp);
		tvp = NULL;
	}

	error = nfs_renamerpc(fdvp, fcnp->cn_nameptr, fcnp->cn_namelen,
		tdvp, tcnp->cn_nameptr, tcnp->cn_namelen, tcnp->cn_cred,
		tcnp->cn_thread);

	if (fvp->v_type == VDIR) {
		if (tvp != NULL && tvp->v_type == VDIR)
			cache_purge(tdvp);
		cache_purge(fdvp);
	}

out:
	if (tdvp == tvp)
		vrele(tdvp);
	else
		vput(tdvp);
	if (tvp)
		vput(tvp);
	vrele(fdvp);
	vrele(fvp);
	/*
	 * Kludge: Map ENOENT => 0 assuming that it is a reply to a retry.
	 */
	if (error == ENOENT)
		error = 0;
	return (error);
}

/*
 * nfs file rename rpc called from nfs_remove() above
 */
static int
nfs_renameit(struct vnode *sdvp, struct componentname *scnp,
    struct sillyrename *sp)
{

	return (nfs_renamerpc(sdvp, scnp->cn_nameptr, scnp->cn_namelen, sdvp,
	    sp->s_name, sp->s_namlen, scnp->cn_cred, scnp->cn_thread));
}

/*
 * Do an nfs rename rpc. Called from nfs_rename() and nfs_renameit().
 */
static int
nfs_renamerpc(struct vnode *fdvp, const char *fnameptr, int fnamelen,
    struct vnode *tdvp, const char *tnameptr, int tnamelen, struct ucred *cred,
    struct thread *td)
{
	caddr_t bpos, dpos;
	int error = 0, fwccflag = NFSV3_WCCRATTR, twccflag = NFSV3_WCCRATTR;
	struct mbuf *mreq, *mrep, *md, *mb;
	int v3 = NFS_ISV3(fdvp);

	nfsstats.rpccnt[NFSPROC_RENAME]++;
	mreq = nfsm_reqhead(fdvp, NFSPROC_RENAME,
		(NFSX_FH(v3) + NFSX_UNSIGNED)*2 + nfsm_rndup(fnamelen) +
		nfsm_rndup(tnamelen));
	mb = mreq;
	bpos = mtod(mb, caddr_t);
	nfsm_fhtom(fdvp, v3);
	nfsm_strtom(fnameptr, fnamelen, NFS_MAXNAMLEN);
	nfsm_fhtom(tdvp, v3);
	nfsm_strtom(tnameptr, tnamelen, NFS_MAXNAMLEN);
	nfsm_request(fdvp, NFSPROC_RENAME, td, cred);
	if (v3) {
		nfsm_wcc_data(fdvp, fwccflag);
		nfsm_wcc_data(tdvp, twccflag);
	}
	m_freem(mrep);
nfsmout:
	mtx_lock(&(VTONFS(fdvp))->n_mtx);
	VTONFS(fdvp)->n_flag |= NMODIFIED;
	mtx_unlock(&(VTONFS(fdvp))->n_mtx);
	mtx_lock(&(VTONFS(tdvp))->n_mtx);
	VTONFS(tdvp)->n_flag |= NMODIFIED;
	mtx_unlock(&(VTONFS(tdvp))->n_mtx);
	if (!fwccflag) {
		VTONFS(fdvp)->n_attrstamp = 0;
		KDTRACE_NFS_ATTRCACHE_FLUSH_DONE(fdvp);
	}
	if (!twccflag) {
		VTONFS(tdvp)->n_attrstamp = 0;
		KDTRACE_NFS_ATTRCACHE_FLUSH_DONE(tdvp);
	}
	return (error);
}

/*
 * nfs hard link create call
 */
static int
nfs_link(struct vop_link_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct vnode *tdvp = ap->a_tdvp;
	struct componentname *cnp = ap->a_cnp;
	caddr_t bpos, dpos;
	int error = 0, wccflag = NFSV3_WCCRATTR, attrflag = 0;
	struct mbuf *mreq, *mrep, *md, *mb;
	int v3;

	if (vp->v_mount != tdvp->v_mount) {
		return (EXDEV);
	}

	/*
	 * Push all writes to the server, so that the attribute cache
	 * doesn't get "out of sync" with the server.
	 * XXX There should be a better way!
	 */
	VOP_FSYNC(vp, MNT_WAIT, cnp->cn_thread);

	v3 = NFS_ISV3(vp);
	nfsstats.rpccnt[NFSPROC_LINK]++;
	mreq = nfsm_reqhead(vp, NFSPROC_LINK,
		NFSX_FH(v3)*2 + NFSX_UNSIGNED + nfsm_rndup(cnp->cn_namelen));
	mb = mreq;
	bpos = mtod(mb, caddr_t);
	nfsm_fhtom(vp, v3);
	nfsm_fhtom(tdvp, v3);
	nfsm_strtom(cnp->cn_nameptr, cnp->cn_namelen, NFS_MAXNAMLEN);
	nfsm_request(vp, NFSPROC_LINK, cnp->cn_thread, cnp->cn_cred);
	if (v3) {
		nfsm_postop_attr(vp, attrflag);
		nfsm_wcc_data(tdvp, wccflag);
	}
	m_freem(mrep);
nfsmout:
	mtx_lock(&(VTONFS(tdvp))->n_mtx);
	VTONFS(tdvp)->n_flag |= NMODIFIED;
	mtx_unlock(&(VTONFS(tdvp))->n_mtx);
	if (!attrflag) {
		VTONFS(vp)->n_attrstamp = 0;
		KDTRACE_NFS_ATTRCACHE_FLUSH_DONE(vp);
	}
	if (!wccflag) {
		VTONFS(tdvp)->n_attrstamp = 0;
		KDTRACE_NFS_ATTRCACHE_FLUSH_DONE(tdvp);
	}
	return (error);
}

/*
 * nfs symbolic link create call
 */
static int
nfs_symlink(struct vop_symlink_args *ap)
{
	struct vnode *dvp = ap->a_dvp;
	struct vattr *vap = ap->a_vap;
	struct componentname *cnp = ap->a_cnp;
	struct nfsv2_sattr *sp;
	caddr_t bpos, dpos;
	int slen, error = 0, wccflag = NFSV3_WCCRATTR, gotvp;
	struct mbuf *mreq, *mrep, *md, *mb;
	struct vnode *newvp = NULL;
	int v3 = NFS_ISV3(dvp);

	nfsstats.rpccnt[NFSPROC_SYMLINK]++;
	slen = strlen(ap->a_target);
	mreq = nfsm_reqhead(dvp, NFSPROC_SYMLINK, NFSX_FH(v3) + 2*NFSX_UNSIGNED +
	    nfsm_rndup(cnp->cn_namelen) + nfsm_rndup(slen) + NFSX_SATTR(v3));
	mb = mreq;
	bpos = mtod(mb, caddr_t);
	nfsm_fhtom(dvp, v3);
	nfsm_strtom(cnp->cn_nameptr, cnp->cn_namelen, NFS_MAXNAMLEN);
	if (v3) {
		nfsm_v3attrbuild(vap, FALSE);
	}
	nfsm_strtom(ap->a_target, slen, NFS_MAXPATHLEN);
	if (!v3) {
		sp = nfsm_build(struct nfsv2_sattr *, NFSX_V2SATTR);
		sp->sa_mode = vtonfsv2_mode(VLNK, vap->va_mode);
		sp->sa_uid = nfs_xdrneg1;
		sp->sa_gid = nfs_xdrneg1;
		sp->sa_size = nfs_xdrneg1;
		txdr_nfsv2time(&vap->va_atime, &sp->sa_atime);
		txdr_nfsv2time(&vap->va_mtime, &sp->sa_mtime);
	}

	/*
	 * Issue the NFS request and get the rpc response.
	 *
	 * Only NFSv3 responses returning an error of 0 actually return
	 * a file handle that can be converted into newvp without having
	 * to do an extra lookup rpc.
	 */
	nfsm_request(dvp, NFSPROC_SYMLINK, cnp->cn_thread, cnp->cn_cred);
	if (v3) {
		if (error == 0)
			nfsm_mtofh(dvp, newvp, v3, gotvp);
		nfsm_wcc_data(dvp, wccflag);
	}

	/*
	 * out code jumps -> here, mrep is also freed.
	 */

	m_freem(mrep);
nfsmout:

	/*
	 * If we do not have an error and we could not extract the newvp from
	 * the response due to the request being NFSv2, we have to do a
	 * lookup in order to obtain a newvp to return.
	 */
	if (error == 0 && newvp == NULL) {
		struct nfsnode *np = NULL;

		error = nfs_lookitup(dvp, cnp->cn_nameptr, cnp->cn_namelen,
		    cnp->cn_cred, cnp->cn_thread, &np);
		if (!error)
			newvp = NFSTOV(np);
	}
	if (error) {
		if (newvp)
			vput(newvp);
	} else {
		*ap->a_vpp = newvp;
	}
	mtx_lock(&(VTONFS(dvp))->n_mtx);
	VTONFS(dvp)->n_flag |= NMODIFIED;
	mtx_unlock(&(VTONFS(dvp))->n_mtx);
	if (!wccflag) {
		VTONFS(dvp)->n_attrstamp = 0;
		KDTRACE_NFS_ATTRCACHE_FLUSH_DONE(dvp);
	}
	return (error);
}

/*
 * nfs make dir call
 */
static int
nfs_mkdir(struct vop_mkdir_args *ap)
{
	struct vnode *dvp = ap->a_dvp;
	struct vattr *vap = ap->a_vap;
	struct componentname *cnp = ap->a_cnp;
	struct nfsv2_sattr *sp;
	int len;
	struct nfsnode *np = NULL;
	struct vnode *newvp = NULL;
	caddr_t bpos, dpos;
	int error = 0, wccflag = NFSV3_WCCRATTR;
	int gotvp = 0;
	struct mbuf *mreq, *mrep, *md, *mb;
	struct vattr vattr;
	int v3 = NFS_ISV3(dvp);

	if ((error = VOP_GETATTR(dvp, &vattr, cnp->cn_cred)) != 0)
		return (error);
	len = cnp->cn_namelen;
	nfsstats.rpccnt[NFSPROC_MKDIR]++;
	mreq = nfsm_reqhead(dvp, NFSPROC_MKDIR,
	  NFSX_FH(v3) + NFSX_UNSIGNED + nfsm_rndup(len) + NFSX_SATTR(v3));
	mb = mreq;
	bpos = mtod(mb, caddr_t);
	nfsm_fhtom(dvp, v3);
	nfsm_strtom(cnp->cn_nameptr, len, NFS_MAXNAMLEN);
	if (v3) {
		nfsm_v3attrbuild(vap, FALSE);
	} else {
		sp = nfsm_build(struct nfsv2_sattr *, NFSX_V2SATTR);
		sp->sa_mode = vtonfsv2_mode(VDIR, vap->va_mode);
		sp->sa_uid = nfs_xdrneg1;
		sp->sa_gid = nfs_xdrneg1;
		sp->sa_size = nfs_xdrneg1;
		txdr_nfsv2time(&vap->va_atime, &sp->sa_atime);
		txdr_nfsv2time(&vap->va_mtime, &sp->sa_mtime);
	}
	nfsm_request(dvp, NFSPROC_MKDIR, cnp->cn_thread, cnp->cn_cred);
	if (!error)
		nfsm_mtofh(dvp, newvp, v3, gotvp);
	if (v3)
		nfsm_wcc_data(dvp, wccflag);
	m_freem(mrep);
nfsmout:
	mtx_lock(&(VTONFS(dvp))->n_mtx);
	VTONFS(dvp)->n_flag |= NMODIFIED;
	mtx_unlock(&(VTONFS(dvp))->n_mtx);
	if (!wccflag) {
		VTONFS(dvp)->n_attrstamp = 0;
		KDTRACE_NFS_ATTRCACHE_FLUSH_DONE(dvp);
	}
	if (error == 0 && newvp == NULL) {
		error = nfs_lookitup(dvp, cnp->cn_nameptr, len, cnp->cn_cred,
			cnp->cn_thread, &np);
		if (!error) {
			newvp = NFSTOV(np);
			if (newvp->v_type != VDIR)
				error = EEXIST;
		}
	}
	if (error) {
		if (newvp)
			vput(newvp);
	} else
		*ap->a_vpp = newvp;
	return (error);
}

/*
 * nfs remove directory call
 */
static int
nfs_rmdir(struct vop_rmdir_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct vnode *dvp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
	caddr_t bpos, dpos;
	int error = 0, wccflag = NFSV3_WCCRATTR;
	struct mbuf *mreq, *mrep, *md, *mb;
	int v3 = NFS_ISV3(dvp);

	if (dvp == vp)
		return (EINVAL);
	nfsstats.rpccnt[NFSPROC_RMDIR]++;
	mreq = nfsm_reqhead(dvp, NFSPROC_RMDIR,
		NFSX_FH(v3) + NFSX_UNSIGNED + nfsm_rndup(cnp->cn_namelen));
	mb = mreq;
	bpos = mtod(mb, caddr_t);
	nfsm_fhtom(dvp, v3);
	nfsm_strtom(cnp->cn_nameptr, cnp->cn_namelen, NFS_MAXNAMLEN);
	nfsm_request(dvp, NFSPROC_RMDIR, cnp->cn_thread, cnp->cn_cred);
	if (v3)
		nfsm_wcc_data(dvp, wccflag);
	m_freem(mrep);
nfsmout:
	mtx_lock(&(VTONFS(dvp))->n_mtx);
	VTONFS(dvp)->n_flag |= NMODIFIED;
	mtx_unlock(&(VTONFS(dvp))->n_mtx);
	if (!wccflag) {
		VTONFS(dvp)->n_attrstamp = 0;
		KDTRACE_NFS_ATTRCACHE_FLUSH_DONE(dvp);
	}
	cache_purge(dvp);
	cache_purge(vp);
	/*
	 * Kludge: Map ENOENT => 0 assuming that you have a reply to a retry.
	 */
	if (error == ENOENT)
		error = 0;
	return (error);
}

/*
 * nfs readdir call
 */
static int
nfs_readdir(struct vop_readdir_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);
	struct uio *uio = ap->a_uio;
	int tresid, error = 0;
	struct vattr vattr;
	
	if (vp->v_type != VDIR) 
		return(EPERM);

	/*
	 * First, check for hit on the EOF offset cache
	 */
	if (np->n_direofoffset > 0 && uio->uio_offset >= np->n_direofoffset &&
	    (np->n_flag & NMODIFIED) == 0) {
		if (VOP_GETATTR(vp, &vattr, ap->a_cred) == 0) {
			mtx_lock(&np->n_mtx);
			if (!NFS_TIMESPEC_COMPARE(&np->n_mtime, &vattr.va_mtime)) {
				mtx_unlock(&np->n_mtx);
				nfsstats.direofcache_hits++;
				goto out;
			} else
				mtx_unlock(&np->n_mtx);
		}
	}

	/*
	 * Call nfs_bioread() to do the real work.
	 */
	tresid = uio->uio_resid;
	error = nfs_bioread(vp, uio, 0, ap->a_cred);

	if (!error && uio->uio_resid == tresid) {
		nfsstats.direofcache_misses++;
	}
out:
	return (error);
}

/*
 * Readdir rpc call.
 * Called from below the buffer cache by nfs_doio().
 */
int
nfs_readdirrpc(struct vnode *vp, struct uio *uiop, struct ucred *cred)
{
	int len, left;
	struct dirent *dp = NULL;
	u_int32_t *tl;
	caddr_t cp;
	nfsuint64 *cookiep;
	caddr_t bpos, dpos;
	struct mbuf *mreq, *mrep, *md, *mb;
	nfsuint64 cookie;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	struct nfsnode *dnp = VTONFS(vp);
	u_quad_t fileno;
	int error = 0, tlen, more_dirs = 1, blksiz = 0, bigenough = 1;
	int attrflag;
	int v3 = NFS_ISV3(vp);

	KASSERT(uiop->uio_iovcnt == 1 &&
	    (uiop->uio_offset & (DIRBLKSIZ - 1)) == 0 &&
	    (uiop->uio_resid & (DIRBLKSIZ - 1)) == 0,
	    ("nfs readdirrpc bad uio"));

	/*
	 * If there is no cookie, assume directory was stale.
	 */
	nfs_dircookie_lock(dnp);
	cookiep = nfs_getcookie(dnp, uiop->uio_offset, 0);
	if (cookiep) {
		cookie = *cookiep;
		nfs_dircookie_unlock(dnp);
	} else {
		nfs_dircookie_unlock(dnp);		
		return (NFSERR_BAD_COOKIE);
	}

	/*
	 * Loop around doing readdir rpc's of size nm_readdirsize
	 * truncated to a multiple of DIRBLKSIZ.
	 * The stopping criteria is EOF or buffer full.
	 */
	while (more_dirs && bigenough) {
		nfsstats.rpccnt[NFSPROC_READDIR]++;
		mreq = nfsm_reqhead(vp, NFSPROC_READDIR, NFSX_FH(v3) +
			NFSX_READDIR(v3));
		mb = mreq;
		bpos = mtod(mb, caddr_t);
		nfsm_fhtom(vp, v3);
		if (v3) {
			tl = nfsm_build(u_int32_t *, 5 * NFSX_UNSIGNED);
			*tl++ = cookie.nfsuquad[0];
			*tl++ = cookie.nfsuquad[1];
			mtx_lock(&dnp->n_mtx);
			*tl++ = dnp->n_cookieverf.nfsuquad[0];
			*tl++ = dnp->n_cookieverf.nfsuquad[1];
			mtx_unlock(&dnp->n_mtx);
		} else {
			tl = nfsm_build(u_int32_t *, 2 * NFSX_UNSIGNED);
			*tl++ = cookie.nfsuquad[0];
		}
		*tl = txdr_unsigned(nmp->nm_readdirsize);
		nfsm_request(vp, NFSPROC_READDIR, uiop->uio_td, cred);
		if (v3) {
			nfsm_postop_attr(vp, attrflag);
			if (!error) {
				tl = nfsm_dissect(u_int32_t *,
				    2 * NFSX_UNSIGNED);
				mtx_lock(&dnp->n_mtx);
				dnp->n_cookieverf.nfsuquad[0] = *tl++;
				dnp->n_cookieverf.nfsuquad[1] = *tl;
				mtx_unlock(&dnp->n_mtx);
			} else {
				m_freem(mrep);
				goto nfsmout;
			}
		}
		tl = nfsm_dissect(u_int32_t *, NFSX_UNSIGNED);
		more_dirs = fxdr_unsigned(int, *tl);

		/* loop thru the dir entries, doctoring them to 4bsd form */
		while (more_dirs && bigenough) {
			if (v3) {
				tl = nfsm_dissect(u_int32_t *,
				    3 * NFSX_UNSIGNED);
				fileno = fxdr_hyper(tl);
				len = fxdr_unsigned(int, *(tl + 2));
			} else {
				tl = nfsm_dissect(u_int32_t *,
				    2 * NFSX_UNSIGNED);
				fileno = fxdr_unsigned(u_quad_t, *tl++);
				len = fxdr_unsigned(int, *tl);
			}
			if (len <= 0 || len > NFS_MAXNAMLEN) {
				error = EBADRPC;
				m_freem(mrep);
				goto nfsmout;
			}
			tlen = nfsm_rndup(len);
			if (tlen == len)
				tlen += 4;	/* To ensure null termination */
			left = DIRBLKSIZ - blksiz;
			if ((tlen + DIRHDSIZ) > left) {
				dp->d_reclen += left;
				uiop->uio_iov->iov_base =
				    (char *)uiop->uio_iov->iov_base + left;
				uiop->uio_iov->iov_len -= left;
				uiop->uio_offset += left;
				uiop->uio_resid -= left;
				blksiz = 0;
			}
			if ((tlen + DIRHDSIZ) > uiop->uio_resid)
				bigenough = 0;
			if (bigenough) {
				dp = (struct dirent *)uiop->uio_iov->iov_base;
				dp->d_fileno = (int)fileno;
				dp->d_namlen = len;
				dp->d_reclen = tlen + DIRHDSIZ;
				dp->d_type = DT_UNKNOWN;
				blksiz += dp->d_reclen;
				if (blksiz == DIRBLKSIZ)
					blksiz = 0;
				uiop->uio_offset += DIRHDSIZ;
				uiop->uio_resid -= DIRHDSIZ;
				uiop->uio_iov->iov_base =
				    (char *)uiop->uio_iov->iov_base + DIRHDSIZ;
				uiop->uio_iov->iov_len -= DIRHDSIZ;
				nfsm_mtouio(uiop, len);
				cp = uiop->uio_iov->iov_base;
				tlen -= len;
				*cp = '\0';	/* null terminate */
				uiop->uio_iov->iov_base =
				    (char *)uiop->uio_iov->iov_base + tlen;
				uiop->uio_iov->iov_len -= tlen;
				uiop->uio_offset += tlen;
				uiop->uio_resid -= tlen;
			} else
				nfsm_adv(nfsm_rndup(len));
			if (v3) {
				tl = nfsm_dissect(u_int32_t *,
				    3 * NFSX_UNSIGNED);
			} else {
				tl = nfsm_dissect(u_int32_t *,
				    2 * NFSX_UNSIGNED);
			}
			if (bigenough) {
				cookie.nfsuquad[0] = *tl++;
				if (v3)
					cookie.nfsuquad[1] = *tl++;
			} else if (v3)
				tl += 2;
			else
				tl++;
			more_dirs = fxdr_unsigned(int, *tl);
		}
		/*
		 * If at end of rpc data, get the eof boolean
		 */
		if (!more_dirs) {
			tl = nfsm_dissect(u_int32_t *, NFSX_UNSIGNED);
			more_dirs = (fxdr_unsigned(int, *tl) == 0);
		}
		m_freem(mrep);
	}
	/*
	 * Fill last record, iff any, out to a multiple of DIRBLKSIZ
	 * by increasing d_reclen for the last record.
	 */
	if (blksiz > 0) {
		left = DIRBLKSIZ - blksiz;
		dp->d_reclen += left;
		uiop->uio_iov->iov_base =
		    (char *)uiop->uio_iov->iov_base + left;
		uiop->uio_iov->iov_len -= left;
		uiop->uio_offset += left;
		uiop->uio_resid -= left;
	}

	/*
	 * We are now either at the end of the directory or have filled the
	 * block.
	 */
	if (bigenough)
		dnp->n_direofoffset = uiop->uio_offset;
	else {
		if (uiop->uio_resid > 0)
			nfs_printf("EEK! readdirrpc resid > 0\n");
		nfs_dircookie_lock(dnp);
		cookiep = nfs_getcookie(dnp, uiop->uio_offset, 1);
		*cookiep = cookie;
		nfs_dircookie_unlock(dnp);
	}
nfsmout:
	return (error);
}

/*
 * NFS V3 readdir plus RPC. Used in place of nfs_readdirrpc().
 */
int
nfs_readdirplusrpc(struct vnode *vp, struct uio *uiop, struct ucred *cred)
{
	int len, left;
	struct dirent *dp;
	u_int32_t *tl;
	caddr_t cp;
	struct vnode *newvp;
	nfsuint64 *cookiep;
	caddr_t bpos, dpos, dpossav1, dpossav2;
	struct mbuf *mreq, *mrep, *md, *mb, *mdsav1, *mdsav2;
	struct nameidata nami, *ndp = &nami;
	struct componentname *cnp = &ndp->ni_cnd;
	nfsuint64 cookie;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	struct nfsnode *dnp = VTONFS(vp), *np;
	struct vattr vattr, dvattr;
	nfsfh_t *fhp;
	u_quad_t fileno;
	int error = 0, tlen, more_dirs = 1, blksiz = 0, doit, bigenough = 1, i;
	int attrflag, dattrflag, fhsize;

#ifndef nolint
	dp = NULL;
#endif
	KASSERT(uiop->uio_iovcnt == 1 &&
	    (uiop->uio_offset & (DIRBLKSIZ - 1)) == 0 &&
	    (uiop->uio_resid & (DIRBLKSIZ - 1)) == 0,
	    ("nfs readdirplusrpc bad uio"));
	ndp->ni_dvp = vp;
	newvp = NULLVP;

	/*
	 * If there is no cookie, assume directory was stale.
	 */
	nfs_dircookie_lock(dnp);
	cookiep = nfs_getcookie(dnp, uiop->uio_offset, 0);
	if (cookiep) {
		cookie = *cookiep;
		nfs_dircookie_unlock(dnp);
	} else {
		nfs_dircookie_unlock(dnp);
		return (NFSERR_BAD_COOKIE);
	}
	/*
	 * Loop around doing readdir rpc's of size nm_readdirsize
	 * truncated to a multiple of DIRBLKSIZ.
	 * The stopping criteria is EOF or buffer full.
	 */
	while (more_dirs && bigenough) {
		nfsstats.rpccnt[NFSPROC_READDIRPLUS]++;
		mreq = nfsm_reqhead(vp, NFSPROC_READDIRPLUS,
			NFSX_FH(1) + 6 * NFSX_UNSIGNED);
		mb = mreq;
		bpos = mtod(mb, caddr_t);
		nfsm_fhtom(vp, 1);
 		tl = nfsm_build(u_int32_t *, 6 * NFSX_UNSIGNED);
		*tl++ = cookie.nfsuquad[0];
		*tl++ = cookie.nfsuquad[1];
		mtx_lock(&dnp->n_mtx);
		*tl++ = dnp->n_cookieverf.nfsuquad[0];
		*tl++ = dnp->n_cookieverf.nfsuquad[1];
		mtx_unlock(&dnp->n_mtx);
		*tl++ = txdr_unsigned(nmp->nm_readdirsize);
		*tl = txdr_unsigned(nmp->nm_rsize);
		nfsm_request(vp, NFSPROC_READDIRPLUS, uiop->uio_td, cred);
		nfsm_postop_attr_va(vp, dattrflag, &dvattr);
		if (error) {
			m_freem(mrep);
			goto nfsmout;
		}
		tl = nfsm_dissect(u_int32_t *, 3 * NFSX_UNSIGNED);
		mtx_lock(&dnp->n_mtx);
		dnp->n_cookieverf.nfsuquad[0] = *tl++;
		dnp->n_cookieverf.nfsuquad[1] = *tl++;
		mtx_unlock(&dnp->n_mtx);
		more_dirs = fxdr_unsigned(int, *tl);

		/* loop thru the dir entries, doctoring them to 4bsd form */
		while (more_dirs && bigenough) {
			tl = nfsm_dissect(u_int32_t *, 3 * NFSX_UNSIGNED);
			fileno = fxdr_hyper(tl);
			len = fxdr_unsigned(int, *(tl + 2));
			if (len <= 0 || len > NFS_MAXNAMLEN) {
				error = EBADRPC;
				m_freem(mrep);
				goto nfsmout;
			}
			tlen = nfsm_rndup(len);
			if (tlen == len)
				tlen += 4;	/* To ensure null termination*/
			left = DIRBLKSIZ - blksiz;
			if ((tlen + DIRHDSIZ) > left) {
				dp->d_reclen += left;
				uiop->uio_iov->iov_base =
				    (char *)uiop->uio_iov->iov_base + left;
				uiop->uio_iov->iov_len -= left;
				uiop->uio_offset += left;
				uiop->uio_resid -= left;
				blksiz = 0;
			}
			if ((tlen + DIRHDSIZ) > uiop->uio_resid)
				bigenough = 0;
			if (bigenough) {
				dp = (struct dirent *)uiop->uio_iov->iov_base;
				dp->d_fileno = (int)fileno;
				dp->d_namlen = len;
				dp->d_reclen = tlen + DIRHDSIZ;
				dp->d_type = DT_UNKNOWN;
				blksiz += dp->d_reclen;
				if (blksiz == DIRBLKSIZ)
					blksiz = 0;
				uiop->uio_offset += DIRHDSIZ;
				uiop->uio_resid -= DIRHDSIZ;
				uiop->uio_iov->iov_base =
				    (char *)uiop->uio_iov->iov_base + DIRHDSIZ;
				uiop->uio_iov->iov_len -= DIRHDSIZ;
				cnp->cn_nameptr = uiop->uio_iov->iov_base;
				cnp->cn_namelen = len;
				nfsm_mtouio(uiop, len);
				cp = uiop->uio_iov->iov_base;
				tlen -= len;
				*cp = '\0';
				uiop->uio_iov->iov_base =
				    (char *)uiop->uio_iov->iov_base + tlen;
				uiop->uio_iov->iov_len -= tlen;
				uiop->uio_offset += tlen;
				uiop->uio_resid -= tlen;
			} else
				nfsm_adv(nfsm_rndup(len));
			tl = nfsm_dissect(u_int32_t *, 3 * NFSX_UNSIGNED);
			if (bigenough) {
				cookie.nfsuquad[0] = *tl++;
				cookie.nfsuquad[1] = *tl++;
			} else
				tl += 2;

			/*
			 * Since the attributes are before the file handle
			 * (sigh), we must skip over the attributes and then
			 * come back and get them.
			 */
			attrflag = fxdr_unsigned(int, *tl);
			if (attrflag) {
			    dpossav1 = dpos;
			    mdsav1 = md;
			    nfsm_adv(NFSX_V3FATTR);
			    tl = nfsm_dissect(u_int32_t *, NFSX_UNSIGNED);
			    doit = fxdr_unsigned(int, *tl);
			    /*
 			     * Skip loading the attrs for "..". There's a 
 			     * race between loading the attrs here and 
 			     * lookups that look for the directory currently
 			     * being read (in the parent). We try to acquire
 			     * the exclusive lock on ".." here, owning the 
 			     * lock on the directory being read. Lookup will
 			     * hold the lock on ".." and try to acquire the 
 			     * lock on the directory being read.
 			     * 
 			     * There are other ways of fixing this, one would
 			     * be to do a trylock on the ".." vnode and skip
 			     * loading the attrs on ".." if it happens to be 
 			     * locked by another process. But skipping the
 			     * attrload on ".." seems the easiest option.
 			     */
 			    if (strcmp(dp->d_name, "..") == 0) {
 				    doit = 0;
 				    /*
 				     * We've already skipped over the attrs, 
 				     * skip over the filehandle. And store d_type
 				     * as VDIR.
 				     */
 				    tl = nfsm_dissect(u_int32_t *, NFSX_UNSIGNED);
 				    i = fxdr_unsigned(int, *tl);
 				    nfsm_adv(nfsm_rndup(i));
 				    dp->d_type = IFTODT(VTTOIF(VDIR));
 			    }	    
			    if (doit) {
				nfsm_getfh(fhp, fhsize, 1);
				if (NFS_CMPFH(dnp, fhp, fhsize)) {
				    VREF(vp);
				    newvp = vp;
				    np = dnp;
				} else {
				    error = nfs_nget(vp->v_mount, fhp,
					fhsize, &np, LK_EXCLUSIVE);
				    if (error)
					doit = 0;
				    else
					newvp = NFSTOV(np);
				}
			    }
			    if (doit && bigenough) {
				dpossav2 = dpos;
				dpos = dpossav1;
				mdsav2 = md;
				md = mdsav1;
				nfsm_loadattr(newvp, &vattr);
				dpos = dpossav2;
				md = mdsav2;
				dp->d_type = IFTODT(VTTOIF(vattr.va_type));
				ndp->ni_vp = newvp;
				if (newvp->v_type != VDIR || dattrflag != 0)
				    cache_enter_time(ndp->ni_dvp, ndp->ni_vp,
					cnp, &vattr.va_ctime,
					newvp->v_type != VDIR ? NULL :
					&dvattr.va_ctime);
			    }
			} else {
			    /* Just skip over the file handle */
			    tl = nfsm_dissect(u_int32_t *, NFSX_UNSIGNED);
			    i = fxdr_unsigned(int, *tl);
			    if (i) {
				    tl = nfsm_dissect(u_int32_t *, NFSX_UNSIGNED);
				    fhsize = fxdr_unsigned(int, *tl);
				    nfsm_adv(nfsm_rndup(fhsize));
			    }
			}
			if (newvp != NULLVP) {
			    if (newvp == vp)
				vrele(newvp);
			    else
				vput(newvp);
			    newvp = NULLVP;
			}
			tl = nfsm_dissect(u_int32_t *, NFSX_UNSIGNED);
			more_dirs = fxdr_unsigned(int, *tl);
		}
		/*
		 * If at end of rpc data, get the eof boolean
		 */
		if (!more_dirs) {
			tl = nfsm_dissect(u_int32_t *, NFSX_UNSIGNED);
			more_dirs = (fxdr_unsigned(int, *tl) == 0);
		}
		m_freem(mrep);
	}
	/*
	 * Fill last record, iff any, out to a multiple of DIRBLKSIZ
	 * by increasing d_reclen for the last record.
	 */
	if (blksiz > 0) {
		left = DIRBLKSIZ - blksiz;
		dp->d_reclen += left;
		uiop->uio_iov->iov_base =
		    (char *)uiop->uio_iov->iov_base + left;
		uiop->uio_iov->iov_len -= left;
		uiop->uio_offset += left;
		uiop->uio_resid -= left;
	}

	/*
	 * We are now either at the end of the directory or have filled the
	 * block.
	 */
	if (bigenough)
		dnp->n_direofoffset = uiop->uio_offset;
	else {
		if (uiop->uio_resid > 0)
			nfs_printf("EEK! readdirplusrpc resid > 0\n");
		nfs_dircookie_lock(dnp);
		cookiep = nfs_getcookie(dnp, uiop->uio_offset, 1);
		*cookiep = cookie;
		nfs_dircookie_unlock(dnp);
	}
nfsmout:
	if (newvp != NULLVP) {
	        if (newvp == vp)
			vrele(newvp);
		else
			vput(newvp);
		newvp = NULLVP;
	}
	return (error);
}

/*
 * Silly rename. To make the NFS filesystem that is stateless look a little
 * more like the "ufs" a remove of an active vnode is translated to a rename
 * to a funny looking filename that is removed by nfs_inactive on the
 * nfsnode. There is the potential for another process on a different client
 * to create the same funny name between the nfs_lookitup() fails and the
 * nfs_rename() completes, but...
 */
static int
nfs_sillyrename(struct vnode *dvp, struct vnode *vp, struct componentname *cnp)
{
	struct sillyrename *sp;
	struct nfsnode *np;
	int error;
	short pid;
	unsigned int lticks;

	cache_purge(dvp);
	np = VTONFS(vp);
	KASSERT(vp->v_type != VDIR, ("nfs: sillyrename dir"));
	sp = malloc(sizeof (struct sillyrename),
		M_NFSREQ, M_WAITOK);
	sp->s_cred = crhold(cnp->cn_cred);
	sp->s_dvp = dvp;
	sp->s_removeit = nfs_removeit;
	VREF(dvp);

	/* 
	 * Fudge together a funny name.
	 * Changing the format of the funny name to accomodate more 
	 * sillynames per directory.
	 * The name is now changed to .nfs.<ticks>.<pid>.4, where ticks is 
	 * CPU ticks since boot.
	 */
	pid = cnp->cn_thread->td_proc->p_pid;
	lticks = (unsigned int)ticks;
	for ( ; ; ) {
		sp->s_namlen = sprintf(sp->s_name, 
				       ".nfs.%08x.%04x4.4", lticks, 
				       pid);
		if (nfs_lookitup(dvp, sp->s_name, sp->s_namlen, sp->s_cred,
				 cnp->cn_thread, NULL))
			break;
		lticks++;
	}
	error = nfs_renameit(dvp, cnp, sp);
	if (error)
		goto bad;
	error = nfs_lookitup(dvp, sp->s_name, sp->s_namlen, sp->s_cred,
		cnp->cn_thread, &np);
	np->n_sillyrename = sp;
	return (0);
bad:
	vrele(sp->s_dvp);
	crfree(sp->s_cred);
	free((caddr_t)sp, M_NFSREQ);
	return (error);
}

/*
 * Look up a file name and optionally either update the file handle or
 * allocate an nfsnode, depending on the value of npp.
 * npp == NULL	--> just do the lookup
 * *npp == NULL --> allocate a new nfsnode and make sure attributes are
 *			handled too
 * *npp != NULL --> update the file handle in the vnode
 */
static int
nfs_lookitup(struct vnode *dvp, const char *name, int len, struct ucred *cred,
    struct thread *td, struct nfsnode **npp)
{
	struct vnode *newvp = NULL;
	struct nfsnode *np, *dnp = VTONFS(dvp);
	caddr_t bpos, dpos;
	int error = 0, fhlen, attrflag;
	struct mbuf *mreq, *mrep, *md, *mb;
	nfsfh_t *nfhp;
	int v3 = NFS_ISV3(dvp);

	nfsstats.rpccnt[NFSPROC_LOOKUP]++;
	mreq = nfsm_reqhead(dvp, NFSPROC_LOOKUP,
		NFSX_FH(v3) + NFSX_UNSIGNED + nfsm_rndup(len));
	mb = mreq;
	bpos = mtod(mb, caddr_t);
	nfsm_fhtom(dvp, v3);
	nfsm_strtom(name, len, NFS_MAXNAMLEN);
	nfsm_request(dvp, NFSPROC_LOOKUP, td, cred);
	if (npp && !error) {
		nfsm_getfh(nfhp, fhlen, v3);
		if (*npp) {
		    np = *npp;
		    if (np->n_fhsize > NFS_SMALLFH && fhlen <= NFS_SMALLFH) {
			free((caddr_t)np->n_fhp, M_NFSBIGFH);
			np->n_fhp = &np->n_fh;
		    } else if (np->n_fhsize <= NFS_SMALLFH && fhlen>NFS_SMALLFH)
			np->n_fhp =(nfsfh_t *)malloc(fhlen, M_NFSBIGFH, M_WAITOK);
		    bcopy((caddr_t)nfhp, (caddr_t)np->n_fhp, fhlen);
		    np->n_fhsize = fhlen;
		    newvp = NFSTOV(np);
		} else if (NFS_CMPFH(dnp, nfhp, fhlen)) {
		    VREF(dvp);
		    newvp = dvp;
		} else {
		    error = nfs_nget(dvp->v_mount, nfhp, fhlen, &np, LK_EXCLUSIVE);
		    if (error) {
			m_freem(mrep);
			return (error);
		    }
		    newvp = NFSTOV(np);
		}
		if (v3) {
			nfsm_postop_attr(newvp, attrflag);
			if (!attrflag && *npp == NULL) {
				m_freem(mrep);
				if (newvp == dvp)
					vrele(newvp);
				else
					vput(newvp);
				return (ENOENT);
			}
		} else
			nfsm_loadattr(newvp, NULL);
	}
	m_freem(mrep);
nfsmout:
	if (npp && *npp == NULL) {
		if (error) {
			if (newvp) {
				if (newvp == dvp)
					vrele(newvp);
				else
					vput(newvp);
			}
		} else
			*npp = np;
	}
	return (error);
}

/*
 * Nfs Version 3 commit rpc
 */
int
nfs_commit(struct vnode *vp, u_quad_t offset, int cnt, struct ucred *cred,
	   struct thread *td)
{
	u_int32_t *tl;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	caddr_t bpos, dpos;
	int error = 0, wccflag = NFSV3_WCCRATTR;
	struct mbuf *mreq, *mrep, *md, *mb;

	mtx_lock(&nmp->nm_mtx);
	if ((nmp->nm_state & NFSSTA_HASWRITEVERF) == 0) {
		mtx_unlock(&nmp->nm_mtx);
		return (0);
	}
	mtx_unlock(&nmp->nm_mtx);
	nfsstats.rpccnt[NFSPROC_COMMIT]++;
	mreq = nfsm_reqhead(vp, NFSPROC_COMMIT, NFSX_FH(1));
	mb = mreq;
	bpos = mtod(mb, caddr_t);
	nfsm_fhtom(vp, 1);
	tl = nfsm_build(u_int32_t *, 3 * NFSX_UNSIGNED);
	txdr_hyper(offset, tl);
	tl += 2;
	*tl = txdr_unsigned(cnt);
	nfsm_request(vp, NFSPROC_COMMIT, td, cred);
	nfsm_wcc_data(vp, wccflag);
	if (!error) {
		tl = nfsm_dissect(u_int32_t *, NFSX_V3WRITEVERF);
		if (bcmp((caddr_t)nmp->nm_verf, (caddr_t)tl,
			NFSX_V3WRITEVERF)) {
			bcopy((caddr_t)tl, (caddr_t)nmp->nm_verf,
				NFSX_V3WRITEVERF);
			error = NFSERR_STALEWRITEVERF;
		}
	}
	m_freem(mrep);
nfsmout:
	return (error);
}

/*
 * Strategy routine.
 * For async requests when nfsiod(s) are running, queue the request by
 * calling nfs_asyncio(), otherwise just all nfs_doio() to do the
 * request.
 */
static int
nfs_strategy(struct vop_strategy_args *ap)
{
	struct buf *bp = ap->a_bp;
	struct ucred *cr;

	KASSERT(!(bp->b_flags & B_DONE),
	    ("nfs_strategy: buffer %p unexpectedly marked B_DONE", bp));
	BUF_ASSERT_HELD(bp);

	if (bp->b_iocmd == BIO_READ)
		cr = bp->b_rcred;
	else
		cr = bp->b_wcred;

	/*
	 * If the op is asynchronous and an i/o daemon is waiting
	 * queue the request, wake it up and wait for completion
	 * otherwise just do it ourselves.
	 */
	if ((bp->b_flags & B_ASYNC) == 0 ||
	    nfs_asyncio(VFSTONFS(ap->a_vp->v_mount), bp, NOCRED, curthread))
		(void)nfs_doio(ap->a_vp, bp, cr, curthread);
	return (0);
}

/*
 * fsync vnode op. Just call nfs_flush() with commit == 1.
 */
/* ARGSUSED */
static int
nfs_fsync(struct vop_fsync_args *ap)
{

	return (nfs_flush(ap->a_vp, ap->a_waitfor, 1));
}

/*
 * Flush all the blocks associated with a vnode.
 * 	Walk through the buffer pool and push any dirty pages
 *	associated with the vnode.
 */
static int
nfs_flush(struct vnode *vp, int waitfor, int commit)
{
	struct nfsnode *np = VTONFS(vp);
	struct buf *bp;
	int i;
	struct buf *nbp;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	int error = 0, slptimeo = 0, slpflag = 0, retv, bvecpos;
	int passone = 1;
	u_quad_t off, endoff, toff;
	struct ucred* wcred = NULL;
	struct buf **bvec = NULL;
	struct bufobj *bo;
	struct thread *td = curthread;
#ifndef NFS_COMMITBVECSIZ
#define NFS_COMMITBVECSIZ	20
#endif
	struct buf *bvec_on_stack[NFS_COMMITBVECSIZ];
	int bvecsize = 0, bveccount;

	if (nmp->nm_flag & NFSMNT_INT)
		slpflag = NFS_PCATCH;
	if (!commit)
		passone = 0;
	bo = &vp->v_bufobj;
	/*
	 * A b_flags == (B_DELWRI | B_NEEDCOMMIT) block has been written to the
	 * server, but has not been committed to stable storage on the server
	 * yet. On the first pass, the byte range is worked out and the commit
	 * rpc is done. On the second pass, nfs_writebp() is called to do the
	 * job.
	 */
again:
	off = (u_quad_t)-1;
	endoff = 0;
	bvecpos = 0;
	if (NFS_ISV3(vp) && commit) {
		if (bvec != NULL && bvec != bvec_on_stack)
			free(bvec, M_TEMP);
		/*
		 * Count up how many buffers waiting for a commit.
		 */
		bveccount = 0;
		BO_LOCK(bo);
		TAILQ_FOREACH_SAFE(bp, &bo->bo_dirty.bv_hd, b_bobufs, nbp) {
			if (!BUF_ISLOCKED(bp) &&
			    (bp->b_flags & (B_DELWRI | B_NEEDCOMMIT))
				== (B_DELWRI | B_NEEDCOMMIT))
				bveccount++;
		}
		/*
		 * Allocate space to remember the list of bufs to commit.  It is
		 * important to use M_NOWAIT here to avoid a race with nfs_write.
		 * If we can't get memory (for whatever reason), we will end up
		 * committing the buffers one-by-one in the loop below.
		 */
		if (bveccount > NFS_COMMITBVECSIZ) {
			/*
			 * Release the vnode interlock to avoid a lock
			 * order reversal.
			 */
			BO_UNLOCK(bo);
			bvec = (struct buf **)
				malloc(bveccount * sizeof(struct buf *),
				       M_TEMP, M_NOWAIT);
			BO_LOCK(bo);
			if (bvec == NULL) {
				bvec = bvec_on_stack;
				bvecsize = NFS_COMMITBVECSIZ;
			} else
				bvecsize = bveccount;
		} else {
			bvec = bvec_on_stack;
			bvecsize = NFS_COMMITBVECSIZ;
		}
		TAILQ_FOREACH_SAFE(bp, &bo->bo_dirty.bv_hd, b_bobufs, nbp) {
			if (bvecpos >= bvecsize)
				break;
			if (BUF_LOCK(bp, LK_EXCLUSIVE | LK_NOWAIT, NULL)) {
				nbp = TAILQ_NEXT(bp, b_bobufs);
				continue;
			}
			if ((bp->b_flags & (B_DELWRI | B_NEEDCOMMIT)) !=
			    (B_DELWRI | B_NEEDCOMMIT)) {
				BUF_UNLOCK(bp);
				nbp = TAILQ_NEXT(bp, b_bobufs);
				continue;
			}
			BO_UNLOCK(bo);
			bremfree(bp);
			/*
			 * Work out if all buffers are using the same cred
			 * so we can deal with them all with one commit.
			 *
			 * NOTE: we are not clearing B_DONE here, so we have
			 * to do it later on in this routine if we intend to
			 * initiate I/O on the bp.
			 *
			 * Note: to avoid loopback deadlocks, we do not
			 * assign b_runningbufspace.
			 */
			if (wcred == NULL)
				wcred = bp->b_wcred;
			else if (wcred != bp->b_wcred)
				wcred = NOCRED;
			vfs_busy_pages(bp, 1);

			BO_LOCK(bo);
			/*
			 * bp is protected by being locked, but nbp is not
			 * and vfs_busy_pages() may sleep.  We have to
			 * recalculate nbp.
			 */
			nbp = TAILQ_NEXT(bp, b_bobufs);

			/*
			 * A list of these buffers is kept so that the
			 * second loop knows which buffers have actually
			 * been committed. This is necessary, since there
			 * may be a race between the commit rpc and new
			 * uncommitted writes on the file.
			 */
			bvec[bvecpos++] = bp;
			toff = ((u_quad_t)bp->b_blkno) * DEV_BSIZE +
				bp->b_dirtyoff;
			if (toff < off)
				off = toff;
			toff += (u_quad_t)(bp->b_dirtyend - bp->b_dirtyoff);
			if (toff > endoff)
				endoff = toff;
		}
		BO_UNLOCK(bo);
	}
	if (bvecpos > 0) {
		/*
		 * Commit data on the server, as required.
		 * If all bufs are using the same wcred, then use that with
		 * one call for all of them, otherwise commit each one
		 * separately.
		 */
		if (wcred != NOCRED)
			retv = nfs_commit(vp, off, (int)(endoff - off),
					  wcred, td);
		else {
			retv = 0;
			for (i = 0; i < bvecpos; i++) {
				off_t off, size;
				bp = bvec[i];
				off = ((u_quad_t)bp->b_blkno) * DEV_BSIZE +
					bp->b_dirtyoff;
				size = (u_quad_t)(bp->b_dirtyend
						  - bp->b_dirtyoff);
				retv = nfs_commit(vp, off, (int)size,
						  bp->b_wcred, td);
				if (retv) break;
			}
		}

		if (retv == NFSERR_STALEWRITEVERF)
			nfs_clearcommit(vp->v_mount);

		/*
		 * Now, either mark the blocks I/O done or mark the
		 * blocks dirty, depending on whether the commit
		 * succeeded.
		 */
		for (i = 0; i < bvecpos; i++) {
			bp = bvec[i];
			bp->b_flags &= ~(B_NEEDCOMMIT | B_CLUSTEROK);
			if (retv) {
				/*
				 * Error, leave B_DELWRI intact
				 */
				vfs_unbusy_pages(bp);
				brelse(bp);
			} else {
				/*
				 * Success, remove B_DELWRI ( bundirty() ).
				 *
				 * b_dirtyoff/b_dirtyend seem to be NFS
				 * specific.  We should probably move that
				 * into bundirty(). XXX
				 */
				bufobj_wref(bo);
				bp->b_flags |= B_ASYNC;
				bundirty(bp);
				bp->b_flags &= ~B_DONE;
				bp->b_ioflags &= ~BIO_ERROR;
				bp->b_dirtyoff = bp->b_dirtyend = 0;
				bufdone(bp);
			}
		}
	}

	/*
	 * Start/do any write(s) that are required.
	 */
loop:
	BO_LOCK(bo);
	TAILQ_FOREACH_SAFE(bp, &bo->bo_dirty.bv_hd, b_bobufs, nbp) {
		if (BUF_LOCK(bp, LK_EXCLUSIVE | LK_NOWAIT, NULL)) {
			if (waitfor != MNT_WAIT || passone)
				continue;

			error = BUF_TIMELOCK(bp,
			    LK_EXCLUSIVE | LK_SLEEPFAIL | LK_INTERLOCK,
			    BO_MTX(bo), "nfsfsync", slpflag, slptimeo);
			if (error == 0) {
				BUF_UNLOCK(bp);
				goto loop;
			}
			if (error == ENOLCK) {
				error = 0;
				goto loop;
			}
			if (nfs_sigintr(nmp, td)) {
				error = EINTR;
				goto done;
			}
			if (slpflag & PCATCH) {
				slpflag = 0;
				slptimeo = 2 * hz;
			}
			goto loop;
		}
		if ((bp->b_flags & B_DELWRI) == 0)
			panic("nfs_fsync: not dirty");
		if ((passone || !commit) && (bp->b_flags & B_NEEDCOMMIT)) {
			BUF_UNLOCK(bp);
			continue;
		}
		BO_UNLOCK(bo);
		bremfree(bp);
		if (passone || !commit)
		    bp->b_flags |= B_ASYNC;
		else
		    bp->b_flags |= B_ASYNC;
		bwrite(bp);
		if (nfs_sigintr(nmp, td)) {
			error = EINTR;
			goto done;
		}
		goto loop;
	}
	if (passone) {
		passone = 0;
		BO_UNLOCK(bo);
		goto again;
	}
	if (waitfor == MNT_WAIT) {
		while (bo->bo_numoutput) {
			error = bufobj_wwait(bo, slpflag, slptimeo);
			if (error) {
			    BO_UNLOCK(bo);
			    error = nfs_sigintr(nmp, td);
			    if (error)
				goto done;
			    if (slpflag & PCATCH) {
				slpflag = 0;
				slptimeo = 2 * hz;
			    }
			    BO_LOCK(bo);
			}
		}
		if (bo->bo_dirty.bv_cnt != 0 && commit) {
			BO_UNLOCK(bo);
			goto loop;
		}
		/*
		 * Wait for all the async IO requests to drain
		 */
		BO_UNLOCK(bo);
		mtx_lock(&np->n_mtx);
		while (np->n_directio_asyncwr > 0) {
			np->n_flag |= NFSYNCWAIT;
			error = nfs_msleep(td, (caddr_t)&np->n_directio_asyncwr,
					   &np->n_mtx, slpflag | (PRIBIO + 1), 
					   "nfsfsync", 0);
			if (error) {
				if (nfs_sigintr(nmp, td)) {
					mtx_unlock(&np->n_mtx);
					error = EINTR;	
					goto done;
				}
			}
		}
		mtx_unlock(&np->n_mtx);
	} else
		BO_UNLOCK(bo);
	mtx_lock(&np->n_mtx);
	if (np->n_flag & NWRITEERR) {
		error = np->n_error;
		np->n_flag &= ~NWRITEERR;
	}
  	if (commit && bo->bo_dirty.bv_cnt == 0 &&
	    bo->bo_numoutput == 0 && np->n_directio_asyncwr == 0)
  		np->n_flag &= ~NMODIFIED;
	mtx_unlock(&np->n_mtx);
done:
	if (bvec != NULL && bvec != bvec_on_stack)
		free(bvec, M_TEMP);
	return (error);
}

/*
 * NFS advisory byte-level locks.
 */
static int
nfs_advlock(struct vop_advlock_args *ap)
{
	struct vnode *vp = ap->a_vp;
	u_quad_t size;
	int error;

	error = vn_lock(vp, LK_SHARED);
	if (error)
		return (error);
	if ((VFSTONFS(vp->v_mount)->nm_flag & NFSMNT_NOLOCKD) != 0) {
		size = VTONFS(vp)->n_size;
		VOP_UNLOCK(vp, 0);
		error = lf_advlock(ap, &(vp->v_lockf), size);
	} else {
		if (nfs_advlock_p)
			error = nfs_advlock_p(ap);
		else
			error = ENOLCK;
	}

	return (error);
}

/*
 * NFS advisory byte-level locks.
 */
static int
nfs_advlockasync(struct vop_advlockasync_args *ap)
{
	struct vnode *vp = ap->a_vp;
	u_quad_t size;
	int error;
	
	error = vn_lock(vp, LK_SHARED);
	if (error)
		return (error);
	if ((VFSTONFS(vp->v_mount)->nm_flag & NFSMNT_NOLOCKD) != 0) {
		size = VTONFS(vp)->n_size;
		VOP_UNLOCK(vp, 0);
		error = lf_advlockasync(ap, &(vp->v_lockf), size);
	} else {
		VOP_UNLOCK(vp, 0);
		error = EOPNOTSUPP;
	}
	return (error);
}

/*
 * Print out the contents of an nfsnode.
 */
static int
nfs_print(struct vop_print_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);

	nfs_printf("\tfileid %ld fsid 0x%x",
	   np->n_vattr.va_fileid, np->n_vattr.va_fsid);
	if (vp->v_type == VFIFO)
		fifo_printinfo(vp);
	printf("\n");
	return (0);
}

/*
 * This is the "real" nfs::bwrite(struct buf*).
 * We set B_CACHE if this is a VMIO buffer.
 */
int
nfs_writebp(struct buf *bp, int force __unused, struct thread *td)
{
	int s;
	int oldflags = bp->b_flags;
#if 0
	int retv = 1;
	off_t off;
#endif

	BUF_ASSERT_HELD(bp);

	if (bp->b_flags & B_INVAL) {
		brelse(bp);
		return(0);
	}

	bp->b_flags |= B_CACHE;

	/*
	 * Undirty the bp.  We will redirty it later if the I/O fails.
	 */

	s = splbio();
	bundirty(bp);
	bp->b_flags &= ~B_DONE;
	bp->b_ioflags &= ~BIO_ERROR;
	bp->b_iocmd = BIO_WRITE;

	bufobj_wref(bp->b_bufobj);
	curthread->td_ru.ru_oublock++;
	splx(s);

	/*
	 * Note: to avoid loopback deadlocks, we do not
	 * assign b_runningbufspace.
	 */
	vfs_busy_pages(bp, 1);

	BUF_KERNPROC(bp);
	bp->b_iooffset = dbtob(bp->b_blkno);
	bstrategy(bp);

	if( (oldflags & B_ASYNC) == 0) {
		int rtval = bufwait(bp);

		if (oldflags & B_DELWRI) {
			s = splbio();
			reassignbuf(bp);
			splx(s);
		}
		brelse(bp);
		return (rtval);
	}

	return (0);
}

/*
 * nfs special file access vnode op.
 * Essentially just get vattr and then imitate iaccess() since the device is
 * local to the client.
 */
static int
nfsspec_access(struct vop_access_args *ap)
{
	struct vattr *vap;
	struct ucred *cred = ap->a_cred;
	struct vnode *vp = ap->a_vp;
	accmode_t accmode = ap->a_accmode;
	struct vattr vattr;
	int error;

	/*
	 * Disallow write attempts on filesystems mounted read-only;
	 * unless the file is a socket, fifo, or a block or character
	 * device resident on the filesystem.
	 */
	if ((accmode & VWRITE) && (vp->v_mount->mnt_flag & MNT_RDONLY)) {
		switch (vp->v_type) {
		case VREG:
		case VDIR:
		case VLNK:
			return (EROFS);
		default:
			break;
		}
	}
	vap = &vattr;
	error = VOP_GETATTR(vp, vap, cred);
	if (error)
		goto out;
	error  = vaccess(vp->v_type, vap->va_mode, vap->va_uid, vap->va_gid,
			 accmode, cred, NULL);
out:
	return error;
}

/*
 * Read wrapper for fifos.
 */
static int
nfsfifo_read(struct vop_read_args *ap)
{
	struct nfsnode *np = VTONFS(ap->a_vp);
	int error;

	/*
	 * Set access flag.
	 */
	mtx_lock(&np->n_mtx);
	np->n_flag |= NACC;
	getnanotime(&np->n_atim);
	mtx_unlock(&np->n_mtx);
	error = fifo_specops.vop_read(ap);
	return error;	
}

/*
 * Write wrapper for fifos.
 */
static int
nfsfifo_write(struct vop_write_args *ap)
{
	struct nfsnode *np = VTONFS(ap->a_vp);

	/*
	 * Set update flag.
	 */
	mtx_lock(&np->n_mtx);
	np->n_flag |= NUPD;
	getnanotime(&np->n_mtim);
	mtx_unlock(&np->n_mtx);
	return(fifo_specops.vop_write(ap));
}

/*
 * Close wrapper for fifos.
 *
 * Update the times on the nfsnode then do fifo close.
 */
static int
nfsfifo_close(struct vop_close_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);
	struct vattr vattr;
	struct timespec ts;

	mtx_lock(&np->n_mtx);
	if (np->n_flag & (NACC | NUPD)) {
		getnanotime(&ts);
		if (np->n_flag & NACC)
			np->n_atim = ts;
		if (np->n_flag & NUPD)
			np->n_mtim = ts;
		np->n_flag |= NCHG;
		if (vrefcnt(vp) == 1 &&
		    (vp->v_mount->mnt_flag & MNT_RDONLY) == 0) {
			VATTR_NULL(&vattr);
			if (np->n_flag & NACC)
				vattr.va_atime = np->n_atim;
			if (np->n_flag & NUPD)
				vattr.va_mtime = np->n_mtim;
			mtx_unlock(&np->n_mtx);
			(void)VOP_SETATTR(vp, &vattr, ap->a_cred);
			goto out;
		}
	}
	mtx_unlock(&np->n_mtx);
out:
	return (fifo_specops.vop_close(ap));
}

/*
 * Just call nfs_writebp() with the force argument set to 1.
 *
 * NOTE: B_DONE may or may not be set in a_bp on call.
 */
static int
nfs_bwrite(struct buf *bp)
{

	return (nfs_writebp(bp, 1, curthread));
}

struct buf_ops buf_ops_nfs = {
	.bop_name	=	"buf_ops_nfs",
	.bop_write	=	nfs_bwrite,
	.bop_strategy	=	bufstrategy,
	.bop_sync	=	bufsync,
	.bop_bdflush	=	bufbdflush,
};
