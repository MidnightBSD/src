/*	$NetBSD: osf1_mount.c,v 1.7 1998/05/20 16:34:29 chs Exp $	*/
/*-
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */
/*
 * Additional Copyright (c) 1999 by Andrew Gallatin
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/alpha/osf1/osf1_mount.c,v 1.25 2005/06/11 14:58:20 pjd Exp $");

#include "opt_mac.h"
#include "opt_nfs.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/vnode.h>
#include <sys/mac.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/syscallsubr.h>
#include <sys/namei.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <nfs/xdr_subs.h>
#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <rpc/rpcclnt.h>
#include <nfsclient/nfs.h>
#include <nfsclient/nfsmount.h>
#include <nfsclient/nfsargs.h>

#include <sys/sysent.h>
#include <alpha/osf1/osf1_signal.h>
#include <alpha/osf1/osf1_proto.h>
#include <alpha/osf1/osf1_syscall.h>
#include <alpha/osf1/osf1_util.h>
#include <alpha/osf1/osf1.h>


void bsd2osf_statfs(struct statfs *, struct osf1_statfs *);
int osf1_mount_mfs(struct thread *, struct osf1_mount_args *,
			struct mount_args *);
int osf1_mount_nfs(struct thread *, struct osf1_mount_args *,
			struct mount_args *);

#ifdef notanymore
static const	char *fsnames[OSF1_MOUNT_MAXTYPE+2] = INITMOUNTNAMES;
#endif

void
bsd2osf_statfs(bsfs, osfs)
	struct statfs *bsfs;
	struct osf1_statfs *osfs;
{

#ifdef notanymore	
bzero(osfs, sizeof (struct osf1_statfs));
	if (!strncmp(fsnames[MOUNT_UFS], bsfs->f_fstypename, MFSNAMELEN))
		osfs->f_type = OSF1_MOUNT_UFS;
	else if (!strncmp(fsnames[MOUNT_NFS], bsfs->f_fstypename, MFSNAMELEN))
		osfs->f_type = OSF1_MOUNT_NFS;
	else if (!strncmp(fsnames[MOUNT_MFS], bsfs->f_fstypename, MFSNAMELEN))
		osfs->f_type = OSF1_MOUNT_MFS;
	else
		/* uh oh...  XXX = PC, CDFS, PROCFS, etc. */
		osfs->f_type = OSF1_MOUNT_ADDON;
	osfs->f_flags = bsfs->f_flags;		/* XXX translate */
	osfs->f_fsize = bsfs->f_bsize;
	osfs->f_bsize = bsfs->f_iosize;
	osfs->f_blocks = bsfs->f_blocks;
	osfs->f_bfree = bsfs->f_bfree;
	osfs->f_bavail = bsfs->f_bavail;
	osfs->f_files = bsfs->f_files;
	osfs->f_ffree = bsfs->f_ffree;
	bcopy(&bsfs->f_fsid, &osfs->f_fsid,
	    max(sizeof bsfs->f_fsid, sizeof osfs->f_fsid));
	/* osfs->f_spare zeroed above */
	bcopy(bsfs->f_mntonname, osfs->f_mntonname,
	    max(sizeof bsfs->f_mntonname, sizeof osfs->f_mntonname));
	bcopy(bsfs->f_mntfromname, osfs->f_mntfromname,
	    max(sizeof bsfs->f_mntfromname, sizeof osfs->f_mntfromname));
	/* XXX osfs->f_xxx should be filled in... */
#endif
}

int
osf1_statfs(td, uap)
	struct thread *td;
	struct osf1_statfs_args *uap;
{
	struct osf1_statfs osfs;
	struct statfs sf;
	int error;

	error = kern_statfs(td, uap->path, UIO_USERSPACE, &sf);
	if (error)
		return (error);
	bsd2osf_statfs(&sf, &osfs);
	return (copyout(&osfs, uap->buf, min(sizeof osfs, uap->len)));
}

int
osf1_fstatfs(td, uap)
	struct thread *td;
	struct osf1_fstatfs_args *uap;
{
	struct osf1_statfs osfs;
	struct statfs sf;
	int error;

	error = kern_fstatfs(td, uap->fd, &sf);
	if (error)
		return (error);
	bsd2osf_statfs(&sf, &osfs);
	return (copyout(&osfs, uap->buf, min(sizeof osfs, uap->len)));
}

int
osf1_getfsstat(td, uap)
	struct thread *td;
	register struct osf1_getfsstat_args *uap;
{
	struct statfs *buf, *sp;
	struct osf1_statfs osfs;
	size_t count, size;
	int error, flags;

	if (uap->flags & ~OSF1_GETFSSTAT_FLAGS)
		return (EINVAL);
	flags = 0;
	if (uap->flags & OSF1_MNT_WAIT)
		flags |= MNT_WAIT;
	if (uap->flags & OSF1_MNT_NOWAIT)
		flags |= MNT_NOWAIT;

	count = uap->bufsize / sizeof(struct osf1_statfs);
	size = count * sizeof(struct statfs);
	error = kern_getfsstat(td, &buf, size, UIO_SYSSPACE, flags);
	if (size > 0) {
		count = td->td_retval[0];
		sp = buf;
		while (count > 0 && error == 0) {
			bsd2osf_statfs(sp, &osfs);
			error = copyout(&osfs, uap->buf, sizeof(osfs));
			sp++;
			uap->buf++;
			count--;
		}
		free(buf, M_TEMP);
	}
	return (error);
}

int
osf1_unmount(td, uap)
	struct thread *td;
	struct osf1_unmount_args *uap;
{
	struct unmount_args a;

	a.path = uap->path;

	if (uap->flags & ~OSF1_UNMOUNT_FLAGS)
		return (EINVAL);
	a.flags = 0;
	if ((uap->flags & OSF1_MNT_FORCE) &&
	    (uap->flags & OSF1_MNT_NOFORCE) == 0)
		a.flags |= MNT_FORCE;

	return unmount(td, &a);
}

int
osf1_mount(td, uap)
	struct thread *td;
	struct osf1_mount_args *uap;
{
	int error;
	struct mount_args a;

	a.path = uap->path;

	if (uap->flags & ~OSF1_MOUNT_FLAGS)
		return (EINVAL);
	a.flags = uap->flags;		/* XXX - xlate */

	switch (uap->type) {
	case OSF1_MOUNT_UFS:				/* XXX */
		return (EINVAL);
		break;

	case OSF1_MOUNT_NFS:				/* XXX */
		if ((error = osf1_mount_nfs(td, uap, &a)))
			return error;
		break;

	case OSF1_MOUNT_MFS:				/* XXX */
#ifdef notyet
		if ((error = osf1_mount_mfs(td, uap, &a)))
			return error;
#endif
		return EINVAL;
		break;

	case OSF1_MOUNT_CDFS:				/* XXX */
		return (EINVAL);
		break;

	case OSF1_MOUNT_PROCFS:				/* XXX */
		return (EINVAL);
		break;

	case OSF1_MOUNT_NONE:
	case OSF1_MOUNT_PC:
	case OSF1_MOUNT_S5FS:
	case OSF1_MOUNT_DFS:
	case OSF1_MOUNT_EFS:
	case OSF1_MOUNT_MSFS:
	case OSF1_MOUNT_FFM:
	case OSF1_MOUNT_FDFS:
	case OSF1_MOUNT_ADDON:
	default:
		return (EINVAL);
	}

	return mount(td, &a);
}

int
osf1_mount_mfs(td, osf_argp, bsd_argp)
	struct thread *td;
	struct osf1_mount_args *osf_argp;
	struct mount_args *bsd_argp;
{
#ifdef notyet
	int error, len;
	caddr_t sg;
	static const char mfs_name[] = "mfs";
	struct osf1_mfs_args osf_ma;
	struct mfs_args bsd_ma;

	sg = stackgap_init();

	if ((error = copyin(osf_argp->data, &osf_ma, sizeof osf_ma)))
		return error;

	bzero(&bsd_ma, sizeof bsd_ma);
	bsd_ma.fspec = osf_ma.name;
	/* XXX export args */
	bsd_ma.base = osf_ma.base;
	bsd_ma.size = osf_ma.size;

	bsd_argp->data = stackgap_alloc(&sg, sizeof bsd_ma);
	if ((error = copyout(&bsd_ma, bsd_argp->data, sizeof bsd_ma)))
		return error;

	len = strlen(mfs_name) + 1;
	bsd_argp->type = stackgap_alloc(&sg, len);
	if ((error = copyout(mfs_name, (void *)bsd_argp->type, len)))
		return error;
#endif
	return 0;
}

int
osf1_mount_nfs(td, osf_argp, bsd_argp)
	struct thread *td;
	struct osf1_mount_args *osf_argp;
	struct mount_args *bsd_argp;
{
	int error, len;
	caddr_t sg;
	static const char nfs_name[] = "nfs";
	struct osf1_nfs_args osf_na;
	struct nfs_args bsd_na;

	sg = stackgap_init();

	if ((error = copyin(osf_argp->data, &osf_na, sizeof osf_na)))
		return error;

	bzero(&bsd_na, sizeof bsd_na);
	bsd_na.addr = (struct sockaddr *)osf_na.addr;
	bsd_na.addrlen = sizeof (struct sockaddr_in);
	bsd_na.sotype = SOCK_DGRAM;
	bsd_na.proto = 0;
	bsd_na.fh = osf_na.fh;

	if (osf_na.flags & ~OSF1_NFSMNT_FLAGS)
		return EINVAL;
	if (osf_na.flags & OSF1_NFSMNT_SOFT)
		bsd_na.flags |= NFSMNT_SOFT;
	if (osf_na.flags & OSF1_NFSMNT_WSIZE) {
		bsd_na.wsize = osf_na.wsize;
		bsd_na.flags |= NFSMNT_WSIZE;
	}
	if (osf_na.flags & OSF1_NFSMNT_RSIZE) {
		bsd_na.rsize = osf_na.rsize;
		bsd_na.flags |= NFSMNT_RSIZE;
	}
	if (osf_na.flags & OSF1_NFSMNT_TIMEO) {
		bsd_na.timeo = osf_na.timeo;
		bsd_na.flags |= NFSMNT_TIMEO;
	}
	if (osf_na.flags & OSF1_NFSMNT_RETRANS) {
		bsd_na.retrans = osf_na.retrans;
		bsd_na.flags |= NFSMNT_RETRANS;
	}
	if (osf_na.flags & OSF1_NFSMNT_HOSTNAME)
		bsd_na.hostname = osf_na.hostname;
	if (osf_na.flags & OSF1_NFSMNT_INT)
		bsd_na.flags |= NFSMNT_INT;
	if (osf_na.flags & OSF1_NFSMNT_NOCONN)
		bsd_na.flags |= NFSMNT_NOCONN;

	bsd_argp->data = stackgap_alloc(&sg, sizeof bsd_na);
	if ((error = copyout(&bsd_na, bsd_argp->data, sizeof bsd_na)))
		return error;

	len = strlen(nfs_name) + 1;
	bsd_argp->type = stackgap_alloc(&sg, len);
	if ((error = copyout(nfs_name, (void *)bsd_argp->type, len)))
		return error;

	return 0;
}
