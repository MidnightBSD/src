/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)ucred.h	8.4 (Berkeley) 1/9/95
 */

#ifndef _SYS_UCRED_H_
#define	_SYS_UCRED_H_

#include <sys/types.h>
#if defined(_KERNEL) || defined(_WANT_UCRED)
#include <sys/_lock.h>
#include <sys/_mutex.h>
#endif
#include <bsm/audit.h>

struct loginclass;

#define	XU_NGROUPS	16

#if defined(_KERNEL) || defined(_WANT_UCRED)
/*
 * Number of groups inlined in 'struct ucred'.  It must stay reasonably low as
 * it is also used by some functions to allocate an array of this size on the
 * stack.
 */
#define	CRED_SMALLGROUPS_NB	16

/*
 * Credentials.
 *
 * Please do not inspect cr_uid directly to determine superuserness.  The
 * priv(9) interface should be used to check for privilege.
 *
 * Lock reference:
 *      c - cr_mtx
 *
 * Unmarked fields are constant after creation.
 *
 * See "Credential management" comment in kern_prot.c for more information.
 */
struct ucred {
	struct mtx cr_mtx;
	u_int	cr_ref;			/* (c) reference count */
	u_int	cr_users;		/* (c) proc + thread using this cred */
	struct auditinfo_addr	cr_audit;	/* Audit properties. */
#define	cr_startcopy cr_uid
	uid_t	cr_uid;			/* effective user id */
	uid_t	cr_ruid;		/* real user id */
	uid_t	cr_svuid;		/* saved user id */
	/*
	 * XXXOC: On the next ABI change, please move 'cr_ngroups' out of the
	 * copied area (crcopy() already copes with this change).
	 */
	int	cr_ngroups;		/* number of groups */
	gid_t	cr_rgid;		/* real group id */
	gid_t	cr_svgid;		/* saved group id */
	struct uidinfo	*cr_uidinfo;	/* per euid resource consumption */
	struct uidinfo	*cr_ruidinfo;	/* per ruid resource consumption */
	struct prison	*cr_prison;	/* jail(2) */
	struct loginclass	*cr_loginclass; /* login class */
	u_int		cr_flags;	/* credential flags */
	void		*cr_pspare2[2];	/* general use 2 */
#define	cr_endcopy	cr_label
	struct label	*cr_label;	/* MAC label */
	gid_t	*cr_groups;		/* groups */
	int	cr_agroups;		/* Available groups */
	/* storage for small groups */
	gid_t   cr_smallgroups[CRED_SMALLGROUPS_NB];
};
#define	NOCRED	((struct ucred *)0)	/* no credential available */
#define	FSCRED	((struct ucred *)-1)	/* filesystem credential */
#endif /* _KERNEL || _WANT_UCRED */

/*
 * Flags for cr_flags.
 */
#define	CRED_FLAG_CAPMODE	0x00000001	/* In capability mode. */

/*
 * This is the external representation of struct ucred.
 */
struct xucred {
	u_int	cr_version;		/* structure layout version */
	uid_t	cr_uid;			/* effective user id */
	short	cr_ngroups;		/* number of groups */
	gid_t	cr_groups[XU_NGROUPS];	/* groups */
	union {
		void	*_cr_unused1;	/* compatibility with old ucred */
		pid_t	cr_pid;
	};
};
#define	XUCRED_VERSION	0

/* This can be used for both ucred and xucred structures. */
#define	cr_gid cr_groups[0]

#ifdef _KERNEL
struct proc;
struct thread;

struct credbatch {
	struct ucred *cred;
	int users;
	int ref;
};

static inline void
credbatch_prep(struct credbatch *crb)
{
	crb->cred = NULL;
	crb->users = 0;
	crb->ref = 0;
}
void	credbatch_add(struct credbatch *crb, struct thread *td);

static inline void
credbatch_process(struct credbatch *crb __unused)
{

}

void	credbatch_final(struct credbatch *crb);

void	change_egid(struct ucred *newcred, gid_t egid);
void	change_euid(struct ucred *newcred, struct uidinfo *euip);
void	change_rgid(struct ucred *newcred, gid_t rgid);
void	change_ruid(struct ucred *newcred, struct uidinfo *ruip);
void	change_svgid(struct ucred *newcred, gid_t svgid);
void	change_svuid(struct ucred *newcred, uid_t svuid);
void	crcopy(struct ucred *dest, struct ucred *src);
struct ucred	*crcopysafe(struct proc *p, struct ucred *cr);
struct ucred	*crdup(struct ucred *cr);
void	crextend(struct ucred *cr, int n);
void	proc_set_cred(struct proc *p, struct ucred *newcred);
bool	proc_set_cred_enforce_proc_lim(struct proc *p, struct ucred *newcred);
void	proc_unset_cred(struct proc *p, bool decrement_proc_count);
void	crfree(struct ucred *cr);
struct ucred	*crcowsync(void);
struct ucred	*crget(void);
struct ucred	*crhold(struct ucred *cr);
struct ucred	*crcowget(struct ucred *cr);
void	crcowfree(struct thread *td);
void	cru2x(struct ucred *cr, struct xucred *xcr);
void	cru2xt(struct thread *td, struct xucred *xcr);
void	crsetgroups(struct ucred *cr, int ngrp, const gid_t *groups);
void	crsetgroups_fallback(struct ucred *cr, int ngrp, const gid_t *groups,
	    const gid_t fallback);

/*
 * Returns whether gid designates a primary group in cred.
 */
static inline int
group_is_primary(const gid_t gid, const struct ucred *const cred)
{
	return (gid == cred->cr_groups[0] || gid == cred->cr_rgid ||
	    gid == cred->cr_svgid);
}
int	group_is_supplementary(const gid_t gid, const struct ucred *const cred);
int	groupmember(gid_t gid, const struct ucred *cred);
int	realgroupmember(gid_t gid, const struct ucred *cred);
#endif /* _KERNEL */

#endif /* !_SYS_UCRED_H_ */
