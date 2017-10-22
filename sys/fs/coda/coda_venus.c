/*-
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
 * 	@(#) src/sys/cfs/coda_venus.c,v 1.1.1.1 1998/08/29 21:14:52 rvb Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/fcntl.h>
#include <sys/ioccom.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sx.h>

#include <fs/coda/coda.h>
#include <fs/coda/cnode.h>
#include <fs/coda/coda_venus.h>
#include <fs/coda/coda_pioctl.h>

#define	DECL_NO_IN(name)						\
	struct coda_in_hdr *inp;					\
	struct name ## _out *outp;					\
	int name ## _size = sizeof (struct coda_in_hdr);		\
	int Isize = sizeof (struct coda_in_hdr);			\
	int Osize = sizeof (struct name ## _out);			\
	int error

#define	DECL(name)							\
	struct name ## _in *inp;					\
	struct name ## _out *outp;					\
	int name ## _size = sizeof (struct name ## _in);		\
	int Isize = sizeof (struct name ## _in);			\
	int Osize = sizeof (struct name ## _out);			\
	int error

#define	DECL_NO_OUT(name)						\
	struct name ## _in *inp;					\
	struct coda_out_hdr *outp;					\
	int name ## _size = sizeof (struct name ## _in);		\
	int Isize = sizeof (struct name ## _in);			\
	int Osize = sizeof (struct coda_out_hdr);			\
	int error

#define	ALLOC_NO_IN(name)						\
	if (Osize > name ## _size)					\
		name ## _size = Osize;					\
	CODA_ALLOC(inp, struct coda_in_hdr *, name ## _size);		\
	outp = (struct name ## _out *) inp

#define	ALLOC(name)							\
	if (Osize > name ## _size)					\
		name ## _size = Osize;					\
	CODA_ALLOC(inp, struct name ## _in *, name ## _size);		\
	outp = (struct name ## _out *) inp

#define	ALLOC_NO_OUT(name)						\
	if (Osize > name ## _size)					\
		name ## _size = Osize;					\
	CODA_ALLOC(inp, struct name ## _in *, name ## _size);		\
	outp = (struct coda_out_hdr *) inp

#define	STRCPY(struc, name, len)					\
	bcopy(name, (char *)inp + (int)inp->struc, len);		\
	((char*)inp + (int)inp->struc)[len++] = 0;			\
	Isize += len

#ifdef CODA_COMPAT_5
#define	INIT_IN(in, op, ident, p) do {					\
	(in)->opcode = (op);						\
	sx_slock(&proctree_lock);					\
	(in)->pid = p ? p->p_pid : -1;					\
	(in)->pgid = p ? p->p_pgid : -1;				\
	(in)->sid = (p && p->p_session && p->p_session->s_leader) ?	\
	    (p->p_session->s_leader->p_pid) : -1;			\
	sx_sunlock(&proctree_lock);					\
	if (ident != NOCRED) {						\
		(in)->cred.cr_uid = ident->cr_uid;			\
		(in)->cred.cr_groupid = ident->cr_gid;			\
	} else {							\
		bzero(&((in)->cred),sizeof(struct coda_cred));		\
		(in)->cred.cr_uid = -1;					\
		(in)->cred.cr_groupid = -1;				\
	}								\
} while (0)
#else
#define	INIT_IN(in, op, ident, p) do {					\
	(in)->opcode = (op);						\
	(in)->pid = p ? p->p_pid : -1;					\
	(in)->pgid = p ? p->p_pgid : -1;				\
	if (ident != NOCRED)						\
		(in)->uid = ident->cr_uid;				\
	else								\
		(in)->uid = -1;						\
} while (0)
#endif

#define	CNV_OFLAG(to, from) do { 					\
	to = 0;								\
	if (from & FREAD)   to |= C_O_READ; 				\
	if (from & FWRITE)  to |= C_O_WRITE; 				\
	if (from & O_TRUNC) to |= C_O_TRUNC; 				\
	if (from & O_EXCL)  to |= C_O_EXCL; 				\
	if (from & O_CREAT) to |= C_O_CREAT;				\
} while (0)

#define	CNV_VV2V_ATTR(top, fromp) do {					\
	(top)->va_type = (fromp)->va_type;				\
	(top)->va_mode = (fromp)->va_mode;				\
	(top)->va_nlink = (fromp)->va_nlink;				\
	(top)->va_uid = (fromp)->va_uid;				\
	(top)->va_gid = (fromp)->va_gid;				\
	(top)->va_fsid = VNOVAL;					\
	(top)->va_fileid = (fromp)->va_fileid;				\
	(top)->va_size = (fromp)->va_size;				\
	(top)->va_blocksize = (fromp)->va_blocksize;			\
	(top)->va_atime = (fromp)->va_atime;				\
	(top)->va_mtime = (fromp)->va_mtime;				\
	(top)->va_ctime = (fromp)->va_ctime;				\
	(top)->va_gen = (fromp)->va_gen;				\
	(top)->va_flags = (fromp)->va_flags;				\
	(top)->va_rdev = (fromp)->va_rdev;				\
	(top)->va_bytes = (fromp)->va_bytes;				\
	(top)->va_filerev = (fromp)->va_filerev;			\
	(top)->va_vaflags = VNOVAL;					\
	(top)->va_spare = VNOVAL;					\
} while (0)

#define	CNV_V2VV_ATTR(top, fromp) do {					\
	(top)->va_type = (fromp)->va_type;				\
	(top)->va_mode = (fromp)->va_mode;				\
	(top)->va_nlink = (fromp)->va_nlink;				\
	(top)->va_uid = (fromp)->va_uid;				\
	(top)->va_gid = (fromp)->va_gid;				\
	(top)->va_fileid = (fromp)->va_fileid;				\
	(top)->va_size = (fromp)->va_size;				\
	(top)->va_blocksize = (fromp)->va_blocksize;			\
	(top)->va_atime = (fromp)->va_atime;				\
	(top)->va_mtime = (fromp)->va_mtime;				\
	(top)->va_ctime = (fromp)->va_ctime;				\
	(top)->va_gen = (fromp)->va_gen;				\
	(top)->va_flags = (fromp)->va_flags;				\
	(top)->va_rdev = (fromp)->va_rdev;				\
	(top)->va_bytes = (fromp)->va_bytes;				\
	(top)->va_filerev = (fromp)->va_filerev;			\
} while (0)

int coda_kernel_version = CODA_KERNEL_VERSION;

int
venus_root(void *mdp, struct ucred *cred, struct proc *p,
    /*out*/ struct CodaFid *VFid)
{
	DECL_NO_IN(coda_root);		/* sets Isize & Osize */
	ALLOC_NO_IN(coda_root);		/* sets inp & outp */

	/* send the open to venus. */
	INIT_IN(inp, CODA_ROOT, cred, p);

	error = coda_call(mdp, Isize, &Osize, (char *)inp);
	if (!error)
		*VFid = outp->Fid;

	CODA_FREE(inp, coda_root_size);
	return (error);
}

int
venus_open(void *mdp, struct CodaFid *fid, int flag, struct ucred *cred,
    struct proc *p, /*out*/ struct vnode **vp)
{
	int cflag;
	DECL(coda_open_by_fd);			/* sets Isize & Osize */
	ALLOC(coda_open_by_fd);			/* sets inp & outp */

	/* send the open to venus. */
	INIT_IN(&inp->ih, CODA_OPEN_BY_FD, cred, p);
	inp->Fid = *fid;
	CNV_OFLAG(cflag, flag);
	inp->flags = cflag;

	error = coda_call(mdp, Isize, &Osize, (char *)inp);
	*vp = error ? NULL : outp->vp;

	CODA_FREE(inp, coda_open_by_fd_size);
	return (error);
}

int
venus_close(void *mdp, struct CodaFid *fid, int flag, struct ucred *cred,
    struct proc *p)
{
	int cflag;
	DECL_NO_OUT(coda_close);		/* sets Isize & Osize */
	ALLOC_NO_OUT(coda_close);		/* sets inp & outp */

	INIT_IN(&inp->ih, CODA_CLOSE, cred, p);
	inp->Fid = *fid;
	CNV_OFLAG(cflag, flag);
	inp->flags = cflag;

	error = coda_call(mdp, Isize, &Osize, (char *)inp);

	CODA_FREE(inp, coda_close_size);
	return (error);
}

/*
 * These two calls will not exist!!!  The container file is read/written
 * directly.
 */
void
venus_read(void)
{
}

void
venus_write(void)
{
}

/*
 * This is a bit sad too.  the ioctl's are for the control file, not for
 * normal files.
 */
int
venus_ioctl(void *mdp, struct CodaFid *fid, int com, int flag, caddr_t data,
    struct ucred *cred, struct proc *p)
{
	DECL(coda_ioctl);			/* sets Isize & Osize */
	struct PioctlData *iap = (struct PioctlData *)data;
	int tmp;

	coda_ioctl_size = VC_MAXMSGSIZE;
	ALLOC(coda_ioctl);			/* sets inp & outp */

	INIT_IN(&inp->ih, CODA_IOCTL, cred, p);
	inp->Fid = *fid;

	/*
	 * Command was mutated by increasing its size field to reflect the
	 * path and follow args. we need to subtract that out before sending
	 * the command to venus.
	 */
	inp->cmd = (com & ~(IOCPARM_MASK << 16));
	tmp = ((com >> 16) & IOCPARM_MASK) - sizeof (char *) - sizeof (int);
	inp->cmd |= (tmp & IOCPARM_MASK) << 16;

	if (iap->vi.in_size > VC_MAXMSGSIZE ||
	    iap->vi.out_size > VC_MAXMSGSIZE) {
		CODA_FREE(inp, coda_ioctl_size);
		return (EINVAL);
	}

	inp->rwflag = flag;
	inp->len = iap->vi.in_size;
	inp->data = (char *)(sizeof (struct coda_ioctl_in));

	error = copyin(iap->vi.in, (char*)inp + (long)inp->data,
	    iap->vi.in_size);
	if (error) {
		CODA_FREE(inp, coda_ioctl_size);
		return (error);
	}

	Osize = VC_MAXMSGSIZE;
	error = coda_call(mdp, Isize + iap->vi.in_size, &Osize, (char *)inp);

	/*
	 * Copy out the out buffer.
	 */
	if (!error) {
		if (outp->len > iap->vi.out_size)
			error = EINVAL;
		else
			error = copyout((char *)outp + (long)outp->data,
			    iap->vi.out, iap->vi.out_size);
	}

	CODA_FREE(inp, coda_ioctl_size);
	return (error);
}

int
venus_getattr(void *mdp, struct CodaFid *fid, struct ucred *cred,
    struct vattr *vap)
{
	struct proc *p;
	DECL(coda_getattr);			/* sets Isize & Osize */
	ALLOC(coda_getattr);			/* sets inp & outp */

	/*
	 * Send the open to venus.
	 */
	p = curthread->td_proc;
	INIT_IN(&inp->ih, CODA_GETATTR, cred, p);
	inp->Fid = *fid;

	error = coda_call(mdp, Isize, &Osize, (char *)inp);
	if (!error)
		CNV_VV2V_ATTR(vap, &outp->attr);

	CODA_FREE(inp, coda_getattr_size);
	return (error);
}

int
venus_setattr(void *mdp, struct CodaFid *fid, struct vattr *vap,
    struct ucred *cred)
{
	struct proc *p;
	DECL_NO_OUT(coda_setattr);		/* sets Isize & Osize */
	ALLOC_NO_OUT(coda_setattr);		/* sets inp & outp */

	/*
	 * Send the open to venus.
	 */
	p = curthread->td_proc;
	INIT_IN(&inp->ih, CODA_SETATTR, cred, p);
	inp->Fid = *fid;
	CNV_V2VV_ATTR(&inp->attr, vap);

	error = coda_call(mdp, Isize, &Osize, (char *)inp);

	CODA_FREE(inp, coda_setattr_size);
	return (error);
}

int
venus_access(void *mdp, struct CodaFid *fid, accmode_t accmode,
    struct ucred *cred, struct proc *p)
{
	DECL_NO_OUT(coda_access);		/* sets Isize & Osize */
	ALLOC_NO_OUT(coda_access);		/* sets inp & outp */

	/*
	 * Send the open to venus.
	 */
	INIT_IN(&inp->ih, CODA_ACCESS, cred, p);
	inp->Fid = *fid;

	/*
	 * NOTE: FreeBSD and Venus internals use the "data" in the low 3
	 * bits.  Hence, the conversion.
	 *
	 * XXX: We cast accmode_t variable into an int.
	 */
	inp->flags = (int)accmode>>6;

	error = coda_call(mdp, Isize, &Osize, (char *)inp);

	CODA_FREE(inp, coda_access_size);
	return (error);
}

int
venus_readlink(void *mdp, struct CodaFid *fid, struct ucred *cred,
    struct proc *p, /*out*/ char **str, int *len)
{
	DECL(coda_readlink);			/* sets Isize & Osize */
	coda_readlink_size += CODA_MAXPATHLEN;
	ALLOC(coda_readlink);			/* sets inp & outp */

	/*
	 * Send the open to venus.
	 */
	INIT_IN(&inp->ih, CODA_READLINK, cred, p);
	inp->Fid = *fid;

	Osize += CODA_MAXPATHLEN;
	error = coda_call(mdp, Isize, &Osize, (char *)inp);
	if (!error) {
		CODA_ALLOC(*str, char *, outp->count);
		*len = outp->count;
		bcopy((char *)outp + (long)outp->data, *str, *len);
	}

	CODA_FREE(inp, coda_readlink_size);
	return (error);
}

int
venus_fsync(void *mdp, struct CodaFid *fid, struct proc *p)
{
	DECL_NO_OUT(coda_fsync);		/* sets Isize & Osize */
	ALLOC_NO_OUT(coda_fsync);		/* sets inp & outp */

	/*
	 * Send the open to venus.
	 *
	 * XXX: should be cached mount cred.
	 */
	INIT_IN(&inp->ih, CODA_FSYNC, NOCRED, p);
	inp->Fid = *fid;

	error = coda_call(mdp, Isize, &Osize, (char *)inp);

	CODA_FREE(inp, coda_fsync_size);
	return (error);
}

int
venus_lookup(void *mdp, struct CodaFid *fid, const char *nm, int len,
    struct ucred *cred, struct proc *p, /*out*/	struct CodaFid *VFid,
    int *vtype)
{
	DECL(coda_lookup);			/* sets Isize & Osize */
	coda_lookup_size += len + 1;
	ALLOC(coda_lookup);			/* sets inp & outp */

	/*
	 * Send the open to venus.
	 */
	INIT_IN(&inp->ih, CODA_LOOKUP, cred, p);
	inp->Fid = *fid;

	/*
	 * NOTE: Between version 1 and version 2 we have added an extra flag
	 * field to this structure.  But because the string was at the end
	 * and because of the wierd way we represent strings by having the
	 * slot point to where the string characters are in the "heap", we
	 * can just slip the flag parameter in after the string slot pointer
	 * and veni that don't know better won't see this new flag field ...
	 * Otherwise we'd need two different venus_lookup functions.
	 */
	inp->name = Isize;
	inp->flags = CLU_CASE_SENSITIVE; /* doesn't really matter for BSD */
	STRCPY(name, nm, len);		/* increments Isize */

	error = coda_call(mdp, Isize, &Osize, (char *)inp);
	if (!error) {
		*VFid = outp->Fid;
		*vtype = outp->vtype;
	}

	CODA_FREE(inp, coda_lookup_size);
	return (error);
}

int
venus_create(void *mdp, struct CodaFid *fid, const char *nm, int len,
    int exclusive, int mode, struct vattr *va, struct ucred *cred,
    struct proc *p, /*out*/ struct CodaFid *VFid, struct vattr *attr)
{
	DECL(coda_create);			/* sets Isize & Osize */
	coda_create_size += len + 1;
	ALLOC(coda_create);			/* sets inp & outp */

	/*
	 * Send the open to venus.
	 */
	INIT_IN(&inp->ih, CODA_CREATE, cred, p);
	inp->Fid = *fid;
	inp->excl = exclusive ? C_O_EXCL : 0;
	inp->mode = mode;
	CNV_V2VV_ATTR(&inp->attr, va);

	inp->name = Isize;
	STRCPY(name, nm, len);		/* increments Isize */

	error = coda_call(mdp, Isize, &Osize, (char *)inp);
	if (!error) {
		*VFid = outp->Fid;
		CNV_VV2V_ATTR(attr, &outp->attr);
	}

	CODA_FREE(inp, coda_create_size);
	return (error);
}

int
venus_remove(void *mdp, struct CodaFid *fid, const char *nm, int len,
    struct ucred *cred, struct proc *p)
{
	DECL_NO_OUT(coda_remove);		/* sets Isize & Osize */
	coda_remove_size += len + 1;
	ALLOC_NO_OUT(coda_remove);		/* sets inp & outp */

	/*
	 * Send the open to venus.
	 */
	INIT_IN(&inp->ih, CODA_REMOVE, cred, p);
	inp->Fid = *fid;

	inp->name = Isize;
	STRCPY(name, nm, len);		/* increments Isize */

	error = coda_call(mdp, Isize, &Osize, (char *)inp);

	CODA_FREE(inp, coda_remove_size);
	return (error);
}

int
venus_link(void *mdp, struct CodaFid *fid, struct CodaFid *tfid,
    const char *nm, int len, struct ucred *cred, struct proc *p)
{
	DECL_NO_OUT(coda_link);		/* sets Isize & Osize */
	coda_link_size += len + 1;
	ALLOC_NO_OUT(coda_link);		/* sets inp & outp */

	/*
	 * Send the open to venus.
	 */
	INIT_IN(&inp->ih, CODA_LINK, cred, p);
	inp->sourceFid = *fid;
	inp->destFid = *tfid;

	inp->tname = Isize;
	STRCPY(tname, nm, len);		/* increments Isize */

	error = coda_call(mdp, Isize, &Osize, (char *)inp);

	CODA_FREE(inp, coda_link_size);
	return (error);
}

int
venus_rename(void *mdp, struct CodaFid *fid, struct CodaFid *tfid,
    const char *nm, int len, const char *tnm, int tlen, struct ucred *cred,
    struct proc *p)
{
	DECL_NO_OUT(coda_rename);		/* sets Isize & Osize */
	coda_rename_size += len + 1 + tlen + 1;
	ALLOC_NO_OUT(coda_rename);		/* sets inp & outp */

	/*
	 * Send the open to venus.
	 */
	INIT_IN(&inp->ih, CODA_RENAME, cred, p);
	inp->sourceFid = *fid;
	inp->destFid = *tfid;

	inp->srcname = Isize;
	STRCPY(srcname, nm, len);		/* increments Isize */

	inp->destname = Isize;
	STRCPY(destname, tnm, tlen);	/* increments Isize */

	error = coda_call(mdp, Isize, &Osize, (char *)inp);

	CODA_FREE(inp, coda_rename_size);
	return (error);
}

int
venus_mkdir(void *mdp, struct CodaFid *fid, const char *nm, int len,
    struct vattr *va, struct ucred *cred, struct proc *p,
    /*out*/ struct CodaFid *VFid, struct vattr *ova)
{
	DECL(coda_mkdir);			/* sets Isize & Osize */
	coda_mkdir_size += len + 1;
	ALLOC(coda_mkdir);			/* sets inp & outp */

	/*
	 * Send the open to venus.
	 */
	INIT_IN(&inp->ih, CODA_MKDIR, cred, p);
	inp->Fid = *fid;
	CNV_V2VV_ATTR(&inp->attr, va);

	inp->name = Isize;
	STRCPY(name, nm, len);		/* increments Isize */

	error = coda_call(mdp, Isize, &Osize, (char *)inp);
	if (!error) {
		*VFid = outp->Fid;
		CNV_VV2V_ATTR(ova, &outp->attr);
	}

	CODA_FREE(inp, coda_mkdir_size);
	return (error);
}

int
venus_rmdir(void *mdp, struct CodaFid *fid, const char *nm, int len,
    struct ucred *cred, struct proc *p)
{
	DECL_NO_OUT(coda_rmdir);		/* sets Isize & Osize */
	coda_rmdir_size += len + 1;
	ALLOC_NO_OUT(coda_rmdir);		/* sets inp & outp */

	/*
	 *  Send the open to venus.
	 */
	INIT_IN(&inp->ih, CODA_RMDIR, cred, p);
	inp->Fid = *fid;

	inp->name = Isize;
	STRCPY(name, nm, len);		/* increments Isize */

	error = coda_call(mdp, Isize, &Osize, (char *)inp);

	CODA_FREE(inp, coda_rmdir_size);
	return (error);
}

int
venus_symlink(void *mdp, struct CodaFid *fid, const char *lnm, int llen,
    const char *nm, int len, struct vattr *va, struct ucred *cred,
    struct proc *p)
{
	DECL_NO_OUT(coda_symlink);		/* sets Isize & Osize */
	coda_symlink_size += llen + 1 + len + 1;
	ALLOC_NO_OUT(coda_symlink);		/* sets inp & outp */

	/*
	 * Send the open to venus.
	 */
	INIT_IN(&inp->ih, CODA_SYMLINK, cred, p);
	inp->Fid = *fid;
	CNV_V2VV_ATTR(&inp->attr, va);

	inp->srcname = Isize;
	STRCPY(srcname, lnm, llen);		/* increments Isize */

	inp->tname = Isize;
	STRCPY(tname, nm, len);		/* increments Isize */

	error = coda_call(mdp, Isize, &Osize, (char *)inp);

	CODA_FREE(inp, coda_symlink_size);
	return (error);
}

/*
 * XXX: Unused.
 */
int
venus_readdir(void *mdp, struct CodaFid *fid, int count, int offset,
    struct ucred *cred, struct proc *p, /*out*/	char *buffer, int *len)
{
	DECL(coda_readdir);			/* sets Isize & Osize */
	coda_readdir_size = VC_MAXMSGSIZE;
	ALLOC(coda_readdir);			/* sets inp & outp */

	/*
	 * Send the open to venus.
	 */
	INIT_IN(&inp->ih, CODA_READDIR, cred, p);
	inp->Fid = *fid;
	inp->count = count;
	inp->offset = offset;

	Osize = VC_MAXMSGSIZE;
	error = coda_call(mdp, Isize, &Osize, (char *)inp);
	if (!error) {
		bcopy((char *)outp + (long)outp->data, buffer, outp->size);
		*len = outp->size;
	}

	CODA_FREE(inp, coda_readdir_size);
	return (error);
}

int
venus_fhtovp(void *mdp, struct CodaFid *fid, struct ucred *cred,
    struct proc *p, /*out*/ struct CodaFid *VFid, int *vtype)
{
	DECL(coda_vget);			/* sets Isize & Osize */
	ALLOC(coda_vget);			/* sets inp & outp */

	/*
	 * Send the open to Venus.
	 */
	INIT_IN(&inp->ih, CODA_VGET, cred, p);
	inp->Fid = *fid;

	error = coda_call(mdp, Isize, &Osize, (char *)inp);
	if (!error) {
		*VFid = outp->Fid;
		*vtype = outp->vtype;
	}

	CODA_FREE(inp, coda_vget_size);
	return (error);
}
