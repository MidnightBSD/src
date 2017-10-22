/*
 * Copyright (c) 2001,2006 Alexander Kabaev, Russell Cattelan Digital Elves Inc.
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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>

#include <geom/geom.h>
#include <geom/geom_vfs.h>

#include "xfs.h"
#include "xfs_types.h"
#include "xfs_bit.h"
#include "xfs_inum.h"
#include "xfs_log.h"
#include "xfs_trans.h"
#include "xfs_sb.h"
#include "xfs_ag.h"
#include "xfs_dir.h"
#include "xfs_dir2.h"
#include "xfs_dmapi.h"
#include "xfs_mount.h"
#include "xfs_alloc_btree.h"
#include "xfs_bmap_btree.h"
#include "xfs_ialloc_btree.h"
#include "xfs_btree.h"
#include "xfs_attr_sf.h"
#include "xfs_dir_sf.h"
#include "xfs_dir2_sf.h"
#include "xfs_dinode.h"
#include "xfs_ialloc.h"
#include "xfs_inode.h"
#include "xfs_alloc.h"
#include "xfs_rtalloc.h"
#include "xfs_bmap.h"
#include "xfs_error.h"
#include "xfs_rw.h"
#include "xfs_quota.h"
#include "xfs_fsops.h"
#include "xfs_clnt.h"

#include <xfs_mountops.h>

MALLOC_DEFINE(M_XFSNODE, "XFS node", "XFS vnode private part");

static vfs_mount_t	_xfs_mount;
static vfs_unmount_t	_xfs_unmount;
static vfs_root_t	_xfs_root;
static vfs_quotactl_t	_xfs_quotactl;
static vfs_statfs_t	_xfs_statfs;
static vfs_sync_t	_xfs_sync;
static vfs_vget_t	_xfs_vget;
static vfs_fhtovp_t	_xfs_fhtovp;
static vfs_init_t	_xfs_init;
static vfs_uninit_t	_xfs_uninit;
static vfs_extattrctl_t	_xfs_extattrctl;

static b_strategy_t	xfs_geom_strategy;

static const char *xfs_opts[] =
	{ "from", "flags", "logbufs", "logbufsize",
	  "rtname", "logname", "iosizelog", "sunit",
	  "swidth", "export",
	  NULL };

static void
parse_int(struct mount *mp, const char *opt, int *val, int *error)
{
	char *tmp, *ep;

	tmp = vfs_getopts(mp->mnt_optnew, opt, error);
	if (*error != 0) {
		return;
	}
	if (tmp != NULL) {
		*val = (int)strtol(tmp, &ep, 10);
		if (*ep) {
			*error = EINVAL;
			return;
		}
	}
}

static int
_xfs_param_copyin(struct mount *mp, struct thread *td)
{
	struct xfsmount *xmp = MNTTOXFS(mp);
	struct xfs_mount_args *args = &xmp->m_args;
	char *path;
	char *fsname;
	char *rtname;
	char *logname;
	int error;

	path = vfs_getopts(mp->mnt_optnew, "fspath", &error);
	if  (error)
		return (error);

	bzero(args, sizeof(struct xfs_mount_args));
	args->logbufs = -1;
	args->logbufsize = -1;

	parse_int(mp, "flags", &args->flags, &error);
	if (error != 0 && error != ENOENT)
		return error;

	args->flags |= XFSMNT_32BITINODES;

	parse_int(mp, "sunit", &args->sunit, &error);
	if (error != 0 && error != ENOENT)
		return error;

	parse_int(mp, "swidth", &args->swidth, &error);
	if (error != 0 && error != ENOENT)
		return error;

	parse_int(mp, "logbufs", &args->logbufs, &error);
	if (error != 0 && error != ENOENT)
		return error;

	parse_int(mp, "logbufsize", &args->logbufsize, &error);
	if (error != 0 && error != ENOENT)
		return error;

	fsname = vfs_getopts(mp->mnt_optnew, "from", &error);
	if (error == 0 && fsname != NULL) {
		strncpy(args->fsname, fsname, sizeof(args->fsname) - 1);
	}

	logname = vfs_getopts(mp->mnt_optnew, "logname", &error);
	if (error == 0 && logname != NULL) {
		strncpy(args->logname, logname, sizeof(args->logname) - 1);
	}

	rtname = vfs_getopts(mp->mnt_optnew, "rtname", &error);
	if (error == 0 && rtname != NULL) {
		strncpy(args->rtname, rtname, sizeof(args->rtname) - 1);
	}

	strncpy(args->mtpt, path, sizeof(args->mtpt));

	printf("fsname '%s' logname '%s' rtname '%s'\n"
	       "flags 0x%x sunit %d swidth %d logbufs %d logbufsize %d\n",
	       args->fsname, args->logname, args->rtname, args->flags,
	       args->sunit, args->swidth, args->logbufs, args->logbufsize);

	vfs_mountedfrom(mp, args->fsname);

	return (0);
}

static int
_xfs_mount(struct mount		*mp)
{
	struct xfsmount		*xmp;
	struct xfs_vnode	*rootvp;
	struct ucred		*curcred;
	struct vnode		*rvp, *devvp;
	struct cdev		*ddev;
	struct g_consumer	*cp;
	struct thread		*td;
	int			error;
	
	td = curthread;
	ddev = NULL;
	cp = NULL;

	if (vfs_filteropt(mp->mnt_optnew, xfs_opts))
		return (EINVAL);

	if (mp->mnt_flag & MNT_UPDATE)
		return (0);
	if ((mp->mnt_flag & MNT_RDONLY) == 0)
		return (EPERM);

        xmp = xfsmount_allocate(mp);
        if (xmp == NULL)
                return (ENOMEM);

	if((error = _xfs_param_copyin(mp, td)) != 0)
		goto fail;

	curcred = td->td_ucred;
	XVFS_MOUNT(XFSTOVFS(xmp), &xmp->m_args, curcred, error);
	if (error)
		goto fail;

 	XVFS_ROOT(XFSTOVFS(xmp), &rootvp, error);
	ddev = XFS_VFSTOM(XFSTOVFS(xmp))->m_ddev_targp->dev;
	devvp = XFS_VFSTOM(XFSTOVFS(xmp))->m_ddev_targp->specvp;
	if (error)
		goto fail_unmount;

 	if (ddev->si_iosize_max != 0)
		mp->mnt_iosize_max = ddev->si_iosize_max;
        if (mp->mnt_iosize_max > MAXPHYS)
		mp->mnt_iosize_max = MAXPHYS;

        mp->mnt_flag |= MNT_LOCAL;
        mp->mnt_stat.f_fsid.val[0] = dev2udev(ddev);
        mp->mnt_stat.f_fsid.val[1] = mp->mnt_vfc->vfc_typenum;

        if ((error = VFS_STATFS(mp, &mp->mnt_stat)) != 0)
		goto fail_unmount;

	rvp = rootvp->v_vnode;
	rvp->v_vflag |= VV_ROOT;
	VN_RELE(rootvp);

	return (0);

 fail_unmount:
	XVFS_UNMOUNT(XFSTOVFS(xmp), 0, curcred, error);

	if (devvp != NULL) {
		cp = devvp->v_bufobj.bo_private;
		if (cp != NULL) {
			DROP_GIANT();
			g_topology_lock();
			g_vfs_close(cp);
			g_topology_unlock();
			PICKUP_GIANT();
		}
	}

 fail:
	if (xmp != NULL)
		xfsmount_deallocate(xmp);

	return (error);
}

/*
 * Free reference to null layer
 */
static int
_xfs_unmount(mp, mntflags)
	struct mount *mp;
	int mntflags;
{
	struct vnode *devvp;
	struct g_consumer *cp;
	int error;
	cp = NULL;
	devvp = NULL;

	devvp = XFS_VFSTOM((MNTTOVFS(mp)))->m_ddev_targp->specvp;
	if (devvp != NULL)
		cp = devvp->v_bufobj.bo_private;

	XVFS_UNMOUNT(MNTTOVFS(mp), 0, curthread->td_ucred, error);
	if (error == 0) {
		if (cp != NULL) {
			DROP_GIANT();
			g_topology_lock();
			g_vfs_close(cp);
			g_topology_unlock();
			PICKUP_GIANT();
		}
	}
	return (error);
}

static int
_xfs_root(mp, flags, vpp)
	struct mount *mp;
	int flags;
	struct vnode **vpp;
{
	xfs_vnode_t *vp;
	int error;

        XVFS_ROOT(MNTTOVFS(mp), &vp, error);
	if (error == 0) {
		*vpp = vp->v_vnode;
		VOP_LOCK(*vpp, flags);
	}
	return (error);
}

static int
_xfs_quotactl(mp, cmd, uid, arg)
	struct mount *mp;
	int cmd;
	uid_t uid;
	void *arg;
{
	printf("xfs_quotactl\n");
	return EOPNOTSUPP;
}

static int
_xfs_statfs(mp, sbp)
	struct mount *mp;
	struct statfs *sbp;
{
	int error;

        XVFS_STATVFS(MNTTOVFS(mp), sbp, NULL, error);
        if (error)
		return error;

	/* Fix up the values XFS statvfs calls does not know about. */
	sbp->f_iosize = sbp->f_bsize;

	return (error);
}

static int
_xfs_sync(mp, waitfor)
	struct mount *mp;
	int waitfor;
{
	int error;
	int flags = SYNC_FSDATA|SYNC_ATTR|SYNC_REFCACHE;

	if (waitfor == MNT_WAIT)
		flags |= SYNC_WAIT;
	else if (waitfor == MNT_LAZY)
		flags |= SYNC_BDFLUSH;
        XVFS_SYNC(MNTTOVFS(mp), flags, curthread->td_ucred, error);
	return (error);
}

static int
_xfs_vget(mp, ino, flags, vpp)
	struct mount *mp;
	ino_t ino;
	int flags;
	struct vnode **vpp;
{
	xfs_vnode_t *vp = NULL;
	int error;

	printf("XVFS_GET_VNODE(MNTTOVFS(mp), &vp, ino, error);\n");
	error = ENOSYS;
	if (error == 0)
		*vpp = vp->v_vnode;
	return (error);
}

static int
_xfs_fhtovp(mp, fidp, flags, vpp)
	struct mount *mp;
	struct fid *fidp;
	int flags;
	struct vnode **vpp;
{
	printf("xfs_fhtovp\n");
	return ENOSYS;
}

static int
_xfs_extattrctl(struct mount *mp, int cm,
                struct vnode *filename_v,
                int attrnamespace, const char *attrname)
{
	printf("xfs_extattrctl\n");
	return ENOSYS;
}

int
_xfs_init(vfsp)
	struct vfsconf *vfsp;
{
	int error;

	error = init_xfs_fs();

	return (error);
}

int
_xfs_uninit(vfsp)
	struct vfsconf *vfsp;
{
	exit_xfs_fs();
	return 0;
}

static struct vfsops xfs_fsops = {
	.vfs_mount =	_xfs_mount,
	.vfs_unmount =	_xfs_unmount,
	.vfs_root =	_xfs_root,
	.vfs_quotactl = _xfs_quotactl,
	.vfs_statfs =	_xfs_statfs,
	.vfs_sync =	_xfs_sync,
	.vfs_vget =	_xfs_vget,
	.vfs_fhtovp =	_xfs_fhtovp,
	.vfs_init =	_xfs_init,
	.vfs_uninit =	_xfs_uninit,
	.vfs_extattrctl = _xfs_extattrctl,
};

VFS_SET(xfs_fsops, xfs, 0);

/*
 *  Copy GEOM VFS functions here to provide a conveniet place to
 *  track all XFS-related IO without being distracted by other
 *  filesystems which happen to be mounted on the machine at the
 *  same time.
 */

static void
xfs_geom_biodone(struct bio *bip)
{
	struct buf *bp;

	if (bip->bio_error) {
		printf("g_vfs_done():");
		g_print_bio(bip);
		printf("error = %d\n", bip->bio_error);
	}
	bp = bip->bio_caller2;
	bp->b_error = bip->bio_error;
	bp->b_ioflags = bip->bio_flags;
	if (bip->bio_error)
		bp->b_ioflags |= BIO_ERROR;
	bp->b_resid = bp->b_bcount - bip->bio_completed;
	g_destroy_bio(bip);
	mtx_lock(&Giant);
	bufdone(bp);
	mtx_unlock(&Giant);
}

static void
xfs_geom_strategy(struct bufobj *bo, struct buf *bp)
{
	struct g_consumer *cp;
	struct bio *bip;

	cp = bo->bo_private;
	G_VALID_CONSUMER(cp);

	bip = g_alloc_bio();
	bip->bio_cmd = bp->b_iocmd;
	bip->bio_offset = bp->b_iooffset;
	bip->bio_data = bp->b_data;
	bip->bio_done = xfs_geom_biodone;
	bip->bio_caller2 = bp;
	bip->bio_length = bp->b_bcount;
	g_io_request(bip, cp);
}

static int
xfs_geom_bufwrite(struct buf *bp)
{
	return bufwrite(bp);
}

static int
xfs_geom_bufsync(struct bufobj *bo, int waitfor)
{

	return (bufsync(bo, waitfor));
}

static void
xfs_geom_bufbdflush(struct bufobj *bo, struct buf *bp)
{
	bufbdflush(bo, bp);
}

struct buf_ops xfs_bo_ops = {
	.bop_name =     "XFS",
	.bop_write =    xfs_geom_bufwrite,
	.bop_strategy = xfs_geom_strategy,
	.bop_sync =     xfs_geom_bufsync,
	.bop_bdflush =	xfs_geom_bufbdflush,
};
