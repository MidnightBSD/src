/*-
 * Copyright (c) 1999-2002, 2006 Robert N. M. Watson
 * Copyright (c) 2001 Ilmar S. Habibulin
 * Copyright (c) 2001-2005 Networks Associates Technology, Inc.
 * Copyright (c) 2005-2006 SPARTA, Inc.
 * All rights reserved.
 *
 * This software was developed by Robert Watson and Ilmar Habibulin for the
 * TrustedBSD Project.
 *
 * This software was developed for the FreeBSD Project in part by Network
 * Associates Laboratories, the Security Research Division of Network
 * Associates, Inc. under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"),
 * as part of the DARPA CHATS research program.
 *
 * This software was enhanced by SPARTA ISSO under SPAWAR contract 
 * N66001-04-C-6019 ("SEFOS").
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
__FBSDID("$FreeBSD: release/7.0.0/sys/security/mac/mac_syscalls.c 171744 2007-08-06 14:26:03Z rwatson $");

#include "opt_mac.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/mac.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/sysent.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/file.h>
#include <sys/namei.h>
#include <sys/socket.h>
#include <sys/pipe.h>
#include <sys/socketvar.h>

#include <security/mac/mac_framework.h>
#include <security/mac/mac_internal.h>
#include <security/mac/mac_policy.h>

#ifdef MAC

int
__mac_get_pid(struct thread *td, struct __mac_get_pid_args *uap)
{
	char *elements, *buffer;
	struct mac mac;
	struct proc *tproc;
	struct ucred *tcred;
	int error;

	error = copyin(uap->mac_p, &mac, sizeof(mac));
	if (error)
		return (error);

	error = mac_check_structmac_consistent(&mac);
	if (error)
		return (error);

	tproc = pfind(uap->pid);
	if (tproc == NULL)
		return (ESRCH);

	tcred = NULL;				/* Satisfy gcc. */
	error = p_cansee(td, tproc);
	if (error == 0)
		tcred = crhold(tproc->p_ucred);
	PROC_UNLOCK(tproc);
	if (error)
		return (error);

	elements = malloc(mac.m_buflen, M_MACTEMP, M_WAITOK);
	error = copyinstr(mac.m_string, elements, mac.m_buflen, NULL);
	if (error) {
		free(elements, M_MACTEMP);
		crfree(tcred);
		return (error);
	}

	buffer = malloc(mac.m_buflen, M_MACTEMP, M_WAITOK | M_ZERO);
	error = mac_externalize_cred_label(tcred->cr_label, elements,
	    buffer, mac.m_buflen);
	if (error == 0)
		error = copyout(buffer, mac.m_string, strlen(buffer)+1);

	free(buffer, M_MACTEMP);
	free(elements, M_MACTEMP);
	crfree(tcred);
	return (error);
}

int
__mac_get_proc(struct thread *td, struct __mac_get_proc_args *uap)
{
	char *elements, *buffer;
	struct mac mac;
	int error;

	error = copyin(uap->mac_p, &mac, sizeof(mac));
	if (error)
		return (error);

	error = mac_check_structmac_consistent(&mac);
	if (error)
		return (error);

	elements = malloc(mac.m_buflen, M_MACTEMP, M_WAITOK);
	error = copyinstr(mac.m_string, elements, mac.m_buflen, NULL);
	if (error) {
		free(elements, M_MACTEMP);
		return (error);
	}

	buffer = malloc(mac.m_buflen, M_MACTEMP, M_WAITOK | M_ZERO);
	error = mac_externalize_cred_label(td->td_ucred->cr_label,
	    elements, buffer, mac.m_buflen);
	if (error == 0)
		error = copyout(buffer, mac.m_string, strlen(buffer)+1);

	free(buffer, M_MACTEMP);
	free(elements, M_MACTEMP);
	return (error);
}

int
__mac_set_proc(struct thread *td, struct __mac_set_proc_args *uap)
{
	struct ucred *newcred, *oldcred;
	struct label *intlabel;
	struct proc *p;
	struct mac mac;
	char *buffer;
	int error;

	error = copyin(uap->mac_p, &mac, sizeof(mac));
	if (error)
		return (error);

	error = mac_check_structmac_consistent(&mac);
	if (error)
		return (error);

	buffer = malloc(mac.m_buflen, M_MACTEMP, M_WAITOK);
	error = copyinstr(mac.m_string, buffer, mac.m_buflen, NULL);
	if (error) {
		free(buffer, M_MACTEMP);
		return (error);
	}

	intlabel = mac_cred_label_alloc();
	error = mac_internalize_cred_label(intlabel, buffer);
	free(buffer, M_MACTEMP);
	if (error)
		goto out;

	newcred = crget();

	p = td->td_proc;
	PROC_LOCK(p);
	oldcred = p->p_ucred;

	error = mac_check_cred_relabel(oldcred, intlabel);
	if (error) {
		PROC_UNLOCK(p);
		crfree(newcred);
		goto out;
	}

	setsugid(p);
	crcopy(newcred, oldcred);
	mac_relabel_cred(newcred, intlabel);
	p->p_ucred = newcred;

	/*
	 * Grab additional reference for use while revoking mmaps, prior to
	 * releasing the proc lock and sharing the cred.
	 */
	crhold(newcred);
	PROC_UNLOCK(p);

	mac_cred_mmapped_drop_perms(td, newcred);

	crfree(newcred);	/* Free revocation reference. */
	crfree(oldcred);

out:
	mac_cred_label_free(intlabel);
	return (error);
}

int
__mac_get_fd(struct thread *td, struct __mac_get_fd_args *uap)
{
	char *elements, *buffer;
	struct label *intlabel;
	struct file *fp;
	struct mac mac;
	struct vnode *vp;
	struct pipe *pipe;
	struct socket *so;
	short label_type;
	int vfslocked, error;

	error = copyin(uap->mac_p, &mac, sizeof(mac));
	if (error)
		return (error);

	error = mac_check_structmac_consistent(&mac);
	if (error)
		return (error);

	elements = malloc(mac.m_buflen, M_MACTEMP, M_WAITOK);
	error = copyinstr(mac.m_string, elements, mac.m_buflen, NULL);
	if (error) {
		free(elements, M_MACTEMP);
		return (error);
	}

	buffer = malloc(mac.m_buflen, M_MACTEMP, M_WAITOK | M_ZERO);
	error = fget(td, uap->fd, &fp);
	if (error)
		goto out;

	label_type = fp->f_type;
	switch (fp->f_type) {
	case DTYPE_FIFO:
	case DTYPE_VNODE:
		vp = fp->f_vnode;
		intlabel = mac_vnode_label_alloc();
		vfslocked = VFS_LOCK_GIANT(vp->v_mount);
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
		mac_copy_vnode_label(vp->v_label, intlabel);
		VOP_UNLOCK(vp, 0, td);
		VFS_UNLOCK_GIANT(vfslocked);
		error = mac_externalize_vnode_label(intlabel, elements,
		    buffer, mac.m_buflen);
		mac_vnode_label_free(intlabel);
		break;

	case DTYPE_PIPE:
		pipe = fp->f_data;
		intlabel = mac_pipe_label_alloc();
		PIPE_LOCK(pipe);
		mac_copy_pipe_label(pipe->pipe_pair->pp_label, intlabel);
		PIPE_UNLOCK(pipe);
		error = mac_externalize_pipe_label(intlabel, elements,
		    buffer, mac.m_buflen);
		mac_pipe_label_free(intlabel);
		break;

	case DTYPE_SOCKET:
		so = fp->f_data;
		intlabel = mac_socket_label_alloc(M_WAITOK);
		SOCK_LOCK(so);
		mac_copy_socket_label(so->so_label, intlabel);
		SOCK_UNLOCK(so);
		error = mac_externalize_socket_label(intlabel, elements,
		    buffer, mac.m_buflen);
		mac_socket_label_free(intlabel);
		break;

	default:
		error = EINVAL;
	}
	fdrop(fp, td);
	if (error == 0)
		error = copyout(buffer, mac.m_string, strlen(buffer)+1);

out:
	free(buffer, M_MACTEMP);
	free(elements, M_MACTEMP);
	return (error);
}

int
__mac_get_file(struct thread *td, struct __mac_get_file_args *uap)
{
	char *elements, *buffer;
	struct nameidata nd;
	struct label *intlabel;
	struct mac mac;
	int vfslocked, error;

	error = copyin(uap->mac_p, &mac, sizeof(mac));
	if (error)
		return (error);

	error = mac_check_structmac_consistent(&mac);
	if (error)
		return (error);

	elements = malloc(mac.m_buflen, M_MACTEMP, M_WAITOK);
	error = copyinstr(mac.m_string, elements, mac.m_buflen, NULL);
	if (error) {
		free(elements, M_MACTEMP);
		return (error);
	}

	buffer = malloc(mac.m_buflen, M_MACTEMP, M_WAITOK | M_ZERO);
	NDINIT(&nd, LOOKUP, MPSAFE | LOCKLEAF | FOLLOW, UIO_USERSPACE,
	    uap->path_p, td);
	error = namei(&nd);
	if (error)
		goto out;

	intlabel = mac_vnode_label_alloc();
	vfslocked = NDHASGIANT(&nd);
	mac_copy_vnode_label(nd.ni_vp->v_label, intlabel);
	error = mac_externalize_vnode_label(intlabel, elements, buffer,
	    mac.m_buflen);

	NDFREE(&nd, 0);
	VFS_UNLOCK_GIANT(vfslocked);
	mac_vnode_label_free(intlabel);
	if (error == 0)
		error = copyout(buffer, mac.m_string, strlen(buffer)+1);

out:
	free(buffer, M_MACTEMP);
	free(elements, M_MACTEMP);

	return (error);
}

int
__mac_get_link(struct thread *td, struct __mac_get_link_args *uap)
{
	char *elements, *buffer;
	struct nameidata nd;
	struct label *intlabel;
	struct mac mac;
	int vfslocked, error;

	error = copyin(uap->mac_p, &mac, sizeof(mac));
	if (error)
		return (error);

	error = mac_check_structmac_consistent(&mac);
	if (error)
		return (error);

	elements = malloc(mac.m_buflen, M_MACTEMP, M_WAITOK);
	error = copyinstr(mac.m_string, elements, mac.m_buflen, NULL);
	if (error) {
		free(elements, M_MACTEMP);
		return (error);
	}

	buffer = malloc(mac.m_buflen, M_MACTEMP, M_WAITOK | M_ZERO);
	NDINIT(&nd, LOOKUP, MPSAFE | LOCKLEAF | NOFOLLOW, UIO_USERSPACE,
	    uap->path_p, td);
	error = namei(&nd);
	if (error)
		goto out;

	intlabel = mac_vnode_label_alloc();
	vfslocked = NDHASGIANT(&nd);
	mac_copy_vnode_label(nd.ni_vp->v_label, intlabel);
	error = mac_externalize_vnode_label(intlabel, elements, buffer,
	    mac.m_buflen);
	NDFREE(&nd, 0);
	VFS_UNLOCK_GIANT(vfslocked);
	mac_vnode_label_free(intlabel);

	if (error == 0)
		error = copyout(buffer, mac.m_string, strlen(buffer)+1);

out:
	free(buffer, M_MACTEMP);
	free(elements, M_MACTEMP);

	return (error);
}

int
__mac_set_fd(struct thread *td, struct __mac_set_fd_args *uap)
{
	struct label *intlabel;
	struct pipe *pipe;
	struct socket *so;
	struct file *fp;
	struct mount *mp;
	struct vnode *vp;
	struct mac mac;
	char *buffer;
	int error, vfslocked;

	error = copyin(uap->mac_p, &mac, sizeof(mac));
	if (error)
		return (error);

	error = mac_check_structmac_consistent(&mac);
	if (error)
		return (error);

	buffer = malloc(mac.m_buflen, M_MACTEMP, M_WAITOK);
	error = copyinstr(mac.m_string, buffer, mac.m_buflen, NULL);
	if (error) {
		free(buffer, M_MACTEMP);
		return (error);
	}

	error = fget(td, uap->fd, &fp);
	if (error)
		goto out;

	switch (fp->f_type) {
	case DTYPE_FIFO:
	case DTYPE_VNODE:
		intlabel = mac_vnode_label_alloc();
		error = mac_internalize_vnode_label(intlabel, buffer);
		if (error) {
			mac_vnode_label_free(intlabel);
			break;
		}
		vp = fp->f_vnode;
		vfslocked = VFS_LOCK_GIANT(vp->v_mount);
		error = vn_start_write(vp, &mp, V_WAIT | PCATCH);
		if (error != 0) {
			VFS_UNLOCK_GIANT(vfslocked);
			mac_vnode_label_free(intlabel);
			break;
		}
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
		error = vn_setlabel(vp, intlabel, td->td_ucred);
		VOP_UNLOCK(vp, 0, td);
		vn_finished_write(mp);
		VFS_UNLOCK_GIANT(vfslocked);
		mac_vnode_label_free(intlabel);
		break;

	case DTYPE_PIPE:
		intlabel = mac_pipe_label_alloc();
		error = mac_internalize_pipe_label(intlabel, buffer);
		if (error == 0) {
			pipe = fp->f_data;
			PIPE_LOCK(pipe);
			error = mac_pipe_label_set(td->td_ucred,
			    pipe->pipe_pair, intlabel);
			PIPE_UNLOCK(pipe);
		}
		mac_pipe_label_free(intlabel);
		break;

	case DTYPE_SOCKET:
		intlabel = mac_socket_label_alloc(M_WAITOK);
		error = mac_internalize_socket_label(intlabel, buffer);
		if (error == 0) {
			so = fp->f_data;
			error = mac_socket_label_set(td->td_ucred, so,
			    intlabel);
		}
		mac_socket_label_free(intlabel);
		break;

	default:
		error = EINVAL;
	}
	fdrop(fp, td);
out:
	free(buffer, M_MACTEMP);
	return (error);
}

int
__mac_set_file(struct thread *td, struct __mac_set_file_args *uap)
{
	struct label *intlabel;
	struct nameidata nd;
	struct mount *mp;
	struct mac mac;
	char *buffer;
	int vfslocked, error;

	error = copyin(uap->mac_p, &mac, sizeof(mac));
	if (error)
		return (error);

	error = mac_check_structmac_consistent(&mac);
	if (error)
		return (error);

	buffer = malloc(mac.m_buflen, M_MACTEMP, M_WAITOK);
	error = copyinstr(mac.m_string, buffer, mac.m_buflen, NULL);
	if (error) {
		free(buffer, M_MACTEMP);
		return (error);
	}

	intlabel = mac_vnode_label_alloc();
	error = mac_internalize_vnode_label(intlabel, buffer);
	free(buffer, M_MACTEMP);
	if (error)
		goto out;

	NDINIT(&nd, LOOKUP, MPSAFE | LOCKLEAF | FOLLOW, UIO_USERSPACE,
	    uap->path_p, td);
	error = namei(&nd);
	vfslocked = NDHASGIANT(&nd);
	if (error == 0) {
		error = vn_start_write(nd.ni_vp, &mp, V_WAIT | PCATCH);
		if (error == 0) {
			error = vn_setlabel(nd.ni_vp, intlabel,
			    td->td_ucred);
			vn_finished_write(mp);
		}
	}

	NDFREE(&nd, 0);
	VFS_UNLOCK_GIANT(vfslocked);
out:
	mac_vnode_label_free(intlabel);
	return (error);
}

int
__mac_set_link(struct thread *td, struct __mac_set_link_args *uap)
{
	struct label *intlabel;
	struct nameidata nd;
	struct mount *mp;
	struct mac mac;
	char *buffer;
	int vfslocked, error;

	error = copyin(uap->mac_p, &mac, sizeof(mac));
	if (error)
		return (error);

	error = mac_check_structmac_consistent(&mac);
	if (error)
		return (error);

	buffer = malloc(mac.m_buflen, M_MACTEMP, M_WAITOK);
	error = copyinstr(mac.m_string, buffer, mac.m_buflen, NULL);
	if (error) {
		free(buffer, M_MACTEMP);
		return (error);
	}

	intlabel = mac_vnode_label_alloc();
	error = mac_internalize_vnode_label(intlabel, buffer);
	free(buffer, M_MACTEMP);
	if (error)
		goto out;

	NDINIT(&nd, LOOKUP, MPSAFE | LOCKLEAF | NOFOLLOW, UIO_USERSPACE,
	    uap->path_p, td);
	error = namei(&nd);
	vfslocked = NDHASGIANT(&nd);
	if (error == 0) {
		error = vn_start_write(nd.ni_vp, &mp, V_WAIT | PCATCH);
		if (error == 0) {
			error = vn_setlabel(nd.ni_vp, intlabel,
			    td->td_ucred);
			vn_finished_write(mp);
		}
	}

	NDFREE(&nd, 0);
	VFS_UNLOCK_GIANT(vfslocked);
out:
	mac_vnode_label_free(intlabel);
	return (error);
}

int
mac_syscall(struct thread *td, struct mac_syscall_args *uap)
{
	struct mac_policy_conf *mpc;
	char target[MAC_MAX_POLICY_NAME];
	int entrycount, error;

	error = copyinstr(uap->policy, target, sizeof(target), NULL);
	if (error)
		return (error);

	error = ENOSYS;
	LIST_FOREACH(mpc, &mac_static_policy_list, mpc_list) {
		if (strcmp(mpc->mpc_name, target) == 0 &&
		    mpc->mpc_ops->mpo_syscall != NULL) {
			error = mpc->mpc_ops->mpo_syscall(td,
			    uap->call, uap->arg);
			goto out;
		}
	}

	if ((entrycount = mac_policy_list_conditional_busy()) != 0) {
		LIST_FOREACH(mpc, &mac_policy_list, mpc_list) {
			if (strcmp(mpc->mpc_name, target) == 0 &&
			    mpc->mpc_ops->mpo_syscall != NULL) {
				error = mpc->mpc_ops->mpo_syscall(td,
				    uap->call, uap->arg);
				break;
			}
		}
		mac_policy_list_unbusy();
	}
out:
	return (error);
}

#else /* !MAC */

int
__mac_get_pid(struct thread *td, struct __mac_get_pid_args *uap)
{

	return (ENOSYS);
}

int
__mac_get_proc(struct thread *td, struct __mac_get_proc_args *uap)
{

	return (ENOSYS);
}

int
__mac_set_proc(struct thread *td, struct __mac_set_proc_args *uap)
{

	return (ENOSYS);
}

int
__mac_get_fd(struct thread *td, struct __mac_get_fd_args *uap)
{

	return (ENOSYS);
}

int
__mac_get_file(struct thread *td, struct __mac_get_file_args *uap)
{

	return (ENOSYS);
}

int
__mac_get_link(struct thread *td, struct __mac_get_link_args *uap)
{

	return (ENOSYS);
}

int
__mac_set_fd(struct thread *td, struct __mac_set_fd_args *uap)
{

	return (ENOSYS);
}

int
__mac_set_file(struct thread *td, struct __mac_set_file_args *uap)
{

	return (ENOSYS);
}

int
__mac_set_link(struct thread *td, struct __mac_set_link_args *uap)
{

	return (ENOSYS);
}

int
mac_syscall(struct thread *td, struct mac_syscall_args *uap)
{

	return (ENOSYS);
}

#endif /* !MAC */
