/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright (c) 2005, 2010, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2013 by Delphix. All rights reserved.
 */

/*
 * ZFS control directory (a.k.a. ".zfs")
 *
 * This directory provides a common location for all ZFS meta-objects.
 * Currently, this is only the 'snapshot' directory, but this may expand in the
 * future.  The elements are built using the GFS primitives, as the hierarchy
 * does not actually exist on disk.
 *
 * For 'snapshot', we don't want to have all snapshots always mounted, because
 * this would take up a huge amount of space in /etc/mnttab.  We have three
 * types of objects:
 *
 * 	ctldir ------> snapshotdir -------> snapshot
 *                                             |
 *                                             |
 *                                             V
 *                                         mounted fs
 *
 * The 'snapshot' node contains just enough information to lookup '..' and act
 * as a mountpoint for the snapshot.  Whenever we lookup a specific snapshot, we
 * perform an automount of the underlying filesystem and return the
 * corresponding vnode.
 *
 * All mounts are handled automatically by the kernel, but unmounts are
 * (currently) handled from user land.  The main reason is that there is no
 * reliable way to auto-unmount the filesystem when it's "no longer in use".
 * When the user unmounts a filesystem, we call zfsctl_unmount(), which
 * unmounts any snapshots within the snapshot directory.
 *
 * The '.zfs', '.zfs/snapshot', and all directories created under
 * '.zfs/snapshot' (ie: '.zfs/snapshot/<snapname>') are all GFS nodes and
 * share the same vfs_t as the head filesystem (what '.zfs' lives under).
 *
 * File systems mounted ontop of the GFS nodes '.zfs/snapshot/<snapname>'
 * (ie: snapshots) are ZFS nodes and have their own unique vfs_t.
 * However, vnodes within these mounted on file systems have their v_vfsp
 * fields set to the head filesystem to make NFS happy (see
 * zfsctl_snapdir_lookup()). We VFS_HOLD the head filesystem's vfs_t
 * so that it cannot be freed until all snapshots have been unmounted.
 */

#include <sys/zfs_context.h>
#include <sys/zfs_ctldir.h>
#include <sys/zfs_ioctl.h>
#include <sys/zfs_vfsops.h>
#include <sys/namei.h>
#include <sys/gfs.h>
#include <sys/stat.h>
#include <sys/dmu.h>
#include <sys/dsl_destroy.h>
#include <sys/dsl_deleg.h>
#include <sys/mount.h>
#include <sys/sunddi.h>

#include "zfs_namecheck.h"

typedef struct zfsctl_node {
	gfs_dir_t	zc_gfs_private;
	uint64_t	zc_id;
	timestruc_t	zc_cmtime;	/* ctime and mtime, always the same */
} zfsctl_node_t;

typedef struct zfsctl_snapdir {
	zfsctl_node_t	sd_node;
	kmutex_t	sd_lock;
	avl_tree_t	sd_snaps;
} zfsctl_snapdir_t;

typedef struct {
	char		*se_name;
	vnode_t		*se_root;
	avl_node_t	se_node;
} zfs_snapentry_t;

static int
snapentry_compare(const void *a, const void *b)
{
	const zfs_snapentry_t *sa = a;
	const zfs_snapentry_t *sb = b;
	int ret = strcmp(sa->se_name, sb->se_name);

	if (ret < 0)
		return (-1);
	else if (ret > 0)
		return (1);
	else
		return (0);
}

#ifdef sun
vnodeops_t *zfsctl_ops_root;
vnodeops_t *zfsctl_ops_snapdir;
vnodeops_t *zfsctl_ops_snapshot;
vnodeops_t *zfsctl_ops_shares;
vnodeops_t *zfsctl_ops_shares_dir;

static const fs_operation_def_t zfsctl_tops_root[];
static const fs_operation_def_t zfsctl_tops_snapdir[];
static const fs_operation_def_t zfsctl_tops_snapshot[];
static const fs_operation_def_t zfsctl_tops_shares[];
#else	/* !sun */
static struct vop_vector zfsctl_ops_root;
static struct vop_vector zfsctl_ops_snapdir;
static struct vop_vector zfsctl_ops_snapshot;
static struct vop_vector zfsctl_ops_shares;
static struct vop_vector zfsctl_ops_shares_dir;
#endif	/* !sun */

static vnode_t *zfsctl_mknode_snapdir(vnode_t *);
static vnode_t *zfsctl_mknode_shares(vnode_t *);
static vnode_t *zfsctl_snapshot_mknode(vnode_t *, uint64_t objset);
static int zfsctl_unmount_snap(zfs_snapentry_t *, int, cred_t *);

#ifdef sun
static gfs_opsvec_t zfsctl_opsvec[] = {
	{ ".zfs", zfsctl_tops_root, &zfsctl_ops_root },
	{ ".zfs/snapshot", zfsctl_tops_snapdir, &zfsctl_ops_snapdir },
	{ ".zfs/snapshot/vnode", zfsctl_tops_snapshot, &zfsctl_ops_snapshot },
	{ ".zfs/shares", zfsctl_tops_shares, &zfsctl_ops_shares_dir },
	{ ".zfs/shares/vnode", zfsctl_tops_shares, &zfsctl_ops_shares },
	{ NULL }
};
#endif	/* sun */

/*
 * Root directory elements.  We only have two entries
 * snapshot and shares.
 */
static gfs_dirent_t zfsctl_root_entries[] = {
	{ "snapshot", zfsctl_mknode_snapdir, GFS_CACHE_VNODE },
	{ "shares", zfsctl_mknode_shares, GFS_CACHE_VNODE },
	{ NULL }
};

/* include . and .. in the calculation */
#define	NROOT_ENTRIES	((sizeof (zfsctl_root_entries) / \
    sizeof (gfs_dirent_t)) + 1)


/*
 * Initialize the various GFS pieces we'll need to create and manipulate .zfs
 * directories.  This is called from the ZFS init routine, and initializes the
 * vnode ops vectors that we'll be using.
 */
void
zfsctl_init(void)
{
#ifdef sun
	VERIFY(gfs_make_opsvec(zfsctl_opsvec) == 0);
#endif
}

void
zfsctl_fini(void)
{
#ifdef sun
	/*
	 * Remove vfsctl vnode ops
	 */
	if (zfsctl_ops_root)
		vn_freevnodeops(zfsctl_ops_root);
	if (zfsctl_ops_snapdir)
		vn_freevnodeops(zfsctl_ops_snapdir);
	if (zfsctl_ops_snapshot)
		vn_freevnodeops(zfsctl_ops_snapshot);
	if (zfsctl_ops_shares)
		vn_freevnodeops(zfsctl_ops_shares);
	if (zfsctl_ops_shares_dir)
		vn_freevnodeops(zfsctl_ops_shares_dir);

	zfsctl_ops_root = NULL;
	zfsctl_ops_snapdir = NULL;
	zfsctl_ops_snapshot = NULL;
	zfsctl_ops_shares = NULL;
	zfsctl_ops_shares_dir = NULL;
#endif	/* sun */
}

boolean_t
zfsctl_is_node(vnode_t *vp)
{
	return (vn_matchops(vp, zfsctl_ops_root) ||
	    vn_matchops(vp, zfsctl_ops_snapdir) ||
	    vn_matchops(vp, zfsctl_ops_snapshot) ||
	    vn_matchops(vp, zfsctl_ops_shares) ||
	    vn_matchops(vp, zfsctl_ops_shares_dir));

}

/*
 * Return the inode number associated with the 'snapshot' or
 * 'shares' directory.
 */
/* ARGSUSED */
static ino64_t
zfsctl_root_inode_cb(vnode_t *vp, int index)
{
	zfsvfs_t *zfsvfs = vp->v_vfsp->vfs_data;

	ASSERT(index <= 2);

	if (index == 0)
		return (ZFSCTL_INO_SNAPDIR);

	return (zfsvfs->z_shares_dir);
}

/*
 * Create the '.zfs' directory.  This directory is cached as part of the VFS
 * structure.  This results in a hold on the vfs_t.  The code in zfs_umount()
 * therefore checks against a vfs_count of 2 instead of 1.  This reference
 * is removed when the ctldir is destroyed in the unmount.
 */
void
zfsctl_create(zfsvfs_t *zfsvfs)
{
	vnode_t *vp, *rvp;
	zfsctl_node_t *zcp;
	uint64_t crtime[2];

	ASSERT(zfsvfs->z_ctldir == NULL);

	vp = gfs_root_create(sizeof (zfsctl_node_t), zfsvfs->z_vfs,
	    &zfsctl_ops_root, ZFSCTL_INO_ROOT, zfsctl_root_entries,
	    zfsctl_root_inode_cb, MAXNAMELEN, NULL, NULL);
	zcp = vp->v_data;
	zcp->zc_id = ZFSCTL_INO_ROOT;

	VERIFY(VFS_ROOT(zfsvfs->z_vfs, LK_EXCLUSIVE, &rvp) == 0);
	VERIFY(0 == sa_lookup(VTOZ(rvp)->z_sa_hdl, SA_ZPL_CRTIME(zfsvfs),
	    &crtime, sizeof (crtime)));
	ZFS_TIME_DECODE(&zcp->zc_cmtime, crtime);
	VN_URELE(rvp);

	/*
	 * We're only faking the fact that we have a root of a filesystem for
	 * the sake of the GFS interfaces.  Undo the flag manipulation it did
	 * for us.
	 */
	vp->v_vflag &= ~VV_ROOT;

	zfsvfs->z_ctldir = vp;

	VOP_UNLOCK(vp, 0);
}

/*
 * Destroy the '.zfs' directory.  Only called when the filesystem is unmounted.
 * There might still be more references if we were force unmounted, but only
 * new zfs_inactive() calls can occur and they don't reference .zfs
 */
void
zfsctl_destroy(zfsvfs_t *zfsvfs)
{
	VN_RELE(zfsvfs->z_ctldir);
	zfsvfs->z_ctldir = NULL;
}

/*
 * Given a root znode, retrieve the associated .zfs directory.
 * Add a hold to the vnode and return it.
 */
vnode_t *
zfsctl_root(znode_t *zp)
{
	ASSERT(zfs_has_ctldir(zp));
	VN_HOLD(zp->z_zfsvfs->z_ctldir);
	return (zp->z_zfsvfs->z_ctldir);
}

/*
 * Common open routine.  Disallow any write access.
 */
/* ARGSUSED */
static int
zfsctl_common_open(struct vop_open_args *ap)
{
	int flags = ap->a_mode;

	if (flags & FWRITE)
		return (SET_ERROR(EACCES));

	return (0);
}

/*
 * Common close routine.  Nothing to do here.
 */
/* ARGSUSED */
static int
zfsctl_common_close(struct vop_close_args *ap)
{
	return (0);
}

/*
 * Common access routine.  Disallow writes.
 */
/* ARGSUSED */
static int
zfsctl_common_access(ap)
	struct vop_access_args /* {
		struct vnode *a_vp;
		accmode_t a_accmode;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{
	accmode_t accmode = ap->a_accmode;

#ifdef TODO
	if (flags & V_ACE_MASK) {
		if (accmode & ACE_ALL_WRITE_PERMS)
			return (SET_ERROR(EACCES));
	} else {
#endif
		if (accmode & VWRITE)
			return (SET_ERROR(EACCES));
#ifdef TODO
	}
#endif

	return (0);
}

/*
 * Common getattr function.  Fill in basic information.
 */
static void
zfsctl_common_getattr(vnode_t *vp, vattr_t *vap)
{
	timestruc_t	now;

	vap->va_uid = 0;
	vap->va_gid = 0;
	vap->va_rdev = 0;
	/*
	 * We are a purely virtual object, so we have no
	 * blocksize or allocated blocks.
	 */
	vap->va_blksize = 0;
	vap->va_nblocks = 0;
	vap->va_seq = 0;
	vap->va_fsid = vp->v_mount->mnt_stat.f_fsid.val[0];
	vap->va_mode = S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP |
	    S_IROTH | S_IXOTH;
	vap->va_type = VDIR;
	/*
	 * We live in the now (for atime).
	 */
	gethrestime(&now);
	vap->va_atime = now;
	/* FreeBSD: Reset chflags(2) flags. */
	vap->va_flags = 0;
}

/*ARGSUSED*/
static int
zfsctl_common_fid(ap)
	struct vop_fid_args /* {
		struct vnode *a_vp;
		struct fid *a_fid;
	} */ *ap;
{
	vnode_t		*vp = ap->a_vp;
	fid_t		*fidp = (void *)ap->a_fid;
	zfsvfs_t	*zfsvfs = vp->v_vfsp->vfs_data;
	zfsctl_node_t	*zcp = vp->v_data;
	uint64_t	object = zcp->zc_id;
	zfid_short_t	*zfid;
	int		i;

	ZFS_ENTER(zfsvfs);

#ifdef illumos
	if (fidp->fid_len < SHORT_FID_LEN) {
		fidp->fid_len = SHORT_FID_LEN;
		ZFS_EXIT(zfsvfs);
		return (SET_ERROR(ENOSPC));
	}
#else
	fidp->fid_len = SHORT_FID_LEN;
#endif

	zfid = (zfid_short_t *)fidp;

	zfid->zf_len = SHORT_FID_LEN;

	for (i = 0; i < sizeof (zfid->zf_object); i++)
		zfid->zf_object[i] = (uint8_t)(object >> (8 * i));

	/* .zfs znodes always have a generation number of 0 */
	for (i = 0; i < sizeof (zfid->zf_gen); i++)
		zfid->zf_gen[i] = 0;

	ZFS_EXIT(zfsvfs);
	return (0);
}


/*ARGSUSED*/
static int
zfsctl_shares_fid(ap)
	struct vop_fid_args /* {
		struct vnode *a_vp;
		struct fid *a_fid;
	} */ *ap;
{
	vnode_t		*vp = ap->a_vp;
	fid_t		*fidp = (void *)ap->a_fid;
	zfsvfs_t	*zfsvfs = vp->v_vfsp->vfs_data;
	znode_t		*dzp;
	int		error;

	ZFS_ENTER(zfsvfs);

	if (zfsvfs->z_shares_dir == 0) {
		ZFS_EXIT(zfsvfs);
		return (SET_ERROR(ENOTSUP));
	}

	if ((error = zfs_zget(zfsvfs, zfsvfs->z_shares_dir, &dzp)) == 0) {
		error = VOP_FID(ZTOV(dzp), fidp);
		VN_RELE(ZTOV(dzp));
	}

	ZFS_EXIT(zfsvfs);
	return (error);
}

static int
zfsctl_common_reclaim(ap)
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
		struct thread *a_td;
	} */ *ap;
{
	vnode_t *vp = ap->a_vp;

	/*
	 * Destroy the vm object and flush associated pages.
	 */
	vnode_destroy_vobject(vp);
	VI_LOCK(vp);
	vp->v_data = NULL;
	VI_UNLOCK(vp);
	return (0);
}

/*
 * .zfs inode namespace
 *
 * We need to generate unique inode numbers for all files and directories
 * within the .zfs pseudo-filesystem.  We use the following scheme:
 *
 * 	ENTRY			ZFSCTL_INODE
 * 	.zfs			1
 * 	.zfs/snapshot		2
 * 	.zfs/snapshot/<snap>	objectid(snap)
 */

#define	ZFSCTL_INO_SNAP(id)	(id)

/*
 * Get root directory attributes.
 */
/* ARGSUSED */
static int
zfsctl_root_getattr(ap)
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct vattr *vap = ap->a_vap;
	zfsvfs_t *zfsvfs = vp->v_vfsp->vfs_data;
	zfsctl_node_t *zcp = vp->v_data;

	ZFS_ENTER(zfsvfs);
	vap->va_nodeid = ZFSCTL_INO_ROOT;
	vap->va_nlink = vap->va_size = NROOT_ENTRIES;
	vap->va_mtime = vap->va_ctime = zcp->zc_cmtime;
	vap->va_birthtime = vap->va_ctime;

	zfsctl_common_getattr(vp, vap);
	ZFS_EXIT(zfsvfs);

	return (0);
}

/*
 * Special case the handling of "..".
 */
/* ARGSUSED */
int
zfsctl_root_lookup(vnode_t *dvp, char *nm, vnode_t **vpp, pathname_t *pnp,
    int flags, vnode_t *rdir, cred_t *cr, caller_context_t *ct,
    int *direntflags, pathname_t *realpnp)
{
	zfsvfs_t *zfsvfs = dvp->v_vfsp->vfs_data;
	int err;

	/*
	 * No extended attributes allowed under .zfs
	 */
	if (flags & LOOKUP_XATTR)
		return (SET_ERROR(EINVAL));

	ZFS_ENTER(zfsvfs);

	if (strcmp(nm, "..") == 0) {
		err = VFS_ROOT(dvp->v_vfsp, LK_EXCLUSIVE, vpp);
		if (err == 0)
			VOP_UNLOCK(*vpp, 0);
	} else {
		err = gfs_vop_lookup(dvp, nm, vpp, pnp, flags, rdir,
		    cr, ct, direntflags, realpnp);
	}

	ZFS_EXIT(zfsvfs);

	return (err);
}

#ifdef sun
static int
zfsctl_pathconf(vnode_t *vp, int cmd, ulong_t *valp, cred_t *cr,
    caller_context_t *ct)
{
	/*
	 * We only care about ACL_ENABLED so that libsec can
	 * display ACL correctly and not default to POSIX draft.
	 */
	if (cmd == _PC_ACL_ENABLED) {
		*valp = _ACL_ACE_ENABLED;
		return (0);
	}

	return (fs_pathconf(vp, cmd, valp, cr, ct));
}
#endif	/* sun */

#ifdef sun
static const fs_operation_def_t zfsctl_tops_root[] = {
	{ VOPNAME_OPEN,		{ .vop_open = zfsctl_common_open }	},
	{ VOPNAME_CLOSE,	{ .vop_close = zfsctl_common_close }	},
	{ VOPNAME_IOCTL,	{ .error = fs_inval }			},
	{ VOPNAME_GETATTR,	{ .vop_getattr = zfsctl_root_getattr }	},
	{ VOPNAME_ACCESS,	{ .vop_access = zfsctl_common_access }	},
	{ VOPNAME_READDIR,	{ .vop_readdir = gfs_vop_readdir } 	},
	{ VOPNAME_LOOKUP,	{ .vop_lookup = zfsctl_root_lookup }	},
	{ VOPNAME_SEEK,		{ .vop_seek = fs_seek }			},
	{ VOPNAME_INACTIVE,	{ .vop_inactive = gfs_vop_inactive }	},
	{ VOPNAME_PATHCONF,	{ .vop_pathconf = zfsctl_pathconf }	},
	{ VOPNAME_FID,		{ .vop_fid = zfsctl_common_fid	}	},
	{ NULL }
};
#endif	/* sun */

/*
 * Special case the handling of "..".
 */
/* ARGSUSED */
int
zfsctl_freebsd_root_lookup(ap)
	struct vop_lookup_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap;
{
	vnode_t *dvp = ap->a_dvp;
	vnode_t **vpp = ap->a_vpp;
	cred_t *cr = ap->a_cnp->cn_cred;
	int flags = ap->a_cnp->cn_flags;
	int nameiop = ap->a_cnp->cn_nameiop;
	char nm[NAME_MAX + 1];
	int err;

	if ((flags & ISLASTCN) && (nameiop == RENAME || nameiop == CREATE))
		return (EOPNOTSUPP);

	ASSERT(ap->a_cnp->cn_namelen < sizeof(nm));
	strlcpy(nm, ap->a_cnp->cn_nameptr, ap->a_cnp->cn_namelen + 1);

	err = zfsctl_root_lookup(dvp, nm, vpp, NULL, 0, NULL, cr, NULL, NULL, NULL);
	if (err == 0 && (nm[0] != '.' || nm[1] != '\0'))
		vn_lock(*vpp, LK_EXCLUSIVE | LK_RETRY);
	return (err);
}

static struct vop_vector zfsctl_ops_root = {
	.vop_default =	&default_vnodeops,
	.vop_open =	zfsctl_common_open,
	.vop_close =	zfsctl_common_close,
	.vop_ioctl =	VOP_EINVAL,
	.vop_getattr =	zfsctl_root_getattr,
	.vop_access =	zfsctl_common_access,
	.vop_readdir =	gfs_vop_readdir,
	.vop_lookup =	zfsctl_freebsd_root_lookup,
	.vop_inactive =	gfs_vop_inactive,
	.vop_reclaim =	zfsctl_common_reclaim,
#ifdef TODO
	.vop_pathconf =	zfsctl_pathconf,
#endif
	.vop_fid =	zfsctl_common_fid,
};

/*
 * Gets the full dataset name that corresponds to the given snapshot name
 * Example:
 * 	zfsctl_snapshot_zname("snap1") -> "mypool/myfs@snap1"
 */
static int
zfsctl_snapshot_zname(vnode_t *vp, const char *name, int len, char *zname)
{
	objset_t *os = ((zfsvfs_t *)((vp)->v_vfsp->vfs_data))->z_os;

	if (snapshot_namecheck(name, NULL, NULL) != 0)
		return (SET_ERROR(EILSEQ));
	dmu_objset_name(os, zname);
	if (strlen(zname) + 1 + strlen(name) >= len)
		return (SET_ERROR(ENAMETOOLONG));
	(void) strcat(zname, "@");
	(void) strcat(zname, name);
	return (0);
}

static int
zfsctl_unmount_snap(zfs_snapentry_t *sep, int fflags, cred_t *cr)
{
	vnode_t *svp = sep->se_root;
	int error;

	ASSERT(vn_ismntpt(svp));

	/* this will be dropped by dounmount() */
	if ((error = vn_vfswlock(svp)) != 0)
		return (error);

#ifdef sun
	VN_HOLD(svp);
	error = dounmount(vn_mountedvfs(svp), fflags, cr);
	if (error) {
		VN_RELE(svp);
		return (error);
	}

	/*
	 * We can't use VN_RELE(), as that will try to invoke
	 * zfsctl_snapdir_inactive(), which would cause us to destroy
	 * the sd_lock mutex held by our caller.
	 */
	ASSERT(svp->v_count == 1);
	gfs_vop_inactive(svp, cr, NULL);

	kmem_free(sep->se_name, strlen(sep->se_name) + 1);
	kmem_free(sep, sizeof (zfs_snapentry_t));

	return (0);
#else	/* !sun */
	return (dounmount(vn_mountedvfs(svp), fflags, curthread));
#endif	/* !sun */
}

#ifdef sun
static void
zfsctl_rename_snap(zfsctl_snapdir_t *sdp, zfs_snapentry_t *sep, const char *nm)
{
	avl_index_t where;
	vfs_t *vfsp;
	refstr_t *pathref;
	char newpath[MAXNAMELEN];
	char *tail;

	ASSERT(MUTEX_HELD(&sdp->sd_lock));
	ASSERT(sep != NULL);

	vfsp = vn_mountedvfs(sep->se_root);
	ASSERT(vfsp != NULL);

	vfs_lock_wait(vfsp);

	/*
	 * Change the name in the AVL tree.
	 */
	avl_remove(&sdp->sd_snaps, sep);
	kmem_free(sep->se_name, strlen(sep->se_name) + 1);
	sep->se_name = kmem_alloc(strlen(nm) + 1, KM_SLEEP);
	(void) strcpy(sep->se_name, nm);
	VERIFY(avl_find(&sdp->sd_snaps, sep, &where) == NULL);
	avl_insert(&sdp->sd_snaps, sep, where);

	/*
	 * Change the current mountpoint info:
	 * 	- update the tail of the mntpoint path
	 *	- update the tail of the resource path
	 */
	pathref = vfs_getmntpoint(vfsp);
	(void) strncpy(newpath, refstr_value(pathref), sizeof (newpath));
	VERIFY((tail = strrchr(newpath, '/')) != NULL);
	*(tail+1) = '\0';
	ASSERT3U(strlen(newpath) + strlen(nm), <, sizeof (newpath));
	(void) strcat(newpath, nm);
	refstr_rele(pathref);
	vfs_setmntpoint(vfsp, newpath, 0);

	pathref = vfs_getresource(vfsp);
	(void) strncpy(newpath, refstr_value(pathref), sizeof (newpath));
	VERIFY((tail = strrchr(newpath, '@')) != NULL);
	*(tail+1) = '\0';
	ASSERT3U(strlen(newpath) + strlen(nm), <, sizeof (newpath));
	(void) strcat(newpath, nm);
	refstr_rele(pathref);
	vfs_setresource(vfsp, newpath, 0);

	vfs_unlock(vfsp);
}
#endif	/* sun */

#ifdef sun
/*ARGSUSED*/
static int
zfsctl_snapdir_rename(vnode_t *sdvp, char *snm, vnode_t *tdvp, char *tnm,
    cred_t *cr, caller_context_t *ct, int flags)
{
	zfsctl_snapdir_t *sdp = sdvp->v_data;
	zfs_snapentry_t search, *sep;
	zfsvfs_t *zfsvfs;
	avl_index_t where;
	char from[MAXNAMELEN], to[MAXNAMELEN];
	char real[MAXNAMELEN], fsname[MAXNAMELEN];
	int err;

	zfsvfs = sdvp->v_vfsp->vfs_data;
	ZFS_ENTER(zfsvfs);

	if ((flags & FIGNORECASE) || zfsvfs->z_case == ZFS_CASE_INSENSITIVE) {
		err = dmu_snapshot_realname(zfsvfs->z_os, snm, real,
		    MAXNAMELEN, NULL);
		if (err == 0) {
			snm = real;
		} else if (err != ENOTSUP) {
			ZFS_EXIT(zfsvfs);
			return (err);
		}
	}

	ZFS_EXIT(zfsvfs);

	dmu_objset_name(zfsvfs->z_os, fsname);

	err = zfsctl_snapshot_zname(sdvp, snm, MAXNAMELEN, from);
	if (err == 0)
		err = zfsctl_snapshot_zname(tdvp, tnm, MAXNAMELEN, to);
	if (err == 0)
		err = zfs_secpolicy_rename_perms(from, to, cr);
	if (err != 0)
		return (err);

	/*
	 * Cannot move snapshots out of the snapdir.
	 */
	if (sdvp != tdvp)
		return (SET_ERROR(EINVAL));

	if (strcmp(snm, tnm) == 0)
		return (0);

	mutex_enter(&sdp->sd_lock);

	search.se_name = (char *)snm;
	if ((sep = avl_find(&sdp->sd_snaps, &search, &where)) == NULL) {
		mutex_exit(&sdp->sd_lock);
		return (SET_ERROR(ENOENT));
	}

	err = dsl_dataset_rename_snapshot(fsname, snm, tnm, 0);
	if (err == 0)
		zfsctl_rename_snap(sdp, sep, tnm);

	mutex_exit(&sdp->sd_lock);

	return (err);
}
#endif	/* sun */

#ifdef sun
/* ARGSUSED */
static int
zfsctl_snapdir_remove(vnode_t *dvp, char *name, vnode_t *cwd, cred_t *cr,
    caller_context_t *ct, int flags)
{
	zfsctl_snapdir_t *sdp = dvp->v_data;
	zfs_snapentry_t *sep;
	zfs_snapentry_t search;
	zfsvfs_t *zfsvfs;
	char snapname[MAXNAMELEN];
	char real[MAXNAMELEN];
	int err;

	zfsvfs = dvp->v_vfsp->vfs_data;
	ZFS_ENTER(zfsvfs);

	if ((flags & FIGNORECASE) || zfsvfs->z_case == ZFS_CASE_INSENSITIVE) {

		err = dmu_snapshot_realname(zfsvfs->z_os, name, real,
		    MAXNAMELEN, NULL);
		if (err == 0) {
			name = real;
		} else if (err != ENOTSUP) {
			ZFS_EXIT(zfsvfs);
			return (err);
		}
	}

	ZFS_EXIT(zfsvfs);

	err = zfsctl_snapshot_zname(dvp, name, MAXNAMELEN, snapname);
	if (err == 0)
		err = zfs_secpolicy_destroy_perms(snapname, cr);
	if (err != 0)
		return (err);

	mutex_enter(&sdp->sd_lock);

	search.se_name = name;
	sep = avl_find(&sdp->sd_snaps, &search, NULL);
	if (sep) {
		avl_remove(&sdp->sd_snaps, sep);
		err = zfsctl_unmount_snap(sep, MS_FORCE, cr);
		if (err != 0)
			avl_add(&sdp->sd_snaps, sep);
		else
			err = dsl_destroy_snapshot(snapname, B_FALSE);
	} else {
		err = SET_ERROR(ENOENT);
	}

	mutex_exit(&sdp->sd_lock);

	return (err);
}
#endif	/* sun */

/*
 * This creates a snapshot under '.zfs/snapshot'.
 */
/* ARGSUSED */
static int
zfsctl_snapdir_mkdir(vnode_t *dvp, char *dirname, vattr_t *vap, vnode_t  **vpp,
    cred_t *cr, caller_context_t *cc, int flags, vsecattr_t *vsecp)
{
	zfsvfs_t *zfsvfs = dvp->v_vfsp->vfs_data;
	char name[MAXNAMELEN];
	int err;
	static enum symfollow follow = NO_FOLLOW;
	static enum uio_seg seg = UIO_SYSSPACE;

	if (snapshot_namecheck(dirname, NULL, NULL) != 0)
		return (SET_ERROR(EILSEQ));

	dmu_objset_name(zfsvfs->z_os, name);

	*vpp = NULL;

	err = zfs_secpolicy_snapshot_perms(name, cr);
	if (err != 0)
		return (err);

	if (err == 0) {
		err = dmu_objset_snapshot_one(name, dirname);
		if (err != 0)
			return (err);
		err = lookupnameat(dirname, seg, follow, NULL, vpp, dvp);
	}

	return (err);
}

static int
zfsctl_freebsd_snapdir_mkdir(ap)
        struct vop_mkdir_args /* {
                struct vnode *a_dvp;
                struct vnode **a_vpp;
                struct componentname *a_cnp;
                struct vattr *a_vap;
        } */ *ap;
{

	ASSERT(ap->a_cnp->cn_flags & SAVENAME);

	return (zfsctl_snapdir_mkdir(ap->a_dvp, ap->a_cnp->cn_nameptr, NULL,
	    ap->a_vpp, ap->a_cnp->cn_cred, NULL, 0, NULL));
}

/*
 * Lookup entry point for the 'snapshot' directory.  Try to open the
 * snapshot if it exist, creating the pseudo filesystem vnode as necessary.
 * Perform a mount of the associated dataset on top of the vnode.
 */
/* ARGSUSED */
int
zfsctl_snapdir_lookup(ap)
	struct vop_lookup_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap;
{
	vnode_t *dvp = ap->a_dvp;
	vnode_t **vpp = ap->a_vpp;
	struct componentname *cnp = ap->a_cnp;
	char nm[NAME_MAX + 1];
	zfsctl_snapdir_t *sdp = dvp->v_data;
	objset_t *snap;
	char snapname[MAXNAMELEN];
	char real[MAXNAMELEN];
	char *mountpoint;
	zfs_snapentry_t *sep, search;
	size_t mountpoint_len;
	avl_index_t where;
	zfsvfs_t *zfsvfs = dvp->v_vfsp->vfs_data;
	int err;
	int flags = 0;

	/*
	 * No extended attributes allowed under .zfs
	 */
	if (flags & LOOKUP_XATTR)
		return (SET_ERROR(EINVAL));
	ASSERT(ap->a_cnp->cn_namelen < sizeof(nm));
	strlcpy(nm, ap->a_cnp->cn_nameptr, ap->a_cnp->cn_namelen + 1);

	ASSERT(dvp->v_type == VDIR);

	*vpp = NULL;

	/*
	 * If we get a recursive call, that means we got called
	 * from the domount() code while it was trying to look up the
	 * spec (which looks like a local path for zfs).  We need to
	 * add some flag to domount() to tell it not to do this lookup.
	 */
	if (MUTEX_HELD(&sdp->sd_lock))
		return (SET_ERROR(ENOENT));

	ZFS_ENTER(zfsvfs);

	if (gfs_lookup_dot(vpp, dvp, zfsvfs->z_ctldir, nm) == 0) {
		ZFS_EXIT(zfsvfs);
		return (0);
	}

	if (flags & FIGNORECASE) {
		boolean_t conflict = B_FALSE;

		err = dmu_snapshot_realname(zfsvfs->z_os, nm, real,
		    MAXNAMELEN, &conflict);
		if (err == 0) {
			strlcpy(nm, real, sizeof(nm));
		} else if (err != ENOTSUP) {
			ZFS_EXIT(zfsvfs);
			return (err);
		}
#if 0
		if (realpnp)
			(void) strlcpy(realpnp->pn_buf, nm,
			    realpnp->pn_bufsize);
		if (conflict && direntflags)
			*direntflags = ED_CASE_CONFLICT;
#endif
	}

	mutex_enter(&sdp->sd_lock);
	search.se_name = (char *)nm;
	if ((sep = avl_find(&sdp->sd_snaps, &search, &where)) != NULL) {
		*vpp = sep->se_root;
		VN_HOLD(*vpp);
		err = traverse(vpp, LK_EXCLUSIVE | LK_RETRY);
		if (err != 0) {
			VN_RELE(*vpp);
			*vpp = NULL;
		} else if (*vpp == sep->se_root) {
			/*
			 * The snapshot was unmounted behind our backs,
			 * try to remount it.
			 */
			VERIFY(zfsctl_snapshot_zname(dvp, nm, MAXNAMELEN, snapname) == 0);
			goto domount;
		} else {
			/*
			 * VROOT was set during the traverse call.  We need
			 * to clear it since we're pretending to be part
			 * of our parent's vfs.
			 */
			(*vpp)->v_flag &= ~VROOT;
		}
		mutex_exit(&sdp->sd_lock);
		ZFS_EXIT(zfsvfs);
		return (err);
	}

	/*
	 * The requested snapshot is not currently mounted, look it up.
	 */
	err = zfsctl_snapshot_zname(dvp, nm, MAXNAMELEN, snapname);
	if (err != 0) {
		mutex_exit(&sdp->sd_lock);
		ZFS_EXIT(zfsvfs);
		/*
		 * handle "ls *" or "?" in a graceful manner,
		 * forcing EILSEQ to ENOENT.
		 * Since shell ultimately passes "*" or "?" as name to lookup
		 */
		return (err == EILSEQ ? ENOENT : err);
	}
	if (dmu_objset_hold(snapname, FTAG, &snap) != 0) {
		mutex_exit(&sdp->sd_lock);
#ifdef illumos
		ZFS_EXIT(zfsvfs);
		return (SET_ERROR(ENOENT));
#else	/* !illumos */
		/* Translate errors and add SAVENAME when needed. */
		if ((cnp->cn_flags & ISLASTCN) && cnp->cn_nameiop == CREATE) {
			err = EJUSTRETURN;
			cnp->cn_flags |= SAVENAME;
		} else {
			err = SET_ERROR(ENOENT);
		}
		ZFS_EXIT(zfsvfs);
		return (err);
#endif	/* !illumos */
	}

	sep = kmem_alloc(sizeof (zfs_snapentry_t), KM_SLEEP);
	sep->se_name = kmem_alloc(strlen(nm) + 1, KM_SLEEP);
	(void) strcpy(sep->se_name, nm);
	*vpp = sep->se_root = zfsctl_snapshot_mknode(dvp, dmu_objset_id(snap));
	VN_HOLD(*vpp);
	avl_insert(&sdp->sd_snaps, sep, where);

	dmu_objset_rele(snap, FTAG);
domount:
	mountpoint_len = strlen(dvp->v_vfsp->mnt_stat.f_mntonname) +
	    strlen("/" ZFS_CTLDIR_NAME "/snapshot/") + strlen(nm) + 1;
	mountpoint = kmem_alloc(mountpoint_len, KM_SLEEP);
	(void) snprintf(mountpoint, mountpoint_len,
	    "%s/" ZFS_CTLDIR_NAME "/snapshot/%s",
	    dvp->v_vfsp->mnt_stat.f_mntonname, nm);
	err = mount_snapshot(curthread, vpp, "zfs", mountpoint, snapname, 0);
	kmem_free(mountpoint, mountpoint_len);
	if (err == 0) {
		/*
		 * Fix up the root vnode mounted on .zfs/snapshot/<snapname>.
		 *
		 * This is where we lie about our v_vfsp in order to
		 * make .zfs/snapshot/<snapname> accessible over NFS
		 * without requiring manual mounts of <snapname>.
		 */
		ASSERT(VTOZ(*vpp)->z_zfsvfs != zfsvfs);
		VTOZ(*vpp)->z_zfsvfs->z_parent = zfsvfs;
	}
	mutex_exit(&sdp->sd_lock);
	ZFS_EXIT(zfsvfs);

#ifdef illumos
	/*
	 * If we had an error, drop our hold on the vnode and
	 * zfsctl_snapshot_inactive() will clean up.
	 */
	if (err != 0) {
		VN_RELE(*vpp);
		*vpp = NULL;
	}
#else
	if (err != 0)
		*vpp = NULL;
#endif
	return (err);
}

/* ARGSUSED */
int
zfsctl_shares_lookup(ap)
	struct vop_lookup_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap;
{
	vnode_t *dvp = ap->a_dvp;
	vnode_t **vpp = ap->a_vpp;
	struct componentname *cnp = ap->a_cnp;
	zfsvfs_t *zfsvfs = dvp->v_vfsp->vfs_data;
	char nm[NAME_MAX + 1];
	znode_t *dzp;
	int error;

	ZFS_ENTER(zfsvfs);

	ASSERT(cnp->cn_namelen < sizeof(nm));
	strlcpy(nm, cnp->cn_nameptr, cnp->cn_namelen + 1);

	if (gfs_lookup_dot(vpp, dvp, zfsvfs->z_ctldir, nm) == 0) {
		ZFS_EXIT(zfsvfs);
		return (0);
	}

	if (zfsvfs->z_shares_dir == 0) {
		ZFS_EXIT(zfsvfs);
		return (SET_ERROR(ENOTSUP));
	}
	if ((error = zfs_zget(zfsvfs, zfsvfs->z_shares_dir, &dzp)) == 0)
		error = VOP_LOOKUP(ZTOV(dzp), vpp, cnp);

	VN_RELE(ZTOV(dzp));
	ZFS_EXIT(zfsvfs);

	return (error);
}

/* ARGSUSED */
static int
zfsctl_snapdir_readdir_cb(vnode_t *vp, void *dp, int *eofp,
    offset_t *offp, offset_t *nextp, void *data, int flags)
{
	zfsvfs_t *zfsvfs = vp->v_vfsp->vfs_data;
	char snapname[MAXNAMELEN];
	uint64_t id, cookie;
	boolean_t case_conflict;
	int error;

	ZFS_ENTER(zfsvfs);

	cookie = *offp;
	dsl_pool_config_enter(dmu_objset_pool(zfsvfs->z_os), FTAG);
	error = dmu_snapshot_list_next(zfsvfs->z_os, MAXNAMELEN, snapname, &id,
	    &cookie, &case_conflict);
	dsl_pool_config_exit(dmu_objset_pool(zfsvfs->z_os), FTAG);
	if (error) {
		ZFS_EXIT(zfsvfs);
		if (error == ENOENT) {
			*eofp = 1;
			return (0);
		}
		return (error);
	}

	if (flags & V_RDDIR_ENTFLAGS) {
		edirent_t *eodp = dp;

		(void) strcpy(eodp->ed_name, snapname);
		eodp->ed_ino = ZFSCTL_INO_SNAP(id);
		eodp->ed_eflags = case_conflict ? ED_CASE_CONFLICT : 0;
	} else {
		struct dirent64 *odp = dp;

		(void) strcpy(odp->d_name, snapname);
		odp->d_ino = ZFSCTL_INO_SNAP(id);
	}
	*nextp = cookie;

	ZFS_EXIT(zfsvfs);

	return (0);
}

/* ARGSUSED */
static int
zfsctl_shares_readdir(ap)
	struct vop_readdir_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
		int *a_eofflag;
		int *a_ncookies;
		u_long **a_cookies;
	} */ *ap;
{
	vnode_t *vp = ap->a_vp;
	uio_t *uiop = ap->a_uio;
	cred_t *cr = ap->a_cred;
	int *eofp = ap->a_eofflag;
	zfsvfs_t *zfsvfs = vp->v_vfsp->vfs_data;
	znode_t *dzp;
	int error;

	ZFS_ENTER(zfsvfs);

	if (zfsvfs->z_shares_dir == 0) {
		ZFS_EXIT(zfsvfs);
		return (SET_ERROR(ENOTSUP));
	}
	if ((error = zfs_zget(zfsvfs, zfsvfs->z_shares_dir, &dzp)) == 0) {
		vn_lock(ZTOV(dzp), LK_SHARED | LK_RETRY);
		error = VOP_READDIR(ZTOV(dzp), uiop, cr, eofp, ap->a_ncookies, ap->a_cookies);
		VN_URELE(ZTOV(dzp));
	} else {
		*eofp = 1;
		error = SET_ERROR(ENOENT);
	}

	ZFS_EXIT(zfsvfs);
	return (error);
}

/*
 * pvp is the '.zfs' directory (zfsctl_node_t).
 *
 * Creates vp, which is '.zfs/snapshot' (zfsctl_snapdir_t).
 *
 * This function is the callback to create a GFS vnode for '.zfs/snapshot'
 * when a lookup is performed on .zfs for "snapshot".
 */
vnode_t *
zfsctl_mknode_snapdir(vnode_t *pvp)
{
	vnode_t *vp;
	zfsctl_snapdir_t *sdp;

	vp = gfs_dir_create(sizeof (zfsctl_snapdir_t), pvp, pvp->v_vfsp,
	    &zfsctl_ops_snapdir, NULL, NULL, MAXNAMELEN,
	    zfsctl_snapdir_readdir_cb, NULL);
	sdp = vp->v_data;
	sdp->sd_node.zc_id = ZFSCTL_INO_SNAPDIR;
	sdp->sd_node.zc_cmtime = ((zfsctl_node_t *)pvp->v_data)->zc_cmtime;
	mutex_init(&sdp->sd_lock, NULL, MUTEX_DEFAULT, NULL);
	avl_create(&sdp->sd_snaps, snapentry_compare,
	    sizeof (zfs_snapentry_t), offsetof(zfs_snapentry_t, se_node));
	VOP_UNLOCK(vp, 0);
	return (vp);
}

vnode_t *
zfsctl_mknode_shares(vnode_t *pvp)
{
	vnode_t *vp;
	zfsctl_node_t *sdp;

	vp = gfs_dir_create(sizeof (zfsctl_node_t), pvp, pvp->v_vfsp,
	    &zfsctl_ops_shares, NULL, NULL, MAXNAMELEN,
	    NULL, NULL);
	sdp = vp->v_data;
	sdp->zc_cmtime = ((zfsctl_node_t *)pvp->v_data)->zc_cmtime;
	VOP_UNLOCK(vp, 0);
	return (vp);

}

/* ARGSUSED */
static int
zfsctl_shares_getattr(ap)
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{
	vnode_t *vp = ap->a_vp;
	vattr_t *vap = ap->a_vap;
	cred_t *cr = ap->a_cred;
	zfsvfs_t *zfsvfs = vp->v_vfsp->vfs_data;
	znode_t *dzp;
	int error;

	ZFS_ENTER(zfsvfs);
	if (zfsvfs->z_shares_dir == 0) {
		ZFS_EXIT(zfsvfs);
		return (SET_ERROR(ENOTSUP));
	}
	if ((error = zfs_zget(zfsvfs, zfsvfs->z_shares_dir, &dzp)) == 0) {
		vn_lock(ZTOV(dzp), LK_SHARED | LK_RETRY);
		error = VOP_GETATTR(ZTOV(dzp), vap, cr);
		VN_URELE(ZTOV(dzp));
	}
	ZFS_EXIT(zfsvfs);
	return (error);


}

/* ARGSUSED */
static int
zfsctl_snapdir_getattr(ap)
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
	} */ *ap;
{
	vnode_t *vp = ap->a_vp;
	vattr_t *vap = ap->a_vap;
	zfsvfs_t *zfsvfs = vp->v_vfsp->vfs_data;
	zfsctl_snapdir_t *sdp = vp->v_data;

	ZFS_ENTER(zfsvfs);
	zfsctl_common_getattr(vp, vap);
	vap->va_nodeid = gfs_file_inode(vp);
	vap->va_nlink = vap->va_size = avl_numnodes(&sdp->sd_snaps) + 2;
	vap->va_ctime = vap->va_mtime = dmu_objset_snap_cmtime(zfsvfs->z_os);
	vap->va_birthtime = vap->va_ctime;
	ZFS_EXIT(zfsvfs);

	return (0);
}

/* ARGSUSED */
static int
zfsctl_snapdir_inactive(ap)
	struct vop_inactive_args /* {
		struct vnode *a_vp;
		struct thread *a_td;
	} */ *ap;
{
	vnode_t *vp = ap->a_vp;
	zfsctl_snapdir_t *sdp = vp->v_data;
	zfs_snapentry_t *sep;

	/*
	 * On forced unmount we have to free snapshots from here.
	 */
	mutex_enter(&sdp->sd_lock);
	while ((sep = avl_first(&sdp->sd_snaps)) != NULL) {
		avl_remove(&sdp->sd_snaps, sep);
		kmem_free(sep->se_name, strlen(sep->se_name) + 1);
		kmem_free(sep, sizeof (zfs_snapentry_t));
	}
	mutex_exit(&sdp->sd_lock);
	gfs_dir_inactive(vp);
	ASSERT(avl_numnodes(&sdp->sd_snaps) == 0);
	mutex_destroy(&sdp->sd_lock);
	avl_destroy(&sdp->sd_snaps);
	kmem_free(sdp, sizeof (zfsctl_snapdir_t));

	return (0);
}

#ifdef sun
static const fs_operation_def_t zfsctl_tops_snapdir[] = {
	{ VOPNAME_OPEN,		{ .vop_open = zfsctl_common_open }	},
	{ VOPNAME_CLOSE,	{ .vop_close = zfsctl_common_close }	},
	{ VOPNAME_IOCTL,	{ .error = fs_inval }			},
	{ VOPNAME_GETATTR,	{ .vop_getattr = zfsctl_snapdir_getattr } },
	{ VOPNAME_ACCESS,	{ .vop_access = zfsctl_common_access }	},
	{ VOPNAME_RENAME,	{ .vop_rename = zfsctl_snapdir_rename }	},
	{ VOPNAME_RMDIR,	{ .vop_rmdir = zfsctl_snapdir_remove }	},
	{ VOPNAME_MKDIR,	{ .vop_mkdir = zfsctl_snapdir_mkdir }	},
	{ VOPNAME_READDIR,	{ .vop_readdir = gfs_vop_readdir }	},
	{ VOPNAME_LOOKUP,	{ .vop_lookup = zfsctl_snapdir_lookup }	},
	{ VOPNAME_SEEK,		{ .vop_seek = fs_seek }			},
	{ VOPNAME_INACTIVE,	{ .vop_inactive = zfsctl_snapdir_inactive } },
	{ VOPNAME_FID,		{ .vop_fid = zfsctl_common_fid }	},
	{ NULL }
};

static const fs_operation_def_t zfsctl_tops_shares[] = {
	{ VOPNAME_OPEN,		{ .vop_open = zfsctl_common_open }	},
	{ VOPNAME_CLOSE,	{ .vop_close = zfsctl_common_close }	},
	{ VOPNAME_IOCTL,	{ .error = fs_inval }			},
	{ VOPNAME_GETATTR,	{ .vop_getattr = zfsctl_shares_getattr } },
	{ VOPNAME_ACCESS,	{ .vop_access = zfsctl_common_access }	},
	{ VOPNAME_READDIR,	{ .vop_readdir = zfsctl_shares_readdir } },
	{ VOPNAME_LOOKUP,	{ .vop_lookup = zfsctl_shares_lookup }	},
	{ VOPNAME_SEEK,		{ .vop_seek = fs_seek }			},
	{ VOPNAME_INACTIVE,	{ .vop_inactive = gfs_vop_inactive } },
	{ VOPNAME_FID,		{ .vop_fid = zfsctl_shares_fid } },
	{ NULL }
};
#else	/* !sun */
static struct vop_vector zfsctl_ops_snapdir = {
	.vop_default =	&default_vnodeops,
	.vop_open =	zfsctl_common_open,
	.vop_close =	zfsctl_common_close,
	.vop_ioctl =	VOP_EINVAL,
	.vop_getattr =	zfsctl_snapdir_getattr,
	.vop_access =	zfsctl_common_access,
	.vop_mkdir =	zfsctl_freebsd_snapdir_mkdir,
	.vop_readdir =	gfs_vop_readdir,
	.vop_lookup =	zfsctl_snapdir_lookup,
	.vop_inactive =	zfsctl_snapdir_inactive,
	.vop_reclaim =	zfsctl_common_reclaim,
	.vop_fid =	zfsctl_common_fid,
};

static struct vop_vector zfsctl_ops_shares = {
	.vop_default =	&default_vnodeops,
	.vop_open =	zfsctl_common_open,
	.vop_close =	zfsctl_common_close,
	.vop_ioctl =	VOP_EINVAL,
	.vop_getattr =	zfsctl_shares_getattr,
	.vop_access =	zfsctl_common_access,
	.vop_readdir =	zfsctl_shares_readdir,
	.vop_lookup =	zfsctl_shares_lookup,
	.vop_inactive =	gfs_vop_inactive,
	.vop_reclaim =	zfsctl_common_reclaim,
	.vop_fid =	zfsctl_shares_fid,
};
#endif	/* !sun */

/*
 * pvp is the GFS vnode '.zfs/snapshot'.
 *
 * This creates a GFS node under '.zfs/snapshot' representing each
 * snapshot.  This newly created GFS node is what we mount snapshot
 * vfs_t's ontop of.
 */
static vnode_t *
zfsctl_snapshot_mknode(vnode_t *pvp, uint64_t objset)
{
	vnode_t *vp;
	zfsctl_node_t *zcp;

	vp = gfs_dir_create(sizeof (zfsctl_node_t), pvp, pvp->v_vfsp,
	    &zfsctl_ops_snapshot, NULL, NULL, MAXNAMELEN, NULL, NULL);
	VN_HOLD(vp);
	zcp = vp->v_data;
	zcp->zc_id = objset;
	VOP_UNLOCK(vp, 0);

	return (vp);
}

static int
zfsctl_snapshot_inactive(ap)
	struct vop_inactive_args /* {
		struct vnode *a_vp;
		struct thread *a_td;
	} */ *ap;
{
	vnode_t *vp = ap->a_vp;
	cred_t *cr = ap->a_td->td_ucred;
	struct vop_inactive_args iap;
	zfsctl_snapdir_t *sdp;
	zfs_snapentry_t *sep, *next;
	int locked;
	vnode_t *dvp;

	if (vp->v_count > 0)
		goto end;

	VERIFY(gfs_dir_lookup(vp, "..", &dvp, cr, 0, NULL, NULL) == 0);
	sdp = dvp->v_data;
	VOP_UNLOCK(dvp, 0);

	if (!(locked = MUTEX_HELD(&sdp->sd_lock)))
		mutex_enter(&sdp->sd_lock);

	ASSERT(!vn_ismntpt(vp));

	sep = avl_first(&sdp->sd_snaps);
	while (sep != NULL) {
		next = AVL_NEXT(&sdp->sd_snaps, sep);

		if (sep->se_root == vp) {
			avl_remove(&sdp->sd_snaps, sep);
			kmem_free(sep->se_name, strlen(sep->se_name) + 1);
			kmem_free(sep, sizeof (zfs_snapentry_t));
			break;
		}
		sep = next;
	}
	ASSERT(sep != NULL);

	if (!locked)
		mutex_exit(&sdp->sd_lock);
	VN_RELE(dvp);

end:
	/*
	 * Dispose of the vnode for the snapshot mount point.
	 * This is safe to do because once this entry has been removed
	 * from the AVL tree, it can't be found again, so cannot become
	 * "active".  If we lookup the same name again we will end up
	 * creating a new vnode.
	 */
	iap.a_vp = vp;
	return (gfs_vop_inactive(&iap));
}

static int
zfsctl_traverse_begin(vnode_t **vpp, int lktype)
{

	VN_HOLD(*vpp);
	/* Snapshot should be already mounted, but just in case. */
	if (vn_mountedvfs(*vpp) == NULL)
		return (ENOENT);
	return (traverse(vpp, lktype));
}

static void
zfsctl_traverse_end(vnode_t *vp, int err)
{

	if (err == 0)
		vput(vp);
	else
		VN_RELE(vp);
}

static int
zfsctl_snapshot_getattr(ap)
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
	} */ *ap;
{
	vnode_t *vp = ap->a_vp;
	int err;

	err = zfsctl_traverse_begin(&vp, LK_SHARED | LK_RETRY);
	if (err == 0)
		err = VOP_GETATTR(vp, ap->a_vap, ap->a_cred);
	zfsctl_traverse_end(vp, err);
	return (err);
}

static int
zfsctl_snapshot_fid(ap)
	struct vop_fid_args /* {
		struct vnode *a_vp;
		struct fid *a_fid;
	} */ *ap;
{
	vnode_t *vp = ap->a_vp;
	int err;

	err = zfsctl_traverse_begin(&vp, LK_SHARED | LK_RETRY);
	if (err == 0)
		err = VOP_VPTOFH(vp, (void *)ap->a_fid);
	zfsctl_traverse_end(vp, err);
	return (err);
}

static int
zfsctl_snapshot_lookup(ap)
	struct vop_lookup_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap;
{
	vnode_t *dvp = ap->a_dvp;
	vnode_t **vpp = ap->a_vpp;
	struct componentname *cnp = ap->a_cnp;
	cred_t *cr = ap->a_cnp->cn_cred;
	zfsvfs_t *zfsvfs = dvp->v_vfsp->vfs_data;
	int error;

	if (cnp->cn_namelen != 2 || cnp->cn_nameptr[0] != '.' ||
	    cnp->cn_nameptr[1] != '.') {
		return (ENOENT);
	}

	ASSERT(dvp->v_type == VDIR);
	ASSERT(zfsvfs->z_ctldir != NULL);

	error = zfsctl_root_lookup(zfsvfs->z_ctldir, "snapshot", vpp,
	    NULL, 0, NULL, cr, NULL, NULL, NULL);
	if (error == 0)
		vn_lock(*vpp, LK_EXCLUSIVE | LK_RETRY);
	return (error);
}

static int
zfsctl_snapshot_vptocnp(struct vop_vptocnp_args *ap)
{
	zfsvfs_t *zfsvfs = ap->a_vp->v_vfsp->vfs_data;
	vnode_t *dvp, *vp;
	zfsctl_snapdir_t *sdp;
	zfs_snapentry_t *sep;
	int error;

	ASSERT(zfsvfs->z_ctldir != NULL);
	error = zfsctl_root_lookup(zfsvfs->z_ctldir, "snapshot", &dvp,
	    NULL, 0, NULL, kcred, NULL, NULL, NULL);
	if (error != 0)
		return (error);
	sdp = dvp->v_data;

	mutex_enter(&sdp->sd_lock);
	sep = avl_first(&sdp->sd_snaps);
	while (sep != NULL) {
		vp = sep->se_root;
		if (vp == ap->a_vp)
			break;
		sep = AVL_NEXT(&sdp->sd_snaps, sep);
	}
	if (sep == NULL) {
		mutex_exit(&sdp->sd_lock);
		error = ENOENT;
	} else {
		size_t len;

		len = strlen(sep->se_name);
		*ap->a_buflen -= len;
		bcopy(sep->se_name, ap->a_buf + *ap->a_buflen, len);
		mutex_exit(&sdp->sd_lock);
		vref(dvp);
		*ap->a_vpp = dvp;
	}
	VN_RELE(dvp);

	return (error);
}

/*
 * These VP's should never see the light of day.  They should always
 * be covered.
 */
static struct vop_vector zfsctl_ops_snapshot = {
	.vop_default =	&default_vnodeops,
	.vop_inactive =	zfsctl_snapshot_inactive,
	.vop_lookup =	zfsctl_snapshot_lookup,
	.vop_reclaim =	zfsctl_common_reclaim,
	.vop_getattr =	zfsctl_snapshot_getattr,
	.vop_fid =	zfsctl_snapshot_fid,
	.vop_vptocnp =	zfsctl_snapshot_vptocnp,
};

int
zfsctl_lookup_objset(vfs_t *vfsp, uint64_t objsetid, zfsvfs_t **zfsvfsp)
{
	zfsvfs_t *zfsvfs = vfsp->vfs_data;
	vnode_t *dvp, *vp;
	zfsctl_snapdir_t *sdp;
	zfsctl_node_t *zcp;
	zfs_snapentry_t *sep;
	int error;

	ASSERT(zfsvfs->z_ctldir != NULL);
	error = zfsctl_root_lookup(zfsvfs->z_ctldir, "snapshot", &dvp,
	    NULL, 0, NULL, kcred, NULL, NULL, NULL);
	if (error != 0)
		return (error);
	sdp = dvp->v_data;

	mutex_enter(&sdp->sd_lock);
	sep = avl_first(&sdp->sd_snaps);
	while (sep != NULL) {
		vp = sep->se_root;
		zcp = vp->v_data;
		if (zcp->zc_id == objsetid)
			break;

		sep = AVL_NEXT(&sdp->sd_snaps, sep);
	}

	if (sep != NULL) {
		VN_HOLD(vp);
		/*
		 * Return the mounted root rather than the covered mount point.
		 * Takes the GFS vnode at .zfs/snapshot/<snapshot objsetid>
		 * and returns the ZFS vnode mounted on top of the GFS node.
		 * This ZFS vnode is the root of the vfs for objset 'objsetid'.
		 */
		error = traverse(&vp, LK_SHARED | LK_RETRY);
		if (error == 0) {
			if (vp == sep->se_root)
				error = SET_ERROR(EINVAL);
			else
				*zfsvfsp = VTOZ(vp)->z_zfsvfs;
		}
		mutex_exit(&sdp->sd_lock);
		if (error == 0)
			VN_URELE(vp);
		else
			VN_RELE(vp);
	} else {
		error = SET_ERROR(EINVAL);
		mutex_exit(&sdp->sd_lock);
	}

	VN_RELE(dvp);

	return (error);
}

/*
 * Unmount any snapshots for the given filesystem.  This is called from
 * zfs_umount() - if we have a ctldir, then go through and unmount all the
 * snapshots.
 */
int
zfsctl_umount_snapshots(vfs_t *vfsp, int fflags, cred_t *cr)
{
	zfsvfs_t *zfsvfs = vfsp->vfs_data;
	vnode_t *dvp;
	zfsctl_snapdir_t *sdp;
	zfs_snapentry_t *sep, *next;
	int error;

	ASSERT(zfsvfs->z_ctldir != NULL);
	error = zfsctl_root_lookup(zfsvfs->z_ctldir, "snapshot", &dvp,
	    NULL, 0, NULL, cr, NULL, NULL, NULL);
	if (error != 0)
		return (error);
	sdp = dvp->v_data;

	mutex_enter(&sdp->sd_lock);

	sep = avl_first(&sdp->sd_snaps);
	while (sep != NULL) {
		next = AVL_NEXT(&sdp->sd_snaps, sep);

		/*
		 * If this snapshot is not mounted, then it must
		 * have just been unmounted by somebody else, and
		 * will be cleaned up by zfsctl_snapdir_inactive().
		 */
		if (vn_ismntpt(sep->se_root)) {
			error = zfsctl_unmount_snap(sep, fflags, cr);
			if (error) {
				avl_index_t where;

				/*
				 * Before reinserting snapshot to the tree,
				 * check if it was actually removed. For example
				 * when snapshot mount point is busy, we will
				 * have an error here, but there will be no need
				 * to reinsert snapshot.
				 */
				if (avl_find(&sdp->sd_snaps, sep, &where) == NULL)
					avl_insert(&sdp->sd_snaps, sep, where);
				break;
			}
		}
		sep = next;
	}

	mutex_exit(&sdp->sd_lock);
	VN_RELE(dvp);

	return (error);
}
