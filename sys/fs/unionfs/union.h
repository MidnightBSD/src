/*-
 * Copyright (c) 1994 The Regents of the University of California.
 * Copyright (c) 1994 Jan-Simon Pendry.
 * All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
 * Jan-Simon Pendry.
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
 *	@(#)union.h	8.9 (Berkeley) 12/10/94
 * $FreeBSD: src/sys/fs/unionfs/union.h,v 1.31 2005/01/06 18:10:42 imp Exp $
 */

#define UNMNT_ABOVE	0x0001		/* Target appears above mount point */
#define UNMNT_BELOW	0x0002		/* Target appears below mount point */
#define UNMNT_REPLACE	0x0003		/* Target replaces mount point */

struct union_mount {
	struct vnode	*um_uppervp;	/* UN_ULOCK holds locking state */
	struct vnode	*um_lowervp;	/* Left unlocked */
	struct ucred	*um_cred;	/* Credentials of user calling mount */
	int		um_cmode;	/* cmask from mount process */
	int		um_op;		/* Operation mode */
	dev_t		um_upperdev;	/* Upper root node fsid[0]*/
};

#ifdef _KERNEL

#ifndef DIAGNOSTIC
#define DIAGNOSTIC
#endif

/*
 * DEFDIRMODE is the mode bits used to create a shadow directory.
 */
#define VRWXMODE (VREAD|VWRITE|VEXEC)
#define VRWMODE (VREAD|VWRITE)
#define UN_DIRMODE ((VRWXMODE)|(VRWXMODE>>3)|(VRWXMODE>>6))
#define UN_FILEMODE ((VRWMODE)|(VRWMODE>>3)|(VRWMODE>>6))

/*
 * A cache of vnode references	(hangs off v_data)
 */
struct union_node {
	LIST_ENTRY(union_node)	un_cache;	/* Hash chain */
	struct vnode		*un_vnode;	/* Back pointer */
	struct vnode	        *un_uppervp;	/* overlaying object */
	struct vnode	        *un_lowervp;	/* underlying object */
	struct vnode		*un_dirvp;	/* Parent dir of uppervp */
	struct vnode		*un_pvp;	/* Parent vnode */
	char			*un_path;	/* saved component name */
	int			un_openl;	/* # of opens on lowervp */
	int			un_exclcnt;	/* exclusive count */
	unsigned int		un_flags;
	struct vnode		**un_dircache;	/* cached union stack */
	off_t			un_uppersz;	/* size of upper object */
	off_t			un_lowersz;	/* size of lower object */
#ifdef DIAGNOSTIC
	pid_t			un_pid;
#endif
};

/*
 * XXX UN_ULOCK -	indicates that the uppervp is locked
 *
 * UN_CACHED -	node is in the union cache
 */

/*#define UN_ULOCK	0x04*/	/* Upper node is locked */
#define UN_CACHED	0x10	/* In union cache */

/*
 * Hash table locking flags
 */

#define UNVP_WANT	0x01
#define UNVP_LOCKED	0x02

extern int union_allocvp(struct vnode **, struct mount *,
				struct vnode *, 
				struct vnode *, 
				struct componentname *, struct vnode *,
				struct vnode *, int);
extern int union_freevp(struct vnode *);
extern struct vnode *union_dircache_get(struct vnode *, struct thread *);
extern void union_dircache_free(struct union_node *);
extern int union_copyup(struct union_node *, int, struct ucred *,
				struct thread *);
extern int union_dowhiteout(struct union_node *, struct ucred *,
					struct thread *);
extern int union_mkshadow(struct union_mount *, struct vnode *,
				struct componentname *, struct vnode **);
extern int union_mkwhiteout(struct union_mount *, struct vnode *,
				struct componentname *, char *);
extern int union_cn_close(struct vnode *, int, struct ucred *,
				struct thread *);
extern void union_removed_upper(struct union_node *un);
extern struct vnode *union_lowervp(struct vnode *);
extern void union_newsize(struct vnode *, off_t, off_t);

extern int (*union_dircheckp)(struct thread *, struct vnode **,
				 struct file *);

#define	MOUNTTOUNIONMOUNT(mp) ((struct union_mount *)((mp)->mnt_data))
#define	VTOUNION(vp) ((struct union_node *)(vp)->v_data)
#define	UNIONTOV(un) ((un)->un_vnode)
#define	LOWERVP(vp) (VTOUNION(vp)->un_lowervp)
#define	UPPERVP(vp) (VTOUNION(vp)->un_uppervp)
#define OTHERVP(vp) (UPPERVP(vp) ? UPPERVP(vp) : LOWERVP(vp))

#define UDEBUG(x)	if (uniondebug) printf x
#define UDEBUG_ENABLED	1

extern struct vop_vector union_vnodeops;
extern struct vfsops union_vfsops;
extern int uniondebug;

#endif /* _KERNEL */
