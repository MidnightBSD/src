/*	$NetBSD: tmpfs_vnops.c,v 1.39 2007/07/23 15:41:01 jmmv Exp $	*/

/*-
 * Copyright (c) 2005, 2006 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Julio M. Merino Vidal, developed as part of Google's Summer of Code
 * 2005 program.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * tmpfs vnode interface.
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/lockf.h>
#include <sys/namei.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/sf_buf.h>
#include <sys/stat.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/unistd.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>

#include <machine/_inttypes.h>

#include <fs/fifofs/fifo.h>
#include <fs/tmpfs/tmpfs_vnops.h>
#include <fs/tmpfs/tmpfs.h>

SYSCTL_DECL(_vfs_tmpfs);

static volatile int tmpfs_rename_restarts;
SYSCTL_INT(_vfs_tmpfs, OID_AUTO, rename_restarts, CTLFLAG_RD,
    __DEVOLATILE(int *, &tmpfs_rename_restarts), 0,
    "Times rename had to restart due to lock contention");

/* --------------------------------------------------------------------- */

static int
tmpfs_lookup(struct vop_cachedlookup_args *v)
{
	struct vnode *dvp = v->a_dvp;
	struct vnode **vpp = v->a_vpp;
	struct componentname *cnp = v->a_cnp;

	int error;
	struct tmpfs_dirent *de;
	struct tmpfs_node *dnode;

	dnode = VP_TO_TMPFS_DIR(dvp);
	*vpp = NULLVP;

	/* Check accessibility of requested node as a first step. */
	error = VOP_ACCESS(dvp, VEXEC, cnp->cn_cred, cnp->cn_thread);
	if (error != 0)
		goto out;

	/* We cannot be requesting the parent directory of the root node. */
	MPASS(IMPLIES(dnode->tn_type == VDIR &&
	    dnode->tn_dir.tn_parent == dnode,
	    !(cnp->cn_flags & ISDOTDOT)));

	TMPFS_ASSERT_LOCKED(dnode);
	if (dnode->tn_dir.tn_parent == NULL) {
		error = ENOENT;
		goto out;
	}
	if (cnp->cn_flags & ISDOTDOT) {
		int ltype = 0;

		ltype = VOP_ISLOCKED(dvp);
		vhold(dvp);
		VOP_UNLOCK(dvp, 0);
		/* Allocate a new vnode on the matching entry. */
		error = tmpfs_alloc_vp(dvp->v_mount, dnode->tn_dir.tn_parent,
		    cnp->cn_lkflags, vpp);

		vn_lock(dvp, ltype | LK_RETRY);
		vdrop(dvp);
	} else if (cnp->cn_namelen == 1 && cnp->cn_nameptr[0] == '.') {
		VREF(dvp);
		*vpp = dvp;
		error = 0;
	} else {
		de = tmpfs_dir_lookup(dnode, NULL, cnp);
		if (de != NULL && de->td_node == NULL)
			cnp->cn_flags |= ISWHITEOUT;
		if (de == NULL || de->td_node == NULL) {
			/* The entry was not found in the directory.
			 * This is OK if we are creating or renaming an
			 * entry and are working on the last component of
			 * the path name. */
			if ((cnp->cn_flags & ISLASTCN) &&
			    (cnp->cn_nameiop == CREATE || \
			    cnp->cn_nameiop == RENAME ||
			    (cnp->cn_nameiop == DELETE &&
			    cnp->cn_flags & DOWHITEOUT &&
			    cnp->cn_flags & ISWHITEOUT))) {
				error = VOP_ACCESS(dvp, VWRITE, cnp->cn_cred,
				    cnp->cn_thread);
				if (error != 0)
					goto out;

				/* Keep the component name in the buffer for
				 * future uses. */
				cnp->cn_flags |= SAVENAME;

				error = EJUSTRETURN;
			} else
				error = ENOENT;
		} else {
			struct tmpfs_node *tnode;

			/* The entry was found, so get its associated
			 * tmpfs_node. */
			tnode = de->td_node;

			/* If we are not at the last path component and
			 * found a non-directory or non-link entry (which
			 * may itself be pointing to a directory), raise
			 * an error. */
			if ((tnode->tn_type != VDIR &&
			    tnode->tn_type != VLNK) &&
			    !(cnp->cn_flags & ISLASTCN)) {
				error = ENOTDIR;
				goto out;
			}

			/* If we are deleting or renaming the entry, keep
			 * track of its tmpfs_dirent so that it can be
			 * easily deleted later. */
			if ((cnp->cn_flags & ISLASTCN) &&
			    (cnp->cn_nameiop == DELETE ||
			    cnp->cn_nameiop == RENAME)) {
				error = VOP_ACCESS(dvp, VWRITE, cnp->cn_cred,
				    cnp->cn_thread);
				if (error != 0)
					goto out;

				/* Allocate a new vnode on the matching entry. */
				error = tmpfs_alloc_vp(dvp->v_mount, tnode,
						cnp->cn_lkflags, vpp);
				if (error != 0)
					goto out;

				if ((dnode->tn_mode & S_ISTXT) &&
				  VOP_ACCESS(dvp, VADMIN, cnp->cn_cred, cnp->cn_thread) &&
				  VOP_ACCESS(*vpp, VADMIN, cnp->cn_cred, cnp->cn_thread)) {
					error = EPERM;
					vput(*vpp);
					*vpp = NULL;
					goto out;
				}
				cnp->cn_flags |= SAVENAME;
			} else {
				error = tmpfs_alloc_vp(dvp->v_mount, tnode,
						cnp->cn_lkflags, vpp);
			}
		}
	}

	/* Store the result of this lookup in the cache.  Avoid this if the
	 * request was for creation, as it does not improve timings on
	 * emprical tests. */
	if ((cnp->cn_flags & MAKEENTRY) && cnp->cn_nameiop != CREATE)
		cache_enter(dvp, *vpp, cnp);

out:
	/* If there were no errors, *vpp cannot be null and it must be
	 * locked. */
	MPASS(IFF(error == 0, *vpp != NULLVP && VOP_ISLOCKED(*vpp)));

	return error;
}

/* --------------------------------------------------------------------- */

static int
tmpfs_create(struct vop_create_args *v)
{
	struct vnode *dvp = v->a_dvp;
	struct vnode **vpp = v->a_vpp;
	struct componentname *cnp = v->a_cnp;
	struct vattr *vap = v->a_vap;

	MPASS(vap->va_type == VREG || vap->va_type == VSOCK);

	return tmpfs_alloc_file(dvp, vpp, vap, cnp, NULL);
}
/* --------------------------------------------------------------------- */

static int
tmpfs_mknod(struct vop_mknod_args *v)
{
	struct vnode *dvp = v->a_dvp;
	struct vnode **vpp = v->a_vpp;
	struct componentname *cnp = v->a_cnp;
	struct vattr *vap = v->a_vap;

	if (vap->va_type != VBLK && vap->va_type != VCHR &&
	    vap->va_type != VFIFO)
		return EINVAL;

	return tmpfs_alloc_file(dvp, vpp, vap, cnp, NULL);
}

/* --------------------------------------------------------------------- */

static int
tmpfs_open(struct vop_open_args *v)
{
	struct vnode *vp = v->a_vp;
	int mode = v->a_mode;

	int error;
	struct tmpfs_node *node;

	MPASS(VOP_ISLOCKED(vp));

	node = VP_TO_TMPFS_NODE(vp);

	/* The file is still active but all its names have been removed
	 * (e.g. by a "rmdir $(pwd)").  It cannot be opened any more as
	 * it is about to die. */
	if (node->tn_links < 1)
		return (ENOENT);

	/* If the file is marked append-only, deny write requests. */
	if (node->tn_flags & APPEND && (mode & (FWRITE | O_APPEND)) == FWRITE)
		error = EPERM;
	else {
		error = 0;
		vnode_create_vobject(vp, node->tn_size, v->a_td);
	}

	MPASS(VOP_ISLOCKED(vp));
	return error;
}

/* --------------------------------------------------------------------- */

static int
tmpfs_close(struct vop_close_args *v)
{
	struct vnode *vp = v->a_vp;

	MPASS(VOP_ISLOCKED(vp));

	/* Update node times. */
	tmpfs_update(vp);

	return (0);
}

/* --------------------------------------------------------------------- */

int
tmpfs_access(struct vop_access_args *v)
{
	struct vnode *vp = v->a_vp;
	accmode_t accmode = v->a_accmode;
	struct ucred *cred = v->a_cred;

	int error;
	struct tmpfs_node *node;

	MPASS(VOP_ISLOCKED(vp));

	node = VP_TO_TMPFS_NODE(vp);

	switch (vp->v_type) {
	case VDIR:
		/* FALLTHROUGH */
	case VLNK:
		/* FALLTHROUGH */
	case VREG:
		if (accmode & VWRITE && vp->v_mount->mnt_flag & MNT_RDONLY) {
			error = EROFS;
			goto out;
		}
		break;

	case VBLK:
		/* FALLTHROUGH */
	case VCHR:
		/* FALLTHROUGH */
	case VSOCK:
		/* FALLTHROUGH */
	case VFIFO:
		break;

	default:
		error = EINVAL;
		goto out;
	}

	if (accmode & VWRITE && node->tn_flags & IMMUTABLE) {
		error = EPERM;
		goto out;
	}

	error = vaccess(vp->v_type, node->tn_mode, node->tn_uid,
	    node->tn_gid, accmode, cred, NULL);

out:
	MPASS(VOP_ISLOCKED(vp));

	return error;
}

/* --------------------------------------------------------------------- */

int
tmpfs_getattr(struct vop_getattr_args *v)
{
	struct vnode *vp = v->a_vp;
	struct vattr *vap = v->a_vap;

	struct tmpfs_node *node;

	node = VP_TO_TMPFS_NODE(vp);

	tmpfs_update(vp);

	vap->va_type = vp->v_type;
	vap->va_mode = node->tn_mode;
	vap->va_nlink = node->tn_links;
	vap->va_uid = node->tn_uid;
	vap->va_gid = node->tn_gid;
	vap->va_fsid = vp->v_mount->mnt_stat.f_fsid.val[0];
	vap->va_fileid = node->tn_id;
	vap->va_size = node->tn_size;
	vap->va_blocksize = PAGE_SIZE;
	vap->va_atime = node->tn_atime;
	vap->va_mtime = node->tn_mtime;
	vap->va_ctime = node->tn_ctime;
	vap->va_birthtime = node->tn_birthtime;
	vap->va_gen = node->tn_gen;
	vap->va_flags = node->tn_flags;
	vap->va_rdev = (vp->v_type == VBLK || vp->v_type == VCHR) ?
		node->tn_rdev : NODEV;
	vap->va_bytes = round_page(node->tn_size);
	vap->va_filerev = 0;

	return 0;
}

/* --------------------------------------------------------------------- */

/* XXX Should this operation be atomic?  I think it should, but code in
 * XXX other places (e.g., ufs) doesn't seem to be... */
int
tmpfs_setattr(struct vop_setattr_args *v)
{
	struct vnode *vp = v->a_vp;
	struct vattr *vap = v->a_vap;
	struct ucred *cred = v->a_cred;
	struct thread *td = curthread;

	int error;

	MPASS(VOP_ISLOCKED(vp));

	error = 0;

	/* Abort if any unsettable attribute is given. */
	if (vap->va_type != VNON ||
	    vap->va_nlink != VNOVAL ||
	    vap->va_fsid != VNOVAL ||
	    vap->va_fileid != VNOVAL ||
	    vap->va_blocksize != VNOVAL ||
	    vap->va_gen != VNOVAL ||
	    vap->va_rdev != VNOVAL ||
	    vap->va_bytes != VNOVAL)
		error = EINVAL;

	if (error == 0 && (vap->va_flags != VNOVAL))
		error = tmpfs_chflags(vp, vap->va_flags, cred, td);

	if (error == 0 && (vap->va_size != VNOVAL))
		error = tmpfs_chsize(vp, vap->va_size, cred, td);

	if (error == 0 && (vap->va_uid != VNOVAL || vap->va_gid != VNOVAL))
		error = tmpfs_chown(vp, vap->va_uid, vap->va_gid, cred, td);

	if (error == 0 && (vap->va_mode != (mode_t)VNOVAL))
		error = tmpfs_chmod(vp, vap->va_mode, cred, td);

	if (error == 0 && ((vap->va_atime.tv_sec != VNOVAL &&
	    vap->va_atime.tv_nsec != VNOVAL) ||
	    (vap->va_mtime.tv_sec != VNOVAL &&
	    vap->va_mtime.tv_nsec != VNOVAL) ||
	    (vap->va_birthtime.tv_sec != VNOVAL &&
	    vap->va_birthtime.tv_nsec != VNOVAL)))
		error = tmpfs_chtimes(vp, &vap->va_atime, &vap->va_mtime,
			&vap->va_birthtime, vap->va_vaflags, cred, td);

	/* Update the node times.  We give preference to the error codes
	 * generated by this function rather than the ones that may arise
	 * from tmpfs_update. */
	tmpfs_update(vp);

	MPASS(VOP_ISLOCKED(vp));

	return error;
}

/* --------------------------------------------------------------------- */
static int
tmpfs_nocacheread(vm_object_t tobj, vm_pindex_t idx,
    vm_offset_t offset, size_t tlen, struct uio *uio)
{
	vm_page_t	m;
	int		error, rv;

	VM_OBJECT_LOCK(tobj);
	m = vm_page_grab(tobj, idx, VM_ALLOC_WIRED |
	    VM_ALLOC_NORMAL | VM_ALLOC_RETRY);
	if (m->valid != VM_PAGE_BITS_ALL) {
		if (vm_pager_has_page(tobj, idx, NULL, NULL)) {
			rv = vm_pager_get_pages(tobj, &m, 1, 0);
			if (rv != VM_PAGER_OK) {
				vm_page_lock(m);
				vm_page_free(m);
				vm_page_unlock(m);
				VM_OBJECT_UNLOCK(tobj);
				return (EIO);
			}
		} else
			vm_page_zero_invalid(m, TRUE);
	}
	VM_OBJECT_UNLOCK(tobj);
	error = uiomove_fromphys(&m, offset, tlen, uio);
	VM_OBJECT_LOCK(tobj);
	vm_page_lock(m);
	vm_page_unwire(m, TRUE);
	vm_page_unlock(m);
	vm_page_wakeup(m);
	VM_OBJECT_UNLOCK(tobj);

	return (error);
}

static __inline int
tmpfs_nocacheread_buf(vm_object_t tobj, vm_pindex_t idx,
    vm_offset_t offset, size_t tlen, void *buf)
{
	struct uio uio;
	struct iovec iov;

	uio.uio_iovcnt = 1;
	uio.uio_iov = &iov;
	iov.iov_base = buf;
	iov.iov_len = tlen;

	uio.uio_offset = 0;
	uio.uio_resid = tlen;
	uio.uio_rw = UIO_READ;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_td = curthread;

	return (tmpfs_nocacheread(tobj, idx, offset, tlen, &uio));
}

static int
tmpfs_mappedread(vm_object_t vobj, vm_object_t tobj, size_t len, struct uio *uio)
{
	struct sf_buf	*sf;
	vm_pindex_t	idx;
	vm_page_t	m;
	vm_offset_t	offset;
	off_t		addr;
	size_t		tlen;
	char		*ma;
	int		error;

	addr = uio->uio_offset;
	idx = OFF_TO_IDX(addr);
	offset = addr & PAGE_MASK;
	tlen = MIN(PAGE_SIZE - offset, len);

	if ((vobj == NULL) ||
	    (vobj->resident_page_count == 0 && vobj->cache == NULL))
		goto nocache;

	VM_OBJECT_LOCK(vobj);
lookupvpg:
	if (((m = vm_page_lookup(vobj, idx)) != NULL) &&
	    vm_page_is_valid(m, offset, tlen)) {
		if ((m->oflags & VPO_BUSY) != 0) {
			/*
			 * Reference the page before unlocking and sleeping so
			 * that the page daemon is less likely to reclaim it.  
			 */
			vm_page_reference(m);
			vm_page_sleep(m, "tmfsmr");
			goto lookupvpg;
		}
		vm_page_busy(m);
		VM_OBJECT_UNLOCK(vobj);
		error = uiomove_fromphys(&m, offset, tlen, uio);
		VM_OBJECT_LOCK(vobj);
		vm_page_wakeup(m);
		VM_OBJECT_UNLOCK(vobj);
		return	(error);
	} else if (m != NULL && uio->uio_segflg == UIO_NOCOPY) {
		KASSERT(offset == 0,
		    ("unexpected offset in tmpfs_mappedread for sendfile"));
		if ((m->oflags & VPO_BUSY) != 0) {
			/*
			 * Reference the page before unlocking and sleeping so
			 * that the page daemon is less likely to reclaim it.  
			 */
			vm_page_reference(m);
			vm_page_sleep(m, "tmfsmr");
			goto lookupvpg;
		}
		vm_page_busy(m);
		VM_OBJECT_UNLOCK(vobj);
		sched_pin();
		sf = sf_buf_alloc(m, SFB_CPUPRIVATE);
		ma = (char *)sf_buf_kva(sf);
		error = tmpfs_nocacheread_buf(tobj, idx, 0, tlen, ma);
		if (error == 0) {
			if (tlen != PAGE_SIZE)
				bzero(ma + tlen, PAGE_SIZE - tlen);
			uio->uio_offset += tlen;
			uio->uio_resid -= tlen;
		}
		sf_buf_free(sf);
		sched_unpin();
		VM_OBJECT_LOCK(vobj);
		if (error == 0)
			m->valid = VM_PAGE_BITS_ALL;
		vm_page_wakeup(m);
		VM_OBJECT_UNLOCK(vobj);
		return	(error);
	}
	VM_OBJECT_UNLOCK(vobj);
nocache:
	error = tmpfs_nocacheread(tobj, idx, offset, tlen, uio);

	return	(error);
}

static int
tmpfs_read(struct vop_read_args *v)
{
	struct vnode *vp = v->a_vp;
	struct uio *uio = v->a_uio;

	struct tmpfs_node *node;
	vm_object_t uobj;
	size_t len;
	int resid;

	int error = 0;

	node = VP_TO_TMPFS_NODE(vp);

	if (vp->v_type != VREG) {
		error = EISDIR;
		goto out;
	}

	if (uio->uio_offset < 0) {
		error = EINVAL;
		goto out;
	}

	node->tn_status |= TMPFS_NODE_ACCESSED;

	uobj = node->tn_reg.tn_aobj;
	while ((resid = uio->uio_resid) > 0) {
		error = 0;
		if (node->tn_size <= uio->uio_offset)
			break;
		len = MIN(node->tn_size - uio->uio_offset, resid);
		if (len == 0)
			break;
		error = tmpfs_mappedread(vp->v_object, uobj, len, uio);
		if ((error != 0) || (resid == uio->uio_resid))
			break;
	}

out:

	return error;
}

/* --------------------------------------------------------------------- */

static int
tmpfs_mappedwrite(vm_object_t vobj, vm_object_t tobj, size_t len, struct uio *uio)
{
	vm_pindex_t	idx;
	vm_page_t	vpg, tpg;
	vm_offset_t	offset;
	off_t		addr;
	size_t		tlen;
	int		error, rv;

	error = 0;
	
	addr = uio->uio_offset;
	idx = OFF_TO_IDX(addr);
	offset = addr & PAGE_MASK;
	tlen = MIN(PAGE_SIZE - offset, len);

	if ((vobj == NULL) ||
	    (vobj->resident_page_count == 0 && vobj->cache == NULL)) {
		vpg = NULL;
		goto nocache;
	}

	VM_OBJECT_LOCK(vobj);
lookupvpg:
	if (((vpg = vm_page_lookup(vobj, idx)) != NULL) &&
	    vm_page_is_valid(vpg, offset, tlen)) {
		if ((vpg->oflags & VPO_BUSY) != 0) {
			/*
			 * Reference the page before unlocking and sleeping so
			 * that the page daemon is less likely to reclaim it.  
			 */
			vm_page_reference(vpg);
			vm_page_sleep(vpg, "tmfsmw");
			goto lookupvpg;
		}
		vm_page_busy(vpg);
		vm_page_undirty(vpg);
		VM_OBJECT_UNLOCK(vobj);
		error = uiomove_fromphys(&vpg, offset, tlen, uio);
	} else {
		if (__predict_false(vobj->cache != NULL))
			vm_page_cache_free(vobj, idx, idx + 1);
		VM_OBJECT_UNLOCK(vobj);
		vpg = NULL;
	}
nocache:
	VM_OBJECT_LOCK(tobj);
	tpg = vm_page_grab(tobj, idx, VM_ALLOC_WIRED |
	    VM_ALLOC_NORMAL | VM_ALLOC_RETRY);
	if (tpg->valid != VM_PAGE_BITS_ALL) {
		if (vm_pager_has_page(tobj, idx, NULL, NULL)) {
			rv = vm_pager_get_pages(tobj, &tpg, 1, 0);
			if (rv != VM_PAGER_OK) {
				vm_page_lock(tpg);
				vm_page_free(tpg);
				vm_page_unlock(tpg);
				error = EIO;
				goto out;
			}
		} else
			vm_page_zero_invalid(tpg, TRUE);
	}
	VM_OBJECT_UNLOCK(tobj);
	if (vpg == NULL)
		error = uiomove_fromphys(&tpg, offset, tlen, uio);
	else {
		KASSERT(vpg->valid == VM_PAGE_BITS_ALL, ("parts of vpg invalid"));
		pmap_copy_page(vpg, tpg);
	}
	VM_OBJECT_LOCK(tobj);
	if (error == 0) {
		KASSERT(tpg->valid == VM_PAGE_BITS_ALL,
		    ("parts of tpg invalid"));
		vm_page_dirty(tpg);
	}
	vm_page_lock(tpg);
	vm_page_unwire(tpg, TRUE);
	vm_page_unlock(tpg);
	vm_page_wakeup(tpg);
out:
	VM_OBJECT_UNLOCK(tobj);
	if (vpg != NULL) {
		VM_OBJECT_LOCK(vobj);
		vm_page_wakeup(vpg);
		VM_OBJECT_UNLOCK(vobj);
	}

	return	(error);
}

static int
tmpfs_write(struct vop_write_args *v)
{
	struct vnode *vp = v->a_vp;
	struct uio *uio = v->a_uio;
	int ioflag = v->a_ioflag;

	boolean_t extended;
	int error = 0;
	off_t oldsize;
	struct tmpfs_node *node;
	vm_object_t uobj;
	size_t len;
	int resid;

	node = VP_TO_TMPFS_NODE(vp);
	oldsize = node->tn_size;

	if (uio->uio_offset < 0 || vp->v_type != VREG) {
		error = EINVAL;
		goto out;
	}

	if (uio->uio_resid == 0) {
		error = 0;
		goto out;
	}

	if (ioflag & IO_APPEND)
		uio->uio_offset = node->tn_size;

	if (uio->uio_offset + uio->uio_resid >
	  VFS_TO_TMPFS(vp->v_mount)->tm_maxfilesize)
		return (EFBIG);

	if (vn_rlimit_fsize(vp, uio, uio->uio_td))
		return (EFBIG);

	extended = uio->uio_offset + uio->uio_resid > node->tn_size;
	if (extended) {
		error = tmpfs_reg_resize(vp, uio->uio_offset + uio->uio_resid,
		    FALSE);
		if (error != 0)
			goto out;
	}

	uobj = node->tn_reg.tn_aobj;
	while ((resid = uio->uio_resid) > 0) {
		if (node->tn_size <= uio->uio_offset)
			break;
		len = MIN(node->tn_size - uio->uio_offset, resid);
		if (len == 0)
			break;
		error = tmpfs_mappedwrite(vp->v_object, uobj, len, uio);
		if ((error != 0) || (resid == uio->uio_resid))
			break;
	}

	node->tn_status |= TMPFS_NODE_ACCESSED | TMPFS_NODE_MODIFIED |
	    (extended ? TMPFS_NODE_CHANGED : 0);

	if (node->tn_mode & (S_ISUID | S_ISGID)) {
		if (priv_check_cred(v->a_cred, PRIV_VFS_RETAINSUGID, 0))
			node->tn_mode &= ~(S_ISUID | S_ISGID);
	}

	if (error != 0)
		(void)tmpfs_reg_resize(vp, oldsize, TRUE);

out:
	MPASS(IMPLIES(error == 0, uio->uio_resid == 0));
	MPASS(IMPLIES(error != 0, oldsize == node->tn_size));

	return error;
}

/* --------------------------------------------------------------------- */

static int
tmpfs_fsync(struct vop_fsync_args *v)
{
	struct vnode *vp = v->a_vp;

	MPASS(VOP_ISLOCKED(vp));

	tmpfs_update(vp);

	return 0;
}

/* --------------------------------------------------------------------- */

static int
tmpfs_remove(struct vop_remove_args *v)
{
	struct vnode *dvp = v->a_dvp;
	struct vnode *vp = v->a_vp;

	int error;
	struct tmpfs_dirent *de;
	struct tmpfs_mount *tmp;
	struct tmpfs_node *dnode;
	struct tmpfs_node *node;

	MPASS(VOP_ISLOCKED(dvp));
	MPASS(VOP_ISLOCKED(vp));

	if (vp->v_type == VDIR) {
		error = EISDIR;
		goto out;
	}

	dnode = VP_TO_TMPFS_DIR(dvp);
	node = VP_TO_TMPFS_NODE(vp);
	tmp = VFS_TO_TMPFS(vp->v_mount);
	de = tmpfs_dir_lookup(dnode, node, v->a_cnp);
	MPASS(de != NULL);

	/* Files marked as immutable or append-only cannot be deleted. */
	if ((node->tn_flags & (IMMUTABLE | APPEND | NOUNLINK)) ||
	    (dnode->tn_flags & APPEND)) {
		error = EPERM;
		goto out;
	}

	/* Remove the entry from the directory; as it is a file, we do not
	 * have to change the number of hard links of the directory. */
	tmpfs_dir_detach(dvp, de);
	if (v->a_cnp->cn_flags & DOWHITEOUT)
		tmpfs_dir_whiteout_add(dvp, v->a_cnp);

	/* Free the directory entry we just deleted.  Note that the node
	 * referred by it will not be removed until the vnode is really
	 * reclaimed. */
	tmpfs_free_dirent(tmp, de, TRUE);

	node->tn_status |= TMPFS_NODE_ACCESSED | TMPFS_NODE_CHANGED;
	error = 0;

out:

	return error;
}

/* --------------------------------------------------------------------- */

static int
tmpfs_link(struct vop_link_args *v)
{
	struct vnode *dvp = v->a_tdvp;
	struct vnode *vp = v->a_vp;
	struct componentname *cnp = v->a_cnp;

	int error;
	struct tmpfs_dirent *de;
	struct tmpfs_node *node;

	MPASS(VOP_ISLOCKED(dvp));
	MPASS(cnp->cn_flags & HASBUF);
	MPASS(dvp != vp); /* XXX When can this be false? */

	node = VP_TO_TMPFS_NODE(vp);

	/* XXX: Why aren't the following two tests done by the caller? */

	/* Hard links of directories are forbidden. */
	if (vp->v_type == VDIR) {
		error = EPERM;
		goto out;
	}

	/* Cannot create cross-device links. */
	if (dvp->v_mount != vp->v_mount) {
		error = EXDEV;
		goto out;
	}

	/* Ensure that we do not overflow the maximum number of links imposed
	 * by the system. */
	MPASS(node->tn_links <= LINK_MAX);
	if (node->tn_links == LINK_MAX) {
		error = EMLINK;
		goto out;
	}

	/* We cannot create links of files marked immutable or append-only. */
	if (node->tn_flags & (IMMUTABLE | APPEND)) {
		error = EPERM;
		goto out;
	}

	/* Allocate a new directory entry to represent the node. */
	error = tmpfs_alloc_dirent(VFS_TO_TMPFS(vp->v_mount), node,
	    cnp->cn_nameptr, cnp->cn_namelen, &de);
	if (error != 0)
		goto out;

	/* Insert the new directory entry into the appropriate directory. */
	if (cnp->cn_flags & ISWHITEOUT)
		tmpfs_dir_whiteout_remove(dvp, cnp);
	tmpfs_dir_attach(dvp, de);

	/* vp link count has changed, so update node times. */
	node->tn_status |= TMPFS_NODE_CHANGED;
	tmpfs_update(vp);

	error = 0;

out:
	return error;
}

/* --------------------------------------------------------------------- */

/*
 * We acquire all but fdvp locks using non-blocking acquisitions.  If we
 * fail to acquire any lock in the path we will drop all held locks,
 * acquire the new lock in a blocking fashion, and then release it and
 * restart the rename.  This acquire/release step ensures that we do not
 * spin on a lock waiting for release.  On error release all vnode locks
 * and decrement references the way tmpfs_rename() would do.
 */
static int
tmpfs_rename_relock(struct vnode *fdvp, struct vnode **fvpp,
    struct vnode *tdvp, struct vnode **tvpp,
    struct componentname *fcnp, struct componentname *tcnp)
{
	struct vnode *nvp;
	struct mount *mp;
	struct tmpfs_dirent *de;
	int error, restarts = 0;

	VOP_UNLOCK(tdvp, 0);
	if (*tvpp != NULL && *tvpp != tdvp)
		VOP_UNLOCK(*tvpp, 0);
	mp = fdvp->v_mount;

relock:
	restarts += 1;
	error = vn_lock(fdvp, LK_EXCLUSIVE);
	if (error)
		goto releout;
	if (vn_lock(tdvp, LK_EXCLUSIVE | LK_NOWAIT) != 0) {
		VOP_UNLOCK(fdvp, 0);
		error = vn_lock(tdvp, LK_EXCLUSIVE);
		if (error)
			goto releout;
		VOP_UNLOCK(tdvp, 0);
		goto relock;
	}
	/*
	 * Re-resolve fvp to be certain it still exists and fetch the
	 * correct vnode.
	 */
	de = tmpfs_dir_lookup(VP_TO_TMPFS_DIR(fdvp), NULL, fcnp);
	if (de == NULL) {
		VOP_UNLOCK(fdvp, 0);
		VOP_UNLOCK(tdvp, 0);
		if ((fcnp->cn_flags & ISDOTDOT) != 0 ||
		    (fcnp->cn_namelen == 1 && fcnp->cn_nameptr[0] == '.'))
			error = EINVAL;
		else
			error = ENOENT;
		goto releout;
	}
	error = tmpfs_alloc_vp(mp, de->td_node, LK_EXCLUSIVE | LK_NOWAIT, &nvp);
	if (error != 0) {
		VOP_UNLOCK(fdvp, 0);
		VOP_UNLOCK(tdvp, 0);
		if (error != EBUSY)
			goto releout;
		error = tmpfs_alloc_vp(mp, de->td_node, LK_EXCLUSIVE, &nvp);
		if (error != 0)
			goto releout;
		VOP_UNLOCK(nvp, 0);
		/*
		 * Concurrent rename race.
		 */
		if (nvp == tdvp) {
			vrele(nvp);
			error = EINVAL;
			goto releout;
		}
		vrele(*fvpp);
		*fvpp = nvp;
		goto relock;
	}
	vrele(*fvpp);
	*fvpp = nvp;
	VOP_UNLOCK(*fvpp, 0);
	/*
	 * Re-resolve tvp and acquire the vnode lock if present.
	 */
	de = tmpfs_dir_lookup(VP_TO_TMPFS_DIR(tdvp), NULL, tcnp);
	/*
	 * If tvp disappeared we just carry on.
	 */
	if (de == NULL && *tvpp != NULL) {
		vrele(*tvpp);
		*tvpp = NULL;
	}
	/*
	 * Get the tvp ino if the lookup succeeded.  We may have to restart
	 * if the non-blocking acquire fails.
	 */
	if (de != NULL) {
		nvp = NULL;
		error = tmpfs_alloc_vp(mp, de->td_node,
		    LK_EXCLUSIVE | LK_NOWAIT, &nvp);
		if (*tvpp != NULL)
			vrele(*tvpp);
		*tvpp = nvp;
		if (error != 0) {
			VOP_UNLOCK(fdvp, 0);
			VOP_UNLOCK(tdvp, 0);
			if (error != EBUSY)
				goto releout;
			error = tmpfs_alloc_vp(mp, de->td_node, LK_EXCLUSIVE,
			    &nvp);
			if (error != 0)
				goto releout;
			VOP_UNLOCK(nvp, 0);
			/*
			 * fdvp contains fvp, thus tvp (=fdvp) is not empty.
			 */
			if (nvp == fdvp) {
				error = ENOTEMPTY;
				goto releout;
			}
			goto relock;
		}
	}
	tmpfs_rename_restarts += restarts;

	return (0);

releout:
	vrele(fdvp);
	vrele(*fvpp);
	vrele(tdvp);
	if (*tvpp != NULL)
		vrele(*tvpp);
	tmpfs_rename_restarts += restarts;

	return (error);
}

static int
tmpfs_rename(struct vop_rename_args *v)
{
	struct vnode *fdvp = v->a_fdvp;
	struct vnode *fvp = v->a_fvp;
	struct componentname *fcnp = v->a_fcnp;
	struct vnode *tdvp = v->a_tdvp;
	struct vnode *tvp = v->a_tvp;
	struct componentname *tcnp = v->a_tcnp;
	struct mount *mp = NULL;

	char *newname;
	int error;
	struct tmpfs_dirent *de;
	struct tmpfs_mount *tmp;
	struct tmpfs_node *fdnode;
	struct tmpfs_node *fnode;
	struct tmpfs_node *tnode;
	struct tmpfs_node *tdnode;

	MPASS(VOP_ISLOCKED(tdvp));
	MPASS(IMPLIES(tvp != NULL, VOP_ISLOCKED(tvp)));
	MPASS(fcnp->cn_flags & HASBUF);
	MPASS(tcnp->cn_flags & HASBUF);

	/* Disallow cross-device renames.
	 * XXX Why isn't this done by the caller? */
	if (fvp->v_mount != tdvp->v_mount ||
	    (tvp != NULL && fvp->v_mount != tvp->v_mount)) {
		error = EXDEV;
		goto out;
	}

	/* If source and target are the same file, there is nothing to do. */
	if (fvp == tvp) {
		error = 0;
		goto out;
	}

	/* If we need to move the directory between entries, lock the
	 * source so that we can safely operate on it. */
	if (fdvp != tdvp && fdvp != tvp) {
		if (vn_lock(fdvp, LK_EXCLUSIVE | LK_NOWAIT) != 0) {
			mp = tdvp->v_mount;
			error = vfs_busy(mp, 0);
			if (error != 0) {
				mp = NULL;
				goto out;
			}
			error = tmpfs_rename_relock(fdvp, &fvp, tdvp, &tvp,
			    fcnp, tcnp);
			if (error != 0) {
				vfs_unbusy(mp);
				return (error);
			}
			ASSERT_VOP_ELOCKED(fdvp,
			    "tmpfs_rename: fdvp not locked");
			ASSERT_VOP_ELOCKED(tdvp,
			    "tmpfs_rename: tdvp not locked");
			if (tvp != NULL)
				ASSERT_VOP_ELOCKED(tvp,
				    "tmpfs_rename: tvp not locked");
			if (fvp == tvp) {
				error = 0;
				goto out_locked;
			}
		}
	}

	tmp = VFS_TO_TMPFS(tdvp->v_mount);
	tdnode = VP_TO_TMPFS_DIR(tdvp);
	tnode = (tvp == NULL) ? NULL : VP_TO_TMPFS_NODE(tvp);
	fdnode = VP_TO_TMPFS_DIR(fdvp);
	fnode = VP_TO_TMPFS_NODE(fvp);
	de = tmpfs_dir_lookup(fdnode, fnode, fcnp);

	/* Entry can disappear before we lock fdvp,
	 * also avoid manipulating '.' and '..' entries. */
	if (de == NULL) {
		if ((fcnp->cn_flags & ISDOTDOT) != 0 ||
		    (fcnp->cn_namelen == 1 && fcnp->cn_nameptr[0] == '.'))
			error = EINVAL;
		else
			error = ENOENT;
		goto out_locked;
	}
	MPASS(de->td_node == fnode);

	/* If re-naming a directory to another preexisting directory
	 * ensure that the target directory is empty so that its
	 * removal causes no side effects.
	 * Kern_rename gurantees the destination to be a directory
	 * if the source is one. */
	if (tvp != NULL) {
		MPASS(tnode != NULL);

		if ((tnode->tn_flags & (NOUNLINK | IMMUTABLE | APPEND)) ||
		    (tdnode->tn_flags & (APPEND | IMMUTABLE))) {
			error = EPERM;
			goto out_locked;
		}

		if (fnode->tn_type == VDIR && tnode->tn_type == VDIR) {
			if (tnode->tn_size > 0) {
				error = ENOTEMPTY;
				goto out_locked;
			}
		} else if (fnode->tn_type == VDIR && tnode->tn_type != VDIR) {
			error = ENOTDIR;
			goto out_locked;
		} else if (fnode->tn_type != VDIR && tnode->tn_type == VDIR) {
			error = EISDIR;
			goto out_locked;
		} else {
			MPASS(fnode->tn_type != VDIR &&
				tnode->tn_type != VDIR);
		}
	}

	if ((fnode->tn_flags & (NOUNLINK | IMMUTABLE | APPEND))
	    || (fdnode->tn_flags & (APPEND | IMMUTABLE))) {
		error = EPERM;
		goto out_locked;
	}

	/* Ensure that we have enough memory to hold the new name, if it
	 * has to be changed. */
	if (fcnp->cn_namelen != tcnp->cn_namelen ||
	    bcmp(fcnp->cn_nameptr, tcnp->cn_nameptr, fcnp->cn_namelen) != 0) {
		newname = malloc(tcnp->cn_namelen, M_TMPFSNAME, M_WAITOK);
	} else
		newname = NULL;

	/* If the node is being moved to another directory, we have to do
	 * the move. */
	if (fdnode != tdnode) {
		/* In case we are moving a directory, we have to adjust its
		 * parent to point to the new parent. */
		if (de->td_node->tn_type == VDIR) {
			struct tmpfs_node *n;

			/* Ensure the target directory is not a child of the
			 * directory being moved.  Otherwise, we'd end up
			 * with stale nodes. */
			n = tdnode;
			/* TMPFS_LOCK garanties that no nodes are freed while
			 * traversing the list. Nodes can only be marked as
			 * removed: tn_parent == NULL. */
			TMPFS_LOCK(tmp);
			TMPFS_NODE_LOCK(n);
			while (n != n->tn_dir.tn_parent) {
				struct tmpfs_node *parent;

				if (n == fnode) {
					TMPFS_NODE_UNLOCK(n);
					TMPFS_UNLOCK(tmp);
					error = EINVAL;
					if (newname != NULL)
						    free(newname, M_TMPFSNAME);
					goto out_locked;
				}
				parent = n->tn_dir.tn_parent;
				TMPFS_NODE_UNLOCK(n);
				if (parent == NULL) {
					n = NULL;
					break;
				}
				TMPFS_NODE_LOCK(parent);
				if (parent->tn_dir.tn_parent == NULL) {
					TMPFS_NODE_UNLOCK(parent);
					n = NULL;
					break;
				}
				n = parent;
			}
			TMPFS_UNLOCK(tmp);
			if (n == NULL) {
				error = EINVAL;
				if (newname != NULL)
					    free(newname, M_TMPFSNAME);
				goto out_locked;
			}
			TMPFS_NODE_UNLOCK(n);

			/* Adjust the parent pointer. */
			TMPFS_VALIDATE_DIR(fnode);
			TMPFS_NODE_LOCK(de->td_node);
			de->td_node->tn_dir.tn_parent = tdnode;
			TMPFS_NODE_UNLOCK(de->td_node);

			/* As a result of changing the target of the '..'
			 * entry, the link count of the source and target
			 * directories has to be adjusted. */
			TMPFS_NODE_LOCK(tdnode);
			TMPFS_ASSERT_LOCKED(tdnode);
			tdnode->tn_links++;
			TMPFS_NODE_UNLOCK(tdnode);

			TMPFS_NODE_LOCK(fdnode);
			TMPFS_ASSERT_LOCKED(fdnode);
			fdnode->tn_links--;
			TMPFS_NODE_UNLOCK(fdnode);
		}

		/* Do the move: just remove the entry from the source directory
		 * and insert it into the target one. */
		tmpfs_dir_detach(fdvp, de);
		if (fcnp->cn_flags & DOWHITEOUT)
			tmpfs_dir_whiteout_add(fdvp, fcnp);
		if (tcnp->cn_flags & ISWHITEOUT)
			tmpfs_dir_whiteout_remove(tdvp, tcnp);
		tmpfs_dir_attach(tdvp, de);
	}

	/* If the name has changed, we need to make it effective by changing
	 * it in the directory entry. */
	if (newname != NULL) {
		MPASS(tcnp->cn_namelen <= MAXNAMLEN);

		free(de->td_name, M_TMPFSNAME);
		de->td_namelen = (uint16_t)tcnp->cn_namelen;
		memcpy(newname, tcnp->cn_nameptr, tcnp->cn_namelen);
		de->td_name = newname;

		fnode->tn_status |= TMPFS_NODE_CHANGED;
		tdnode->tn_status |= TMPFS_NODE_MODIFIED;
	}

	/* If we are overwriting an entry, we have to remove the old one
	 * from the target directory. */
	if (tvp != NULL) {
		/* Remove the old entry from the target directory. */
		de = tmpfs_dir_lookup(tdnode, tnode, tcnp);
		tmpfs_dir_detach(tdvp, de);

		/* Free the directory entry we just deleted.  Note that the
		 * node referred by it will not be removed until the vnode is
		 * really reclaimed. */
		tmpfs_free_dirent(VFS_TO_TMPFS(tvp->v_mount), de, TRUE);
	}
	cache_purge(fvp);
	if (tvp != NULL)
		cache_purge(tvp);

	error = 0;

out_locked:
	if (fdvp != tdvp && fdvp != tvp)
		VOP_UNLOCK(fdvp, 0);

out:
	/* Release target nodes. */
	/* XXX: I don't understand when tdvp can be the same as tvp, but
	 * other code takes care of this... */
	if (tdvp == tvp)
		vrele(tdvp);
	else
		vput(tdvp);
	if (tvp != NULL)
		vput(tvp);

	/* Release source nodes. */
	vrele(fdvp);
	vrele(fvp);

	if (mp != NULL)
		vfs_unbusy(mp);

	return error;
}

/* --------------------------------------------------------------------- */

static int
tmpfs_mkdir(struct vop_mkdir_args *v)
{
	struct vnode *dvp = v->a_dvp;
	struct vnode **vpp = v->a_vpp;
	struct componentname *cnp = v->a_cnp;
	struct vattr *vap = v->a_vap;

	MPASS(vap->va_type == VDIR);

	return tmpfs_alloc_file(dvp, vpp, vap, cnp, NULL);
}

/* --------------------------------------------------------------------- */

static int
tmpfs_rmdir(struct vop_rmdir_args *v)
{
	struct vnode *dvp = v->a_dvp;
	struct vnode *vp = v->a_vp;

	int error;
	struct tmpfs_dirent *de;
	struct tmpfs_mount *tmp;
	struct tmpfs_node *dnode;
	struct tmpfs_node *node;

	MPASS(VOP_ISLOCKED(dvp));
	MPASS(VOP_ISLOCKED(vp));

	tmp = VFS_TO_TMPFS(dvp->v_mount);
	dnode = VP_TO_TMPFS_DIR(dvp);
	node = VP_TO_TMPFS_DIR(vp);

	/* Directories with more than two entries ('.' and '..') cannot be
	 * removed. */
	 if (node->tn_size > 0) {
		 error = ENOTEMPTY;
		 goto out;
	 }

	if ((dnode->tn_flags & APPEND)
	    || (node->tn_flags & (NOUNLINK | IMMUTABLE | APPEND))) {
		error = EPERM;
		goto out;
	}

	/* This invariant holds only if we are not trying to remove "..".
	  * We checked for that above so this is safe now. */
	MPASS(node->tn_dir.tn_parent == dnode);

	/* Get the directory entry associated with node (vp).  This was
	 * filled by tmpfs_lookup while looking up the entry. */
	de = tmpfs_dir_lookup(dnode, node, v->a_cnp);
	MPASS(TMPFS_DIRENT_MATCHES(de,
	    v->a_cnp->cn_nameptr,
	    v->a_cnp->cn_namelen));

	/* Check flags to see if we are allowed to remove the directory. */
	if (dnode->tn_flags & APPEND
		|| node->tn_flags & (NOUNLINK | IMMUTABLE | APPEND)) {
		error = EPERM;
		goto out;
	}


	/* Detach the directory entry from the directory (dnode). */
	tmpfs_dir_detach(dvp, de);
	if (v->a_cnp->cn_flags & DOWHITEOUT)
		tmpfs_dir_whiteout_add(dvp, v->a_cnp);

	/* No vnode should be allocated for this entry from this point */
	TMPFS_NODE_LOCK(node);
	TMPFS_ASSERT_ELOCKED(node);
	node->tn_links--;
	node->tn_dir.tn_parent = NULL;
	node->tn_status |= TMPFS_NODE_ACCESSED | TMPFS_NODE_CHANGED | \
	    TMPFS_NODE_MODIFIED;

	TMPFS_NODE_UNLOCK(node);

	TMPFS_NODE_LOCK(dnode);
	TMPFS_ASSERT_ELOCKED(dnode);
	dnode->tn_links--;
	dnode->tn_status |= TMPFS_NODE_ACCESSED | \
	    TMPFS_NODE_CHANGED | TMPFS_NODE_MODIFIED;
	TMPFS_NODE_UNLOCK(dnode);

	cache_purge(dvp);
	cache_purge(vp);

	/* Free the directory entry we just deleted.  Note that the node
	 * referred by it will not be removed until the vnode is really
	 * reclaimed. */
	tmpfs_free_dirent(tmp, de, TRUE);

	/* Release the deleted vnode (will destroy the node, notify
	 * interested parties and clean it from the cache). */

	dnode->tn_status |= TMPFS_NODE_CHANGED;
	tmpfs_update(dvp);

	error = 0;

out:
	return error;
}

/* --------------------------------------------------------------------- */

static int
tmpfs_symlink(struct vop_symlink_args *v)
{
	struct vnode *dvp = v->a_dvp;
	struct vnode **vpp = v->a_vpp;
	struct componentname *cnp = v->a_cnp;
	struct vattr *vap = v->a_vap;
	char *target = v->a_target;

#ifdef notyet /* XXX FreeBSD BUG: kern_symlink is not setting VLNK */
	MPASS(vap->va_type == VLNK);
#else
	vap->va_type = VLNK;
#endif

	return tmpfs_alloc_file(dvp, vpp, vap, cnp, target);
}

/* --------------------------------------------------------------------- */

static int
tmpfs_readdir(struct vop_readdir_args *v)
{
	struct vnode *vp = v->a_vp;
	struct uio *uio = v->a_uio;
	int *eofflag = v->a_eofflag;
	u_long **cookies = v->a_cookies;
	int *ncookies = v->a_ncookies;

	int error;
	off_t startoff;
	off_t cnt = 0;
	struct tmpfs_node *node;

	/* This operation only makes sense on directory nodes. */
	if (vp->v_type != VDIR)
		return ENOTDIR;

	node = VP_TO_TMPFS_DIR(vp);

	startoff = uio->uio_offset;

	if (uio->uio_offset == TMPFS_DIRCOOKIE_DOT) {
		error = tmpfs_dir_getdotdent(node, uio);
		if (error != 0)
			goto outok;
		cnt++;
	}

	if (uio->uio_offset == TMPFS_DIRCOOKIE_DOTDOT) {
		error = tmpfs_dir_getdotdotdent(node, uio);
		if (error != 0)
			goto outok;
		cnt++;
	}

	error = tmpfs_dir_getdents(node, uio, &cnt);

outok:
	MPASS(error >= -1);

	if (error == -1)
		error = (cnt != 0) ? 0 : EINVAL;

	if (eofflag != NULL)
		*eofflag =
		    (error == 0 && uio->uio_offset == TMPFS_DIRCOOKIE_EOF);

	/* Update NFS-related variables. */
	if (error == 0 && cookies != NULL && ncookies != NULL) {
		off_t i;
		off_t off = startoff;
		struct tmpfs_dirent *de = NULL;

		*ncookies = cnt;
		*cookies = malloc(cnt * sizeof(off_t), M_TEMP, M_WAITOK);

		for (i = 0; i < cnt; i++) {
			MPASS(off != TMPFS_DIRCOOKIE_EOF);
			if (off == TMPFS_DIRCOOKIE_DOT) {
				off = TMPFS_DIRCOOKIE_DOTDOT;
			} else {
				if (off == TMPFS_DIRCOOKIE_DOTDOT) {
					de = TAILQ_FIRST(&node->tn_dir.tn_dirhead);
				} else if (de != NULL) {
					de = TAILQ_NEXT(de, td_entries);
				} else {
					de = tmpfs_dir_lookupbycookie(node,
					    off);
					MPASS(de != NULL);
					de = TAILQ_NEXT(de, td_entries);
				}
				if (de == NULL)
					off = TMPFS_DIRCOOKIE_EOF;
				else
					off = tmpfs_dircookie(de);
			}

			(*cookies)[i] = off;
		}
		MPASS(uio->uio_offset == off);
	}

	return error;
}

/* --------------------------------------------------------------------- */

static int
tmpfs_readlink(struct vop_readlink_args *v)
{
	struct vnode *vp = v->a_vp;
	struct uio *uio = v->a_uio;

	int error;
	struct tmpfs_node *node;

	MPASS(uio->uio_offset == 0);
	MPASS(vp->v_type == VLNK);

	node = VP_TO_TMPFS_NODE(vp);

	error = uiomove(node->tn_link, MIN(node->tn_size, uio->uio_resid),
	    uio);
	node->tn_status |= TMPFS_NODE_ACCESSED;

	return error;
}

/* --------------------------------------------------------------------- */

static int
tmpfs_inactive(struct vop_inactive_args *v)
{
	struct vnode *vp = v->a_vp;
	struct thread *l = v->a_td;

	struct tmpfs_node *node;

	MPASS(VOP_ISLOCKED(vp));

	node = VP_TO_TMPFS_NODE(vp);

	if (node->tn_links == 0)
		vrecycle(vp, l);

	return 0;
}

/* --------------------------------------------------------------------- */

int
tmpfs_reclaim(struct vop_reclaim_args *v)
{
	struct vnode *vp = v->a_vp;

	struct tmpfs_mount *tmp;
	struct tmpfs_node *node;

	node = VP_TO_TMPFS_NODE(vp);
	tmp = VFS_TO_TMPFS(vp->v_mount);

	vnode_destroy_vobject(vp);
	cache_purge(vp);

	TMPFS_NODE_LOCK(node);
	TMPFS_ASSERT_ELOCKED(node);
	tmpfs_free_vp(vp);

	/* If the node referenced by this vnode was deleted by the user,
	 * we must free its associated data structures (now that the vnode
	 * is being reclaimed). */
	if (node->tn_links == 0 &&
	    (node->tn_vpstate & TMPFS_VNODE_ALLOCATING) == 0) {
		node->tn_vpstate = TMPFS_VNODE_DOOMED;
		TMPFS_NODE_UNLOCK(node);
		tmpfs_free_node(tmp, node);
	} else
		TMPFS_NODE_UNLOCK(node);

	MPASS(vp->v_data == NULL);
	return 0;
}

/* --------------------------------------------------------------------- */

static int
tmpfs_print(struct vop_print_args *v)
{
	struct vnode *vp = v->a_vp;

	struct tmpfs_node *node;

	node = VP_TO_TMPFS_NODE(vp);

	printf("tag VT_TMPFS, tmpfs_node %p, flags 0x%x, links %d\n",
	    node, node->tn_flags, node->tn_links);
	printf("\tmode 0%o, owner %d, group %d, size %" PRIdMAX
	    ", status 0x%x\n",
	    node->tn_mode, node->tn_uid, node->tn_gid,
	    (uintmax_t)node->tn_size, node->tn_status);

	if (vp->v_type == VFIFO)
		fifo_printinfo(vp);

	printf("\n");

	return 0;
}

/* --------------------------------------------------------------------- */

static int
tmpfs_pathconf(struct vop_pathconf_args *v)
{
	int name = v->a_name;
	register_t *retval = v->a_retval;

	int error;

	error = 0;

	switch (name) {
	case _PC_LINK_MAX:
		*retval = LINK_MAX;
		break;

	case _PC_NAME_MAX:
		*retval = NAME_MAX;
		break;

	case _PC_PATH_MAX:
		*retval = PATH_MAX;
		break;

	case _PC_PIPE_BUF:
		*retval = PIPE_BUF;
		break;

	case _PC_CHOWN_RESTRICTED:
		*retval = 1;
		break;

	case _PC_NO_TRUNC:
		*retval = 1;
		break;

	case _PC_SYNC_IO:
		*retval = 1;
		break;

	case _PC_FILESIZEBITS:
		*retval = 0; /* XXX Don't know which value should I return. */
		break;

	default:
		error = EINVAL;
	}

	return error;
}

static int
tmpfs_vptofh(struct vop_vptofh_args *ap)
{
	struct tmpfs_fid *tfhp;
	struct tmpfs_node *node;

	tfhp = (struct tmpfs_fid *)ap->a_fhp;
	node = VP_TO_TMPFS_NODE(ap->a_vp);

	tfhp->tf_len = sizeof(struct tmpfs_fid);
	tfhp->tf_id = node->tn_id;
	tfhp->tf_gen = node->tn_gen;

	return (0);
}

static int
tmpfs_whiteout(struct vop_whiteout_args *ap)
{
	struct vnode *dvp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
	struct tmpfs_dirent *de;

	switch (ap->a_flags) {
	case LOOKUP:
		return (0);
	case CREATE:
		de = tmpfs_dir_lookup(VP_TO_TMPFS_DIR(dvp), NULL, cnp);
		if (de != NULL)
			return (de->td_node == NULL ? 0 : EEXIST);
		return (tmpfs_dir_whiteout_add(dvp, cnp));
	case DELETE:
		tmpfs_dir_whiteout_remove(dvp, cnp);
		return (0);
	default:
		panic("tmpfs_whiteout: unknown op");
	}
}

/* --------------------------------------------------------------------- */

/*
 * vnode operations vector used for files stored in a tmpfs file system.
 */
struct vop_vector tmpfs_vnodeop_entries = {
	.vop_default =			&default_vnodeops,
	.vop_lookup =			vfs_cache_lookup,
	.vop_cachedlookup =		tmpfs_lookup,
	.vop_create =			tmpfs_create,
	.vop_mknod =			tmpfs_mknod,
	.vop_open =			tmpfs_open,
	.vop_close =			tmpfs_close,
	.vop_access =			tmpfs_access,
	.vop_getattr =			tmpfs_getattr,
	.vop_setattr =			tmpfs_setattr,
	.vop_read =			tmpfs_read,
	.vop_write =			tmpfs_write,
	.vop_fsync =			tmpfs_fsync,
	.vop_remove =			tmpfs_remove,
	.vop_link =			tmpfs_link,
	.vop_rename =			tmpfs_rename,
	.vop_mkdir =			tmpfs_mkdir,
	.vop_rmdir =			tmpfs_rmdir,
	.vop_symlink =			tmpfs_symlink,
	.vop_readdir =			tmpfs_readdir,
	.vop_readlink =			tmpfs_readlink,
	.vop_inactive =			tmpfs_inactive,
	.vop_reclaim =			tmpfs_reclaim,
	.vop_print =			tmpfs_print,
	.vop_pathconf =			tmpfs_pathconf,
	.vop_vptofh =			tmpfs_vptofh,
	.vop_whiteout =			tmpfs_whiteout,
	.vop_bmap =			VOP_EOPNOTSUPP,
};

