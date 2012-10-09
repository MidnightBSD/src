/*-
 * Copyright (c) 1999, 2000 Boris Popov
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
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/malloc.h>
#include <sys/bio.h>
#include <sys/buf.h>

#include <netncp/ncp.h>
#include <netncp/ncp_conn.h>
#include <netncp/ncp_subr.h>
#include <netncp/ncp_ncp.h>
#include <netncp/ncp_nls.h>

#include <fs/nwfs/nwfs.h>
#include <fs/nwfs/nwfs_node.h>
#include <fs/nwfs/nwfs_subr.h>

int nwfs_debuglevel = 0;

static int nwfs_version = NWFS_VERSION;

SYSCTL_DECL(_vfs_nwfs);
SYSCTL_NODE(_vfs, OID_AUTO, nwfs, CTLFLAG_RW, 0, "Netware filesystem");
SYSCTL_INT(_vfs_nwfs, OID_AUTO, version, CTLFLAG_RD, &nwfs_version, 0, "");
SYSCTL_INT(_vfs_nwfs, OID_AUTO, debuglevel, CTLFLAG_RW, &nwfs_debuglevel, 0, "");

MODULE_DEPEND(nwfs, ncp, 1, 1, 1);
MODULE_DEPEND(nwfs, libmchain, 1, 1, 1);

static vfs_cmount_t	nwfs_cmount;
static vfs_mount_t	nwfs_mount;
static vfs_quotactl_t	nwfs_quotactl;
static vfs_root_t	nwfs_root;
static vfs_statfs_t	nwfs_statfs;
static vfs_unmount_t	nwfs_unmount;
static vfs_init_t	nwfs_init;
static vfs_uninit_t	nwfs_uninit;

static struct vfsops nwfs_vfsops = {
	.vfs_init =		nwfs_init,
	.vfs_mount =		nwfs_mount,
	.vfs_cmount =		nwfs_cmount,
	.vfs_quotactl =		nwfs_quotactl,
	.vfs_root =		nwfs_root,
	.vfs_statfs =		nwfs_statfs,
	.vfs_sync =		vfs_stdsync,
	.vfs_uninit =		nwfs_uninit,
	.vfs_unmount =		nwfs_unmount,
};


VFS_SET(nwfs_vfsops, nwfs, VFCF_NETWORK);

int nwfs_pbuf_freecnt = -1;	/* start out unlimited */
static int nwfsid = 1;

static int
nwfs_initnls(struct nwmount *nmp) {
	char	*pc, *pe;
	int	error = 0;
#define COPY_TABLE(t,d)	{ \
		if (t) { \
			error = copyin((t), pc, 256); \
			if (error) break; \
		} else \
			bcopy(d, pc, 256); \
		(t) = pc; pc += 256; \
	}

	nmp->m.nls.opt |= NWHP_NLS | NWHP_DOS;
	if ((nmp->m.flags & NWFS_MOUNT_HAVE_NLS) == 0) {
		nmp->m.nls.to_lower = ncp_defnls.to_lower;
		nmp->m.nls.to_upper = ncp_defnls.to_upper;
		nmp->m.nls.n2u = ncp_defnls.n2u;
		nmp->m.nls.u2n = ncp_defnls.u2n;
		return 0;
	}
	pe = malloc(256 * 4, M_NWFSDATA, M_WAITOK);
	pc = pe;
	do {
		COPY_TABLE(nmp->m.nls.to_lower, ncp_defnls.to_lower);
		COPY_TABLE(nmp->m.nls.to_upper, ncp_defnls.to_upper);
		COPY_TABLE(nmp->m.nls.n2u, ncp_defnls.n2u);
		COPY_TABLE(nmp->m.nls.u2n, ncp_defnls.u2n);
	} while(0);
	if (error) {
		free(pe, M_NWFSDATA);
		return error;
	}
	return 0;
}

static int nwfs_cmount(struct mntarg *ma, void *data, uint64_t flags)
{
	struct nwfs_args args; 	  /* will hold data from mount request */
	int error;

	error = copyin(data, &args, sizeof(struct nwfs_args));
	if (error)
		return (error);

	/*
	 * XXX: cheap cop-out here, args contains a structure I don't
	 * XXX: know how we should handle, and I don't see any immediate
	 * XXX: prospect of avoiding a mount_nwfs(8) binary anyway.
	 */
	ma = mount_arg(ma, "nwfs_args", &args, sizeof args);

	error = kernel_mount(ma, flags);

	return (error);
}

/*
 * mp - path - addr in user space of mount point (ie /usr or whatever)
 * data - addr in user space of mount params 
 */
static int nwfs_mount(struct mount *mp)
{
	struct nwfs_args args; 	  /* will hold data from mount request */
	int error;
	struct nwmount *nmp = NULL;
	struct ncp_conn *conn = NULL;
	struct ncp_handle *handle = NULL;
	struct vnode *vp;
	struct thread *td;
	char *pc,*pe;

	td = curthread;
	if (mp->mnt_flag & MNT_ROOTFS)
		return (EOPNOTSUPP);
	if (mp->mnt_flag & MNT_UPDATE) {
		nwfs_printf("MNT_UPDATE not implemented");
		return (EOPNOTSUPP);
	}
	error = vfs_copyopt(mp->mnt_optnew, "nwfs_args", &args, sizeof args);
	if (error)
		return (error);
	if (args.version != NWFS_VERSION) {
		nwfs_printf("mount version mismatch: kernel=%d, mount=%d\n",NWFS_VERSION,args.version);
		return (1);
	}
	error = ncp_conn_getbyref(args.connRef, td , td->td_ucred,NCPM_EXECUTE,&conn);
	if (error) {
		nwfs_printf("invalid connection refernce %d\n",args.connRef);
		return (error);
	}
	error = ncp_conn_gethandle(conn, NULL, &handle);
	if (error) {
		nwfs_printf("can't get connection handle\n");
		return (error);
	}
	ncp_conn_unlock(conn, td);	/* we keep the ref */
	mp->mnt_stat.f_iosize = conn->buffer_size;
        /* We must malloc our own mount info */
        nmp = malloc(sizeof(struct nwmount), M_NWFSDATA, M_WAITOK | M_ZERO);
        if (nmp == NULL) {
                nwfs_printf("could not alloc nwmount\n");
                error = ENOMEM;
		goto bad;
        }
        mp->mnt_data = nmp;
	nmp->connh = handle;
	nmp->n_root = NULL;
	nmp->n_id = nwfsid++;
        nmp->m = args;
	nmp->m.file_mode = (nmp->m.file_mode &
			    (S_IRWXU|S_IRWXG|S_IRWXO)) | S_IFREG;
	nmp->m.dir_mode  = (nmp->m.dir_mode &
			    (S_IRWXU|S_IRWXG|S_IRWXO)) | S_IFDIR;
	if ((error = nwfs_initnls(nmp)) != 0) goto bad;
	pc = mp->mnt_stat.f_mntfromname;
	pe = pc+sizeof(mp->mnt_stat.f_mntfromname);
	bzero(pc, MNAMELEN);
	*(pc++) = '/';
	pc = index(strncpy(pc, conn->li.server, pe-pc-2),0);
	if (pc < pe-1) {
		*(pc++) = ':';
		pc=index(strncpy(pc, conn->li.user, pe-pc-2),0);
		if (pc < pe-1) {
			*(pc++) = '/';
			strncpy(pc, nmp->m.mounted_vol, pe-pc-2);
		}
	}
	/* protect against invalid mount points */
	nmp->m.mount_point[sizeof(nmp->m.mount_point)-1] = '\0';
	vfs_getnewfsid(mp);
	error = nwfs_root(mp, LK_EXCLUSIVE, &vp);
	if (error)
		goto bad;
	/*
	 * Lose the lock but keep the ref.
	 */
	VOP_UNLOCK(vp, 0);
	NCPVODEBUG("rootvp.vrefcnt=%d\n",vrefcnt(vp));
	return error;
bad:
        if (nmp)
		free(nmp, M_NWFSDATA);
	if (handle)
		ncp_conn_puthandle(handle, NULL, 0);
        return error;
}

/* Unmount the filesystem described by mp. */
static int
nwfs_unmount(struct mount *mp, int mntflags)
{
	struct thread *td;
	struct nwmount *nmp = VFSTONWFS(mp);
	struct ncp_conn *conn;
	int error, flags;

	NCPVODEBUG("nwfs_unmount: flags=%04x\n",mntflags);
	td = curthread;
	flags = 0;
	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;
	/* There is 1 extra root vnode reference from nwfs_mount(). */
	error = vflush(mp, 1, flags, td);
	if (error)
		return (error);
	conn = NWFSTOCONN(nmp);
	ncp_conn_puthandle(nmp->connh,NULL,0);
	if (ncp_conn_lock(conn, td, td->td_ucred,NCPM_WRITE | NCPM_EXECUTE) == 0) {
		if(ncp_conn_free(conn))
			ncp_conn_unlock(conn, td);
	}
	mp->mnt_data = NULL;
	if (nmp->m.flags & NWFS_MOUNT_HAVE_NLS)
		free(nmp->m.nls.to_lower, M_NWFSDATA);
	free(nmp, M_NWFSDATA);
	MNT_ILOCK(mp);
	mp->mnt_flag &= ~MNT_LOCAL;
	MNT_IUNLOCK(mp);
	return (error);
}

/*  Return locked vnode to root of a filesystem */
static int
nwfs_root(struct mount *mp, int flags, struct vnode **vpp) {
	struct vnode *vp;
	struct nwmount *nmp;
	struct nwnode *np;
	struct ncp_conn *conn;
	struct nw_entry_info fattr;
	struct thread *td;
	struct ucred *cred;
	int error, nsf, opt;
	u_char vol;

	td = curthread;
	cred = td->td_ucred;

	nmp = VFSTONWFS(mp);
	conn = NWFSTOCONN(nmp);
	if (nmp->n_root) {
		*vpp = NWTOV(nmp->n_root);
		while (vget(*vpp, LK_EXCLUSIVE, curthread) != 0)
			;
		return 0;
	}
	error = ncp_lookup_volume(conn, nmp->m.mounted_vol, &vol, 
		&nmp->n_rootent.f_id, td, cred);
	if (error)
		return ENOENT;
	nmp->n_volume = vol;
	error = ncp_get_namespaces(conn, vol, &nsf, td, cred);
	if (error)
		return ENOENT;
	if (nsf & NW_NSB_OS2) {
		NCPVODEBUG("volume %s has os2 namespace\n",nmp->m.mounted_vol);
		if ((nmp->m.flags & NWFS_MOUNT_NO_OS2) == 0) {
			nmp->name_space = NW_NS_OS2;
			nmp->m.nls.opt &= ~NWHP_DOS;
		}
	}
	opt = nmp->m.nls.opt;
	nsf = opt & (NWHP_UPPER | NWHP_LOWER);
	if (opt & NWHP_DOS) {
		if (nsf == (NWHP_UPPER | NWHP_LOWER)) {
			nmp->m.nls.opt &= ~(NWHP_LOWER | NWHP_UPPER);
		} else if (nsf == 0) {
			nmp->m.nls.opt |= NWHP_LOWER;
		}
	} else {
		if (nsf == (NWHP_UPPER | NWHP_LOWER)) {
			nmp->m.nls.opt &= ~(NWHP_LOWER | NWHP_UPPER);
		}
	}
	if (nmp->m.root_path[0]) {
		nmp->m.root_path[0]--;
		error = ncp_obtain_info(nmp, nmp->n_rootent.f_id,
		    -nmp->m.root_path[0], nmp->m.root_path, &fattr, td, cred);
		if (error) {
			NCPFATAL("Invalid root path specified\n");
			return ENOENT;
		}
		nmp->n_rootent.f_parent = fattr.dirEntNum;
		nmp->m.root_path[0]++;
		error = ncp_obtain_info(nmp, nmp->n_rootent.f_id,
		    -nmp->m.root_path[0], nmp->m.root_path, &fattr, td, cred);
		if (error) {
			NCPFATAL("Invalid root path specified\n");
			return ENOENT;
		}
		nmp->n_rootent.f_id = fattr.dirEntNum;
	} else {
		error = ncp_obtain_info(nmp, nmp->n_rootent.f_id,
		    0, NULL, &fattr, td, cred);
		if (error) {
			NCPFATAL("Can't obtain volume info\n");
			return ENOENT;
		}
		fattr.nameLen = strlen(strcpy(fattr.entryName, "#.ROOT"));
		nmp->n_rootent.f_parent = nmp->n_rootent.f_id;
	}
	error = nwfs_nget(mp, nmp->n_rootent, &fattr, NULL, &vp);
	if (error)
		return (error);
	vp->v_vflag |= VV_ROOT;
	np = VTONW(vp);
	if (nmp->m.root_path[0] == 0)
		np->n_flag |= NVOLUME;
	nmp->n_root = np;
/*	error = VOP_GETATTR(vp, &vattr, cred, td);
	if (error) {
		vput(vp);
		NCPFATAL("Can't get root directory entry\n");
		return error;
	}*/
	*vpp = vp;
	return (0);
}

/*
 * Do operations associated with quotas, not supported
 */
/* ARGSUSED */
static int
nwfs_quotactl(mp, cmd, uid, arg)
	struct mount *mp;
	int cmd;
	uid_t uid;
	void *arg;
{
	NCPVODEBUG("return EOPNOTSUPP\n");
	return (EOPNOTSUPP);
}

/*ARGSUSED*/
int
nwfs_init(struct vfsconf *vfsp)
{
	nwfs_hash_init();
	nwfs_pbuf_freecnt = nswbuf / 2 + 1;
	NCPVODEBUG("always happy to load!\n");
	return (0);
}

/*ARGSUSED*/
int
nwfs_uninit(struct vfsconf *vfsp)
{

	nwfs_hash_free();
	NCPVODEBUG("unloaded\n");
	return (0);
}

/*
 * nwfs_statfs call
 */
int
nwfs_statfs(mp, sbp)
	struct mount *mp;
	struct statfs *sbp;
{
	struct nwmount *nmp = VFSTONWFS(mp);
	struct thread *td = curthread;
	int error = 0, secsize;
	struct nwnode *np = nmp->n_root;
	struct ncp_volume_info vi;

	if (np == NULL) return EINVAL;
	error = ncp_get_volume_info_with_number(NWFSTOCONN(nmp),
	    nmp->n_volume, &vi, td, td->td_ucred);
	if (error) return error;
	secsize = 512;			/* XXX how to get real value ??? */
	/* fundamental filesystem block size */
	sbp->f_bsize = vi.sectors_per_block*secsize;
	/* optimal transfer block size */
	sbp->f_iosize = NWFSTOCONN(nmp)->buffer_size;
	/* total data blocks in filesystem */
	sbp->f_blocks= vi.total_blocks;
	/* free blocks in fs */
	sbp->f_bfree = vi.free_blocks + vi.purgeable_blocks;
	/* free blocks avail to non-superuser */
	sbp->f_bavail= vi.free_blocks+vi.purgeable_blocks;
	/* total file nodes in filesystem */
	sbp->f_files = vi.total_dir_entries;
	/* free file nodes in fs */
	sbp->f_ffree = vi.available_dir_entries;
	sbp->f_flags = 0;		/* copy of mount exported flags */
	return 0;
}
