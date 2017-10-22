/*-
 * Copyright (c) 1999-2001 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed by Robert Watson for the TrustedBSD Project.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/7.0.0/sys/kern/vfs_extattr.c 165474 2006-12-23 00:30:03Z rwatson $");

#include "opt_mac.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/sysproto.h>
#include <sys/namei.h>
#include <sys/filedesc.h>
#include <sys/limits.h>
#include <sys/vnode.h>
#include <sys/proc.h>
#include <sys/extattr.h>

#include <security/audit/audit.h>
#include <security/mac/mac_framework.h>

/*
 * Syscall to push extended attribute configuration information into the VFS.
 * Accepts a path, which it converts to a mountpoint, as well as a command
 * (int cmd), and attribute name and misc data.
 *
 * Currently this is used only by UFS1 extended attributes.
 */
int
extattrctl(td, uap)
	struct thread *td;
	struct extattrctl_args /* {
		const char *path;
		int cmd;
		const char *filename;
		int attrnamespace;
		const char *attrname;
	} */ *uap;
{
	struct vnode *filename_vp;
	struct nameidata nd;
	struct mount *mp, *mp_writable;
	char attrname[EXTATTR_MAXNAMELEN];
	int vfslocked, fnvfslocked, error;

	AUDIT_ARG(cmd, uap->cmd);
	AUDIT_ARG(value, uap->attrnamespace);
	/*
	 * uap->attrname is not always defined.  We check again later when we
	 * invoke the VFS call so as to pass in NULL there if needed.
	 */
	if (uap->attrname != NULL) {
		error = copyinstr(uap->attrname, attrname, EXTATTR_MAXNAMELEN,
		    NULL);
		if (error)
			return (error);
	}
	AUDIT_ARG(text, attrname);

	vfslocked = fnvfslocked = 0;
	/*
	 * uap->filename is not always defined.  If it is, grab a vnode lock,
	 * which VFS_EXTATTRCTL() will later release.
	 */
	filename_vp = NULL;
	if (uap->filename != NULL) {
		NDINIT(&nd, LOOKUP, MPSAFE | FOLLOW | LOCKLEAF |
		    AUDITVNODE2, UIO_USERSPACE, uap->filename, td);
		error = namei(&nd);
		if (error)
			return (error);
		fnvfslocked = NDHASGIANT(&nd);
		filename_vp = nd.ni_vp;
		NDFREE(&nd, NDF_NO_VP_RELE | NDF_NO_VP_UNLOCK);
	}

	/* uap->path is always defined. */
	NDINIT(&nd, LOOKUP, MPSAFE | FOLLOW | AUDITVNODE1, UIO_USERSPACE,
	    uap->path, td);
	error = namei(&nd);
	if (error) {
		if (filename_vp != NULL)
			vput(filename_vp);
		goto out;
	}
	vfslocked = NDHASGIANT(&nd);
	mp = nd.ni_vp->v_mount;
	error = vn_start_write(nd.ni_vp, &mp_writable, V_WAIT | PCATCH);
	NDFREE(&nd, 0);
	if (error) {
		if (filename_vp != NULL)
			vput(filename_vp);
		goto out;
	}

	error = VFS_EXTATTRCTL(mp, uap->cmd, filename_vp, uap->attrnamespace,
	    uap->attrname != NULL ? attrname : NULL, td);

	vn_finished_write(mp_writable);
	/*
	 * VFS_EXTATTRCTL will have unlocked, but not de-ref'd, filename_vp,
	 * so vrele it if it is defined.
	 */
	if (filename_vp != NULL)
		vrele(filename_vp);
out:
	VFS_UNLOCK_GIANT(fnvfslocked);
	VFS_UNLOCK_GIANT(vfslocked);
	return (error);
}

/*-
 * Set a named extended attribute on a file or directory
 *
 * Arguments: unlocked vnode "vp", attribute namespace "attrnamespace",
 *            kernelspace string pointer "attrname", userspace buffer
 *            pointer "data", buffer length "nbytes", thread "td".
 * Returns: 0 on success, an error number otherwise
 * Locks: none
 * References: vp must be a valid reference for the duration of the call
 */
static int
extattr_set_vp(struct vnode *vp, int attrnamespace, const char *attrname,
    void *data, size_t nbytes, struct thread *td)
{
	struct mount *mp;
	struct uio auio;
	struct iovec aiov;
	ssize_t cnt;
	int error;

	VFS_ASSERT_GIANT(vp->v_mount);
	error = vn_start_write(vp, &mp, V_WAIT | PCATCH);
	if (error)
		return (error);
	VOP_LEASE(vp, td, td->td_ucred, LEASE_WRITE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);

	aiov.iov_base = data;
	aiov.iov_len = nbytes;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = 0;
	if (nbytes > INT_MAX) {
		error = EINVAL;
		goto done;
	}
	auio.uio_resid = nbytes;
	auio.uio_rw = UIO_WRITE;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_td = td;
	cnt = nbytes;

#ifdef MAC
	error = mac_check_vnode_setextattr(td->td_ucred, vp, attrnamespace,
	    attrname, &auio);
	if (error)
		goto done;
#endif

	error = VOP_SETEXTATTR(vp, attrnamespace, attrname, &auio,
	    td->td_ucred, td);
	cnt -= auio.uio_resid;
	td->td_retval[0] = cnt;

done:
	VOP_UNLOCK(vp, 0, td);
	vn_finished_write(mp);
	return (error);
}

int
extattr_set_fd(td, uap)
	struct thread *td;
	struct extattr_set_fd_args /* {
		int fd;
		int attrnamespace;
		const char *attrname;
		void *data;
		size_t nbytes;
	} */ *uap;
{
	struct file *fp;
	char attrname[EXTATTR_MAXNAMELEN];
	int vfslocked, error;

	AUDIT_ARG(fd, uap->fd);
	AUDIT_ARG(value, uap->attrnamespace);
	error = copyinstr(uap->attrname, attrname, EXTATTR_MAXNAMELEN, NULL);
	if (error)
		return (error);
	AUDIT_ARG(text, attrname);

	error = getvnode(td->td_proc->p_fd, uap->fd, &fp);
	if (error)
		return (error);

	vfslocked = VFS_LOCK_GIANT(fp->f_vnode->v_mount);
	error = extattr_set_vp(fp->f_vnode, uap->attrnamespace,
	    attrname, uap->data, uap->nbytes, td);
	fdrop(fp, td);
	VFS_UNLOCK_GIANT(vfslocked);

	return (error);
}

int
extattr_set_file(td, uap)
	struct thread *td;
	struct extattr_set_file_args /* {
		const char *path;
		int attrnamespace;
		const char *attrname;
		void *data;
		size_t nbytes;
	} */ *uap;
{
	struct nameidata nd;
	char attrname[EXTATTR_MAXNAMELEN];
	int vfslocked, error;

	AUDIT_ARG(value, uap->attrnamespace);
	error = copyinstr(uap->attrname, attrname, EXTATTR_MAXNAMELEN, NULL);
	if (error)
		return (error);
	AUDIT_ARG(text, attrname);

	NDINIT(&nd, LOOKUP, MPSAFE | FOLLOW | AUDITVNODE1, UIO_USERSPACE,
	    uap->path, td);
	error = namei(&nd);
	if (error)
		return (error);
	NDFREE(&nd, NDF_ONLY_PNBUF);

	vfslocked = NDHASGIANT(&nd);
	error = extattr_set_vp(nd.ni_vp, uap->attrnamespace, attrname,
	    uap->data, uap->nbytes, td);

	vrele(nd.ni_vp);
	VFS_UNLOCK_GIANT(vfslocked);
	return (error);
}

int
extattr_set_link(td, uap)
	struct thread *td;
	struct extattr_set_link_args /* {
		const char *path;
		int attrnamespace;
		const char *attrname;
		void *data;
		size_t nbytes;
	} */ *uap;
{
	struct nameidata nd;
	char attrname[EXTATTR_MAXNAMELEN];
	int vfslocked, error;

	AUDIT_ARG(value, uap->attrnamespace);
	error = copyinstr(uap->attrname, attrname, EXTATTR_MAXNAMELEN, NULL);
	if (error)
		return (error);
	AUDIT_ARG(text, attrname);

	NDINIT(&nd, LOOKUP, MPSAFE | NOFOLLOW | AUDITVNODE1, UIO_USERSPACE,
	    uap->path, td);
	error = namei(&nd);
	if (error)
		return (error);
	NDFREE(&nd, NDF_ONLY_PNBUF);

	vfslocked = NDHASGIANT(&nd);
	error = extattr_set_vp(nd.ni_vp, uap->attrnamespace, attrname,
	    uap->data, uap->nbytes, td);

	vrele(nd.ni_vp);
	VFS_UNLOCK_GIANT(vfslocked);
	return (error);
}

/*-
 * Get a named extended attribute on a file or directory
 *
 * Arguments: unlocked vnode "vp", attribute namespace "attrnamespace",
 *            kernelspace string pointer "attrname", userspace buffer
 *            pointer "data", buffer length "nbytes", thread "td".
 * Returns: 0 on success, an error number otherwise
 * Locks: none
 * References: vp must be a valid reference for the duration of the call
 */
static int
extattr_get_vp(struct vnode *vp, int attrnamespace, const char *attrname,
    void *data, size_t nbytes, struct thread *td)
{
	struct uio auio, *auiop;
	struct iovec aiov;
	ssize_t cnt;
	size_t size, *sizep;
	int error;

	VFS_ASSERT_GIANT(vp->v_mount);
	VOP_LEASE(vp, td, td->td_ucred, LEASE_READ);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);

	/*
	 * Slightly unusual semantics: if the user provides a NULL data
	 * pointer, they don't want to receive the data, just the maximum
	 * read length.
	 */
	auiop = NULL;
	sizep = NULL;
	cnt = 0;
	if (data != NULL) {
		aiov.iov_base = data;
		aiov.iov_len = nbytes;
		auio.uio_iov = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_offset = 0;
		if (nbytes > INT_MAX) {
			error = EINVAL;
			goto done;
		}
		auio.uio_resid = nbytes;
		auio.uio_rw = UIO_READ;
		auio.uio_segflg = UIO_USERSPACE;
		auio.uio_td = td;
		auiop = &auio;
		cnt = nbytes;
	} else
		sizep = &size;

#ifdef MAC
	error = mac_check_vnode_getextattr(td->td_ucred, vp, attrnamespace,
	    attrname, &auio);
	if (error)
		goto done;
#endif

	error = VOP_GETEXTATTR(vp, attrnamespace, attrname, auiop, sizep,
	    td->td_ucred, td);

	if (auiop != NULL) {
		cnt -= auio.uio_resid;
		td->td_retval[0] = cnt;
	} else
		td->td_retval[0] = size;

done:
	VOP_UNLOCK(vp, 0, td);
	return (error);
}

int
extattr_get_fd(td, uap)
	struct thread *td;
	struct extattr_get_fd_args /* {
		int fd;
		int attrnamespace;
		const char *attrname;
		void *data;
		size_t nbytes;
	} */ *uap;
{
	struct file *fp;
	char attrname[EXTATTR_MAXNAMELEN];
	int vfslocked, error;

	AUDIT_ARG(fd, uap->fd);
	AUDIT_ARG(value, uap->attrnamespace);
	error = copyinstr(uap->attrname, attrname, EXTATTR_MAXNAMELEN, NULL);
	if (error)
		return (error);
	AUDIT_ARG(text, attrname);

	error = getvnode(td->td_proc->p_fd, uap->fd, &fp);
	if (error)
		return (error);

	vfslocked = VFS_LOCK_GIANT(fp->f_vnode->v_mount);
	error = extattr_get_vp(fp->f_vnode, uap->attrnamespace,
	    attrname, uap->data, uap->nbytes, td);

	fdrop(fp, td);
	VFS_UNLOCK_GIANT(vfslocked);
	return (error);
}

int
extattr_get_file(td, uap)
	struct thread *td;
	struct extattr_get_file_args /* {
		const char *path;
		int attrnamespace;
		const char *attrname;
		void *data;
		size_t nbytes;
	} */ *uap;
{
	struct nameidata nd;
	char attrname[EXTATTR_MAXNAMELEN];
	int vfslocked, error;

	AUDIT_ARG(value, uap->attrnamespace);
	error = copyinstr(uap->attrname, attrname, EXTATTR_MAXNAMELEN, NULL);
	if (error)
		return (error);
	AUDIT_ARG(text, attrname);

	NDINIT(&nd, LOOKUP, MPSAFE | FOLLOW | AUDITVNODE1, UIO_USERSPACE,
	    uap->path, td);
	error = namei(&nd);
	if (error)
		return (error);
	NDFREE(&nd, NDF_ONLY_PNBUF);

	vfslocked = NDHASGIANT(&nd);
	error = extattr_get_vp(nd.ni_vp, uap->attrnamespace, attrname,
	    uap->data, uap->nbytes, td);

	vrele(nd.ni_vp);
	VFS_UNLOCK_GIANT(vfslocked);
	return (error);
}

int
extattr_get_link(td, uap)
	struct thread *td;
	struct extattr_get_link_args /* {
		const char *path;
		int attrnamespace;
		const char *attrname;
		void *data;
		size_t nbytes;
	} */ *uap;
{
	struct nameidata nd;
	char attrname[EXTATTR_MAXNAMELEN];
	int vfslocked, error;

	AUDIT_ARG(value, uap->attrnamespace);
	error = copyinstr(uap->attrname, attrname, EXTATTR_MAXNAMELEN, NULL);
	if (error)
		return (error);
	AUDIT_ARG(text, attrname);

	NDINIT(&nd, LOOKUP, MPSAFE | NOFOLLOW | AUDITVNODE1, UIO_USERSPACE,
	    uap->path, td);
	error = namei(&nd);
	if (error)
		return (error);
	NDFREE(&nd, NDF_ONLY_PNBUF);

	vfslocked = NDHASGIANT(&nd);
	error = extattr_get_vp(nd.ni_vp, uap->attrnamespace, attrname,
	    uap->data, uap->nbytes, td);

	vrele(nd.ni_vp);
	VFS_UNLOCK_GIANT(vfslocked);
	return (error);
}

/*
 * extattr_delete_vp(): Delete a named extended attribute on a file or
 *                      directory
 *
 * Arguments: unlocked vnode "vp", attribute namespace "attrnamespace",
 *            kernelspace string pointer "attrname", proc "p"
 * Returns: 0 on success, an error number otherwise
 * Locks: none
 * References: vp must be a valid reference for the duration of the call
 */
static int
extattr_delete_vp(struct vnode *vp, int attrnamespace, const char *attrname,
    struct thread *td)
{
	struct mount *mp;
	int error;

	VFS_ASSERT_GIANT(vp->v_mount);
	error = vn_start_write(vp, &mp, V_WAIT | PCATCH);
	if (error)
		return (error);
	VOP_LEASE(vp, td, td->td_ucred, LEASE_WRITE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);

#ifdef MAC
	error = mac_check_vnode_deleteextattr(td->td_ucred, vp, attrnamespace,
	    attrname);
	if (error)
		goto done;
#endif

	error = VOP_DELETEEXTATTR(vp, attrnamespace, attrname, td->td_ucred,
	    td);
	if (error == EOPNOTSUPP)
		error = VOP_SETEXTATTR(vp, attrnamespace, attrname, NULL,
		    td->td_ucred, td);
#ifdef MAC
done:
#endif
	VOP_UNLOCK(vp, 0, td);
	vn_finished_write(mp);
	return (error);
}

int
extattr_delete_fd(td, uap)
	struct thread *td;
	struct extattr_delete_fd_args /* {
		int fd;
		int attrnamespace;
		const char *attrname;
	} */ *uap;
{
	struct file *fp;
	char attrname[EXTATTR_MAXNAMELEN];
	int vfslocked, error;

	AUDIT_ARG(fd, uap->fd);
	AUDIT_ARG(value, uap->attrnamespace);
	error = copyinstr(uap->attrname, attrname, EXTATTR_MAXNAMELEN, NULL);
	if (error)
		return (error);
	AUDIT_ARG(text, attrname);

	error = getvnode(td->td_proc->p_fd, uap->fd, &fp);
	if (error)
		return (error);

	vfslocked = VFS_LOCK_GIANT(fp->f_vnode->v_mount);
	error = extattr_delete_vp(fp->f_vnode, uap->attrnamespace,
	    attrname, td);
	fdrop(fp, td);
	VFS_UNLOCK_GIANT(vfslocked);
	return (error);
}

int
extattr_delete_file(td, uap)
	struct thread *td;
	struct extattr_delete_file_args /* {
		const char *path;
		int attrnamespace;
		const char *attrname;
	} */ *uap;
{
	struct nameidata nd;
	char attrname[EXTATTR_MAXNAMELEN];
	int vfslocked, error;

	AUDIT_ARG(value, uap->attrnamespace);
	error = copyinstr(uap->attrname, attrname, EXTATTR_MAXNAMELEN, NULL);
	if (error)
		return(error);
	AUDIT_ARG(text, attrname);

	NDINIT(&nd, LOOKUP, MPSAFE | FOLLOW | AUDITVNODE1, UIO_USERSPACE,
	    uap->path, td);
	error = namei(&nd);
	if (error)
		return(error);
	NDFREE(&nd, NDF_ONLY_PNBUF);

	vfslocked = NDHASGIANT(&nd);
	error = extattr_delete_vp(nd.ni_vp, uap->attrnamespace, attrname, td);
	vrele(nd.ni_vp);
	VFS_UNLOCK_GIANT(vfslocked);
	return(error);
}

int
extattr_delete_link(td, uap)
	struct thread *td;
	struct extattr_delete_link_args /* {
		const char *path;
		int attrnamespace;
		const char *attrname;
	} */ *uap;
{
	struct nameidata nd;
	char attrname[EXTATTR_MAXNAMELEN];
	int vfslocked, error;

	AUDIT_ARG(value, uap->attrnamespace);
	error = copyinstr(uap->attrname, attrname, EXTATTR_MAXNAMELEN, NULL);
	if (error)
		return(error);
	AUDIT_ARG(text, attrname);

	NDINIT(&nd, LOOKUP, MPSAFE | NOFOLLOW | AUDITVNODE1, UIO_USERSPACE,
	    uap->path, td);
	error = namei(&nd);
	if (error)
		return(error);
	NDFREE(&nd, NDF_ONLY_PNBUF);

	vfslocked = NDHASGIANT(&nd);
	error = extattr_delete_vp(nd.ni_vp, uap->attrnamespace, attrname, td);
	vrele(nd.ni_vp);
	VFS_UNLOCK_GIANT(vfslocked);
	return(error);
}

/*-
 * Retrieve a list of extended attributes on a file or directory.
 *
 * Arguments: unlocked vnode "vp", attribute namespace 'attrnamespace",
 *            userspace buffer pointer "data", buffer length "nbytes",
 *            thread "td".
 * Returns: 0 on success, an error number otherwise
 * Locks: none
 * References: vp must be a valid reference for the duration of the call
 */
static int
extattr_list_vp(struct vnode *vp, int attrnamespace, void *data,
    size_t nbytes, struct thread *td)
{
	struct uio auio, *auiop;
	size_t size, *sizep;
	struct iovec aiov;
	ssize_t cnt;
	int error;

	VFS_ASSERT_GIANT(vp->v_mount);
	VOP_LEASE(vp, td, td->td_ucred, LEASE_READ);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);

	auiop = NULL;
	sizep = NULL;
	cnt = 0;
	if (data != NULL) {
		aiov.iov_base = data;
		aiov.iov_len = nbytes;
		auio.uio_iov = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_offset = 0;
		if (nbytes > INT_MAX) {
			error = EINVAL;
			goto done;
		}
		auio.uio_resid = nbytes;
		auio.uio_rw = UIO_READ;
		auio.uio_segflg = UIO_USERSPACE;
		auio.uio_td = td;
		auiop = &auio;
		cnt = nbytes;
	} else
		sizep = &size;

#ifdef MAC
	error = mac_check_vnode_listextattr(td->td_ucred, vp, attrnamespace);
	if (error)
		goto done;
#endif

	error = VOP_LISTEXTATTR(vp, attrnamespace, auiop, sizep,
	    td->td_ucred, td);

	if (auiop != NULL) {
		cnt -= auio.uio_resid;
		td->td_retval[0] = cnt;
	} else
		td->td_retval[0] = size;

done:
	VOP_UNLOCK(vp, 0, td);
	return (error);
}


int
extattr_list_fd(td, uap)
	struct thread *td;
	struct extattr_list_fd_args /* {
		int fd;
		int attrnamespace;
		void *data;
		size_t nbytes;
	} */ *uap;
{
	struct file *fp;
	int vfslocked, error;

	AUDIT_ARG(fd, uap->fd);
	AUDIT_ARG(value, uap->attrnamespace);
	error = getvnode(td->td_proc->p_fd, uap->fd, &fp);
	if (error)
		return (error);

	vfslocked = VFS_LOCK_GIANT(fp->f_vnode->v_mount);
	error = extattr_list_vp(fp->f_vnode, uap->attrnamespace, uap->data,
	    uap->nbytes, td);

	fdrop(fp, td);
	VFS_UNLOCK_GIANT(vfslocked);
	return (error);
}

int
extattr_list_file(td, uap)
	struct thread*td;
	struct extattr_list_file_args /* {
		const char *path;
		int attrnamespace;
		void *data;
		size_t nbytes;
	} */ *uap;
{
	struct nameidata nd;
	int vfslocked, error;

	AUDIT_ARG(value, uap->attrnamespace);
	NDINIT(&nd, LOOKUP, MPSAFE | FOLLOW | AUDITVNODE1, UIO_USERSPACE,
	    uap->path, td);
	error = namei(&nd);
	if (error)
		return (error);
	NDFREE(&nd, NDF_ONLY_PNBUF);

	vfslocked = NDHASGIANT(&nd);
	error = extattr_list_vp(nd.ni_vp, uap->attrnamespace, uap->data,
	    uap->nbytes, td);

	vrele(nd.ni_vp);
	VFS_UNLOCK_GIANT(vfslocked);
	return (error);
}

int
extattr_list_link(td, uap)
	struct thread*td;
	struct extattr_list_link_args /* {
		const char *path;
		int attrnamespace;
		void *data;
		size_t nbytes;
	} */ *uap;
{
	struct nameidata nd;
	int vfslocked, error;

	AUDIT_ARG(value, uap->attrnamespace);
	NDINIT(&nd, LOOKUP, MPSAFE | NOFOLLOW | AUDITVNODE1, UIO_USERSPACE,
	    uap->path, td);
	error = namei(&nd);
	if (error)
		return (error);
	NDFREE(&nd, NDF_ONLY_PNBUF);

	vfslocked = NDHASGIANT(&nd);
	error = extattr_list_vp(nd.ni_vp, uap->attrnamespace, uap->data,
	    uap->nbytes, td);

	vrele(nd.ni_vp);
	VFS_UNLOCK_GIANT(vfslocked);
	return (error);
}
