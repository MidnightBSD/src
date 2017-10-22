/*-
 *
 *             Coda: an Experimental Distributed File System
 *                              Release 3.1
 *
 *           Copyright (c) 1987-1998 Carnegie Mellon University
 *                          All Rights Reserved
 *
 * Permission  to  use, copy, modify and distribute this software and its
 * documentation is hereby granted,  provided  that  both  the  copyright
 * notice  and  this  permission  notice  appear  in  all  copies  of the
 * software, derivative works or  modified  versions,  and  any  portions
 * thereof, and that both notices appear in supporting documentation, and
 * that credit is given to Carnegie Mellon University  in  all  documents
 * and publicity pertaining to direct or indirect use of this code or its
 * derivatives.
 *
 * CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
 * SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
 * FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
 * DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
 * RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
 * ANY DERIVATIVE WORK.
 *
 * Carnegie  Mellon  encourages  users  of  this  software  to return any
 * improvements or extensions that  they  make,  and  to  grant  Carnegie
 * Mellon the rights to redistribute these changes without encumbrance.
 *
 * 	@(#) src/sys/coda/coda_venus.h,v 1.1.1.1 1998/08/29 21:14:52 rvb Exp $
 * $FreeBSD$
 */

#ifndef _CODA_VENUS_H_
#define	_CODA_VENUS_H_

int	venus_root(void *mdp, struct ucred *cred, struct proc *p,
	    /*out*/ struct CodaFid *VFid);
int	venus_open(void *mdp, struct CodaFid *fid, int flag,
	    struct ucred *cred, struct proc *p, /*out*/ struct vnode **vp);
int	venus_close(void *mdp, struct CodaFid *fid, int flag,
	    struct ucred *cred, struct proc *p);
void	venus_read(void);
void	venus_write(void);
int	venus_ioctl(void *mdp, struct CodaFid *fid, int com, int flag,
	    caddr_t data, struct ucred *cred, struct proc *p);
int	venus_getattr(void *mdp, struct CodaFid *fid, struct ucred *cred,
	    struct vattr *vap);
int	venus_setattr(void *mdp, struct CodaFid *fid, struct vattr *vap,
	    struct ucred *cred);
int	venus_access(void *mdp, struct CodaFid *fid, int mode,
	    struct ucred *cred, struct proc *p);
int	venus_readlink(void *mdp, struct CodaFid *fid, struct ucred *cred,
	    struct proc *p, /*out*/ char **str, int *len);
int	venus_fsync(void *mdp, struct CodaFid *fid, struct proc *p);
int	venus_lookup(void *mdp, struct CodaFid *fid, const char *nm, int len,
	    struct ucred *cred, struct proc *p, /*out*/ struct CodaFid *VFid,
	    int *vtype);
int	venus_create(void *mdp, struct CodaFid *fid, const char *nm, int len,
	    int exclusive, int mode, struct vattr *va, struct ucred *cred,
	    struct proc *p, /*out*/ struct CodaFid *VFid,
	    struct vattr *attr);
int	venus_remove(void *mdp, struct CodaFid *fid, const char *nm, int len,
	    struct ucred *cred, struct proc *p);
int	venus_link(void *mdp, struct CodaFid *fid, struct CodaFid *tfid,
	    const char *nm, int len, struct ucred *cred, struct proc *p);
int	venus_rename(void *mdp, struct CodaFid *fid, struct CodaFid *tfid,
	    const char *nm, int len, const char *tnm, int tlen,
	    struct ucred *cred, struct proc *p);
int	venus_mkdir(void *mdp, struct CodaFid *fid, const char *nm, int len,
	    struct vattr *va, struct ucred *cred, struct proc *p,
	    /*out*/ struct CodaFid *VFid, struct vattr *ova);
int	venus_rmdir(void *mdp, struct CodaFid *fid, const char *nm, int len,
	    struct ucred *cred, struct proc *p);
int	venus_symlink(void *mdp, struct CodaFid *fid, const char *lnm,
	    int llen, const char *nm, int len, struct vattr *va,
	    struct ucred *cred, struct proc *p);
int	venus_readdir(void *mdp, struct CodaFid *fid, int count, int offset,
	    struct ucred *cred, struct proc *p, /*out*/	char *buffer,
	    int *len);
int	venus_fhtovp(void *mdp, struct CodaFid *fid, struct ucred *cred,
	    struct proc *p, /*out*/ struct CodaFid *VFid, int *vtype);

#endif /* !_CODA_VENUS_H_ */
