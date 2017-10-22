/*-
 * Copyright (c) 2007 Pawel Jakub Dawidek <pjd@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 $ $FreeBSD: release/7.0.0/sys/compat/opensolaris/sys/policy.h 168404 2007-04-06 01:09:06Z pjd $
 */

#ifndef _OPENSOLARIS_SYS_POLICY_H_
#define	_OPENSOLARIS_SYS_POLICY_H_

#include <sys/param.h>

#ifdef _KERNEL

struct mount;
struct ucred;
struct vattr;
struct vnode;

int	secpolicy_zfs(struct ucred  *cred);
int	secpolicy_sys_config(struct ucred  *cred, int checkonly);
int	secpolicy_zinject(struct ucred  *cred);
int	secpolicy_fs_unmount(struct ucred  *cred, struct mount *vfsp);
int	secpolicy_basic_link(struct ucred  *cred);
int	secpolicy_vnode_stky_modify(struct ucred *cred);
int	secpolicy_vnode_remove(struct ucred *cred);
int	secpolicy_vnode_access(struct ucred *cred, struct vnode *vp,
	    uint64_t owner, int mode);
int	secpolicy_vnode_setdac(struct ucred *cred, uid_t owner);
int	secpolicy_vnode_setattr(struct ucred *cred, struct vnode *vp,
	    struct vattr *vap, const struct vattr *ovap, int flags,
	    int unlocked_access(void *, int, struct ucred *), void *node);
int	secpolicy_vnode_create_gid(struct ucred *cred);
int	secpolicy_vnode_setids_setgids(struct ucred *cred, gid_t gid);
int	secpolicy_vnode_setid_retain(struct ucred *cred, boolean_t issuidroot);
void	secpolicy_setid_clear(struct vattr *vap, struct ucred *cred);
int	secpolicy_setid_setsticky_clear(struct vnode *vp, struct vattr *vap,
	    const struct vattr *ovap, struct ucred *cred);

#endif	/* _KERNEL */

#endif	/* _OPENSOLARIS_SYS_POLICY_H_ */
