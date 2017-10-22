/*-
 * Copyright 1998, 2000 Marshall Kirk McKusick. All Rights Reserved.
 *
 * The soft updates code is derived from the appendix of a University
 * of Michigan technical report (Gregory R. Ganger and Yale N. Patt,
 * "Soft Updates: A Solution to the Metadata Update Problem in File
 * Systems", CSE-TR-254-95, August 1995).
 *
 * Further information about soft updates can be obtained from:
 *
 *	Marshall Kirk McKusick		http://www.mckusick.com/softdep/
 *	1614 Oxford Street		mckusick@mckusick.com
 *	Berkeley, CA 94709-1608		+1-510-843-9542
 *	USA
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY MARSHALL KIRK MCKUSICK ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL MARSHALL KIRK MCKUSICK BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)ffs_softdep.c	9.59 (McKusick) 6/21/00
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/7.0.0/sys/ufs/ffs/ffs_softdep.c 170991 2007-06-22 13:22:37Z kib $");

/*
 * For now we want the safety net that the DIAGNOSTIC and DEBUG flags provide.
 */
#ifndef DIAGNOSTIC
#define DIAGNOSTIC
#endif
#ifndef DEBUG
#define DEBUG
#endif

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/kdb.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/vnode.h>
#include <sys/conf.h>
#include <ufs/ufs/dir.h>
#include <ufs/ufs/extattr.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ffs/fs.h>
#include <ufs/ffs/softdep.h>
#include <ufs/ffs/ffs_extern.h>
#include <ufs/ufs/ufs_extern.h>

#include <vm/vm.h>

#include "opt_ffs.h"
#include "opt_quota.h"

#ifndef SOFTUPDATES

int
softdep_flushfiles(oldmnt, flags, td)
	struct mount *oldmnt;
	int flags;
	struct thread *td;
{

	panic("softdep_flushfiles called");
}

int
softdep_mount(devvp, mp, fs, cred)
	struct vnode *devvp;
	struct mount *mp;
	struct fs *fs;
	struct ucred *cred;
{

	return (0);
}

void 
softdep_initialize()
{

	return;
}

void
softdep_uninitialize()
{

	return;
}

void
softdep_setup_inomapdep(bp, ip, newinum)
	struct buf *bp;
	struct inode *ip;
	ino_t newinum;
{

	panic("softdep_setup_inomapdep called");
}

void
softdep_setup_blkmapdep(bp, mp, newblkno)
	struct buf *bp;
	struct mount *mp;
	ufs2_daddr_t newblkno;
{

	panic("softdep_setup_blkmapdep called");
}

void 
softdep_setup_allocdirect(ip, lbn, newblkno, oldblkno, newsize, oldsize, bp)
	struct inode *ip;
	ufs_lbn_t lbn;
	ufs2_daddr_t newblkno;
	ufs2_daddr_t oldblkno;
	long newsize;
	long oldsize;
	struct buf *bp;
{
	
	panic("softdep_setup_allocdirect called");
}

void 
softdep_setup_allocext(ip, lbn, newblkno, oldblkno, newsize, oldsize, bp)
	struct inode *ip;
	ufs_lbn_t lbn;
	ufs2_daddr_t newblkno;
	ufs2_daddr_t oldblkno;
	long newsize;
	long oldsize;
	struct buf *bp;
{
	
	panic("softdep_setup_allocext called");
}

void
softdep_setup_allocindir_page(ip, lbn, bp, ptrno, newblkno, oldblkno, nbp)
	struct inode *ip;
	ufs_lbn_t lbn;
	struct buf *bp;
	int ptrno;
	ufs2_daddr_t newblkno;
	ufs2_daddr_t oldblkno;
	struct buf *nbp;
{

	panic("softdep_setup_allocindir_page called");
}

void
softdep_setup_allocindir_meta(nbp, ip, bp, ptrno, newblkno)
	struct buf *nbp;
	struct inode *ip;
	struct buf *bp;
	int ptrno;
	ufs2_daddr_t newblkno;
{

	panic("softdep_setup_allocindir_meta called");
}

void
softdep_setup_freeblocks(ip, length, flags)
	struct inode *ip;
	off_t length;
	int flags;
{
	
	panic("softdep_setup_freeblocks called");
}

void
softdep_freefile(pvp, ino, mode)
		struct vnode *pvp;
		ino_t ino;
		int mode;
{

	panic("softdep_freefile called");
}

int 
softdep_setup_directory_add(bp, dp, diroffset, newinum, newdirbp, isnewblk)
	struct buf *bp;
	struct inode *dp;
	off_t diroffset;
	ino_t newinum;
	struct buf *newdirbp;
	int isnewblk;
{

	panic("softdep_setup_directory_add called");
}

void 
softdep_change_directoryentry_offset(dp, base, oldloc, newloc, entrysize)
	struct inode *dp;
	caddr_t base;
	caddr_t oldloc;
	caddr_t newloc;
	int entrysize;
{

	panic("softdep_change_directoryentry_offset called");
}

void 
softdep_setup_remove(bp, dp, ip, isrmdir)
	struct buf *bp;
	struct inode *dp;
	struct inode *ip;
	int isrmdir;
{
	
	panic("softdep_setup_remove called");
}

void 
softdep_setup_directory_change(bp, dp, ip, newinum, isrmdir)
	struct buf *bp;
	struct inode *dp;
	struct inode *ip;
	ino_t newinum;
	int isrmdir;
{

	panic("softdep_setup_directory_change called");
}

void
softdep_change_linkcnt(ip)
	struct inode *ip;
{

	panic("softdep_change_linkcnt called");
}

void 
softdep_load_inodeblock(ip)
	struct inode *ip;
{

	panic("softdep_load_inodeblock called");
}

void 
softdep_update_inodeblock(ip, bp, waitfor)
	struct inode *ip;
	struct buf *bp;
	int waitfor;
{

	panic("softdep_update_inodeblock called");
}

int
softdep_fsync(vp)
	struct vnode *vp;	/* the "in_core" copy of the inode */
{

	return (0);
}

void
softdep_fsync_mountdev(vp)
	struct vnode *vp;
{

	return;
}

int
softdep_flushworklist(oldmnt, countp, td)
	struct mount *oldmnt;
	int *countp;
	struct thread *td;
{

	*countp = 0;
	return (0);
}

int
softdep_sync_metadata(struct vnode *vp)
{

	return (0);
}

int
softdep_slowdown(vp)
	struct vnode *vp;
{

	panic("softdep_slowdown called");
}

void
softdep_releasefile(ip)
	struct inode *ip;	/* inode with the zero effective link count */
{

	panic("softdep_releasefile called");
}

int
softdep_request_cleanup(fs, vp)
	struct fs *fs;
	struct vnode *vp;
{

	return (0);
}

int
softdep_check_suspend(struct mount *mp,
		      struct vnode *devvp,
		      int softdep_deps,
		      int softdep_accdeps,
		      int secondary_writes,
		      int secondary_accwrites)
{
	struct bufobj *bo;
	int error;
	
	(void) softdep_deps,
	(void) softdep_accdeps;

	ASSERT_VI_LOCKED(devvp, "softdep_check_suspend");
	bo = &devvp->v_bufobj;

	for (;;) {
		if (!MNT_ITRYLOCK(mp)) {
			VI_UNLOCK(devvp);
			MNT_ILOCK(mp);
			MNT_IUNLOCK(mp);
			VI_LOCK(devvp);
			continue;
		}
		if (mp->mnt_secondary_writes != 0) {
			VI_UNLOCK(devvp);
			msleep(&mp->mnt_secondary_writes,
			       MNT_MTX(mp),
			       (PUSER - 1) | PDROP, "secwr", 0);
			VI_LOCK(devvp);
			continue;
		}
		break;
	}

	/*
	 * Reasons for needing more work before suspend:
	 * - Dirty buffers on devvp.
	 * - Secondary writes occurred after start of vnode sync loop
	 */
	error = 0;
	if (bo->bo_numoutput > 0 ||
	    bo->bo_dirty.bv_cnt > 0 ||
	    secondary_writes != 0 ||
	    mp->mnt_secondary_writes != 0 ||
	    secondary_accwrites != mp->mnt_secondary_accwrites)
		error = EAGAIN;
	VI_UNLOCK(devvp);
	return (error);
}

void
softdep_get_depcounts(struct mount *mp,
		      int *softdepactivep,
		      int *softdepactiveaccp)
{
	(void) mp;
	*softdepactivep = 0;
	*softdepactiveaccp = 0;
}

#else
/*
 * These definitions need to be adapted to the system to which
 * this file is being ported.
 */
/*
 * malloc types defined for the softdep system.
 */
static MALLOC_DEFINE(M_PAGEDEP, "pagedep","File page dependencies");
static MALLOC_DEFINE(M_INODEDEP, "inodedep","Inode dependencies");
static MALLOC_DEFINE(M_NEWBLK, "newblk","New block allocation");
static MALLOC_DEFINE(M_BMSAFEMAP, "bmsafemap","Block or frag allocated from cyl group map");
static MALLOC_DEFINE(M_ALLOCDIRECT, "allocdirect","Block or frag dependency for an inode");
static MALLOC_DEFINE(M_INDIRDEP, "indirdep","Indirect block dependencies");
static MALLOC_DEFINE(M_ALLOCINDIR, "allocindir","Block dependency for an indirect block");
static MALLOC_DEFINE(M_FREEFRAG, "freefrag","Previously used frag for an inode");
static MALLOC_DEFINE(M_FREEBLKS, "freeblks","Blocks freed from an inode");
static MALLOC_DEFINE(M_FREEFILE, "freefile","Inode deallocated");
static MALLOC_DEFINE(M_DIRADD, "diradd","New directory entry");
static MALLOC_DEFINE(M_MKDIR, "mkdir","New directory");
static MALLOC_DEFINE(M_DIRREM, "dirrem","Directory entry deleted");
static MALLOC_DEFINE(M_NEWDIRBLK, "newdirblk","Unclaimed new directory block");
static MALLOC_DEFINE(M_SAVEDINO, "savedino","Saved inodes");

#define M_SOFTDEP_FLAGS	(M_WAITOK | M_USE_RESERVE)

#define	D_PAGEDEP	0
#define	D_INODEDEP	1
#define	D_NEWBLK	2
#define	D_BMSAFEMAP	3
#define	D_ALLOCDIRECT	4
#define	D_INDIRDEP	5
#define	D_ALLOCINDIR	6
#define	D_FREEFRAG	7
#define	D_FREEBLKS	8
#define	D_FREEFILE	9
#define	D_DIRADD	10
#define	D_MKDIR		11
#define	D_DIRREM	12
#define	D_NEWDIRBLK	13
#define	D_LAST		D_NEWDIRBLK

/* 
 * translate from workitem type to memory type
 * MUST match the defines above, such that memtype[D_XXX] == M_XXX
 */
static struct malloc_type *memtype[] = {
	M_PAGEDEP,
	M_INODEDEP,
	M_NEWBLK,
	M_BMSAFEMAP,
	M_ALLOCDIRECT,
	M_INDIRDEP,
	M_ALLOCINDIR,
	M_FREEFRAG,
	M_FREEBLKS,
	M_FREEFILE,
	M_DIRADD,
	M_MKDIR,
	M_DIRREM,
	M_NEWDIRBLK
};

#define DtoM(type) (memtype[type])

/*
 * Names of malloc types.
 */
#define TYPENAME(type)  \
	((unsigned)(type) < D_LAST ? memtype[type]->ks_shortdesc : "???")
/*
 * End system adaptation definitions.
 */

/*
 * Forward declarations.
 */
struct inodedep_hashhead;
struct newblk_hashhead;
struct pagedep_hashhead;

/*
 * Internal function prototypes.
 */
static	void softdep_error(char *, int);
static	void drain_output(struct vnode *);
static	struct buf *getdirtybuf(struct buf *, struct mtx *, int);
static	void clear_remove(struct thread *);
static	void clear_inodedeps(struct thread *);
static	int flush_pagedep_deps(struct vnode *, struct mount *,
	    struct diraddhd *);
static	int flush_inodedep_deps(struct mount *, ino_t);
static	int flush_deplist(struct allocdirectlst *, int, int *);
static	int handle_written_filepage(struct pagedep *, struct buf *);
static  void diradd_inode_written(struct diradd *, struct inodedep *);
static	int handle_written_inodeblock(struct inodedep *, struct buf *);
static	void handle_allocdirect_partdone(struct allocdirect *);
static	void handle_allocindir_partdone(struct allocindir *);
static	void initiate_write_filepage(struct pagedep *, struct buf *);
static	void handle_written_mkdir(struct mkdir *, int);
static	void initiate_write_inodeblock_ufs1(struct inodedep *, struct buf *);
static	void initiate_write_inodeblock_ufs2(struct inodedep *, struct buf *);
static	void handle_workitem_freefile(struct freefile *);
static	void handle_workitem_remove(struct dirrem *, struct vnode *);
static	struct dirrem *newdirrem(struct buf *, struct inode *,
	    struct inode *, int, struct dirrem **);
static	void free_diradd(struct diradd *);
static	void free_allocindir(struct allocindir *, struct inodedep *);
static	void free_newdirblk(struct newdirblk *);
static	int indir_trunc(struct freeblks *, ufs2_daddr_t, int, ufs_lbn_t,
	    ufs2_daddr_t *);
static	void deallocate_dependencies(struct buf *, struct inodedep *);
static	void free_allocdirect(struct allocdirectlst *,
	    struct allocdirect *, int);
static	int check_inode_unwritten(struct inodedep *);
static	int free_inodedep(struct inodedep *);
static	void handle_workitem_freeblocks(struct freeblks *, int);
static	void merge_inode_lists(struct allocdirectlst *,struct allocdirectlst *);
static	void setup_allocindir_phase2(struct buf *, struct inode *,
	    struct allocindir *);
static	struct allocindir *newallocindir(struct inode *, int, ufs2_daddr_t,
	    ufs2_daddr_t);
static	void handle_workitem_freefrag(struct freefrag *);
static	struct freefrag *newfreefrag(struct inode *, ufs2_daddr_t, long);
static	void allocdirect_merge(struct allocdirectlst *,
	    struct allocdirect *, struct allocdirect *);
static	struct bmsafemap *bmsafemap_lookup(struct mount *, struct buf *);
static	int newblk_find(struct newblk_hashhead *, struct fs *, ufs2_daddr_t,
	    struct newblk **);
static	int newblk_lookup(struct fs *, ufs2_daddr_t, int, struct newblk **);
static	int inodedep_find(struct inodedep_hashhead *, struct fs *, ino_t,
	    struct inodedep **);
static	int inodedep_lookup(struct mount *, ino_t, int, struct inodedep **);
static	int pagedep_lookup(struct inode *, ufs_lbn_t, int, struct pagedep **);
static	int pagedep_find(struct pagedep_hashhead *, ino_t, ufs_lbn_t,
	    struct mount *mp, int, struct pagedep **);
static	void pause_timer(void *);
static	int request_cleanup(struct mount *, int);
static	int process_worklist_item(struct mount *, int);
static	void add_to_worklist(struct worklist *);
static	void softdep_flush(void);
static	int softdep_speedup(void);

/*
 * Exported softdep operations.
 */
static	void softdep_disk_io_initiation(struct buf *);
static	void softdep_disk_write_complete(struct buf *);
static	void softdep_deallocate_dependencies(struct buf *);
static	int softdep_count_dependencies(struct buf *bp, int);

static struct mtx lk;
MTX_SYSINIT(softdep_lock, &lk, "Softdep Lock", MTX_DEF);

#define TRY_ACQUIRE_LOCK(lk)		mtx_trylock(lk)
#define ACQUIRE_LOCK(lk)		mtx_lock(lk)
#define FREE_LOCK(lk)			mtx_unlock(lk)

/*
 * Worklist queue management.
 * These routines require that the lock be held.
 */
#ifndef /* NOT */ DEBUG
#define WORKLIST_INSERT(head, item) do {	\
	(item)->wk_state |= ONWORKLIST;		\
	LIST_INSERT_HEAD(head, item, wk_list);	\
} while (0)
#define WORKLIST_REMOVE(item) do {		\
	(item)->wk_state &= ~ONWORKLIST;	\
	LIST_REMOVE(item, wk_list);		\
} while (0)
#else /* DEBUG */
static	void worklist_insert(struct workhead *, struct worklist *);
static	void worklist_remove(struct worklist *);

#define WORKLIST_INSERT(head, item) worklist_insert(head, item)
#define WORKLIST_REMOVE(item) worklist_remove(item)

static void
worklist_insert(head, item)
	struct workhead *head;
	struct worklist *item;
{

	mtx_assert(&lk, MA_OWNED);
	if (item->wk_state & ONWORKLIST)
		panic("worklist_insert: already on list");
	item->wk_state |= ONWORKLIST;
	LIST_INSERT_HEAD(head, item, wk_list);
}

static void
worklist_remove(item)
	struct worklist *item;
{

	mtx_assert(&lk, MA_OWNED);
	if ((item->wk_state & ONWORKLIST) == 0)
		panic("worklist_remove: not on list");
	item->wk_state &= ~ONWORKLIST;
	LIST_REMOVE(item, wk_list);
}
#endif /* DEBUG */

/*
 * Routines for tracking and managing workitems.
 */
static	void workitem_free(struct worklist *, int);
static	void workitem_alloc(struct worklist *, int, struct mount *);

#define	WORKITEM_FREE(item, type) workitem_free((struct worklist *)(item), (type))

static void
workitem_free(item, type)
	struct worklist *item;
	int type;
{
	struct ufsmount *ump;
	mtx_assert(&lk, MA_OWNED);

#ifdef DEBUG
	if (item->wk_state & ONWORKLIST)
		panic("workitem_free: still on list");
	if (item->wk_type != type)
		panic("workitem_free: type mismatch");
#endif
	ump = VFSTOUFS(item->wk_mp);
	if (--ump->softdep_deps == 0 && ump->softdep_req)
		wakeup(&ump->softdep_deps);
	FREE(item, DtoM(type));
}

static void
workitem_alloc(item, type, mp)
	struct worklist *item;
	int type;
	struct mount *mp;
{
	item->wk_type = type;
	item->wk_mp = mp;
	item->wk_state = 0;
	ACQUIRE_LOCK(&lk);
	VFSTOUFS(mp)->softdep_deps++;
	VFSTOUFS(mp)->softdep_accdeps++;
	FREE_LOCK(&lk);
}

/*
 * Workitem queue management
 */
static int max_softdeps;	/* maximum number of structs before slowdown */
static int maxindirdeps = 50;	/* max number of indirdeps before slowdown */
static int tickdelay = 2;	/* number of ticks to pause during slowdown */
static int proc_waiting;	/* tracks whether we have a timeout posted */
static int *stat_countp;	/* statistic to count in proc_waiting timeout */
static struct callout_handle handle; /* handle on posted proc_waiting timeout */
static int req_pending;
static int req_clear_inodedeps;	/* syncer process flush some inodedeps */
#define FLUSH_INODES		1
static int req_clear_remove;	/* syncer process flush some freeblks */
#define FLUSH_REMOVE		2
#define FLUSH_REMOVE_WAIT	3
/*
 * runtime statistics
 */
static int stat_worklist_push;	/* number of worklist cleanups */
static int stat_blk_limit_push;	/* number of times block limit neared */
static int stat_ino_limit_push;	/* number of times inode limit neared */
static int stat_blk_limit_hit;	/* number of times block slowdown imposed */
static int stat_ino_limit_hit;	/* number of times inode slowdown imposed */
static int stat_sync_limit_hit;	/* number of synchronous slowdowns imposed */
static int stat_indir_blk_ptrs;	/* bufs redirtied as indir ptrs not written */
static int stat_inode_bitmap;	/* bufs redirtied as inode bitmap not written */
static int stat_direct_blk_ptrs;/* bufs redirtied as direct ptrs not written */
static int stat_dir_entry;	/* bufs redirtied as dir entry cannot write */

SYSCTL_INT(_debug, OID_AUTO, max_softdeps, CTLFLAG_RW, &max_softdeps, 0, "");
SYSCTL_INT(_debug, OID_AUTO, tickdelay, CTLFLAG_RW, &tickdelay, 0, "");
SYSCTL_INT(_debug, OID_AUTO, maxindirdeps, CTLFLAG_RW, &maxindirdeps, 0, "");
SYSCTL_INT(_debug, OID_AUTO, worklist_push, CTLFLAG_RW, &stat_worklist_push, 0,"");
SYSCTL_INT(_debug, OID_AUTO, blk_limit_push, CTLFLAG_RW, &stat_blk_limit_push, 0,"");
SYSCTL_INT(_debug, OID_AUTO, ino_limit_push, CTLFLAG_RW, &stat_ino_limit_push, 0,"");
SYSCTL_INT(_debug, OID_AUTO, blk_limit_hit, CTLFLAG_RW, &stat_blk_limit_hit, 0, "");
SYSCTL_INT(_debug, OID_AUTO, ino_limit_hit, CTLFLAG_RW, &stat_ino_limit_hit, 0, "");
SYSCTL_INT(_debug, OID_AUTO, sync_limit_hit, CTLFLAG_RW, &stat_sync_limit_hit, 0, "");
SYSCTL_INT(_debug, OID_AUTO, indir_blk_ptrs, CTLFLAG_RW, &stat_indir_blk_ptrs, 0, "");
SYSCTL_INT(_debug, OID_AUTO, inode_bitmap, CTLFLAG_RW, &stat_inode_bitmap, 0, "");
SYSCTL_INT(_debug, OID_AUTO, direct_blk_ptrs, CTLFLAG_RW, &stat_direct_blk_ptrs, 0, "");
SYSCTL_INT(_debug, OID_AUTO, dir_entry, CTLFLAG_RW, &stat_dir_entry, 0, "");
/* SYSCTL_INT(_debug, OID_AUTO, worklist_num, CTLFLAG_RD, &softdep_on_worklist, 0, ""); */

SYSCTL_DECL(_vfs_ffs);

static int compute_summary_at_mount = 0;	/* Whether to recompute the summary at mount time */
SYSCTL_INT(_vfs_ffs, OID_AUTO, compute_summary_at_mount, CTLFLAG_RW,
	   &compute_summary_at_mount, 0, "Recompute summary at mount");

static struct proc *softdepproc;
static struct kproc_desc softdep_kp = {
	"softdepflush",
	softdep_flush,
	&softdepproc
};
SYSINIT(sdproc, SI_SUB_KTHREAD_UPDATE, SI_ORDER_ANY, kproc_start, &softdep_kp)

static void
softdep_flush(void)
{
	struct mount *nmp;
	struct mount *mp;
	struct ufsmount *ump;
	struct thread *td;
	int remaining;
	int vfslocked;

	td = curthread;
	td->td_pflags |= TDP_NORUNNINGBUF;

	for (;;) {	
		kthread_suspend_check(softdepproc);
		vfslocked = VFS_LOCK_GIANT((struct mount *)NULL);
		ACQUIRE_LOCK(&lk);
		/*
		 * If requested, try removing inode or removal dependencies.
		 */
		if (req_clear_inodedeps) {
			clear_inodedeps(td);
			req_clear_inodedeps -= 1;
			wakeup_one(&proc_waiting);
		}
		if (req_clear_remove) {
			clear_remove(td);
			req_clear_remove -= 1;
			wakeup_one(&proc_waiting);
		}
		FREE_LOCK(&lk);
		VFS_UNLOCK_GIANT(vfslocked);
		remaining = 0;
		mtx_lock(&mountlist_mtx);
		for (mp = TAILQ_FIRST(&mountlist); mp != NULL; mp = nmp)  {
			nmp = TAILQ_NEXT(mp, mnt_list);
			if ((mp->mnt_flag & MNT_SOFTDEP) == 0)
				continue;
			if (vfs_busy(mp, LK_NOWAIT, &mountlist_mtx, td))
				continue;
			vfslocked = VFS_LOCK_GIANT(mp);
			softdep_process_worklist(mp, 0);
			ump = VFSTOUFS(mp);
			remaining += ump->softdep_on_worklist -
				ump->softdep_on_worklist_inprogress;
			VFS_UNLOCK_GIANT(vfslocked);
			mtx_lock(&mountlist_mtx);
			nmp = TAILQ_NEXT(mp, mnt_list);
			vfs_unbusy(mp, td);
		}
		mtx_unlock(&mountlist_mtx);
		if (remaining)
			continue;
		ACQUIRE_LOCK(&lk);
		if (!req_pending)
			msleep(&req_pending, &lk, PVM, "sdflush", hz);
		req_pending = 0;
		FREE_LOCK(&lk);
	}
}

static int
softdep_speedup(void)
{

	mtx_assert(&lk, MA_OWNED);
	if (req_pending == 0) {
		req_pending = 1;
		wakeup(&req_pending);
	}

	return speedup_syncer();
}

/*
 * Add an item to the end of the work queue.
 * This routine requires that the lock be held.
 * This is the only routine that adds items to the list.
 * The following routine is the only one that removes items
 * and does so in order from first to last.
 */
static void
add_to_worklist(wk)
	struct worklist *wk;
{
	struct ufsmount *ump;

	mtx_assert(&lk, MA_OWNED);
	ump = VFSTOUFS(wk->wk_mp);
	if (wk->wk_state & ONWORKLIST)
		panic("add_to_worklist: already on list");
	wk->wk_state |= ONWORKLIST;
	if (LIST_EMPTY(&ump->softdep_workitem_pending))
		LIST_INSERT_HEAD(&ump->softdep_workitem_pending, wk, wk_list);
	else
		LIST_INSERT_AFTER(ump->softdep_worklist_tail, wk, wk_list);
	ump->softdep_worklist_tail = wk;
	ump->softdep_on_worklist += 1;
}

/*
 * Process that runs once per second to handle items in the background queue.
 *
 * Note that we ensure that everything is done in the order in which they
 * appear in the queue. The code below depends on this property to ensure
 * that blocks of a file are freed before the inode itself is freed. This
 * ordering ensures that no new <vfsid, inum, lbn> triples will be generated
 * until all the old ones have been purged from the dependency lists.
 */
int 
softdep_process_worklist(mp, full)
	struct mount *mp;
	int full;
{
	struct thread *td = curthread;
	int cnt, matchcnt, loopcount;
	struct ufsmount *ump;
	long starttime;

	KASSERT(mp != NULL, ("softdep_process_worklist: NULL mp"));
	/*
	 * Record the process identifier of our caller so that we can give
	 * this process preferential treatment in request_cleanup below.
	 */
	matchcnt = 0;
	ump = VFSTOUFS(mp);
	ACQUIRE_LOCK(&lk);
	loopcount = 1;
	starttime = time_second;
	while (ump->softdep_on_worklist > 0) {
		if ((cnt = process_worklist_item(mp, 0)) == -1)
			break;
		else
			matchcnt += cnt;
		/*
		 * If requested, try removing inode or removal dependencies.
		 */
		if (req_clear_inodedeps) {
			clear_inodedeps(td);
			req_clear_inodedeps -= 1;
			wakeup_one(&proc_waiting);
		}
		if (req_clear_remove) {
			clear_remove(td);
			req_clear_remove -= 1;
			wakeup_one(&proc_waiting);
		}
		/*
		 * We do not generally want to stop for buffer space, but if
		 * we are really being a buffer hog, we will stop and wait.
		 */
		if (loopcount++ % 128 == 0) {
			FREE_LOCK(&lk);
			bwillwrite();
			ACQUIRE_LOCK(&lk);
		}
		/*
		 * Never allow processing to run for more than one
		 * second. Otherwise the other mountpoints may get
		 * excessively backlogged.
		 */
		if (!full && starttime != time_second) {
			matchcnt = -1;
			break;
		}
	}
	FREE_LOCK(&lk);
	return (matchcnt);
}

/*
 * Process one item on the worklist.
 */
static int
process_worklist_item(mp, flags)
	struct mount *mp;
	int flags;
{
	struct worklist *wk, *wkend;
	struct ufsmount *ump;
	struct vnode *vp;
	int matchcnt = 0;

	mtx_assert(&lk, MA_OWNED);
	KASSERT(mp != NULL, ("process_worklist_item: NULL mp"));
	/*
	 * If we are being called because of a process doing a
	 * copy-on-write, then it is not safe to write as we may
	 * recurse into the copy-on-write routine.
	 */
	if (curthread->td_pflags & TDP_COWINPROGRESS)
		return (-1);
	/*
	 * Normally we just process each item on the worklist in order.
	 * However, if we are in a situation where we cannot lock any
	 * inodes, we have to skip over any dirrem requests whose
	 * vnodes are resident and locked.
	 */
	ump = VFSTOUFS(mp);
	vp = NULL;
	LIST_FOREACH(wk, &ump->softdep_workitem_pending, wk_list) {
		if (wk->wk_state & INPROGRESS)
			continue;
		if ((flags & LK_NOWAIT) == 0 || wk->wk_type != D_DIRREM)
			break;
		wk->wk_state |= INPROGRESS;
		ump->softdep_on_worklist_inprogress++;
		FREE_LOCK(&lk);
		ffs_vget(mp, WK_DIRREM(wk)->dm_oldinum,
		    LK_NOWAIT | LK_EXCLUSIVE, &vp);
		ACQUIRE_LOCK(&lk);
		wk->wk_state &= ~INPROGRESS;
		ump->softdep_on_worklist_inprogress--;
		if (vp != NULL)
			break;
	}
	if (wk == 0)
		return (-1);
	/*
	 * Remove the item to be processed. If we are removing the last
	 * item on the list, we need to recalculate the tail pointer.
	 * As this happens rarely and usually when the list is short,
	 * we just run down the list to find it rather than tracking it
	 * in the above loop.
	 */
	WORKLIST_REMOVE(wk);
	if (wk == ump->softdep_worklist_tail) {
		LIST_FOREACH(wkend, &ump->softdep_workitem_pending, wk_list)
			if (LIST_NEXT(wkend, wk_list) == NULL)
				break;
		ump->softdep_worklist_tail = wkend;
	}
	ump->softdep_on_worklist -= 1;
	FREE_LOCK(&lk);
	if (vn_start_secondary_write(NULL, &mp, V_NOWAIT))
		panic("process_worklist_item: suspended filesystem");
	matchcnt++;
	switch (wk->wk_type) {

	case D_DIRREM:
		/* removal of a directory entry */
		handle_workitem_remove(WK_DIRREM(wk), vp);
		break;

	case D_FREEBLKS:
		/* releasing blocks and/or fragments from a file */
		handle_workitem_freeblocks(WK_FREEBLKS(wk), flags & LK_NOWAIT);
		break;

	case D_FREEFRAG:
		/* releasing a fragment when replaced as a file grows */
		handle_workitem_freefrag(WK_FREEFRAG(wk));
		break;

	case D_FREEFILE:
		/* releasing an inode when its link count drops to 0 */
		handle_workitem_freefile(WK_FREEFILE(wk));
		break;

	default:
		panic("%s_process_worklist: Unknown type %s",
		    "softdep", TYPENAME(wk->wk_type));
		/* NOTREACHED */
	}
	vn_finished_secondary_write(mp);
	ACQUIRE_LOCK(&lk);
	return (matchcnt);
}

/*
 * Move dependencies from one buffer to another.
 */
void
softdep_move_dependencies(oldbp, newbp)
	struct buf *oldbp;
	struct buf *newbp;
{
	struct worklist *wk, *wktail;

	if (!LIST_EMPTY(&newbp->b_dep))
		panic("softdep_move_dependencies: need merge code");
	wktail = 0;
	ACQUIRE_LOCK(&lk);
	while ((wk = LIST_FIRST(&oldbp->b_dep)) != NULL) {
		LIST_REMOVE(wk, wk_list);
		if (wktail == 0)
			LIST_INSERT_HEAD(&newbp->b_dep, wk, wk_list);
		else
			LIST_INSERT_AFTER(wktail, wk, wk_list);
		wktail = wk;
	}
	FREE_LOCK(&lk);
}

/*
 * Purge the work list of all items associated with a particular mount point.
 */
int
softdep_flushworklist(oldmnt, countp, td)
	struct mount *oldmnt;
	int *countp;
	struct thread *td;
{
	struct vnode *devvp;
	int count, error = 0;
	struct ufsmount *ump;

	/*
	 * Alternately flush the block device associated with the mount
	 * point and process any dependencies that the flushing
	 * creates. We continue until no more worklist dependencies
	 * are found.
	 */
	*countp = 0;
	ump = VFSTOUFS(oldmnt);
	devvp = ump->um_devvp;
	while ((count = softdep_process_worklist(oldmnt, 1)) > 0) {
		*countp += count;
		vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY, td);
		error = VOP_FSYNC(devvp, MNT_WAIT, td);
		VOP_UNLOCK(devvp, 0, td);
		if (error)
			break;
	}
	return (error);
}

int
softdep_waitidle(struct mount *mp)
{
	struct ufsmount *ump;
	int error;
	int i;

	ump = VFSTOUFS(mp);
	ACQUIRE_LOCK(&lk);
	for (i = 0; i < 10 && ump->softdep_deps; i++) {
		ump->softdep_req = 1;
		if (ump->softdep_on_worklist)
			panic("softdep_waitidle: work added after flush.");
		msleep(&ump->softdep_deps, &lk, PVM, "softdeps", 1);
	}
	ump->softdep_req = 0;
	FREE_LOCK(&lk);
	error = 0;
	if (i == 10) {
		error = EBUSY;
		printf("softdep_waitidle: Failed to flush worklist for %p\n",
		    mp);
	}

	return (error);
}

/*
 * Flush all vnodes and worklist items associated with a specified mount point.
 */
int
softdep_flushfiles(oldmnt, flags, td)
	struct mount *oldmnt;
	int flags;
	struct thread *td;
{
	int error, count, loopcnt;

	error = 0;

	/*
	 * Alternately flush the vnodes associated with the mount
	 * point and process any dependencies that the flushing
	 * creates. In theory, this loop can happen at most twice,
	 * but we give it a few extra just to be sure.
	 */
	for (loopcnt = 10; loopcnt > 0; loopcnt--) {
		/*
		 * Do another flush in case any vnodes were brought in
		 * as part of the cleanup operations.
		 */
		if ((error = ffs_flushfiles(oldmnt, flags, td)) != 0)
			break;
		if ((error = softdep_flushworklist(oldmnt, &count, td)) != 0 ||
		    count == 0)
			break;
	}
	/*
	 * If we are unmounting then it is an error to fail. If we
	 * are simply trying to downgrade to read-only, then filesystem
	 * activity can keep us busy forever, so we just fail with EBUSY.
	 */
	if (loopcnt == 0) {
		if (oldmnt->mnt_kern_flag & MNTK_UNMOUNT)
			panic("softdep_flushfiles: looping");
		error = EBUSY;
	}
	if (!error)
		error = softdep_waitidle(oldmnt);
	return (error);
}

/*
 * Structure hashing.
 * 
 * There are three types of structures that can be looked up:
 *	1) pagedep structures identified by mount point, inode number,
 *	   and logical block.
 *	2) inodedep structures identified by mount point and inode number.
 *	3) newblk structures identified by mount point and
 *	   physical block number.
 *
 * The "pagedep" and "inodedep" dependency structures are hashed
 * separately from the file blocks and inodes to which they correspond.
 * This separation helps when the in-memory copy of an inode or
 * file block must be replaced. It also obviates the need to access
 * an inode or file page when simply updating (or de-allocating)
 * dependency structures. Lookup of newblk structures is needed to
 * find newly allocated blocks when trying to associate them with
 * their allocdirect or allocindir structure.
 *
 * The lookup routines optionally create and hash a new instance when
 * an existing entry is not found.
 */
#define DEPALLOC	0x0001	/* allocate structure if lookup fails */
#define NODELAY		0x0002	/* cannot do background work */

/*
 * Structures and routines associated with pagedep caching.
 */
LIST_HEAD(pagedep_hashhead, pagedep) *pagedep_hashtbl;
u_long	pagedep_hash;		/* size of hash table - 1 */
#define	PAGEDEP_HASH(mp, inum, lbn) \
	(&pagedep_hashtbl[((((register_t)(mp)) >> 13) + (inum) + (lbn)) & \
	    pagedep_hash])

static int
pagedep_find(pagedephd, ino, lbn, mp, flags, pagedeppp)
	struct pagedep_hashhead *pagedephd;
	ino_t ino;
	ufs_lbn_t lbn;
	struct mount *mp;
	int flags;
	struct pagedep **pagedeppp;
{
	struct pagedep *pagedep;

	LIST_FOREACH(pagedep, pagedephd, pd_hash)
		if (ino == pagedep->pd_ino &&
		    lbn == pagedep->pd_lbn &&
		    mp == pagedep->pd_list.wk_mp)
			break;
	if (pagedep) {
		*pagedeppp = pagedep;
		if ((flags & DEPALLOC) != 0 &&
		    (pagedep->pd_state & ONWORKLIST) == 0)
			return (0);
		return (1);
	}
	*pagedeppp = NULL;
	return (0);
}
/*
 * Look up a pagedep. Return 1 if found, 0 if not found or found
 * when asked to allocate but not associated with any buffer.
 * If not found, allocate if DEPALLOC flag is passed.
 * Found or allocated entry is returned in pagedeppp.
 * This routine must be called with splbio interrupts blocked.
 */
static int
pagedep_lookup(ip, lbn, flags, pagedeppp)
	struct inode *ip;
	ufs_lbn_t lbn;
	int flags;
	struct pagedep **pagedeppp;
{
	struct pagedep *pagedep;
	struct pagedep_hashhead *pagedephd;
	struct mount *mp;
	int ret;
	int i;

	mtx_assert(&lk, MA_OWNED);
	mp = ITOV(ip)->v_mount;
	pagedephd = PAGEDEP_HASH(mp, ip->i_number, lbn);

	ret = pagedep_find(pagedephd, ip->i_number, lbn, mp, flags, pagedeppp);
	if (*pagedeppp || (flags & DEPALLOC) == 0)
		return (ret);
	FREE_LOCK(&lk);
	MALLOC(pagedep, struct pagedep *, sizeof(struct pagedep),
	    M_PAGEDEP, M_SOFTDEP_FLAGS|M_ZERO);
	workitem_alloc(&pagedep->pd_list, D_PAGEDEP, mp);
	ACQUIRE_LOCK(&lk);
	ret = pagedep_find(pagedephd, ip->i_number, lbn, mp, flags, pagedeppp);
	if (*pagedeppp) {
		WORKITEM_FREE(pagedep, D_PAGEDEP);
		return (ret);
	}
	pagedep->pd_ino = ip->i_number;
	pagedep->pd_lbn = lbn;
	LIST_INIT(&pagedep->pd_dirremhd);
	LIST_INIT(&pagedep->pd_pendinghd);
	for (i = 0; i < DAHASHSZ; i++)
		LIST_INIT(&pagedep->pd_diraddhd[i]);
	LIST_INSERT_HEAD(pagedephd, pagedep, pd_hash);
	*pagedeppp = pagedep;
	return (0);
}

/*
 * Structures and routines associated with inodedep caching.
 */
LIST_HEAD(inodedep_hashhead, inodedep) *inodedep_hashtbl;
static u_long	inodedep_hash;	/* size of hash table - 1 */
static long	num_inodedep;	/* number of inodedep allocated */
#define	INODEDEP_HASH(fs, inum) \
      (&inodedep_hashtbl[((((register_t)(fs)) >> 13) + (inum)) & inodedep_hash])

static int
inodedep_find(inodedephd, fs, inum, inodedeppp)
	struct inodedep_hashhead *inodedephd;
	struct fs *fs;
	ino_t inum;
	struct inodedep **inodedeppp;
{
	struct inodedep *inodedep;

	LIST_FOREACH(inodedep, inodedephd, id_hash)
		if (inum == inodedep->id_ino && fs == inodedep->id_fs)
			break;
	if (inodedep) {
		*inodedeppp = inodedep;
		return (1);
	}
	*inodedeppp = NULL;

	return (0);
}
/*
 * Look up an inodedep. Return 1 if found, 0 if not found.
 * If not found, allocate if DEPALLOC flag is passed.
 * Found or allocated entry is returned in inodedeppp.
 * This routine must be called with splbio interrupts blocked.
 */
static int
inodedep_lookup(mp, inum, flags, inodedeppp)
	struct mount *mp;
	ino_t inum;
	int flags;
	struct inodedep **inodedeppp;
{
	struct inodedep *inodedep;
	struct inodedep_hashhead *inodedephd;
	struct fs *fs;

	mtx_assert(&lk, MA_OWNED);
	fs = VFSTOUFS(mp)->um_fs;
	inodedephd = INODEDEP_HASH(fs, inum);

	if (inodedep_find(inodedephd, fs, inum, inodedeppp))
		return (1);
	if ((flags & DEPALLOC) == 0)
		return (0);
	/*
	 * If we are over our limit, try to improve the situation.
	 */
	if (num_inodedep > max_softdeps && (flags & NODELAY) == 0)
		request_cleanup(mp, FLUSH_INODES);
	FREE_LOCK(&lk);
	MALLOC(inodedep, struct inodedep *, sizeof(struct inodedep),
		M_INODEDEP, M_SOFTDEP_FLAGS);
	workitem_alloc(&inodedep->id_list, D_INODEDEP, mp);
	ACQUIRE_LOCK(&lk);
	if (inodedep_find(inodedephd, fs, inum, inodedeppp)) {
		WORKITEM_FREE(inodedep, D_INODEDEP);
		return (1);
	}
	num_inodedep += 1;
	inodedep->id_fs = fs;
	inodedep->id_ino = inum;
	inodedep->id_state = ALLCOMPLETE;
	inodedep->id_nlinkdelta = 0;
	inodedep->id_savedino1 = NULL;
	inodedep->id_savedsize = -1;
	inodedep->id_savedextsize = -1;
	inodedep->id_buf = NULL;
	LIST_INIT(&inodedep->id_pendinghd);
	LIST_INIT(&inodedep->id_inowait);
	LIST_INIT(&inodedep->id_bufwait);
	TAILQ_INIT(&inodedep->id_inoupdt);
	TAILQ_INIT(&inodedep->id_newinoupdt);
	TAILQ_INIT(&inodedep->id_extupdt);
	TAILQ_INIT(&inodedep->id_newextupdt);
	LIST_INSERT_HEAD(inodedephd, inodedep, id_hash);
	*inodedeppp = inodedep;
	return (0);
}

/*
 * Structures and routines associated with newblk caching.
 */
LIST_HEAD(newblk_hashhead, newblk) *newblk_hashtbl;
u_long	newblk_hash;		/* size of hash table - 1 */
#define	NEWBLK_HASH(fs, inum) \
	(&newblk_hashtbl[((((register_t)(fs)) >> 13) + (inum)) & newblk_hash])

static int
newblk_find(newblkhd, fs, newblkno, newblkpp)
	struct newblk_hashhead *newblkhd;
	struct fs *fs;
	ufs2_daddr_t newblkno;
	struct newblk **newblkpp;
{
	struct newblk *newblk;

	LIST_FOREACH(newblk, newblkhd, nb_hash)
		if (newblkno == newblk->nb_newblkno && fs == newblk->nb_fs)
			break;
	if (newblk) {
		*newblkpp = newblk;
		return (1);
	}
	*newblkpp = NULL;
	return (0);
}

/*
 * Look up a newblk. Return 1 if found, 0 if not found.
 * If not found, allocate if DEPALLOC flag is passed.
 * Found or allocated entry is returned in newblkpp.
 */
static int
newblk_lookup(fs, newblkno, flags, newblkpp)
	struct fs *fs;
	ufs2_daddr_t newblkno;
	int flags;
	struct newblk **newblkpp;
{
	struct newblk *newblk;
	struct newblk_hashhead *newblkhd;

	newblkhd = NEWBLK_HASH(fs, newblkno);
	if (newblk_find(newblkhd, fs, newblkno, newblkpp))
		return (1);
	if ((flags & DEPALLOC) == 0)
		return (0);
	FREE_LOCK(&lk);
	MALLOC(newblk, struct newblk *, sizeof(struct newblk),
		M_NEWBLK, M_SOFTDEP_FLAGS);
	ACQUIRE_LOCK(&lk);
	if (newblk_find(newblkhd, fs, newblkno, newblkpp)) {
		FREE(newblk, M_NEWBLK);
		return (1);
	}
	newblk->nb_state = 0;
	newblk->nb_fs = fs;
	newblk->nb_newblkno = newblkno;
	LIST_INSERT_HEAD(newblkhd, newblk, nb_hash);
	*newblkpp = newblk;
	return (0);
}

/*
 * Executed during filesystem system initialization before
 * mounting any filesystems.
 */
void 
softdep_initialize()
{

	LIST_INIT(&mkdirlisthd);
	max_softdeps = desiredvnodes * 4;
	pagedep_hashtbl = hashinit(desiredvnodes / 5, M_PAGEDEP,
	    &pagedep_hash);
	inodedep_hashtbl = hashinit(desiredvnodes, M_INODEDEP, &inodedep_hash);
	newblk_hashtbl = hashinit(64, M_NEWBLK, &newblk_hash);

	/* initialise bioops hack */
	bioops.io_start = softdep_disk_io_initiation;
	bioops.io_complete = softdep_disk_write_complete;
	bioops.io_deallocate = softdep_deallocate_dependencies;
	bioops.io_countdeps = softdep_count_dependencies;
}

/*
 * Executed after all filesystems have been unmounted during
 * filesystem module unload.
 */
void
softdep_uninitialize()
{

	hashdestroy(pagedep_hashtbl, M_PAGEDEP, pagedep_hash);
	hashdestroy(inodedep_hashtbl, M_INODEDEP, inodedep_hash);
	hashdestroy(newblk_hashtbl, M_NEWBLK, newblk_hash);
}

/*
 * Called at mount time to notify the dependency code that a
 * filesystem wishes to use it.
 */
int
softdep_mount(devvp, mp, fs, cred)
	struct vnode *devvp;
	struct mount *mp;
	struct fs *fs;
	struct ucred *cred;
{
	struct csum_total cstotal;
	struct ufsmount *ump;
	struct cg *cgp;
	struct buf *bp;
	int error, cyl;

	MNT_ILOCK(mp);
	mp->mnt_flag = (mp->mnt_flag & ~MNT_ASYNC) | MNT_SOFTDEP;
	if ((mp->mnt_kern_flag & MNTK_SOFTDEP) == 0) {
		mp->mnt_kern_flag = (mp->mnt_kern_flag & ~MNTK_ASYNC) | 
			MNTK_SOFTDEP;
		mp->mnt_noasync++;
	}
	MNT_IUNLOCK(mp);
	ump = VFSTOUFS(mp);
	LIST_INIT(&ump->softdep_workitem_pending);
	ump->softdep_worklist_tail = NULL;
	ump->softdep_on_worklist = 0;
	ump->softdep_deps = 0;
	/*
	 * When doing soft updates, the counters in the
	 * superblock may have gotten out of sync. Recomputation
	 * can take a long time and can be deferred for background
	 * fsck.  However, the old behavior of scanning the cylinder
	 * groups and recalculating them at mount time is available
	 * by setting vfs.ffs.compute_summary_at_mount to one.
	 */
	if (compute_summary_at_mount == 0 || fs->fs_clean != 0)
		return (0);
	bzero(&cstotal, sizeof cstotal);
	for (cyl = 0; cyl < fs->fs_ncg; cyl++) {
		if ((error = bread(devvp, fsbtodb(fs, cgtod(fs, cyl)),
		    fs->fs_cgsize, cred, &bp)) != 0) {
			brelse(bp);
			return (error);
		}
		cgp = (struct cg *)bp->b_data;
		cstotal.cs_nffree += cgp->cg_cs.cs_nffree;
		cstotal.cs_nbfree += cgp->cg_cs.cs_nbfree;
		cstotal.cs_nifree += cgp->cg_cs.cs_nifree;
		cstotal.cs_ndir += cgp->cg_cs.cs_ndir;
		fs->fs_cs(fs, cyl) = cgp->cg_cs;
		brelse(bp);
	}
#ifdef DEBUG
	if (bcmp(&cstotal, &fs->fs_cstotal, sizeof cstotal))
		printf("%s: superblock summary recomputed\n", fs->fs_fsmnt);
#endif
	bcopy(&cstotal, &fs->fs_cstotal, sizeof cstotal);
	return (0);
}

/*
 * Protecting the freemaps (or bitmaps).
 * 
 * To eliminate the need to execute fsck before mounting a filesystem
 * after a power failure, one must (conservatively) guarantee that the
 * on-disk copy of the bitmaps never indicate that a live inode or block is
 * free.  So, when a block or inode is allocated, the bitmap should be
 * updated (on disk) before any new pointers.  When a block or inode is
 * freed, the bitmap should not be updated until all pointers have been
 * reset.  The latter dependency is handled by the delayed de-allocation
 * approach described below for block and inode de-allocation.  The former
 * dependency is handled by calling the following procedure when a block or
 * inode is allocated. When an inode is allocated an "inodedep" is created
 * with its DEPCOMPLETE flag cleared until its bitmap is written to disk.
 * Each "inodedep" is also inserted into the hash indexing structure so
 * that any additional link additions can be made dependent on the inode
 * allocation.
 * 
 * The ufs filesystem maintains a number of free block counts (e.g., per
 * cylinder group, per cylinder and per <cylinder, rotational position> pair)
 * in addition to the bitmaps.  These counts are used to improve efficiency
 * during allocation and therefore must be consistent with the bitmaps.
 * There is no convenient way to guarantee post-crash consistency of these
 * counts with simple update ordering, for two main reasons: (1) The counts
 * and bitmaps for a single cylinder group block are not in the same disk
 * sector.  If a disk write is interrupted (e.g., by power failure), one may
 * be written and the other not.  (2) Some of the counts are located in the
 * superblock rather than the cylinder group block. So, we focus our soft
 * updates implementation on protecting the bitmaps. When mounting a
 * filesystem, we recompute the auxiliary counts from the bitmaps.
 */

/*
 * Called just after updating the cylinder group block to allocate an inode.
 */
void
softdep_setup_inomapdep(bp, ip, newinum)
	struct buf *bp;		/* buffer for cylgroup block with inode map */
	struct inode *ip;	/* inode related to allocation */
	ino_t newinum;		/* new inode number being allocated */
{
	struct inodedep *inodedep;
	struct bmsafemap *bmsafemap;

	/*
	 * Create a dependency for the newly allocated inode.
	 * Panic if it already exists as something is seriously wrong.
	 * Otherwise add it to the dependency list for the buffer holding
	 * the cylinder group map from which it was allocated.
	 */
	ACQUIRE_LOCK(&lk);
	if ((inodedep_lookup(UFSTOVFS(ip->i_ump), newinum, DEPALLOC|NODELAY,
	    &inodedep)))
		panic("softdep_setup_inomapdep: dependency for new inode "
		    "already exists");
	inodedep->id_buf = bp;
	inodedep->id_state &= ~DEPCOMPLETE;
	bmsafemap = bmsafemap_lookup(inodedep->id_list.wk_mp, bp);
	LIST_INSERT_HEAD(&bmsafemap->sm_inodedephd, inodedep, id_deps);
	FREE_LOCK(&lk);
}

/*
 * Called just after updating the cylinder group block to
 * allocate block or fragment.
 */
void
softdep_setup_blkmapdep(bp, mp, newblkno)
	struct buf *bp;		/* buffer for cylgroup block with block map */
	struct mount *mp;	/* filesystem doing allocation */
	ufs2_daddr_t newblkno;	/* number of newly allocated block */
{
	struct newblk *newblk;
	struct bmsafemap *bmsafemap;
	struct fs *fs;

	fs = VFSTOUFS(mp)->um_fs;
	/*
	 * Create a dependency for the newly allocated block.
	 * Add it to the dependency list for the buffer holding
	 * the cylinder group map from which it was allocated.
	 */
	ACQUIRE_LOCK(&lk);
	if (newblk_lookup(fs, newblkno, DEPALLOC, &newblk) != 0)
		panic("softdep_setup_blkmapdep: found block");
	newblk->nb_bmsafemap = bmsafemap = bmsafemap_lookup(mp, bp);
	LIST_INSERT_HEAD(&bmsafemap->sm_newblkhd, newblk, nb_deps);
	FREE_LOCK(&lk);
}

/*
 * Find the bmsafemap associated with a cylinder group buffer.
 * If none exists, create one. The buffer must be locked when
 * this routine is called and this routine must be called with
 * splbio interrupts blocked.
 */
static struct bmsafemap *
bmsafemap_lookup(mp, bp)
	struct mount *mp;
	struct buf *bp;
{
	struct bmsafemap *bmsafemap;
	struct worklist *wk;

	mtx_assert(&lk, MA_OWNED);
	LIST_FOREACH(wk, &bp->b_dep, wk_list)
		if (wk->wk_type == D_BMSAFEMAP)
			return (WK_BMSAFEMAP(wk));
	FREE_LOCK(&lk);
	MALLOC(bmsafemap, struct bmsafemap *, sizeof(struct bmsafemap),
		M_BMSAFEMAP, M_SOFTDEP_FLAGS);
	workitem_alloc(&bmsafemap->sm_list, D_BMSAFEMAP, mp);
	bmsafemap->sm_buf = bp;
	LIST_INIT(&bmsafemap->sm_allocdirecthd);
	LIST_INIT(&bmsafemap->sm_allocindirhd);
	LIST_INIT(&bmsafemap->sm_inodedephd);
	LIST_INIT(&bmsafemap->sm_newblkhd);
	ACQUIRE_LOCK(&lk);
	WORKLIST_INSERT(&bp->b_dep, &bmsafemap->sm_list);
	return (bmsafemap);
}

/*
 * Direct block allocation dependencies.
 * 
 * When a new block is allocated, the corresponding disk locations must be
 * initialized (with zeros or new data) before the on-disk inode points to
 * them.  Also, the freemap from which the block was allocated must be
 * updated (on disk) before the inode's pointer. These two dependencies are
 * independent of each other and are needed for all file blocks and indirect
 * blocks that are pointed to directly by the inode.  Just before the
 * "in-core" version of the inode is updated with a newly allocated block
 * number, a procedure (below) is called to setup allocation dependency
 * structures.  These structures are removed when the corresponding
 * dependencies are satisfied or when the block allocation becomes obsolete
 * (i.e., the file is deleted, the block is de-allocated, or the block is a
 * fragment that gets upgraded).  All of these cases are handled in
 * procedures described later.
 * 
 * When a file extension causes a fragment to be upgraded, either to a larger
 * fragment or to a full block, the on-disk location may change (if the
 * previous fragment could not simply be extended). In this case, the old
 * fragment must be de-allocated, but not until after the inode's pointer has
 * been updated. In most cases, this is handled by later procedures, which
 * will construct a "freefrag" structure to be added to the workitem queue
 * when the inode update is complete (or obsolete).  The main exception to
 * this is when an allocation occurs while a pending allocation dependency
 * (for the same block pointer) remains.  This case is handled in the main
 * allocation dependency setup procedure by immediately freeing the
 * unreferenced fragments.
 */ 
void 
softdep_setup_allocdirect(ip, lbn, newblkno, oldblkno, newsize, oldsize, bp)
	struct inode *ip;	/* inode to which block is being added */
	ufs_lbn_t lbn;		/* block pointer within inode */
	ufs2_daddr_t newblkno;	/* disk block number being added */
	ufs2_daddr_t oldblkno;	/* previous block number, 0 unless frag */
	long newsize;		/* size of new block */
	long oldsize;		/* size of new block */
	struct buf *bp;		/* bp for allocated block */
{
	struct allocdirect *adp, *oldadp;
	struct allocdirectlst *adphead;
	struct bmsafemap *bmsafemap;
	struct inodedep *inodedep;
	struct pagedep *pagedep;
	struct newblk *newblk;
	struct mount *mp;

	mp = UFSTOVFS(ip->i_ump);
	MALLOC(adp, struct allocdirect *, sizeof(struct allocdirect),
		M_ALLOCDIRECT, M_SOFTDEP_FLAGS|M_ZERO);
	workitem_alloc(&adp->ad_list, D_ALLOCDIRECT, mp);
	adp->ad_lbn = lbn;
	adp->ad_newblkno = newblkno;
	adp->ad_oldblkno = oldblkno;
	adp->ad_newsize = newsize;
	adp->ad_oldsize = oldsize;
	adp->ad_state = ATTACHED;
	LIST_INIT(&adp->ad_newdirblk);
	if (newblkno == oldblkno)
		adp->ad_freefrag = NULL;
	else
		adp->ad_freefrag = newfreefrag(ip, oldblkno, oldsize);

	ACQUIRE_LOCK(&lk);
	if (lbn >= NDADDR) {
		/* allocating an indirect block */
		if (oldblkno != 0)
			panic("softdep_setup_allocdirect: non-zero indir");
	} else {
		/*
		 * Allocating a direct block.
		 *
		 * If we are allocating a directory block, then we must
		 * allocate an associated pagedep to track additions and
		 * deletions.
		 */
		if ((ip->i_mode & IFMT) == IFDIR &&
		    pagedep_lookup(ip, lbn, DEPALLOC, &pagedep) == 0)
			WORKLIST_INSERT(&bp->b_dep, &pagedep->pd_list);
	}
	if (newblk_lookup(ip->i_fs, newblkno, 0, &newblk) == 0)
		panic("softdep_setup_allocdirect: lost block");
	if (newblk->nb_state == DEPCOMPLETE) {
		adp->ad_state |= DEPCOMPLETE;
		adp->ad_buf = NULL;
	} else {
		bmsafemap = newblk->nb_bmsafemap;
		adp->ad_buf = bmsafemap->sm_buf;
		LIST_REMOVE(newblk, nb_deps);
		LIST_INSERT_HEAD(&bmsafemap->sm_allocdirecthd, adp, ad_deps);
	}
	LIST_REMOVE(newblk, nb_hash);
	FREE(newblk, M_NEWBLK);

	inodedep_lookup(mp, ip->i_number, DEPALLOC | NODELAY, &inodedep);
	adp->ad_inodedep = inodedep;
	WORKLIST_INSERT(&bp->b_dep, &adp->ad_list);
	/*
	 * The list of allocdirects must be kept in sorted and ascending
	 * order so that the rollback routines can quickly determine the
	 * first uncommitted block (the size of the file stored on disk
	 * ends at the end of the lowest committed fragment, or if there
	 * are no fragments, at the end of the highest committed block).
	 * Since files generally grow, the typical case is that the new
	 * block is to be added at the end of the list. We speed this
	 * special case by checking against the last allocdirect in the
	 * list before laboriously traversing the list looking for the
	 * insertion point.
	 */
	adphead = &inodedep->id_newinoupdt;
	oldadp = TAILQ_LAST(adphead, allocdirectlst);
	if (oldadp == NULL || oldadp->ad_lbn <= lbn) {
		/* insert at end of list */
		TAILQ_INSERT_TAIL(adphead, adp, ad_next);
		if (oldadp != NULL && oldadp->ad_lbn == lbn)
			allocdirect_merge(adphead, adp, oldadp);
		FREE_LOCK(&lk);
		return;
	}
	TAILQ_FOREACH(oldadp, adphead, ad_next) {
		if (oldadp->ad_lbn >= lbn)
			break;
	}
	if (oldadp == NULL)
		panic("softdep_setup_allocdirect: lost entry");
	/* insert in middle of list */
	TAILQ_INSERT_BEFORE(oldadp, adp, ad_next);
	if (oldadp->ad_lbn == lbn)
		allocdirect_merge(adphead, adp, oldadp);
	FREE_LOCK(&lk);
}

/*
 * Replace an old allocdirect dependency with a newer one.
 * This routine must be called with splbio interrupts blocked.
 */
static void
allocdirect_merge(adphead, newadp, oldadp)
	struct allocdirectlst *adphead;	/* head of list holding allocdirects */
	struct allocdirect *newadp;	/* allocdirect being added */
	struct allocdirect *oldadp;	/* existing allocdirect being checked */
{
	struct worklist *wk;
	struct freefrag *freefrag;
	struct newdirblk *newdirblk;

	mtx_assert(&lk, MA_OWNED);
	if (newadp->ad_oldblkno != oldadp->ad_newblkno ||
	    newadp->ad_oldsize != oldadp->ad_newsize ||
	    newadp->ad_lbn >= NDADDR)
		panic("%s %jd != new %jd || old size %ld != new %ld",
		    "allocdirect_merge: old blkno",
		    (intmax_t)newadp->ad_oldblkno,
		    (intmax_t)oldadp->ad_newblkno,
		    newadp->ad_oldsize, oldadp->ad_newsize);
	newadp->ad_oldblkno = oldadp->ad_oldblkno;
	newadp->ad_oldsize = oldadp->ad_oldsize;
	/*
	 * If the old dependency had a fragment to free or had never
	 * previously had a block allocated, then the new dependency
	 * can immediately post its freefrag and adopt the old freefrag.
	 * This action is done by swapping the freefrag dependencies.
	 * The new dependency gains the old one's freefrag, and the
	 * old one gets the new one and then immediately puts it on
	 * the worklist when it is freed by free_allocdirect. It is
	 * not possible to do this swap when the old dependency had a
	 * non-zero size but no previous fragment to free. This condition
	 * arises when the new block is an extension of the old block.
	 * Here, the first part of the fragment allocated to the new
	 * dependency is part of the block currently claimed on disk by
	 * the old dependency, so cannot legitimately be freed until the
	 * conditions for the new dependency are fulfilled.
	 */
	if (oldadp->ad_freefrag != NULL || oldadp->ad_oldblkno == 0) {
		freefrag = newadp->ad_freefrag;
		newadp->ad_freefrag = oldadp->ad_freefrag;
		oldadp->ad_freefrag = freefrag;
	}
	/*
	 * If we are tracking a new directory-block allocation,
	 * move it from the old allocdirect to the new allocdirect.
	 */
	if ((wk = LIST_FIRST(&oldadp->ad_newdirblk)) != NULL) {
		newdirblk = WK_NEWDIRBLK(wk);
		WORKLIST_REMOVE(&newdirblk->db_list);
		if (!LIST_EMPTY(&oldadp->ad_newdirblk))
			panic("allocdirect_merge: extra newdirblk");
		WORKLIST_INSERT(&newadp->ad_newdirblk, &newdirblk->db_list);
	}
	free_allocdirect(adphead, oldadp, 0);
}
		
/*
 * Allocate a new freefrag structure if needed.
 */
static struct freefrag *
newfreefrag(ip, blkno, size)
	struct inode *ip;
	ufs2_daddr_t blkno;
	long size;
{
	struct freefrag *freefrag;
	struct fs *fs;

	if (blkno == 0)
		return (NULL);
	fs = ip->i_fs;
	if (fragnum(fs, blkno) + numfrags(fs, size) > fs->fs_frag)
		panic("newfreefrag: frag size");
	MALLOC(freefrag, struct freefrag *, sizeof(struct freefrag),
		M_FREEFRAG, M_SOFTDEP_FLAGS);
	workitem_alloc(&freefrag->ff_list, D_FREEFRAG, UFSTOVFS(ip->i_ump));
	freefrag->ff_inum = ip->i_number;
	freefrag->ff_blkno = blkno;
	freefrag->ff_fragsize = size;
	return (freefrag);
}

/*
 * This workitem de-allocates fragments that were replaced during
 * file block allocation.
 */
static void 
handle_workitem_freefrag(freefrag)
	struct freefrag *freefrag;
{
	struct ufsmount *ump = VFSTOUFS(freefrag->ff_list.wk_mp);

	ffs_blkfree(ump, ump->um_fs, ump->um_devvp, freefrag->ff_blkno,
	    freefrag->ff_fragsize, freefrag->ff_inum);
	ACQUIRE_LOCK(&lk);
	WORKITEM_FREE(freefrag, D_FREEFRAG);
	FREE_LOCK(&lk);
}

/*
 * Set up a dependency structure for an external attributes data block.
 * This routine follows much of the structure of softdep_setup_allocdirect.
 * See the description of softdep_setup_allocdirect above for details.
 */
void 
softdep_setup_allocext(ip, lbn, newblkno, oldblkno, newsize, oldsize, bp)
	struct inode *ip;
	ufs_lbn_t lbn;
	ufs2_daddr_t newblkno;
	ufs2_daddr_t oldblkno;
	long newsize;
	long oldsize;
	struct buf *bp;
{
	struct allocdirect *adp, *oldadp;
	struct allocdirectlst *adphead;
	struct bmsafemap *bmsafemap;
	struct inodedep *inodedep;
	struct newblk *newblk;
	struct mount *mp;

	mp = UFSTOVFS(ip->i_ump);
	MALLOC(adp, struct allocdirect *, sizeof(struct allocdirect),
		M_ALLOCDIRECT, M_SOFTDEP_FLAGS|M_ZERO);
	workitem_alloc(&adp->ad_list, D_ALLOCDIRECT, mp);
	adp->ad_lbn = lbn;
	adp->ad_newblkno = newblkno;
	adp->ad_oldblkno = oldblkno;
	adp->ad_newsize = newsize;
	adp->ad_oldsize = oldsize;
	adp->ad_state = ATTACHED | EXTDATA;
	LIST_INIT(&adp->ad_newdirblk);
	if (newblkno == oldblkno)
		adp->ad_freefrag = NULL;
	else
		adp->ad_freefrag = newfreefrag(ip, oldblkno, oldsize);

	ACQUIRE_LOCK(&lk);
	if (newblk_lookup(ip->i_fs, newblkno, 0, &newblk) == 0)
		panic("softdep_setup_allocext: lost block");

	inodedep_lookup(mp, ip->i_number, DEPALLOC | NODELAY, &inodedep);
	adp->ad_inodedep = inodedep;

	if (newblk->nb_state == DEPCOMPLETE) {
		adp->ad_state |= DEPCOMPLETE;
		adp->ad_buf = NULL;
	} else {
		bmsafemap = newblk->nb_bmsafemap;
		adp->ad_buf = bmsafemap->sm_buf;
		LIST_REMOVE(newblk, nb_deps);
		LIST_INSERT_HEAD(&bmsafemap->sm_allocdirecthd, adp, ad_deps);
	}
	LIST_REMOVE(newblk, nb_hash);
	FREE(newblk, M_NEWBLK);

	WORKLIST_INSERT(&bp->b_dep, &adp->ad_list);
	if (lbn >= NXADDR)
		panic("softdep_setup_allocext: lbn %lld > NXADDR",
		    (long long)lbn);
	/*
	 * The list of allocdirects must be kept in sorted and ascending
	 * order so that the rollback routines can quickly determine the
	 * first uncommitted block (the size of the file stored on disk
	 * ends at the end of the lowest committed fragment, or if there
	 * are no fragments, at the end of the highest committed block).
	 * Since files generally grow, the typical case is that the new
	 * block is to be added at the end of the list. We speed this
	 * special case by checking against the last allocdirect in the
	 * list before laboriously traversing the list looking for the
	 * insertion point.
	 */
	adphead = &inodedep->id_newextupdt;
	oldadp = TAILQ_LAST(adphead, allocdirectlst);
	if (oldadp == NULL || oldadp->ad_lbn <= lbn) {
		/* insert at end of list */
		TAILQ_INSERT_TAIL(adphead, adp, ad_next);
		if (oldadp != NULL && oldadp->ad_lbn == lbn)
			allocdirect_merge(adphead, adp, oldadp);
		FREE_LOCK(&lk);
		return;
	}
	TAILQ_FOREACH(oldadp, adphead, ad_next) {
		if (oldadp->ad_lbn >= lbn)
			break;
	}
	if (oldadp == NULL)
		panic("softdep_setup_allocext: lost entry");
	/* insert in middle of list */
	TAILQ_INSERT_BEFORE(oldadp, adp, ad_next);
	if (oldadp->ad_lbn == lbn)
		allocdirect_merge(adphead, adp, oldadp);
	FREE_LOCK(&lk);
}

/*
 * Indirect block allocation dependencies.
 * 
 * The same dependencies that exist for a direct block also exist when
 * a new block is allocated and pointed to by an entry in a block of
 * indirect pointers. The undo/redo states described above are also
 * used here. Because an indirect block contains many pointers that
 * may have dependencies, a second copy of the entire in-memory indirect
 * block is kept. The buffer cache copy is always completely up-to-date.
 * The second copy, which is used only as a source for disk writes,
 * contains only the safe pointers (i.e., those that have no remaining
 * update dependencies). The second copy is freed when all pointers
 * are safe. The cache is not allowed to replace indirect blocks with
 * pending update dependencies. If a buffer containing an indirect
 * block with dependencies is written, these routines will mark it
 * dirty again. It can only be successfully written once all the
 * dependencies are removed. The ffs_fsync routine in conjunction with
 * softdep_sync_metadata work together to get all the dependencies
 * removed so that a file can be successfully written to disk. Three
 * procedures are used when setting up indirect block pointer
 * dependencies. The division is necessary because of the organization
 * of the "balloc" routine and because of the distinction between file
 * pages and file metadata blocks.
 */

/*
 * Allocate a new allocindir structure.
 */
static struct allocindir *
newallocindir(ip, ptrno, newblkno, oldblkno)
	struct inode *ip;	/* inode for file being extended */
	int ptrno;		/* offset of pointer in indirect block */
	ufs2_daddr_t newblkno;	/* disk block number being added */
	ufs2_daddr_t oldblkno;	/* previous block number, 0 if none */
{
	struct allocindir *aip;

	MALLOC(aip, struct allocindir *, sizeof(struct allocindir),
		M_ALLOCINDIR, M_SOFTDEP_FLAGS|M_ZERO);
	workitem_alloc(&aip->ai_list, D_ALLOCINDIR, UFSTOVFS(ip->i_ump));
	aip->ai_state = ATTACHED;
	aip->ai_offset = ptrno;
	aip->ai_newblkno = newblkno;
	aip->ai_oldblkno = oldblkno;
	aip->ai_freefrag = newfreefrag(ip, oldblkno, ip->i_fs->fs_bsize);
	return (aip);
}

/*
 * Called just before setting an indirect block pointer
 * to a newly allocated file page.
 */
void
softdep_setup_allocindir_page(ip, lbn, bp, ptrno, newblkno, oldblkno, nbp)
	struct inode *ip;	/* inode for file being extended */
	ufs_lbn_t lbn;		/* allocated block number within file */
	struct buf *bp;		/* buffer with indirect blk referencing page */
	int ptrno;		/* offset of pointer in indirect block */
	ufs2_daddr_t newblkno;	/* disk block number being added */
	ufs2_daddr_t oldblkno;	/* previous block number, 0 if none */
	struct buf *nbp;	/* buffer holding allocated page */
{
	struct allocindir *aip;
	struct pagedep *pagedep;

	ASSERT_VOP_LOCKED(ITOV(ip), "softdep_setup_allocindir_page");
	aip = newallocindir(ip, ptrno, newblkno, oldblkno);
	ACQUIRE_LOCK(&lk);
	/*
	 * If we are allocating a directory page, then we must
	 * allocate an associated pagedep to track additions and
	 * deletions.
	 */
	if ((ip->i_mode & IFMT) == IFDIR &&
	    pagedep_lookup(ip, lbn, DEPALLOC, &pagedep) == 0)
		WORKLIST_INSERT(&nbp->b_dep, &pagedep->pd_list);
	WORKLIST_INSERT(&nbp->b_dep, &aip->ai_list);
	setup_allocindir_phase2(bp, ip, aip);
	FREE_LOCK(&lk);
}

/*
 * Called just before setting an indirect block pointer to a
 * newly allocated indirect block.
 */
void
softdep_setup_allocindir_meta(nbp, ip, bp, ptrno, newblkno)
	struct buf *nbp;	/* newly allocated indirect block */
	struct inode *ip;	/* inode for file being extended */
	struct buf *bp;		/* indirect block referencing allocated block */
	int ptrno;		/* offset of pointer in indirect block */
	ufs2_daddr_t newblkno;	/* disk block number being added */
{
	struct allocindir *aip;

	ASSERT_VOP_LOCKED(ITOV(ip), "softdep_setup_allocindir_meta");
	aip = newallocindir(ip, ptrno, newblkno, 0);
	ACQUIRE_LOCK(&lk);
	WORKLIST_INSERT(&nbp->b_dep, &aip->ai_list);
	setup_allocindir_phase2(bp, ip, aip);
	FREE_LOCK(&lk);
}

/*
 * Called to finish the allocation of the "aip" allocated
 * by one of the two routines above.
 */
static void 
setup_allocindir_phase2(bp, ip, aip)
	struct buf *bp;		/* in-memory copy of the indirect block */
	struct inode *ip;	/* inode for file being extended */
	struct allocindir *aip;	/* allocindir allocated by the above routines */
{
	struct worklist *wk;
	struct indirdep *indirdep, *newindirdep;
	struct bmsafemap *bmsafemap;
	struct allocindir *oldaip;
	struct freefrag *freefrag;
	struct newblk *newblk;
	ufs2_daddr_t blkno;

	mtx_assert(&lk, MA_OWNED);
	if (bp->b_lblkno >= 0)
		panic("setup_allocindir_phase2: not indir blk");
	for (indirdep = NULL, newindirdep = NULL; ; ) {
		LIST_FOREACH(wk, &bp->b_dep, wk_list) {
			if (wk->wk_type != D_INDIRDEP)
				continue;
			indirdep = WK_INDIRDEP(wk);
			break;
		}
		if (indirdep == NULL && newindirdep) {
			indirdep = newindirdep;
			WORKLIST_INSERT(&bp->b_dep, &indirdep->ir_list);
			newindirdep = NULL;
		}
		if (indirdep) {
			if (newblk_lookup(ip->i_fs, aip->ai_newblkno, 0,
			    &newblk) == 0)
				panic("setup_allocindir: lost block");
			if (newblk->nb_state == DEPCOMPLETE) {
				aip->ai_state |= DEPCOMPLETE;
				aip->ai_buf = NULL;
			} else {
				bmsafemap = newblk->nb_bmsafemap;
				aip->ai_buf = bmsafemap->sm_buf;
				LIST_REMOVE(newblk, nb_deps);
				LIST_INSERT_HEAD(&bmsafemap->sm_allocindirhd,
				    aip, ai_deps);
			}
			LIST_REMOVE(newblk, nb_hash);
			FREE(newblk, M_NEWBLK);
			aip->ai_indirdep = indirdep;
			/*
			 * Check to see if there is an existing dependency
			 * for this block. If there is, merge the old
			 * dependency into the new one.
			 */
			if (aip->ai_oldblkno == 0)
				oldaip = NULL;
			else

				LIST_FOREACH(oldaip, &indirdep->ir_deplisthd, ai_next)
					if (oldaip->ai_offset == aip->ai_offset)
						break;
			freefrag = NULL;
			if (oldaip != NULL) {
				if (oldaip->ai_newblkno != aip->ai_oldblkno)
					panic("setup_allocindir_phase2: blkno");
				aip->ai_oldblkno = oldaip->ai_oldblkno;
				freefrag = aip->ai_freefrag;
				aip->ai_freefrag = oldaip->ai_freefrag;
				oldaip->ai_freefrag = NULL;
				free_allocindir(oldaip, NULL);
			}
			LIST_INSERT_HEAD(&indirdep->ir_deplisthd, aip, ai_next);
			if (ip->i_ump->um_fstype == UFS1)
				((ufs1_daddr_t *)indirdep->ir_savebp->b_data)
				    [aip->ai_offset] = aip->ai_oldblkno;
			else
				((ufs2_daddr_t *)indirdep->ir_savebp->b_data)
				    [aip->ai_offset] = aip->ai_oldblkno;
			FREE_LOCK(&lk);
			if (freefrag != NULL)
				handle_workitem_freefrag(freefrag);
		} else
			FREE_LOCK(&lk);
		if (newindirdep) {
			newindirdep->ir_savebp->b_flags |= B_INVAL | B_NOCACHE;
			brelse(newindirdep->ir_savebp);
			ACQUIRE_LOCK(&lk);
			WORKITEM_FREE((caddr_t)newindirdep, D_INDIRDEP);
			if (indirdep)
				break;
			FREE_LOCK(&lk);
		}
		if (indirdep) {
			ACQUIRE_LOCK(&lk);
			break;
		}
		MALLOC(newindirdep, struct indirdep *, sizeof(struct indirdep),
			M_INDIRDEP, M_SOFTDEP_FLAGS);
		workitem_alloc(&newindirdep->ir_list, D_INDIRDEP,
		    UFSTOVFS(ip->i_ump));
		newindirdep->ir_state = ATTACHED;
		if (ip->i_ump->um_fstype == UFS1)
			newindirdep->ir_state |= UFS1FMT;
		LIST_INIT(&newindirdep->ir_deplisthd);
		LIST_INIT(&newindirdep->ir_donehd);
		if (bp->b_blkno == bp->b_lblkno) {
			ufs_bmaparray(bp->b_vp, bp->b_lblkno, &blkno, bp,
			    NULL, NULL);
			bp->b_blkno = blkno;
		}
		newindirdep->ir_savebp =
		    getblk(ip->i_devvp, bp->b_blkno, bp->b_bcount, 0, 0, 0);
		BUF_KERNPROC(newindirdep->ir_savebp);
		bcopy(bp->b_data, newindirdep->ir_savebp->b_data, bp->b_bcount);
		ACQUIRE_LOCK(&lk);
	}
}

/*
 * Block de-allocation dependencies.
 * 
 * When blocks are de-allocated, the on-disk pointers must be nullified before
 * the blocks are made available for use by other files.  (The true
 * requirement is that old pointers must be nullified before new on-disk
 * pointers are set.  We chose this slightly more stringent requirement to
 * reduce complexity.) Our implementation handles this dependency by updating
 * the inode (or indirect block) appropriately but delaying the actual block
 * de-allocation (i.e., freemap and free space count manipulation) until
 * after the updated versions reach stable storage.  After the disk is
 * updated, the blocks can be safely de-allocated whenever it is convenient.
 * This implementation handles only the common case of reducing a file's
 * length to zero. Other cases are handled by the conventional synchronous
 * write approach.
 *
 * The ffs implementation with which we worked double-checks
 * the state of the block pointers and file size as it reduces
 * a file's length.  Some of this code is replicated here in our
 * soft updates implementation.  The freeblks->fb_chkcnt field is
 * used to transfer a part of this information to the procedure
 * that eventually de-allocates the blocks.
 *
 * This routine should be called from the routine that shortens
 * a file's length, before the inode's size or block pointers
 * are modified. It will save the block pointer information for
 * later release and zero the inode so that the calling routine
 * can release it.
 */
void
softdep_setup_freeblocks(ip, length, flags)
	struct inode *ip;	/* The inode whose length is to be reduced */
	off_t length;		/* The new length for the file */
	int flags;		/* IO_EXT and/or IO_NORMAL */
{
	struct freeblks *freeblks;
	struct inodedep *inodedep;
	struct allocdirect *adp;
	struct vnode *vp;
	struct buf *bp;
	struct fs *fs;
	ufs2_daddr_t extblocks, datablocks;
	struct mount *mp;
	int i, delay, error;

	fs = ip->i_fs;
	mp = UFSTOVFS(ip->i_ump);
	if (length != 0)
		panic("softdep_setup_freeblocks: non-zero length");
	MALLOC(freeblks, struct freeblks *, sizeof(struct freeblks),
		M_FREEBLKS, M_SOFTDEP_FLAGS|M_ZERO);
	workitem_alloc(&freeblks->fb_list, D_FREEBLKS, mp);
	freeblks->fb_state = ATTACHED;
	freeblks->fb_uid = ip->i_uid;
	freeblks->fb_previousinum = ip->i_number;
	freeblks->fb_devvp = ip->i_devvp;
	extblocks = 0;
	if (fs->fs_magic == FS_UFS2_MAGIC)
		extblocks = btodb(fragroundup(fs, ip->i_din2->di_extsize));
	datablocks = DIP(ip, i_blocks) - extblocks;
	if ((flags & IO_NORMAL) == 0) {
		freeblks->fb_oldsize = 0;
		freeblks->fb_chkcnt = 0;
	} else {
		freeblks->fb_oldsize = ip->i_size;
		ip->i_size = 0;
		DIP_SET(ip, i_size, 0);
		freeblks->fb_chkcnt = datablocks;
		for (i = 0; i < NDADDR; i++) {
			freeblks->fb_dblks[i] = DIP(ip, i_db[i]);
			DIP_SET(ip, i_db[i], 0);
		}
		for (i = 0; i < NIADDR; i++) {
			freeblks->fb_iblks[i] = DIP(ip, i_ib[i]);
			DIP_SET(ip, i_ib[i], 0);
		}
		/*
		 * If the file was removed, then the space being freed was
		 * accounted for then (see softdep_releasefile()). If the
		 * file is merely being truncated, then we account for it now.
		 */
		if ((ip->i_flag & IN_SPACECOUNTED) == 0) {
			UFS_LOCK(ip->i_ump);
			fs->fs_pendingblocks += datablocks;
			UFS_UNLOCK(ip->i_ump);
		}
	}
	if ((flags & IO_EXT) == 0) {
		freeblks->fb_oldextsize = 0;
	} else {
		freeblks->fb_oldextsize = ip->i_din2->di_extsize;
		ip->i_din2->di_extsize = 0;
		freeblks->fb_chkcnt += extblocks;
		for (i = 0; i < NXADDR; i++) {
			freeblks->fb_eblks[i] = ip->i_din2->di_extb[i];
			ip->i_din2->di_extb[i] = 0;
		}
	}
	DIP_SET(ip, i_blocks, DIP(ip, i_blocks) - freeblks->fb_chkcnt);
	/*
	 * Push the zero'ed inode to to its disk buffer so that we are free
	 * to delete its dependencies below. Once the dependencies are gone
	 * the buffer can be safely released.
	 */
	if ((error = bread(ip->i_devvp,
	    fsbtodb(fs, ino_to_fsba(fs, ip->i_number)),
	    (int)fs->fs_bsize, NOCRED, &bp)) != 0) {
		brelse(bp);
		softdep_error("softdep_setup_freeblocks", error);
	}
	if (ip->i_ump->um_fstype == UFS1)
		*((struct ufs1_dinode *)bp->b_data +
		    ino_to_fsbo(fs, ip->i_number)) = *ip->i_din1;
	else
		*((struct ufs2_dinode *)bp->b_data +
		    ino_to_fsbo(fs, ip->i_number)) = *ip->i_din2;
	/*
	 * Find and eliminate any inode dependencies.
	 */
	ACQUIRE_LOCK(&lk);
	(void) inodedep_lookup(mp, ip->i_number, DEPALLOC, &inodedep);
	if ((inodedep->id_state & IOSTARTED) != 0)
		panic("softdep_setup_freeblocks: inode busy");
	/*
	 * Add the freeblks structure to the list of operations that
	 * must await the zero'ed inode being written to disk. If we
	 * still have a bitmap dependency (delay == 0), then the inode
	 * has never been written to disk, so we can process the
	 * freeblks below once we have deleted the dependencies.
	 */
	delay = (inodedep->id_state & DEPCOMPLETE);
	if (delay)
		WORKLIST_INSERT(&inodedep->id_bufwait, &freeblks->fb_list);
	/*
	 * Because the file length has been truncated to zero, any
	 * pending block allocation dependency structures associated
	 * with this inode are obsolete and can simply be de-allocated.
	 * We must first merge the two dependency lists to get rid of
	 * any duplicate freefrag structures, then purge the merged list.
	 * If we still have a bitmap dependency, then the inode has never
	 * been written to disk, so we can free any fragments without delay.
	 */
	if (flags & IO_NORMAL) {
		merge_inode_lists(&inodedep->id_newinoupdt,
		    &inodedep->id_inoupdt);
		while ((adp = TAILQ_FIRST(&inodedep->id_inoupdt)) != 0)
			free_allocdirect(&inodedep->id_inoupdt, adp, delay);
	}
	if (flags & IO_EXT) {
		merge_inode_lists(&inodedep->id_newextupdt,
		    &inodedep->id_extupdt);
		while ((adp = TAILQ_FIRST(&inodedep->id_extupdt)) != 0)
			free_allocdirect(&inodedep->id_extupdt, adp, delay);
	}
	FREE_LOCK(&lk);
	bdwrite(bp);
	/*
	 * We must wait for any I/O in progress to finish so that
	 * all potential buffers on the dirty list will be visible.
	 * Once they are all there, walk the list and get rid of
	 * any dependencies.
	 */
	vp = ITOV(ip);
	VI_LOCK(vp);
	drain_output(vp);
restart:
	TAILQ_FOREACH(bp, &vp->v_bufobj.bo_dirty.bv_hd, b_bobufs) {
		if (((flags & IO_EXT) == 0 && (bp->b_xflags & BX_ALTDATA)) ||
		    ((flags & IO_NORMAL) == 0 &&
		      (bp->b_xflags & BX_ALTDATA) == 0))
			continue;
		if ((bp = getdirtybuf(bp, VI_MTX(vp), MNT_WAIT)) == NULL)
			goto restart;
		VI_UNLOCK(vp);
		ACQUIRE_LOCK(&lk);
		(void) inodedep_lookup(mp, ip->i_number, 0, &inodedep);
		deallocate_dependencies(bp, inodedep);
		FREE_LOCK(&lk);
		bp->b_flags |= B_INVAL | B_NOCACHE;
		brelse(bp);
		VI_LOCK(vp);
		goto restart;
	}
	VI_UNLOCK(vp);
	ACQUIRE_LOCK(&lk);
	if (inodedep_lookup(mp, ip->i_number, 0, &inodedep) != 0)
		(void) free_inodedep(inodedep);

	if(delay) {
		freeblks->fb_state |= DEPCOMPLETE;
		/*
		 * If the inode with zeroed block pointers is now on disk
		 * we can start freeing blocks. Add freeblks to the worklist
		 * instead of calling  handle_workitem_freeblocks directly as
		 * it is more likely that additional IO is needed to complete
		 * the request here than in the !delay case.
		 */  
		if ((freeblks->fb_state & ALLCOMPLETE) == ALLCOMPLETE)
			add_to_worklist(&freeblks->fb_list);
	}

	FREE_LOCK(&lk);
	/*
	 * If the inode has never been written to disk (delay == 0),
	 * then we can process the freeblks now that we have deleted
	 * the dependencies.
	 */
	if (!delay)
		handle_workitem_freeblocks(freeblks, 0);
}

/*
 * Reclaim any dependency structures from a buffer that is about to
 * be reallocated to a new vnode. The buffer must be locked, thus,
 * no I/O completion operations can occur while we are manipulating
 * its associated dependencies. The mutex is held so that other I/O's
 * associated with related dependencies do not occur.
 */
static void
deallocate_dependencies(bp, inodedep)
	struct buf *bp;
	struct inodedep *inodedep;
{
	struct worklist *wk;
	struct indirdep *indirdep;
	struct allocindir *aip;
	struct pagedep *pagedep;
	struct dirrem *dirrem;
	struct diradd *dap;
	int i;

	mtx_assert(&lk, MA_OWNED);
	while ((wk = LIST_FIRST(&bp->b_dep)) != NULL) {
		switch (wk->wk_type) {

		case D_INDIRDEP:
			indirdep = WK_INDIRDEP(wk);
			/*
			 * None of the indirect pointers will ever be visible,
			 * so they can simply be tossed. GOINGAWAY ensures
			 * that allocated pointers will be saved in the buffer
			 * cache until they are freed. Note that they will
			 * only be able to be found by their physical address
			 * since the inode mapping the logical address will
			 * be gone. The save buffer used for the safe copy
			 * was allocated in setup_allocindir_phase2 using
			 * the physical address so it could be used for this
			 * purpose. Hence we swap the safe copy with the real
			 * copy, allowing the safe copy to be freed and holding
			 * on to the real copy for later use in indir_trunc.
			 */
			if (indirdep->ir_state & GOINGAWAY)
				panic("deallocate_dependencies: already gone");
			indirdep->ir_state |= GOINGAWAY;
			VFSTOUFS(bp->b_vp->v_mount)->um_numindirdeps += 1;
			while ((aip = LIST_FIRST(&indirdep->ir_deplisthd)) != 0)
				free_allocindir(aip, inodedep);
			if (bp->b_lblkno >= 0 ||
			    bp->b_blkno != indirdep->ir_savebp->b_lblkno)
				panic("deallocate_dependencies: not indir");
			bcopy(bp->b_data, indirdep->ir_savebp->b_data,
			    bp->b_bcount);
			WORKLIST_REMOVE(wk);
			WORKLIST_INSERT(&indirdep->ir_savebp->b_dep, wk);
			continue;

		case D_PAGEDEP:
			pagedep = WK_PAGEDEP(wk);
			/*
			 * None of the directory additions will ever be
			 * visible, so they can simply be tossed.
			 */
			for (i = 0; i < DAHASHSZ; i++)
				while ((dap =
				    LIST_FIRST(&pagedep->pd_diraddhd[i])))
					free_diradd(dap);
			while ((dap = LIST_FIRST(&pagedep->pd_pendinghd)) != 0)
				free_diradd(dap);
			/*
			 * Copy any directory remove dependencies to the list
			 * to be processed after the zero'ed inode is written.
			 * If the inode has already been written, then they 
			 * can be dumped directly onto the work list.
			 */
			LIST_FOREACH(dirrem, &pagedep->pd_dirremhd, dm_next) {
				LIST_REMOVE(dirrem, dm_next);
				dirrem->dm_dirinum = pagedep->pd_ino;
				if (inodedep == NULL ||
				    (inodedep->id_state & ALLCOMPLETE) ==
				     ALLCOMPLETE)
					add_to_worklist(&dirrem->dm_list);
				else
					WORKLIST_INSERT(&inodedep->id_bufwait,
					    &dirrem->dm_list);
			}
			if ((pagedep->pd_state & NEWBLOCK) != 0) {
				LIST_FOREACH(wk, &inodedep->id_bufwait, wk_list)
					if (wk->wk_type == D_NEWDIRBLK &&
					    WK_NEWDIRBLK(wk)->db_pagedep ==
					      pagedep)
						break;
				if (wk != NULL) {
					WORKLIST_REMOVE(wk);
					free_newdirblk(WK_NEWDIRBLK(wk));
				} else
					panic("deallocate_dependencies: "
					      "lost pagedep");
			}
			WORKLIST_REMOVE(&pagedep->pd_list);
			LIST_REMOVE(pagedep, pd_hash);
			WORKITEM_FREE(pagedep, D_PAGEDEP);
			continue;

		case D_ALLOCINDIR:
			free_allocindir(WK_ALLOCINDIR(wk), inodedep);
			continue;

		case D_ALLOCDIRECT:
		case D_INODEDEP:
			panic("deallocate_dependencies: Unexpected type %s",
			    TYPENAME(wk->wk_type));
			/* NOTREACHED */

		default:
			panic("deallocate_dependencies: Unknown type %s",
			    TYPENAME(wk->wk_type));
			/* NOTREACHED */
		}
	}
}

/*
 * Free an allocdirect. Generate a new freefrag work request if appropriate.
 * This routine must be called with splbio interrupts blocked.
 */
static void
free_allocdirect(adphead, adp, delay)
	struct allocdirectlst *adphead;
	struct allocdirect *adp;
	int delay;
{
	struct newdirblk *newdirblk;
	struct worklist *wk;

	mtx_assert(&lk, MA_OWNED);
	if ((adp->ad_state & DEPCOMPLETE) == 0)
		LIST_REMOVE(adp, ad_deps);
	TAILQ_REMOVE(adphead, adp, ad_next);
	if ((adp->ad_state & COMPLETE) == 0)
		WORKLIST_REMOVE(&adp->ad_list);
	if (adp->ad_freefrag != NULL) {
		if (delay)
			WORKLIST_INSERT(&adp->ad_inodedep->id_bufwait,
			    &adp->ad_freefrag->ff_list);
		else
			add_to_worklist(&adp->ad_freefrag->ff_list);
	}
	if ((wk = LIST_FIRST(&adp->ad_newdirblk)) != NULL) {
		newdirblk = WK_NEWDIRBLK(wk);
		WORKLIST_REMOVE(&newdirblk->db_list);
		if (!LIST_EMPTY(&adp->ad_newdirblk))
			panic("free_allocdirect: extra newdirblk");
		if (delay)
			WORKLIST_INSERT(&adp->ad_inodedep->id_bufwait,
			    &newdirblk->db_list);
		else
			free_newdirblk(newdirblk);
	}
	WORKITEM_FREE(adp, D_ALLOCDIRECT);
}

/*
 * Free a newdirblk. Clear the NEWBLOCK flag on its associated pagedep.
 * This routine must be called with splbio interrupts blocked.
 */
static void
free_newdirblk(newdirblk)
	struct newdirblk *newdirblk;
{
	struct pagedep *pagedep;
	struct diradd *dap;
	int i;

	mtx_assert(&lk, MA_OWNED);
	/*
	 * If the pagedep is still linked onto the directory buffer
	 * dependency chain, then some of the entries on the
	 * pd_pendinghd list may not be committed to disk yet. In
	 * this case, we will simply clear the NEWBLOCK flag and
	 * let the pd_pendinghd list be processed when the pagedep
	 * is next written. If the pagedep is no longer on the buffer
	 * dependency chain, then all the entries on the pd_pending
	 * list are committed to disk and we can free them here.
	 */
	pagedep = newdirblk->db_pagedep;
	pagedep->pd_state &= ~NEWBLOCK;
	if ((pagedep->pd_state & ONWORKLIST) == 0)
		while ((dap = LIST_FIRST(&pagedep->pd_pendinghd)) != NULL)
			free_diradd(dap);
	/*
	 * If no dependencies remain, the pagedep will be freed.
	 */
	for (i = 0; i < DAHASHSZ; i++)
		if (!LIST_EMPTY(&pagedep->pd_diraddhd[i]))
			break;
	if (i == DAHASHSZ && (pagedep->pd_state & ONWORKLIST) == 0) {
		LIST_REMOVE(pagedep, pd_hash);
		WORKITEM_FREE(pagedep, D_PAGEDEP);
	}
	WORKITEM_FREE(newdirblk, D_NEWDIRBLK);
}

/*
 * Prepare an inode to be freed. The actual free operation is not
 * done until the zero'ed inode has been written to disk.
 */
void
softdep_freefile(pvp, ino, mode)
	struct vnode *pvp;
	ino_t ino;
	int mode;
{
	struct inode *ip = VTOI(pvp);
	struct inodedep *inodedep;
	struct freefile *freefile;

	/*
	 * This sets up the inode de-allocation dependency.
	 */
	MALLOC(freefile, struct freefile *, sizeof(struct freefile),
		M_FREEFILE, M_SOFTDEP_FLAGS);
	workitem_alloc(&freefile->fx_list, D_FREEFILE, pvp->v_mount);
	freefile->fx_mode = mode;
	freefile->fx_oldinum = ino;
	freefile->fx_devvp = ip->i_devvp;
	if ((ip->i_flag & IN_SPACECOUNTED) == 0) {
		UFS_LOCK(ip->i_ump);
		ip->i_fs->fs_pendinginodes += 1;
		UFS_UNLOCK(ip->i_ump);
	}

	/*
	 * If the inodedep does not exist, then the zero'ed inode has
	 * been written to disk. If the allocated inode has never been
	 * written to disk, then the on-disk inode is zero'ed. In either
	 * case we can free the file immediately.
	 */
	ACQUIRE_LOCK(&lk);
	if (inodedep_lookup(pvp->v_mount, ino, 0, &inodedep) == 0 ||
	    check_inode_unwritten(inodedep)) {
		FREE_LOCK(&lk);
		handle_workitem_freefile(freefile);
		return;
	}
	WORKLIST_INSERT(&inodedep->id_inowait, &freefile->fx_list);
	FREE_LOCK(&lk);
	ip->i_flag |= IN_MODIFIED;
}

/*
 * Check to see if an inode has never been written to disk. If
 * so free the inodedep and return success, otherwise return failure.
 * This routine must be called with splbio interrupts blocked.
 *
 * If we still have a bitmap dependency, then the inode has never
 * been written to disk. Drop the dependency as it is no longer
 * necessary since the inode is being deallocated. We set the
 * ALLCOMPLETE flags since the bitmap now properly shows that the
 * inode is not allocated. Even if the inode is actively being
 * written, it has been rolled back to its zero'ed state, so we
 * are ensured that a zero inode is what is on the disk. For short
 * lived files, this change will usually result in removing all the
 * dependencies from the inode so that it can be freed immediately.
 */
static int
check_inode_unwritten(inodedep)
	struct inodedep *inodedep;
{

	mtx_assert(&lk, MA_OWNED);
	if ((inodedep->id_state & DEPCOMPLETE) != 0 ||
	    !LIST_EMPTY(&inodedep->id_pendinghd) ||
	    !LIST_EMPTY(&inodedep->id_bufwait) ||
	    !LIST_EMPTY(&inodedep->id_inowait) ||
	    !TAILQ_EMPTY(&inodedep->id_inoupdt) ||
	    !TAILQ_EMPTY(&inodedep->id_newinoupdt) ||
	    !TAILQ_EMPTY(&inodedep->id_extupdt) ||
	    !TAILQ_EMPTY(&inodedep->id_newextupdt) ||
	    inodedep->id_nlinkdelta != 0)
		return (0);

	/*
	 * Another process might be in initiate_write_inodeblock_ufs[12]
	 * trying to allocate memory without holding "Softdep Lock".
	 */
	if ((inodedep->id_state & IOSTARTED) != 0 &&
	    inodedep->id_savedino1 == NULL)
		return (0);

	inodedep->id_state |= ALLCOMPLETE;
	LIST_REMOVE(inodedep, id_deps);
	inodedep->id_buf = NULL;
	if (inodedep->id_state & ONWORKLIST)
		WORKLIST_REMOVE(&inodedep->id_list);
	if (inodedep->id_savedino1 != NULL) {
		FREE(inodedep->id_savedino1, M_SAVEDINO);
		inodedep->id_savedino1 = NULL;
	}
	if (free_inodedep(inodedep) == 0)
		panic("check_inode_unwritten: busy inode");
	return (1);
}

/*
 * Try to free an inodedep structure. Return 1 if it could be freed.
 */
static int
free_inodedep(inodedep)
	struct inodedep *inodedep;
{

	mtx_assert(&lk, MA_OWNED);
	if ((inodedep->id_state & ONWORKLIST) != 0 ||
	    (inodedep->id_state & ALLCOMPLETE) != ALLCOMPLETE ||
	    !LIST_EMPTY(&inodedep->id_pendinghd) ||
	    !LIST_EMPTY(&inodedep->id_bufwait) ||
	    !LIST_EMPTY(&inodedep->id_inowait) ||
	    !TAILQ_EMPTY(&inodedep->id_inoupdt) ||
	    !TAILQ_EMPTY(&inodedep->id_newinoupdt) ||
	    !TAILQ_EMPTY(&inodedep->id_extupdt) ||
	    !TAILQ_EMPTY(&inodedep->id_newextupdt) ||
	    inodedep->id_nlinkdelta != 0 || inodedep->id_savedino1 != NULL)
		return (0);
	LIST_REMOVE(inodedep, id_hash);
	WORKITEM_FREE(inodedep, D_INODEDEP);
	num_inodedep -= 1;
	return (1);
}

/*
 * This workitem routine performs the block de-allocation.
 * The workitem is added to the pending list after the updated
 * inode block has been written to disk.  As mentioned above,
 * checks regarding the number of blocks de-allocated (compared
 * to the number of blocks allocated for the file) are also
 * performed in this function.
 */
static void
handle_workitem_freeblocks(freeblks, flags)
	struct freeblks *freeblks;
	int flags;
{
	struct inode *ip;
	struct vnode *vp;
	struct fs *fs;
	struct ufsmount *ump;
	int i, nblocks, level, bsize;
	ufs2_daddr_t bn, blocksreleased = 0;
	int error, allerror = 0;
	ufs_lbn_t baselbns[NIADDR], tmpval;
	int fs_pendingblocks;

	ump = VFSTOUFS(freeblks->fb_list.wk_mp);
	fs = ump->um_fs;
	fs_pendingblocks = 0;
	tmpval = 1;
	baselbns[0] = NDADDR;
	for (i = 1; i < NIADDR; i++) {
		tmpval *= NINDIR(fs);
		baselbns[i] = baselbns[i - 1] + tmpval;
	}
	nblocks = btodb(fs->fs_bsize);
	blocksreleased = 0;
	/*
	 * Release all extended attribute blocks or frags.
	 */
	if (freeblks->fb_oldextsize > 0) {
		for (i = (NXADDR - 1); i >= 0; i--) {
			if ((bn = freeblks->fb_eblks[i]) == 0)
				continue;
			bsize = sblksize(fs, freeblks->fb_oldextsize, i);
			ffs_blkfree(ump, fs, freeblks->fb_devvp, bn, bsize,
			    freeblks->fb_previousinum);
			blocksreleased += btodb(bsize);
		}
	}
	/*
	 * Release all data blocks or frags.
	 */
	if (freeblks->fb_oldsize > 0) {
		/*
		 * Indirect blocks first.
		 */
		for (level = (NIADDR - 1); level >= 0; level--) {
			if ((bn = freeblks->fb_iblks[level]) == 0)
				continue;
			if ((error = indir_trunc(freeblks, fsbtodb(fs, bn),
			    level, baselbns[level], &blocksreleased)) != 0)
				allerror = error;
			ffs_blkfree(ump, fs, freeblks->fb_devvp, bn,
			    fs->fs_bsize, freeblks->fb_previousinum);
			fs_pendingblocks += nblocks;
			blocksreleased += nblocks;
		}
		/*
		 * All direct blocks or frags.
		 */
		for (i = (NDADDR - 1); i >= 0; i--) {
			if ((bn = freeblks->fb_dblks[i]) == 0)
				continue;
			bsize = sblksize(fs, freeblks->fb_oldsize, i);
			ffs_blkfree(ump, fs, freeblks->fb_devvp, bn, bsize,
			    freeblks->fb_previousinum);
			fs_pendingblocks += btodb(bsize);
			blocksreleased += btodb(bsize);
		}
	}
	UFS_LOCK(ump);
	fs->fs_pendingblocks -= fs_pendingblocks;
	UFS_UNLOCK(ump);
	/*
	 * If we still have not finished background cleanup, then check
	 * to see if the block count needs to be adjusted.
	 */
	if (freeblks->fb_chkcnt != blocksreleased &&
	    (fs->fs_flags & FS_UNCLEAN) != 0 &&
	    ffs_vget(freeblks->fb_list.wk_mp, freeblks->fb_previousinum,
	    (flags & LK_NOWAIT) | LK_EXCLUSIVE, &vp) == 0) {
		ip = VTOI(vp);
		DIP_SET(ip, i_blocks, DIP(ip, i_blocks) + \
		    freeblks->fb_chkcnt - blocksreleased);
		ip->i_flag |= IN_CHANGE;
		vput(vp);
	}

#ifdef DIAGNOSTIC
	if (freeblks->fb_chkcnt != blocksreleased &&
	    ((fs->fs_flags & FS_UNCLEAN) == 0 || (flags & LK_NOWAIT) != 0))
		printf("handle_workitem_freeblocks: block count\n");
	if (allerror)
		softdep_error("handle_workitem_freeblks", allerror);
#endif /* DIAGNOSTIC */

	ACQUIRE_LOCK(&lk);
	WORKITEM_FREE(freeblks, D_FREEBLKS);
	FREE_LOCK(&lk);
}

/*
 * Release blocks associated with the inode ip and stored in the indirect
 * block dbn. If level is greater than SINGLE, the block is an indirect block
 * and recursive calls to indirtrunc must be used to cleanse other indirect
 * blocks.
 */
static int
indir_trunc(freeblks, dbn, level, lbn, countp)
	struct freeblks *freeblks;
	ufs2_daddr_t dbn;
	int level;
	ufs_lbn_t lbn;
	ufs2_daddr_t *countp;
{
	struct buf *bp;
	struct fs *fs;
	struct worklist *wk;
	struct indirdep *indirdep;
	struct ufsmount *ump;
	ufs1_daddr_t *bap1 = 0;
	ufs2_daddr_t nb, *bap2 = 0;
	ufs_lbn_t lbnadd;
	int i, nblocks, ufs1fmt;
	int error, allerror = 0;
	int fs_pendingblocks;

	ump = VFSTOUFS(freeblks->fb_list.wk_mp);
	fs = ump->um_fs;
	fs_pendingblocks = 0;
	lbnadd = 1;
	for (i = level; i > 0; i--)
		lbnadd *= NINDIR(fs);
	/*
	 * Get buffer of block pointers to be freed. This routine is not
	 * called until the zero'ed inode has been written, so it is safe
	 * to free blocks as they are encountered. Because the inode has
	 * been zero'ed, calls to bmap on these blocks will fail. So, we
	 * have to use the on-disk address and the block device for the
	 * filesystem to look them up. If the file was deleted before its
	 * indirect blocks were all written to disk, the routine that set
	 * us up (deallocate_dependencies) will have arranged to leave
	 * a complete copy of the indirect block in memory for our use.
	 * Otherwise we have to read the blocks in from the disk.
	 */
#ifdef notyet
	bp = getblk(freeblks->fb_devvp, dbn, (int)fs->fs_bsize, 0, 0,
	    GB_NOCREAT);
#else
	bp = incore(&freeblks->fb_devvp->v_bufobj, dbn);
#endif
	ACQUIRE_LOCK(&lk);
	if (bp != NULL && (wk = LIST_FIRST(&bp->b_dep)) != NULL) {
		if (wk->wk_type != D_INDIRDEP ||
		    (indirdep = WK_INDIRDEP(wk))->ir_savebp != bp ||
		    (indirdep->ir_state & GOINGAWAY) == 0)
			panic("indir_trunc: lost indirdep");
		WORKLIST_REMOVE(wk);
		WORKITEM_FREE(indirdep, D_INDIRDEP);
		if (!LIST_EMPTY(&bp->b_dep))
			panic("indir_trunc: dangling dep");
		ump->um_numindirdeps -= 1;
		FREE_LOCK(&lk);
	} else {
#ifdef notyet
		if (bp)
			brelse(bp);
#endif
		FREE_LOCK(&lk);
		error = bread(freeblks->fb_devvp, dbn, (int)fs->fs_bsize,
		    NOCRED, &bp);
		if (error) {
			brelse(bp);
			return (error);
		}
	}
	/*
	 * Recursively free indirect blocks.
	 */
	if (ump->um_fstype == UFS1) {
		ufs1fmt = 1;
		bap1 = (ufs1_daddr_t *)bp->b_data;
	} else {
		ufs1fmt = 0;
		bap2 = (ufs2_daddr_t *)bp->b_data;
	}
	nblocks = btodb(fs->fs_bsize);
	for (i = NINDIR(fs) - 1; i >= 0; i--) {
		if (ufs1fmt)
			nb = bap1[i];
		else
			nb = bap2[i];
		if (nb == 0)
			continue;
		if (level != 0) {
			if ((error = indir_trunc(freeblks, fsbtodb(fs, nb),
			     level - 1, lbn + (i * lbnadd), countp)) != 0)
				allerror = error;
		}
		ffs_blkfree(ump, fs, freeblks->fb_devvp, nb, fs->fs_bsize,
		    freeblks->fb_previousinum);
		fs_pendingblocks += nblocks;
		*countp += nblocks;
	}
	UFS_LOCK(ump);
	fs->fs_pendingblocks -= fs_pendingblocks;
	UFS_UNLOCK(ump);
	bp->b_flags |= B_INVAL | B_NOCACHE;
	brelse(bp);
	return (allerror);
}

/*
 * Free an allocindir.
 * This routine must be called with splbio interrupts blocked.
 */
static void
free_allocindir(aip, inodedep)
	struct allocindir *aip;
	struct inodedep *inodedep;
{
	struct freefrag *freefrag;

	mtx_assert(&lk, MA_OWNED);
	if ((aip->ai_state & DEPCOMPLETE) == 0)
		LIST_REMOVE(aip, ai_deps);
	if (aip->ai_state & ONWORKLIST)
		WORKLIST_REMOVE(&aip->ai_list);
	LIST_REMOVE(aip, ai_next);
	if ((freefrag = aip->ai_freefrag) != NULL) {
		if (inodedep == NULL)
			add_to_worklist(&freefrag->ff_list);
		else
			WORKLIST_INSERT(&inodedep->id_bufwait,
			    &freefrag->ff_list);
	}
	WORKITEM_FREE(aip, D_ALLOCINDIR);
}

/*
 * Directory entry addition dependencies.
 * 
 * When adding a new directory entry, the inode (with its incremented link
 * count) must be written to disk before the directory entry's pointer to it.
 * Also, if the inode is newly allocated, the corresponding freemap must be
 * updated (on disk) before the directory entry's pointer. These requirements
 * are met via undo/redo on the directory entry's pointer, which consists
 * simply of the inode number.
 * 
 * As directory entries are added and deleted, the free space within a
 * directory block can become fragmented.  The ufs filesystem will compact
 * a fragmented directory block to make space for a new entry. When this
 * occurs, the offsets of previously added entries change. Any "diradd"
 * dependency structures corresponding to these entries must be updated with
 * the new offsets.
 */

/*
 * This routine is called after the in-memory inode's link
 * count has been incremented, but before the directory entry's
 * pointer to the inode has been set.
 */
int
softdep_setup_directory_add(bp, dp, diroffset, newinum, newdirbp, isnewblk)
	struct buf *bp;		/* buffer containing directory block */
	struct inode *dp;	/* inode for directory */
	off_t diroffset;	/* offset of new entry in directory */
	ino_t newinum;		/* inode referenced by new directory entry */
	struct buf *newdirbp;	/* non-NULL => contents of new mkdir */
	int isnewblk;		/* entry is in a newly allocated block */
{
	int offset;		/* offset of new entry within directory block */
	ufs_lbn_t lbn;		/* block in directory containing new entry */
	struct fs *fs;
	struct diradd *dap;
	struct allocdirect *adp;
	struct pagedep *pagedep;
	struct inodedep *inodedep;
	struct newdirblk *newdirblk = 0;
	struct mkdir *mkdir1, *mkdir2;
	struct mount *mp;

	/*
	 * Whiteouts have no dependencies.
	 */
	if (newinum == WINO) {
		if (newdirbp != NULL)
			bdwrite(newdirbp);
		return (0);
	}
	mp = UFSTOVFS(dp->i_ump);
	fs = dp->i_fs;
	lbn = lblkno(fs, diroffset);
	offset = blkoff(fs, diroffset);
	MALLOC(dap, struct diradd *, sizeof(struct diradd), M_DIRADD,
		M_SOFTDEP_FLAGS|M_ZERO);
	workitem_alloc(&dap->da_list, D_DIRADD, mp);
	dap->da_offset = offset;
	dap->da_newinum = newinum;
	dap->da_state = ATTACHED;
	if (isnewblk && lbn < NDADDR && fragoff(fs, diroffset) == 0) {
		MALLOC(newdirblk, struct newdirblk *, sizeof(struct newdirblk),
		    M_NEWDIRBLK, M_SOFTDEP_FLAGS);
		workitem_alloc(&newdirblk->db_list, D_NEWDIRBLK, mp);
	}
	if (newdirbp == NULL) {
		dap->da_state |= DEPCOMPLETE;
		ACQUIRE_LOCK(&lk);
	} else {
		dap->da_state |= MKDIR_BODY | MKDIR_PARENT;
		MALLOC(mkdir1, struct mkdir *, sizeof(struct mkdir), M_MKDIR,
		    M_SOFTDEP_FLAGS);
		workitem_alloc(&mkdir1->md_list, D_MKDIR, mp);
		mkdir1->md_state = MKDIR_BODY;
		mkdir1->md_diradd = dap;
		MALLOC(mkdir2, struct mkdir *, sizeof(struct mkdir), M_MKDIR,
		    M_SOFTDEP_FLAGS);
		workitem_alloc(&mkdir2->md_list, D_MKDIR, mp);
		mkdir2->md_state = MKDIR_PARENT;
		mkdir2->md_diradd = dap;
		/*
		 * Dependency on "." and ".." being written to disk.
		 */
		mkdir1->md_buf = newdirbp;
		ACQUIRE_LOCK(&lk);
		LIST_INSERT_HEAD(&mkdirlisthd, mkdir1, md_mkdirs);
		WORKLIST_INSERT(&newdirbp->b_dep, &mkdir1->md_list);
		FREE_LOCK(&lk);
		bdwrite(newdirbp);
		/*
		 * Dependency on link count increase for parent directory
		 */
		ACQUIRE_LOCK(&lk);
		if (inodedep_lookup(mp, dp->i_number, 0, &inodedep) == 0
		    || (inodedep->id_state & ALLCOMPLETE) == ALLCOMPLETE) {
			dap->da_state &= ~MKDIR_PARENT;
			WORKITEM_FREE(mkdir2, D_MKDIR);
		} else {
			LIST_INSERT_HEAD(&mkdirlisthd, mkdir2, md_mkdirs);
			WORKLIST_INSERT(&inodedep->id_bufwait,&mkdir2->md_list);
		}
	}
	/*
	 * Link into parent directory pagedep to await its being written.
	 */
	if (pagedep_lookup(dp, lbn, DEPALLOC, &pagedep) == 0)
		WORKLIST_INSERT(&bp->b_dep, &pagedep->pd_list);
	dap->da_pagedep = pagedep;
	LIST_INSERT_HEAD(&pagedep->pd_diraddhd[DIRADDHASH(offset)], dap,
	    da_pdlist);
	/*
	 * Link into its inodedep. Put it on the id_bufwait list if the inode
	 * is not yet written. If it is written, do the post-inode write
	 * processing to put it on the id_pendinghd list.
	 */
	(void) inodedep_lookup(mp, newinum, DEPALLOC, &inodedep);
	if ((inodedep->id_state & ALLCOMPLETE) == ALLCOMPLETE)
		diradd_inode_written(dap, inodedep);
	else
		WORKLIST_INSERT(&inodedep->id_bufwait, &dap->da_list);
	if (isnewblk) {
		/*
		 * Directories growing into indirect blocks are rare
		 * enough and the frequency of new block allocation
		 * in those cases even more rare, that we choose not
		 * to bother tracking them. Rather we simply force the
		 * new directory entry to disk.
		 */
		if (lbn >= NDADDR) {
			FREE_LOCK(&lk);
			/*
			 * We only have a new allocation when at the
			 * beginning of a new block, not when we are
			 * expanding into an existing block.
			 */
			if (blkoff(fs, diroffset) == 0)
				return (1);
			return (0);
		}
		/*
		 * We only have a new allocation when at the beginning
		 * of a new fragment, not when we are expanding into an
		 * existing fragment. Also, there is nothing to do if we
		 * are already tracking this block.
		 */
		if (fragoff(fs, diroffset) != 0) {
			FREE_LOCK(&lk);
			return (0);
		}
		if ((pagedep->pd_state & NEWBLOCK) != 0) {
			WORKITEM_FREE(newdirblk, D_NEWDIRBLK);
			FREE_LOCK(&lk);
			return (0);
		}
		/*
		 * Find our associated allocdirect and have it track us.
		 */
		if (inodedep_lookup(mp, dp->i_number, 0, &inodedep) == 0)
			panic("softdep_setup_directory_add: lost inodedep");
		adp = TAILQ_LAST(&inodedep->id_newinoupdt, allocdirectlst);
		if (adp == NULL || adp->ad_lbn != lbn)
			panic("softdep_setup_directory_add: lost entry");
		pagedep->pd_state |= NEWBLOCK;
		newdirblk->db_pagedep = pagedep;
		WORKLIST_INSERT(&adp->ad_newdirblk, &newdirblk->db_list);
	}
	FREE_LOCK(&lk);
	return (0);
}

/*
 * This procedure is called to change the offset of a directory
 * entry when compacting a directory block which must be owned
 * exclusively by the caller. Note that the actual entry movement
 * must be done in this procedure to ensure that no I/O completions
 * occur while the move is in progress.
 */
void 
softdep_change_directoryentry_offset(dp, base, oldloc, newloc, entrysize)
	struct inode *dp;	/* inode for directory */
	caddr_t base;		/* address of dp->i_offset */
	caddr_t oldloc;		/* address of old directory location */
	caddr_t newloc;		/* address of new directory location */
	int entrysize;		/* size of directory entry */
{
	int offset, oldoffset, newoffset;
	struct pagedep *pagedep;
	struct diradd *dap;
	ufs_lbn_t lbn;

	ACQUIRE_LOCK(&lk);
	lbn = lblkno(dp->i_fs, dp->i_offset);
	offset = blkoff(dp->i_fs, dp->i_offset);
	if (pagedep_lookup(dp, lbn, 0, &pagedep) == 0)
		goto done;
	oldoffset = offset + (oldloc - base);
	newoffset = offset + (newloc - base);

	LIST_FOREACH(dap, &pagedep->pd_diraddhd[DIRADDHASH(oldoffset)], da_pdlist) {
		if (dap->da_offset != oldoffset)
			continue;
		dap->da_offset = newoffset;
		if (DIRADDHASH(newoffset) == DIRADDHASH(oldoffset))
			break;
		LIST_REMOVE(dap, da_pdlist);
		LIST_INSERT_HEAD(&pagedep->pd_diraddhd[DIRADDHASH(newoffset)],
		    dap, da_pdlist);
		break;
	}
	if (dap == NULL) {

		LIST_FOREACH(dap, &pagedep->pd_pendinghd, da_pdlist) {
			if (dap->da_offset == oldoffset) {
				dap->da_offset = newoffset;
				break;
			}
		}
	}
done:
	bcopy(oldloc, newloc, entrysize);
	FREE_LOCK(&lk);
}

/*
 * Free a diradd dependency structure. This routine must be called
 * with splbio interrupts blocked.
 */
static void
free_diradd(dap)
	struct diradd *dap;
{
	struct dirrem *dirrem;
	struct pagedep *pagedep;
	struct inodedep *inodedep;
	struct mkdir *mkdir, *nextmd;

	mtx_assert(&lk, MA_OWNED);
	WORKLIST_REMOVE(&dap->da_list);
	LIST_REMOVE(dap, da_pdlist);
	if ((dap->da_state & DIRCHG) == 0) {
		pagedep = dap->da_pagedep;
	} else {
		dirrem = dap->da_previous;
		pagedep = dirrem->dm_pagedep;
		dirrem->dm_dirinum = pagedep->pd_ino;
		add_to_worklist(&dirrem->dm_list);
	}
	if (inodedep_lookup(pagedep->pd_list.wk_mp, dap->da_newinum,
	    0, &inodedep) != 0)
		(void) free_inodedep(inodedep);
	if ((dap->da_state & (MKDIR_PARENT | MKDIR_BODY)) != 0) {
		for (mkdir = LIST_FIRST(&mkdirlisthd); mkdir; mkdir = nextmd) {
			nextmd = LIST_NEXT(mkdir, md_mkdirs);
			if (mkdir->md_diradd != dap)
				continue;
			dap->da_state &= ~mkdir->md_state;
			WORKLIST_REMOVE(&mkdir->md_list);
			LIST_REMOVE(mkdir, md_mkdirs);
			WORKITEM_FREE(mkdir, D_MKDIR);
		}
		if ((dap->da_state & (MKDIR_PARENT | MKDIR_BODY)) != 0)
			panic("free_diradd: unfound ref");
	}
	WORKITEM_FREE(dap, D_DIRADD);
}

/*
 * Directory entry removal dependencies.
 * 
 * When removing a directory entry, the entry's inode pointer must be
 * zero'ed on disk before the corresponding inode's link count is decremented
 * (possibly freeing the inode for re-use). This dependency is handled by
 * updating the directory entry but delaying the inode count reduction until
 * after the directory block has been written to disk. After this point, the
 * inode count can be decremented whenever it is convenient.
 */

/*
 * This routine should be called immediately after removing
 * a directory entry.  The inode's link count should not be
 * decremented by the calling procedure -- the soft updates
 * code will do this task when it is safe.
 */
void 
softdep_setup_remove(bp, dp, ip, isrmdir)
	struct buf *bp;		/* buffer containing directory block */
	struct inode *dp;	/* inode for the directory being modified */
	struct inode *ip;	/* inode for directory entry being removed */
	int isrmdir;		/* indicates if doing RMDIR */
{
	struct dirrem *dirrem, *prevdirrem;

	/*
	 * Allocate a new dirrem if appropriate and ACQUIRE_LOCK.
	 */
	dirrem = newdirrem(bp, dp, ip, isrmdir, &prevdirrem);

	/*
	 * If the COMPLETE flag is clear, then there were no active
	 * entries and we want to roll back to a zeroed entry until
	 * the new inode is committed to disk. If the COMPLETE flag is
	 * set then we have deleted an entry that never made it to
	 * disk. If the entry we deleted resulted from a name change,
	 * then the old name still resides on disk. We cannot delete
	 * its inode (returned to us in prevdirrem) until the zeroed
	 * directory entry gets to disk. The new inode has never been
	 * referenced on the disk, so can be deleted immediately.
	 */
	if ((dirrem->dm_state & COMPLETE) == 0) {
		LIST_INSERT_HEAD(&dirrem->dm_pagedep->pd_dirremhd, dirrem,
		    dm_next);
		FREE_LOCK(&lk);
	} else {
		if (prevdirrem != NULL)
			LIST_INSERT_HEAD(&dirrem->dm_pagedep->pd_dirremhd,
			    prevdirrem, dm_next);
		dirrem->dm_dirinum = dirrem->dm_pagedep->pd_ino;
		FREE_LOCK(&lk);
		handle_workitem_remove(dirrem, NULL);
	}
}

/*
 * Allocate a new dirrem if appropriate and return it along with
 * its associated pagedep. Called without a lock, returns with lock.
 */
static long num_dirrem;		/* number of dirrem allocated */
static struct dirrem *
newdirrem(bp, dp, ip, isrmdir, prevdirremp)
	struct buf *bp;		/* buffer containing directory block */
	struct inode *dp;	/* inode for the directory being modified */
	struct inode *ip;	/* inode for directory entry being removed */
	int isrmdir;		/* indicates if doing RMDIR */
	struct dirrem **prevdirremp; /* previously referenced inode, if any */
{
	int offset;
	ufs_lbn_t lbn;
	struct diradd *dap;
	struct dirrem *dirrem;
	struct pagedep *pagedep;

	/*
	 * Whiteouts have no deletion dependencies.
	 */
	if (ip == NULL)
		panic("newdirrem: whiteout");
	/*
	 * If we are over our limit, try to improve the situation.
	 * Limiting the number of dirrem structures will also limit
	 * the number of freefile and freeblks structures.
	 */
	ACQUIRE_LOCK(&lk);
	if (num_dirrem > max_softdeps / 2)
		(void) request_cleanup(ITOV(dp)->v_mount, FLUSH_REMOVE);
	num_dirrem += 1;
	FREE_LOCK(&lk);
	MALLOC(dirrem, struct dirrem *, sizeof(struct dirrem),
		M_DIRREM, M_SOFTDEP_FLAGS|M_ZERO);
	workitem_alloc(&dirrem->dm_list, D_DIRREM, ITOV(dp)->v_mount);
	dirrem->dm_state = isrmdir ? RMDIR : 0;
	dirrem->dm_oldinum = ip->i_number;
	*prevdirremp = NULL;

	ACQUIRE_LOCK(&lk);
	lbn = lblkno(dp->i_fs, dp->i_offset);
	offset = blkoff(dp->i_fs, dp->i_offset);
	if (pagedep_lookup(dp, lbn, DEPALLOC, &pagedep) == 0)
		WORKLIST_INSERT(&bp->b_dep, &pagedep->pd_list);
	dirrem->dm_pagedep = pagedep;
	/*
	 * Check for a diradd dependency for the same directory entry.
	 * If present, then both dependencies become obsolete and can
	 * be de-allocated. Check for an entry on both the pd_dirraddhd
	 * list and the pd_pendinghd list.
	 */

	LIST_FOREACH(dap, &pagedep->pd_diraddhd[DIRADDHASH(offset)], da_pdlist)
		if (dap->da_offset == offset)
			break;
	if (dap == NULL) {

		LIST_FOREACH(dap, &pagedep->pd_pendinghd, da_pdlist)
			if (dap->da_offset == offset)
				break;
		if (dap == NULL)
			return (dirrem);
	}
	/*
	 * Must be ATTACHED at this point.
	 */
	if ((dap->da_state & ATTACHED) == 0)
		panic("newdirrem: not ATTACHED");
	if (dap->da_newinum != ip->i_number)
		panic("newdirrem: inum %d should be %d",
		    ip->i_number, dap->da_newinum);
	/*
	 * If we are deleting a changed name that never made it to disk,
	 * then return the dirrem describing the previous inode (which
	 * represents the inode currently referenced from this entry on disk).
	 */
	if ((dap->da_state & DIRCHG) != 0) {
		*prevdirremp = dap->da_previous;
		dap->da_state &= ~DIRCHG;
		dap->da_pagedep = pagedep;
	}
	/*
	 * We are deleting an entry that never made it to disk.
	 * Mark it COMPLETE so we can delete its inode immediately.
	 */
	dirrem->dm_state |= COMPLETE;
	free_diradd(dap);
	return (dirrem);
}

/*
 * Directory entry change dependencies.
 * 
 * Changing an existing directory entry requires that an add operation
 * be completed first followed by a deletion. The semantics for the addition
 * are identical to the description of adding a new entry above except
 * that the rollback is to the old inode number rather than zero. Once
 * the addition dependency is completed, the removal is done as described
 * in the removal routine above.
 */

/*
 * This routine should be called immediately after changing
 * a directory entry.  The inode's link count should not be
 * decremented by the calling procedure -- the soft updates
 * code will perform this task when it is safe.
 */
void 
softdep_setup_directory_change(bp, dp, ip, newinum, isrmdir)
	struct buf *bp;		/* buffer containing directory block */
	struct inode *dp;	/* inode for the directory being modified */
	struct inode *ip;	/* inode for directory entry being removed */
	ino_t newinum;		/* new inode number for changed entry */
	int isrmdir;		/* indicates if doing RMDIR */
{
	int offset;
	struct diradd *dap = NULL;
	struct dirrem *dirrem, *prevdirrem;
	struct pagedep *pagedep;
	struct inodedep *inodedep;
	struct mount *mp;

	offset = blkoff(dp->i_fs, dp->i_offset);
	mp = UFSTOVFS(dp->i_ump);

	/*
	 * Whiteouts do not need diradd dependencies.
	 */
	if (newinum != WINO) {
		MALLOC(dap, struct diradd *, sizeof(struct diradd),
		    M_DIRADD, M_SOFTDEP_FLAGS|M_ZERO);
		workitem_alloc(&dap->da_list, D_DIRADD, mp);
		dap->da_state = DIRCHG | ATTACHED | DEPCOMPLETE;
		dap->da_offset = offset;
		dap->da_newinum = newinum;
	}

	/*
	 * Allocate a new dirrem and ACQUIRE_LOCK.
	 */
	dirrem = newdirrem(bp, dp, ip, isrmdir, &prevdirrem);
	pagedep = dirrem->dm_pagedep;
	/*
	 * The possible values for isrmdir:
	 *	0 - non-directory file rename
	 *	1 - directory rename within same directory
	 *   inum - directory rename to new directory of given inode number
	 * When renaming to a new directory, we are both deleting and
	 * creating a new directory entry, so the link count on the new
	 * directory should not change. Thus we do not need the followup
	 * dirrem which is usually done in handle_workitem_remove. We set
	 * the DIRCHG flag to tell handle_workitem_remove to skip the 
	 * followup dirrem.
	 */
	if (isrmdir > 1)
		dirrem->dm_state |= DIRCHG;

	/*
	 * Whiteouts have no additional dependencies,
	 * so just put the dirrem on the correct list.
	 */
	if (newinum == WINO) {
		if ((dirrem->dm_state & COMPLETE) == 0) {
			LIST_INSERT_HEAD(&pagedep->pd_dirremhd, dirrem,
			    dm_next);
		} else {
			dirrem->dm_dirinum = pagedep->pd_ino;
			add_to_worklist(&dirrem->dm_list);
		}
		FREE_LOCK(&lk);
		return;
	}

	/*
	 * If the COMPLETE flag is clear, then there were no active
	 * entries and we want to roll back to the previous inode until
	 * the new inode is committed to disk. If the COMPLETE flag is
	 * set, then we have deleted an entry that never made it to disk.
	 * If the entry we deleted resulted from a name change, then the old
	 * inode reference still resides on disk. Any rollback that we do
	 * needs to be to that old inode (returned to us in prevdirrem). If
	 * the entry we deleted resulted from a create, then there is
	 * no entry on the disk, so we want to roll back to zero rather
	 * than the uncommitted inode. In either of the COMPLETE cases we
	 * want to immediately free the unwritten and unreferenced inode.
	 */
	if ((dirrem->dm_state & COMPLETE) == 0) {
		dap->da_previous = dirrem;
	} else {
		if (prevdirrem != NULL) {
			dap->da_previous = prevdirrem;
		} else {
			dap->da_state &= ~DIRCHG;
			dap->da_pagedep = pagedep;
		}
		dirrem->dm_dirinum = pagedep->pd_ino;
		add_to_worklist(&dirrem->dm_list);
	}
	/*
	 * Link into its inodedep. Put it on the id_bufwait list if the inode
	 * is not yet written. If it is written, do the post-inode write
	 * processing to put it on the id_pendinghd list.
	 */
	if (inodedep_lookup(mp, newinum, DEPALLOC, &inodedep) == 0 ||
	    (inodedep->id_state & ALLCOMPLETE) == ALLCOMPLETE) {
		dap->da_state |= COMPLETE;
		LIST_INSERT_HEAD(&pagedep->pd_pendinghd, dap, da_pdlist);
		WORKLIST_INSERT(&inodedep->id_pendinghd, &dap->da_list);
	} else {
		LIST_INSERT_HEAD(&pagedep->pd_diraddhd[DIRADDHASH(offset)],
		    dap, da_pdlist);
		WORKLIST_INSERT(&inodedep->id_bufwait, &dap->da_list);
	}
	FREE_LOCK(&lk);
}

/*
 * Called whenever the link count on an inode is changed.
 * It creates an inode dependency so that the new reference(s)
 * to the inode cannot be committed to disk until the updated
 * inode has been written.
 */
void
softdep_change_linkcnt(ip)
	struct inode *ip;	/* the inode with the increased link count */
{
	struct inodedep *inodedep;

	ACQUIRE_LOCK(&lk);
	(void) inodedep_lookup(UFSTOVFS(ip->i_ump), ip->i_number,
	    DEPALLOC, &inodedep);
	if (ip->i_nlink < ip->i_effnlink)
		panic("softdep_change_linkcnt: bad delta");
	inodedep->id_nlinkdelta = ip->i_nlink - ip->i_effnlink;
	FREE_LOCK(&lk);
}

/*
 * Called when the effective link count and the reference count
 * on an inode drops to zero. At this point there are no names
 * referencing the file in the filesystem and no active file
 * references. The space associated with the file will be freed
 * as soon as the necessary soft dependencies are cleared.
 */
void
softdep_releasefile(ip)
	struct inode *ip;	/* inode with the zero effective link count */
{
	struct inodedep *inodedep;
	struct fs *fs;
	int extblocks;

	if (ip->i_effnlink > 0)
		panic("softdep_releasefile: file still referenced");
	/*
	 * We may be called several times as the on-disk link count
	 * drops to zero. We only want to account for the space once.
	 */
	if (ip->i_flag & IN_SPACECOUNTED)
		return;
	/*
	 * We have to deactivate a snapshot otherwise copyonwrites may
	 * add blocks and the cleanup may remove blocks after we have
	 * tried to account for them.
	 */
	if ((ip->i_flags & SF_SNAPSHOT) != 0)
		ffs_snapremove(ITOV(ip));
	/*
	 * If we are tracking an nlinkdelta, we have to also remember
	 * whether we accounted for the freed space yet.
	 */
	ACQUIRE_LOCK(&lk);
	if ((inodedep_lookup(UFSTOVFS(ip->i_ump), ip->i_number, 0, &inodedep)))
		inodedep->id_state |= SPACECOUNTED;
	FREE_LOCK(&lk);
	fs = ip->i_fs;
	extblocks = 0;
	if (fs->fs_magic == FS_UFS2_MAGIC)
		extblocks = btodb(fragroundup(fs, ip->i_din2->di_extsize));
	UFS_LOCK(ip->i_ump);
	ip->i_fs->fs_pendingblocks += DIP(ip, i_blocks) - extblocks;
	ip->i_fs->fs_pendinginodes += 1;
	UFS_UNLOCK(ip->i_ump);
	ip->i_flag |= IN_SPACECOUNTED;
}

/*
 * This workitem decrements the inode's link count.
 * If the link count reaches zero, the file is removed.
 */
static void 
handle_workitem_remove(dirrem, xp)
	struct dirrem *dirrem;
	struct vnode *xp;
{
	struct thread *td = curthread;
	struct inodedep *inodedep;
	struct vnode *vp;
	struct inode *ip;
	ino_t oldinum;
	int error;

	if ((vp = xp) == NULL &&
	    (error = ffs_vget(dirrem->dm_list.wk_mp,
	    dirrem->dm_oldinum, LK_EXCLUSIVE, &vp)) != 0) {
		softdep_error("handle_workitem_remove: vget", error);
		return;
	}
	ip = VTOI(vp);
	ACQUIRE_LOCK(&lk);
	if ((inodedep_lookup(dirrem->dm_list.wk_mp,
	    dirrem->dm_oldinum, 0, &inodedep)) == 0)
		panic("handle_workitem_remove: lost inodedep");
	/*
	 * Normal file deletion.
	 */
	if ((dirrem->dm_state & RMDIR) == 0) {
		ip->i_nlink--;
		DIP_SET(ip, i_nlink, ip->i_nlink);
		ip->i_flag |= IN_CHANGE;
		if (ip->i_nlink < ip->i_effnlink)
			panic("handle_workitem_remove: bad file delta");
		inodedep->id_nlinkdelta = ip->i_nlink - ip->i_effnlink;
		num_dirrem -= 1;
		WORKITEM_FREE(dirrem, D_DIRREM);
		FREE_LOCK(&lk);
		vput(vp);
		return;
	}
	/*
	 * Directory deletion. Decrement reference count for both the
	 * just deleted parent directory entry and the reference for ".".
	 * Next truncate the directory to length zero. When the
	 * truncation completes, arrange to have the reference count on
	 * the parent decremented to account for the loss of "..".
	 */
	ip->i_nlink -= 2;
	DIP_SET(ip, i_nlink, ip->i_nlink);
	ip->i_flag |= IN_CHANGE;
	if (ip->i_nlink < ip->i_effnlink)
		panic("handle_workitem_remove: bad dir delta");
	inodedep->id_nlinkdelta = ip->i_nlink - ip->i_effnlink;
	FREE_LOCK(&lk);
	if ((error = ffs_truncate(vp, (off_t)0, 0, td->td_ucred, td)) != 0)
		softdep_error("handle_workitem_remove: truncate", error);
	ACQUIRE_LOCK(&lk);
	/*
	 * Rename a directory to a new parent. Since, we are both deleting
	 * and creating a new directory entry, the link count on the new
	 * directory should not change. Thus we skip the followup dirrem.
	 */
	if (dirrem->dm_state & DIRCHG) {
		num_dirrem -= 1;
		WORKITEM_FREE(dirrem, D_DIRREM);
		FREE_LOCK(&lk);
		vput(vp);
		return;
	}
	/*
	 * If the inodedep does not exist, then the zero'ed inode has
	 * been written to disk. If the allocated inode has never been
	 * written to disk, then the on-disk inode is zero'ed. In either
	 * case we can remove the file immediately.
	 */
	dirrem->dm_state = 0;
	oldinum = dirrem->dm_oldinum;
	dirrem->dm_oldinum = dirrem->dm_dirinum;
	if (inodedep_lookup(dirrem->dm_list.wk_mp, oldinum,
	    0, &inodedep) == 0 || check_inode_unwritten(inodedep)) {
		if (xp != NULL)
			add_to_worklist(&dirrem->dm_list);
		FREE_LOCK(&lk);
		vput(vp);
		if (xp == NULL)
			handle_workitem_remove(dirrem, NULL);
		return;
	}
	WORKLIST_INSERT(&inodedep->id_inowait, &dirrem->dm_list);
	FREE_LOCK(&lk);
	ip->i_flag |= IN_CHANGE;
	ffs_update(vp, 0);
	vput(vp);
}

/*
 * Inode de-allocation dependencies.
 * 
 * When an inode's link count is reduced to zero, it can be de-allocated. We
 * found it convenient to postpone de-allocation until after the inode is
 * written to disk with its new link count (zero).  At this point, all of the
 * on-disk inode's block pointers are nullified and, with careful dependency
 * list ordering, all dependencies related to the inode will be satisfied and
 * the corresponding dependency structures de-allocated.  So, if/when the
 * inode is reused, there will be no mixing of old dependencies with new
 * ones.  This artificial dependency is set up by the block de-allocation
 * procedure above (softdep_setup_freeblocks) and completed by the
 * following procedure.
 */
static void 
handle_workitem_freefile(freefile)
	struct freefile *freefile;
{
	struct fs *fs;
	struct inodedep *idp;
	struct ufsmount *ump;
	int error;

	ump = VFSTOUFS(freefile->fx_list.wk_mp);
	fs = ump->um_fs;
#ifdef DEBUG
	ACQUIRE_LOCK(&lk);
	error = inodedep_lookup(UFSTOVFS(ump), freefile->fx_oldinum, 0, &idp);
	FREE_LOCK(&lk);
	if (error)
		panic("handle_workitem_freefile: inodedep survived");
#endif
	UFS_LOCK(ump);
	fs->fs_pendinginodes -= 1;
	UFS_UNLOCK(ump);
	if ((error = ffs_freefile(ump, fs, freefile->fx_devvp,
	    freefile->fx_oldinum, freefile->fx_mode)) != 0)
		softdep_error("handle_workitem_freefile", error);
	ACQUIRE_LOCK(&lk);
	WORKITEM_FREE(freefile, D_FREEFILE);
	FREE_LOCK(&lk);
}


/*
 * Helper function which unlinks marker element from work list and returns
 * the next element on the list.
 */
static __inline struct worklist *
markernext(struct worklist *marker)
{
	struct worklist *next;
	
	next = LIST_NEXT(marker, wk_list);
	LIST_REMOVE(marker, wk_list);
	return next;
}

/*
 * Disk writes.
 * 
 * The dependency structures constructed above are most actively used when file
 * system blocks are written to disk.  No constraints are placed on when a
 * block can be written, but unsatisfied update dependencies are made safe by
 * modifying (or replacing) the source memory for the duration of the disk
 * write.  When the disk write completes, the memory block is again brought
 * up-to-date.
 *
 * In-core inode structure reclamation.
 * 
 * Because there are a finite number of "in-core" inode structures, they are
 * reused regularly.  By transferring all inode-related dependencies to the
 * in-memory inode block and indexing them separately (via "inodedep"s), we
 * can allow "in-core" inode structures to be reused at any time and avoid
 * any increase in contention.
 *
 * Called just before entering the device driver to initiate a new disk I/O.
 * The buffer must be locked, thus, no I/O completion operations can occur
 * while we are manipulating its associated dependencies.
 */
static void 
softdep_disk_io_initiation(bp)
	struct buf *bp;		/* structure describing disk write to occur */
{
	struct worklist *wk;
	struct worklist marker;
	struct indirdep *indirdep;
	struct inodedep *inodedep;

	/*
	 * We only care about write operations. There should never
	 * be dependencies for reads.
	 */
	if (bp->b_iocmd != BIO_WRITE)
		panic("softdep_disk_io_initiation: not write");

	marker.wk_type = D_LAST + 1;	/* Not a normal workitem */
	PHOLD(curproc);			/* Don't swap out kernel stack */

	ACQUIRE_LOCK(&lk);
	/*
	 * Do any necessary pre-I/O processing.
	 */
	for (wk = LIST_FIRST(&bp->b_dep); wk != NULL;
	     wk = markernext(&marker)) {
		LIST_INSERT_AFTER(wk, &marker, wk_list);
		switch (wk->wk_type) {

		case D_PAGEDEP:
			initiate_write_filepage(WK_PAGEDEP(wk), bp);
			continue;

		case D_INODEDEP:
			inodedep = WK_INODEDEP(wk);
			if (inodedep->id_fs->fs_magic == FS_UFS1_MAGIC)
				initiate_write_inodeblock_ufs1(inodedep, bp);
			else
				initiate_write_inodeblock_ufs2(inodedep, bp);
			continue;

		case D_INDIRDEP:
			indirdep = WK_INDIRDEP(wk);
			if (indirdep->ir_state & GOINGAWAY)
				panic("disk_io_initiation: indirdep gone");
			/*
			 * If there are no remaining dependencies, this
			 * will be writing the real pointers, so the
			 * dependency can be freed.
			 */
			if (LIST_EMPTY(&indirdep->ir_deplisthd)) {
				struct buf *bp;

				bp = indirdep->ir_savebp;
				bp->b_flags |= B_INVAL | B_NOCACHE;
				/* inline expand WORKLIST_REMOVE(wk); */
				wk->wk_state &= ~ONWORKLIST;
				LIST_REMOVE(wk, wk_list);
				WORKITEM_FREE(indirdep, D_INDIRDEP);
				FREE_LOCK(&lk);
				brelse(bp);
				ACQUIRE_LOCK(&lk);
				continue;
			}
			/*
			 * Replace up-to-date version with safe version.
			 */
			FREE_LOCK(&lk);
			MALLOC(indirdep->ir_saveddata, caddr_t, bp->b_bcount,
			    M_INDIRDEP, M_SOFTDEP_FLAGS);
			ACQUIRE_LOCK(&lk);
			indirdep->ir_state &= ~ATTACHED;
			indirdep->ir_state |= UNDONE;
			bcopy(bp->b_data, indirdep->ir_saveddata, bp->b_bcount);
			bcopy(indirdep->ir_savebp->b_data, bp->b_data,
			    bp->b_bcount);
			continue;

		case D_MKDIR:
		case D_BMSAFEMAP:
		case D_ALLOCDIRECT:
		case D_ALLOCINDIR:
			continue;

		default:
			panic("handle_disk_io_initiation: Unexpected type %s",
			    TYPENAME(wk->wk_type));
			/* NOTREACHED */
		}
	}
	FREE_LOCK(&lk);
	PRELE(curproc);			/* Allow swapout of kernel stack */
}

/*
 * Called from within the procedure above to deal with unsatisfied
 * allocation dependencies in a directory. The buffer must be locked,
 * thus, no I/O completion operations can occur while we are
 * manipulating its associated dependencies.
 */
static void
initiate_write_filepage(pagedep, bp)
	struct pagedep *pagedep;
	struct buf *bp;
{
	struct diradd *dap;
	struct direct *ep;
	int i;

	if (pagedep->pd_state & IOSTARTED) {
		/*
		 * This can only happen if there is a driver that does not
		 * understand chaining. Here biodone will reissue the call
		 * to strategy for the incomplete buffers.
		 */
		printf("initiate_write_filepage: already started\n");
		return;
	}
	pagedep->pd_state |= IOSTARTED;
	for (i = 0; i < DAHASHSZ; i++) {
		LIST_FOREACH(dap, &pagedep->pd_diraddhd[i], da_pdlist) {
			ep = (struct direct *)
			    ((char *)bp->b_data + dap->da_offset);
			if (ep->d_ino != dap->da_newinum)
				panic("%s: dir inum %d != new %d",
				    "initiate_write_filepage",
				    ep->d_ino, dap->da_newinum);
			if (dap->da_state & DIRCHG)
				ep->d_ino = dap->da_previous->dm_oldinum;
			else
				ep->d_ino = 0;
			dap->da_state &= ~ATTACHED;
			dap->da_state |= UNDONE;
		}
	}
}

/*
 * Version of initiate_write_inodeblock that handles UFS1 dinodes.
 * Note that any bug fixes made to this routine must be done in the
 * version found below.
 *
 * Called from within the procedure above to deal with unsatisfied
 * allocation dependencies in an inodeblock. The buffer must be
 * locked, thus, no I/O completion operations can occur while we
 * are manipulating its associated dependencies.
 */
static void 
initiate_write_inodeblock_ufs1(inodedep, bp)
	struct inodedep *inodedep;
	struct buf *bp;			/* The inode block */
{
	struct allocdirect *adp, *lastadp;
	struct ufs1_dinode *dp;
	struct ufs1_dinode *sip;
	struct fs *fs;
	ufs_lbn_t i, prevlbn = 0;
	int deplist;

	if (inodedep->id_state & IOSTARTED)
		panic("initiate_write_inodeblock_ufs1: already started");
	inodedep->id_state |= IOSTARTED;
	fs = inodedep->id_fs;
	dp = (struct ufs1_dinode *)bp->b_data +
	    ino_to_fsbo(fs, inodedep->id_ino);
	/*
	 * If the bitmap is not yet written, then the allocated
	 * inode cannot be written to disk.
	 */
	if ((inodedep->id_state & DEPCOMPLETE) == 0) {
		if (inodedep->id_savedino1 != NULL)
			panic("initiate_write_inodeblock_ufs1: I/O underway");
		FREE_LOCK(&lk);
		MALLOC(sip, struct ufs1_dinode *,
		    sizeof(struct ufs1_dinode), M_SAVEDINO, M_SOFTDEP_FLAGS);
		ACQUIRE_LOCK(&lk);
		inodedep->id_savedino1 = sip;
		*inodedep->id_savedino1 = *dp;
		bzero((caddr_t)dp, sizeof(struct ufs1_dinode));
		dp->di_gen = inodedep->id_savedino1->di_gen;
		return;
	}
	/*
	 * If no dependencies, then there is nothing to roll back.
	 */
	inodedep->id_savedsize = dp->di_size;
	inodedep->id_savedextsize = 0;
	if (TAILQ_EMPTY(&inodedep->id_inoupdt))
		return;
	/*
	 * Set the dependencies to busy.
	 */
	for (deplist = 0, adp = TAILQ_FIRST(&inodedep->id_inoupdt); adp;
	     adp = TAILQ_NEXT(adp, ad_next)) {
#ifdef DIAGNOSTIC
		if (deplist != 0 && prevlbn >= adp->ad_lbn)
			panic("softdep_write_inodeblock: lbn order");
		prevlbn = adp->ad_lbn;
		if (adp->ad_lbn < NDADDR &&
		    dp->di_db[adp->ad_lbn] != adp->ad_newblkno)
			panic("%s: direct pointer #%jd mismatch %d != %jd",
			    "softdep_write_inodeblock",
			    (intmax_t)adp->ad_lbn,
			    dp->di_db[adp->ad_lbn],
			    (intmax_t)adp->ad_newblkno);
		if (adp->ad_lbn >= NDADDR &&
		    dp->di_ib[adp->ad_lbn - NDADDR] != adp->ad_newblkno)
			panic("%s: indirect pointer #%jd mismatch %d != %jd",
			    "softdep_write_inodeblock",
			    (intmax_t)adp->ad_lbn - NDADDR,
			    dp->di_ib[adp->ad_lbn - NDADDR],
			    (intmax_t)adp->ad_newblkno);
		deplist |= 1 << adp->ad_lbn;
		if ((adp->ad_state & ATTACHED) == 0)
			panic("softdep_write_inodeblock: Unknown state 0x%x",
			    adp->ad_state);
#endif /* DIAGNOSTIC */
		adp->ad_state &= ~ATTACHED;
		adp->ad_state |= UNDONE;
	}
	/*
	 * The on-disk inode cannot claim to be any larger than the last
	 * fragment that has been written. Otherwise, the on-disk inode
	 * might have fragments that were not the last block in the file
	 * which would corrupt the filesystem.
	 */
	for (lastadp = NULL, adp = TAILQ_FIRST(&inodedep->id_inoupdt); adp;
	     lastadp = adp, adp = TAILQ_NEXT(adp, ad_next)) {
		if (adp->ad_lbn >= NDADDR)
			break;
		dp->di_db[adp->ad_lbn] = adp->ad_oldblkno;
		/* keep going until hitting a rollback to a frag */
		if (adp->ad_oldsize == 0 || adp->ad_oldsize == fs->fs_bsize)
			continue;
		dp->di_size = fs->fs_bsize * adp->ad_lbn + adp->ad_oldsize;
		for (i = adp->ad_lbn + 1; i < NDADDR; i++) {
#ifdef DIAGNOSTIC
			if (dp->di_db[i] != 0 && (deplist & (1 << i)) == 0)
				panic("softdep_write_inodeblock: lost dep1");
#endif /* DIAGNOSTIC */
			dp->di_db[i] = 0;
		}
		for (i = 0; i < NIADDR; i++) {
#ifdef DIAGNOSTIC
			if (dp->di_ib[i] != 0 &&
			    (deplist & ((1 << NDADDR) << i)) == 0)
				panic("softdep_write_inodeblock: lost dep2");
#endif /* DIAGNOSTIC */
			dp->di_ib[i] = 0;
		}
		return;
	}
	/*
	 * If we have zero'ed out the last allocated block of the file,
	 * roll back the size to the last currently allocated block.
	 * We know that this last allocated block is a full-sized as
	 * we already checked for fragments in the loop above.
	 */
	if (lastadp != NULL &&
	    dp->di_size <= (lastadp->ad_lbn + 1) * fs->fs_bsize) {
		for (i = lastadp->ad_lbn; i >= 0; i--)
			if (dp->di_db[i] != 0)
				break;
		dp->di_size = (i + 1) * fs->fs_bsize;
	}
	/*
	 * The only dependencies are for indirect blocks.
	 *
	 * The file size for indirect block additions is not guaranteed.
	 * Such a guarantee would be non-trivial to achieve. The conventional
	 * synchronous write implementation also does not make this guarantee.
	 * Fsck should catch and fix discrepancies. Arguably, the file size
	 * can be over-estimated without destroying integrity when the file
	 * moves into the indirect blocks (i.e., is large). If we want to
	 * postpone fsck, we are stuck with this argument.
	 */
	for (; adp; adp = TAILQ_NEXT(adp, ad_next))
		dp->di_ib[adp->ad_lbn - NDADDR] = 0;
}
		
/*
 * Version of initiate_write_inodeblock that handles UFS2 dinodes.
 * Note that any bug fixes made to this routine must be done in the
 * version found above.
 *
 * Called from within the procedure above to deal with unsatisfied
 * allocation dependencies in an inodeblock. The buffer must be
 * locked, thus, no I/O completion operations can occur while we
 * are manipulating its associated dependencies.
 */
static void 
initiate_write_inodeblock_ufs2(inodedep, bp)
	struct inodedep *inodedep;
	struct buf *bp;			/* The inode block */
{
	struct allocdirect *adp, *lastadp;
	struct ufs2_dinode *dp;
	struct ufs2_dinode *sip;
	struct fs *fs;
	ufs_lbn_t i, prevlbn = 0;
	int deplist;

	if (inodedep->id_state & IOSTARTED)
		panic("initiate_write_inodeblock_ufs2: already started");
	inodedep->id_state |= IOSTARTED;
	fs = inodedep->id_fs;
	dp = (struct ufs2_dinode *)bp->b_data +
	    ino_to_fsbo(fs, inodedep->id_ino);
	/*
	 * If the bitmap is not yet written, then the allocated
	 * inode cannot be written to disk.
	 */
	if ((inodedep->id_state & DEPCOMPLETE) == 0) {
		if (inodedep->id_savedino2 != NULL)
			panic("initiate_write_inodeblock_ufs2: I/O underway");
		FREE_LOCK(&lk);
		MALLOC(sip, struct ufs2_dinode *,
		    sizeof(struct ufs2_dinode), M_SAVEDINO, M_SOFTDEP_FLAGS);
		ACQUIRE_LOCK(&lk);
		inodedep->id_savedino2 = sip;
		*inodedep->id_savedino2 = *dp;
		bzero((caddr_t)dp, sizeof(struct ufs2_dinode));
		dp->di_gen = inodedep->id_savedino2->di_gen;
		return;
	}
	/*
	 * If no dependencies, then there is nothing to roll back.
	 */
	inodedep->id_savedsize = dp->di_size;
	inodedep->id_savedextsize = dp->di_extsize;
	if (TAILQ_EMPTY(&inodedep->id_inoupdt) &&
	    TAILQ_EMPTY(&inodedep->id_extupdt))
		return;
	/*
	 * Set the ext data dependencies to busy.
	 */
	for (deplist = 0, adp = TAILQ_FIRST(&inodedep->id_extupdt); adp;
	     adp = TAILQ_NEXT(adp, ad_next)) {
#ifdef DIAGNOSTIC
		if (deplist != 0 && prevlbn >= adp->ad_lbn)
			panic("softdep_write_inodeblock: lbn order");
		prevlbn = adp->ad_lbn;
		if (dp->di_extb[adp->ad_lbn] != adp->ad_newblkno)
			panic("%s: direct pointer #%jd mismatch %jd != %jd",
			    "softdep_write_inodeblock",
			    (intmax_t)adp->ad_lbn,
			    (intmax_t)dp->di_extb[adp->ad_lbn],
			    (intmax_t)adp->ad_newblkno);
		deplist |= 1 << adp->ad_lbn;
		if ((adp->ad_state & ATTACHED) == 0)
			panic("softdep_write_inodeblock: Unknown state 0x%x",
			    adp->ad_state);
#endif /* DIAGNOSTIC */
		adp->ad_state &= ~ATTACHED;
		adp->ad_state |= UNDONE;
	}
	/*
	 * The on-disk inode cannot claim to be any larger than the last
	 * fragment that has been written. Otherwise, the on-disk inode
	 * might have fragments that were not the last block in the ext
	 * data which would corrupt the filesystem.
	 */
	for (lastadp = NULL, adp = TAILQ_FIRST(&inodedep->id_extupdt); adp;
	     lastadp = adp, adp = TAILQ_NEXT(adp, ad_next)) {
		dp->di_extb[adp->ad_lbn] = adp->ad_oldblkno;
		/* keep going until hitting a rollback to a frag */
		if (adp->ad_oldsize == 0 || adp->ad_oldsize == fs->fs_bsize)
			continue;
		dp->di_extsize = fs->fs_bsize * adp->ad_lbn + adp->ad_oldsize;
		for (i = adp->ad_lbn + 1; i < NXADDR; i++) {
#ifdef DIAGNOSTIC
			if (dp->di_extb[i] != 0 && (deplist & (1 << i)) == 0)
				panic("softdep_write_inodeblock: lost dep1");
#endif /* DIAGNOSTIC */
			dp->di_extb[i] = 0;
		}
		lastadp = NULL;
		break;
	}
	/*
	 * If we have zero'ed out the last allocated block of the ext
	 * data, roll back the size to the last currently allocated block.
	 * We know that this last allocated block is a full-sized as
	 * we already checked for fragments in the loop above.
	 */
	if (lastadp != NULL &&
	    dp->di_extsize <= (lastadp->ad_lbn + 1) * fs->fs_bsize) {
		for (i = lastadp->ad_lbn; i >= 0; i--)
			if (dp->di_extb[i] != 0)
				break;
		dp->di_extsize = (i + 1) * fs->fs_bsize;
	}
	/*
	 * Set the file data dependencies to busy.
	 */
	for (deplist = 0, adp = TAILQ_FIRST(&inodedep->id_inoupdt); adp;
	     adp = TAILQ_NEXT(adp, ad_next)) {
#ifdef DIAGNOSTIC
		if (deplist != 0 && prevlbn >= adp->ad_lbn)
			panic("softdep_write_inodeblock: lbn order");
		prevlbn = adp->ad_lbn;
		if (adp->ad_lbn < NDADDR &&
		    dp->di_db[adp->ad_lbn] != adp->ad_newblkno)
			panic("%s: direct pointer #%jd mismatch %jd != %jd",
			    "softdep_write_inodeblock",
			    (intmax_t)adp->ad_lbn,
			    (intmax_t)dp->di_db[adp->ad_lbn],
			    (intmax_t)adp->ad_newblkno);
		if (adp->ad_lbn >= NDADDR &&
		    dp->di_ib[adp->ad_lbn - NDADDR] != adp->ad_newblkno)
			panic("%s indirect pointer #%jd mismatch %jd != %jd",
			    "softdep_write_inodeblock:",
			    (intmax_t)adp->ad_lbn - NDADDR,
			    (intmax_t)dp->di_ib[adp->ad_lbn - NDADDR],
			    (intmax_t)adp->ad_newblkno);
		deplist |= 1 << adp->ad_lbn;
		if ((adp->ad_state & ATTACHED) == 0)
			panic("softdep_write_inodeblock: Unknown state 0x%x",
			    adp->ad_state);
#endif /* DIAGNOSTIC */
		adp->ad_state &= ~ATTACHED;
		adp->ad_state |= UNDONE;
	}
	/*
	 * The on-disk inode cannot claim to be any larger than the last
	 * fragment that has been written. Otherwise, the on-disk inode
	 * might have fragments that were not the last block in the file
	 * which would corrupt the filesystem.
	 */
	for (lastadp = NULL, adp = TAILQ_FIRST(&inodedep->id_inoupdt); adp;
	     lastadp = adp, adp = TAILQ_NEXT(adp, ad_next)) {
		if (adp->ad_lbn >= NDADDR)
			break;
		dp->di_db[adp->ad_lbn] = adp->ad_oldblkno;
		/* keep going until hitting a rollback to a frag */
		if (adp->ad_oldsize == 0 || adp->ad_oldsize == fs->fs_bsize)
			continue;
		dp->di_size = fs->fs_bsize * adp->ad_lbn + adp->ad_oldsize;
		for (i = adp->ad_lbn + 1; i < NDADDR; i++) {
#ifdef DIAGNOSTIC
			if (dp->di_db[i] != 0 && (deplist & (1 << i)) == 0)
				panic("softdep_write_inodeblock: lost dep2");
#endif /* DIAGNOSTIC */
			dp->di_db[i] = 0;
		}
		for (i = 0; i < NIADDR; i++) {
#ifdef DIAGNOSTIC
			if (dp->di_ib[i] != 0 &&
			    (deplist & ((1 << NDADDR) << i)) == 0)
				panic("softdep_write_inodeblock: lost dep3");
#endif /* DIAGNOSTIC */
			dp->di_ib[i] = 0;
		}
		return;
	}
	/*
	 * If we have zero'ed out the last allocated block of the file,
	 * roll back the size to the last currently allocated block.
	 * We know that this last allocated block is a full-sized as
	 * we already checked for fragments in the loop above.
	 */
	if (lastadp != NULL &&
	    dp->di_size <= (lastadp->ad_lbn + 1) * fs->fs_bsize) {
		for (i = lastadp->ad_lbn; i >= 0; i--)
			if (dp->di_db[i] != 0)
				break;
		dp->di_size = (i + 1) * fs->fs_bsize;
	}
	/*
	 * The only dependencies are for indirect blocks.
	 *
	 * The file size for indirect block additions is not guaranteed.
	 * Such a guarantee would be non-trivial to achieve. The conventional
	 * synchronous write implementation also does not make this guarantee.
	 * Fsck should catch and fix discrepancies. Arguably, the file size
	 * can be over-estimated without destroying integrity when the file
	 * moves into the indirect blocks (i.e., is large). If we want to
	 * postpone fsck, we are stuck with this argument.
	 */
	for (; adp; adp = TAILQ_NEXT(adp, ad_next))
		dp->di_ib[adp->ad_lbn - NDADDR] = 0;
}

/*
 * This routine is called during the completion interrupt
 * service routine for a disk write (from the procedure called
 * by the device driver to inform the filesystem caches of
 * a request completion).  It should be called early in this
 * procedure, before the block is made available to other
 * processes or other routines are called.
 */
static void 
softdep_disk_write_complete(bp)
	struct buf *bp;		/* describes the completed disk write */
{
	struct worklist *wk;
	struct worklist *owk;
	struct workhead reattach;
	struct newblk *newblk;
	struct allocindir *aip;
	struct allocdirect *adp;
	struct indirdep *indirdep;
	struct inodedep *inodedep;
	struct bmsafemap *bmsafemap;

	/*
	 * If an error occurred while doing the write, then the data
	 * has not hit the disk and the dependencies cannot be unrolled.
	 */
	if ((bp->b_ioflags & BIO_ERROR) != 0 && (bp->b_flags & B_INVAL) == 0)
		return;
	LIST_INIT(&reattach);
	/*
	 * This lock must not be released anywhere in this code segment.
	 */
	ACQUIRE_LOCK(&lk);
	owk = NULL;
	while ((wk = LIST_FIRST(&bp->b_dep)) != NULL) {
		WORKLIST_REMOVE(wk);
		if (wk == owk)
			panic("duplicate worklist: %p\n", wk);
		owk = wk;
		switch (wk->wk_type) {

		case D_PAGEDEP:
			if (handle_written_filepage(WK_PAGEDEP(wk), bp))
				WORKLIST_INSERT(&reattach, wk);
			continue;

		case D_INODEDEP:
			if (handle_written_inodeblock(WK_INODEDEP(wk), bp))
				WORKLIST_INSERT(&reattach, wk);
			continue;

		case D_BMSAFEMAP:
			bmsafemap = WK_BMSAFEMAP(wk);
			while ((newblk = LIST_FIRST(&bmsafemap->sm_newblkhd))) {
				newblk->nb_state |= DEPCOMPLETE;
				newblk->nb_bmsafemap = NULL;
				LIST_REMOVE(newblk, nb_deps);
			}
			while ((adp =
			   LIST_FIRST(&bmsafemap->sm_allocdirecthd))) {
				adp->ad_state |= DEPCOMPLETE;
				adp->ad_buf = NULL;
				LIST_REMOVE(adp, ad_deps);
				handle_allocdirect_partdone(adp);
			}
			while ((aip =
			    LIST_FIRST(&bmsafemap->sm_allocindirhd))) {
				aip->ai_state |= DEPCOMPLETE;
				aip->ai_buf = NULL;
				LIST_REMOVE(aip, ai_deps);
				handle_allocindir_partdone(aip);
			}
			while ((inodedep =
			     LIST_FIRST(&bmsafemap->sm_inodedephd)) != NULL) {
				inodedep->id_state |= DEPCOMPLETE;
				LIST_REMOVE(inodedep, id_deps);
				inodedep->id_buf = NULL;
			}
			WORKITEM_FREE(bmsafemap, D_BMSAFEMAP);
			continue;

		case D_MKDIR:
			handle_written_mkdir(WK_MKDIR(wk), MKDIR_BODY);
			continue;

		case D_ALLOCDIRECT:
			adp = WK_ALLOCDIRECT(wk);
			adp->ad_state |= COMPLETE;
			handle_allocdirect_partdone(adp);
			continue;

		case D_ALLOCINDIR:
			aip = WK_ALLOCINDIR(wk);
			aip->ai_state |= COMPLETE;
			handle_allocindir_partdone(aip);
			continue;

		case D_INDIRDEP:
			indirdep = WK_INDIRDEP(wk);
			if (indirdep->ir_state & GOINGAWAY)
				panic("disk_write_complete: indirdep gone");
			bcopy(indirdep->ir_saveddata, bp->b_data, bp->b_bcount);
			FREE(indirdep->ir_saveddata, M_INDIRDEP);
			indirdep->ir_saveddata = 0;
			indirdep->ir_state &= ~UNDONE;
			indirdep->ir_state |= ATTACHED;
			while ((aip = LIST_FIRST(&indirdep->ir_donehd)) != 0) {
				handle_allocindir_partdone(aip);
				if (aip == LIST_FIRST(&indirdep->ir_donehd))
					panic("disk_write_complete: not gone");
			}
			WORKLIST_INSERT(&reattach, wk);
			if ((bp->b_flags & B_DELWRI) == 0)
				stat_indir_blk_ptrs++;
			bdirty(bp);
			continue;

		default:
			panic("handle_disk_write_complete: Unknown type %s",
			    TYPENAME(wk->wk_type));
			/* NOTREACHED */
		}
	}
	/*
	 * Reattach any requests that must be redone.
	 */
	while ((wk = LIST_FIRST(&reattach)) != NULL) {
		WORKLIST_REMOVE(wk);
		WORKLIST_INSERT(&bp->b_dep, wk);
	}
	FREE_LOCK(&lk);
}

/*
 * Called from within softdep_disk_write_complete above. Note that
 * this routine is always called from interrupt level with further
 * splbio interrupts blocked.
 */
static void 
handle_allocdirect_partdone(adp)
	struct allocdirect *adp;	/* the completed allocdirect */
{
	struct allocdirectlst *listhead;
	struct allocdirect *listadp;
	struct inodedep *inodedep;
	long bsize, delay;

	if ((adp->ad_state & ALLCOMPLETE) != ALLCOMPLETE)
		return;
	if (adp->ad_buf != NULL)
		panic("handle_allocdirect_partdone: dangling dep");
	/*
	 * The on-disk inode cannot claim to be any larger than the last
	 * fragment that has been written. Otherwise, the on-disk inode
	 * might have fragments that were not the last block in the file
	 * which would corrupt the filesystem. Thus, we cannot free any
	 * allocdirects after one whose ad_oldblkno claims a fragment as
	 * these blocks must be rolled back to zero before writing the inode.
	 * We check the currently active set of allocdirects in id_inoupdt
	 * or id_extupdt as appropriate.
	 */
	inodedep = adp->ad_inodedep;
	bsize = inodedep->id_fs->fs_bsize;
	if (adp->ad_state & EXTDATA)
		listhead = &inodedep->id_extupdt;
	else
		listhead = &inodedep->id_inoupdt;
	TAILQ_FOREACH(listadp, listhead, ad_next) {
		/* found our block */
		if (listadp == adp)
			break;
		/* continue if ad_oldlbn is not a fragment */
		if (listadp->ad_oldsize == 0 ||
		    listadp->ad_oldsize == bsize)
			continue;
		/* hit a fragment */
		return;
	}
	/*
	 * If we have reached the end of the current list without
	 * finding the just finished dependency, then it must be
	 * on the future dependency list. Future dependencies cannot
	 * be freed until they are moved to the current list.
	 */
	if (listadp == NULL) {
#ifdef DEBUG
		if (adp->ad_state & EXTDATA)
			listhead = &inodedep->id_newextupdt;
		else
			listhead = &inodedep->id_newinoupdt;
		TAILQ_FOREACH(listadp, listhead, ad_next)
			/* found our block */
			if (listadp == adp)
				break;
		if (listadp == NULL)
			panic("handle_allocdirect_partdone: lost dep");
#endif /* DEBUG */
		return;
	}
	/*
	 * If we have found the just finished dependency, then free
	 * it along with anything that follows it that is complete.
	 * If the inode still has a bitmap dependency, then it has
	 * never been written to disk, hence the on-disk inode cannot
	 * reference the old fragment so we can free it without delay.
	 */
	delay = (inodedep->id_state & DEPCOMPLETE);
	for (; adp; adp = listadp) {
		listadp = TAILQ_NEXT(adp, ad_next);
		if ((adp->ad_state & ALLCOMPLETE) != ALLCOMPLETE)
			return;
		free_allocdirect(listhead, adp, delay);
	}
}

/*
 * Called from within softdep_disk_write_complete above. Note that
 * this routine is always called from interrupt level with further
 * splbio interrupts blocked.
 */
static void
handle_allocindir_partdone(aip)
	struct allocindir *aip;		/* the completed allocindir */
{
	struct indirdep *indirdep;

	if ((aip->ai_state & ALLCOMPLETE) != ALLCOMPLETE)
		return;
	if (aip->ai_buf != NULL)
		panic("handle_allocindir_partdone: dangling dependency");
	indirdep = aip->ai_indirdep;
	if (indirdep->ir_state & UNDONE) {
		LIST_REMOVE(aip, ai_next);
		LIST_INSERT_HEAD(&indirdep->ir_donehd, aip, ai_next);
		return;
	}
	if (indirdep->ir_state & UFS1FMT)
		((ufs1_daddr_t *)indirdep->ir_savebp->b_data)[aip->ai_offset] =
		    aip->ai_newblkno;
	else
		((ufs2_daddr_t *)indirdep->ir_savebp->b_data)[aip->ai_offset] =
		    aip->ai_newblkno;
	LIST_REMOVE(aip, ai_next);
	if (aip->ai_freefrag != NULL)
		add_to_worklist(&aip->ai_freefrag->ff_list);
	WORKITEM_FREE(aip, D_ALLOCINDIR);
}

/*
 * Called from within softdep_disk_write_complete above to restore
 * in-memory inode block contents to their most up-to-date state. Note
 * that this routine is always called from interrupt level with further
 * splbio interrupts blocked.
 */
static int 
handle_written_inodeblock(inodedep, bp)
	struct inodedep *inodedep;
	struct buf *bp;		/* buffer containing the inode block */
{
	struct worklist *wk, *filefree;
	struct allocdirect *adp, *nextadp;
	struct ufs1_dinode *dp1 = NULL;
	struct ufs2_dinode *dp2 = NULL;
	int hadchanges, fstype;

	if ((inodedep->id_state & IOSTARTED) == 0)
		panic("handle_written_inodeblock: not started");
	inodedep->id_state &= ~IOSTARTED;
	if (inodedep->id_fs->fs_magic == FS_UFS1_MAGIC) {
		fstype = UFS1;
		dp1 = (struct ufs1_dinode *)bp->b_data +
		    ino_to_fsbo(inodedep->id_fs, inodedep->id_ino);
	} else {
		fstype = UFS2;
		dp2 = (struct ufs2_dinode *)bp->b_data +
		    ino_to_fsbo(inodedep->id_fs, inodedep->id_ino);
	}
	/*
	 * If we had to rollback the inode allocation because of
	 * bitmaps being incomplete, then simply restore it.
	 * Keep the block dirty so that it will not be reclaimed until
	 * all associated dependencies have been cleared and the
	 * corresponding updates written to disk.
	 */
	if (inodedep->id_savedino1 != NULL) {
		if (fstype == UFS1)
			*dp1 = *inodedep->id_savedino1;
		else
			*dp2 = *inodedep->id_savedino2;
		FREE(inodedep->id_savedino1, M_SAVEDINO);
		inodedep->id_savedino1 = NULL;
		if ((bp->b_flags & B_DELWRI) == 0)
			stat_inode_bitmap++;
		bdirty(bp);
		return (1);
	}
	inodedep->id_state |= COMPLETE;
	/*
	 * Roll forward anything that had to be rolled back before 
	 * the inode could be updated.
	 */
	hadchanges = 0;
	for (adp = TAILQ_FIRST(&inodedep->id_inoupdt); adp; adp = nextadp) {
		nextadp = TAILQ_NEXT(adp, ad_next);
		if (adp->ad_state & ATTACHED)
			panic("handle_written_inodeblock: new entry");
		if (fstype == UFS1) {
			if (adp->ad_lbn < NDADDR) {
				if (dp1->di_db[adp->ad_lbn]!=adp->ad_oldblkno)
					panic("%s %s #%jd mismatch %d != %jd",
					    "handle_written_inodeblock:",
					    "direct pointer",
					    (intmax_t)adp->ad_lbn,
					    dp1->di_db[adp->ad_lbn],
					    (intmax_t)adp->ad_oldblkno);
				dp1->di_db[adp->ad_lbn] = adp->ad_newblkno;
			} else {
				if (dp1->di_ib[adp->ad_lbn - NDADDR] != 0)
					panic("%s: %s #%jd allocated as %d",
					    "handle_written_inodeblock",
					    "indirect pointer",
					    (intmax_t)adp->ad_lbn - NDADDR,
					    dp1->di_ib[adp->ad_lbn - NDADDR]);
				dp1->di_ib[adp->ad_lbn - NDADDR] =
				    adp->ad_newblkno;
			}
		} else {
			if (adp->ad_lbn < NDADDR) {
				if (dp2->di_db[adp->ad_lbn]!=adp->ad_oldblkno)
					panic("%s: %s #%jd %s %jd != %jd",
					    "handle_written_inodeblock",
					    "direct pointer",
					    (intmax_t)adp->ad_lbn, "mismatch",
					    (intmax_t)dp2->di_db[adp->ad_lbn],
					    (intmax_t)adp->ad_oldblkno);
				dp2->di_db[adp->ad_lbn] = adp->ad_newblkno;
			} else {
				if (dp2->di_ib[adp->ad_lbn - NDADDR] != 0)
					panic("%s: %s #%jd allocated as %jd",
					    "handle_written_inodeblock",
					    "indirect pointer",
					    (intmax_t)adp->ad_lbn - NDADDR,
					    (intmax_t)
					    dp2->di_ib[adp->ad_lbn - NDADDR]);
				dp2->di_ib[adp->ad_lbn - NDADDR] =
				    adp->ad_newblkno;
			}
		}
		adp->ad_state &= ~UNDONE;
		adp->ad_state |= ATTACHED;
		hadchanges = 1;
	}
	for (adp = TAILQ_FIRST(&inodedep->id_extupdt); adp; adp = nextadp) {
		nextadp = TAILQ_NEXT(adp, ad_next);
		if (adp->ad_state & ATTACHED)
			panic("handle_written_inodeblock: new entry");
		if (dp2->di_extb[adp->ad_lbn] != adp->ad_oldblkno)
			panic("%s: direct pointers #%jd %s %jd != %jd",
			    "handle_written_inodeblock",
			    (intmax_t)adp->ad_lbn, "mismatch",
			    (intmax_t)dp2->di_extb[adp->ad_lbn],
			    (intmax_t)adp->ad_oldblkno);
		dp2->di_extb[adp->ad_lbn] = adp->ad_newblkno;
		adp->ad_state &= ~UNDONE;
		adp->ad_state |= ATTACHED;
		hadchanges = 1;
	}
	if (hadchanges && (bp->b_flags & B_DELWRI) == 0)
		stat_direct_blk_ptrs++;
	/*
	 * Reset the file size to its most up-to-date value.
	 */
	if (inodedep->id_savedsize == -1 || inodedep->id_savedextsize == -1)
		panic("handle_written_inodeblock: bad size");
	if (fstype == UFS1) {
		if (dp1->di_size != inodedep->id_savedsize) {
			dp1->di_size = inodedep->id_savedsize;
			hadchanges = 1;
		}
	} else {
		if (dp2->di_size != inodedep->id_savedsize) {
			dp2->di_size = inodedep->id_savedsize;
			hadchanges = 1;
		}
		if (dp2->di_extsize != inodedep->id_savedextsize) {
			dp2->di_extsize = inodedep->id_savedextsize;
			hadchanges = 1;
		}
	}
	inodedep->id_savedsize = -1;
	inodedep->id_savedextsize = -1;
	/*
	 * If there were any rollbacks in the inode block, then it must be
	 * marked dirty so that its will eventually get written back in
	 * its correct form.
	 */
	if (hadchanges)
		bdirty(bp);
	/*
	 * Process any allocdirects that completed during the update.
	 */
	if ((adp = TAILQ_FIRST(&inodedep->id_inoupdt)) != NULL)
		handle_allocdirect_partdone(adp);
	if ((adp = TAILQ_FIRST(&inodedep->id_extupdt)) != NULL)
		handle_allocdirect_partdone(adp);
	/*
	 * Process deallocations that were held pending until the
	 * inode had been written to disk. Freeing of the inode
	 * is delayed until after all blocks have been freed to
	 * avoid creation of new <vfsid, inum, lbn> triples
	 * before the old ones have been deleted.
	 */
	filefree = NULL;
	while ((wk = LIST_FIRST(&inodedep->id_bufwait)) != NULL) {
		WORKLIST_REMOVE(wk);
		switch (wk->wk_type) {

		case D_FREEFILE:
			/*
			 * We defer adding filefree to the worklist until
			 * all other additions have been made to ensure
			 * that it will be done after all the old blocks
			 * have been freed.
			 */
			if (filefree != NULL)
				panic("handle_written_inodeblock: filefree");
			filefree = wk;
			continue;

		case D_MKDIR:
			handle_written_mkdir(WK_MKDIR(wk), MKDIR_PARENT);
			continue;

		case D_DIRADD:
			diradd_inode_written(WK_DIRADD(wk), inodedep);
			continue;

		case D_FREEBLKS:
			wk->wk_state |= COMPLETE;
			if ((wk->wk_state  & ALLCOMPLETE) != ALLCOMPLETE)
				continue;
			 /* -- fall through -- */
		case D_FREEFRAG:
		case D_DIRREM:
			add_to_worklist(wk);
			continue;

		case D_NEWDIRBLK:
			free_newdirblk(WK_NEWDIRBLK(wk));
			continue;

		default:
			panic("handle_written_inodeblock: Unknown type %s",
			    TYPENAME(wk->wk_type));
			/* NOTREACHED */
		}
	}
	if (filefree != NULL) {
		if (free_inodedep(inodedep) == 0)
			panic("handle_written_inodeblock: live inodedep");
		add_to_worklist(filefree);
		return (0);
	}

	/*
	 * If no outstanding dependencies, free it.
	 */
	if (free_inodedep(inodedep) ||
	    (TAILQ_FIRST(&inodedep->id_inoupdt) == 0 &&
	     TAILQ_FIRST(&inodedep->id_extupdt) == 0))
		return (0);
	return (hadchanges);
}

/*
 * Process a diradd entry after its dependent inode has been written.
 * This routine must be called with splbio interrupts blocked.
 */
static void
diradd_inode_written(dap, inodedep)
	struct diradd *dap;
	struct inodedep *inodedep;
{
	struct pagedep *pagedep;

	dap->da_state |= COMPLETE;
	if ((dap->da_state & ALLCOMPLETE) == ALLCOMPLETE) {
		if (dap->da_state & DIRCHG)
			pagedep = dap->da_previous->dm_pagedep;
		else
			pagedep = dap->da_pagedep;
		LIST_REMOVE(dap, da_pdlist);
		LIST_INSERT_HEAD(&pagedep->pd_pendinghd, dap, da_pdlist);
	}
	WORKLIST_INSERT(&inodedep->id_pendinghd, &dap->da_list);
}

/*
 * Handle the completion of a mkdir dependency.
 */
static void
handle_written_mkdir(mkdir, type)
	struct mkdir *mkdir;
	int type;
{
	struct diradd *dap;
	struct pagedep *pagedep;

	if (mkdir->md_state != type)
		panic("handle_written_mkdir: bad type");
	dap = mkdir->md_diradd;
	dap->da_state &= ~type;
	if ((dap->da_state & (MKDIR_PARENT | MKDIR_BODY)) == 0)
		dap->da_state |= DEPCOMPLETE;
	if ((dap->da_state & ALLCOMPLETE) == ALLCOMPLETE) {
		if (dap->da_state & DIRCHG)
			pagedep = dap->da_previous->dm_pagedep;
		else
			pagedep = dap->da_pagedep;
		LIST_REMOVE(dap, da_pdlist);
		LIST_INSERT_HEAD(&pagedep->pd_pendinghd, dap, da_pdlist);
	}
	LIST_REMOVE(mkdir, md_mkdirs);
	WORKITEM_FREE(mkdir, D_MKDIR);
}

/*
 * Called from within softdep_disk_write_complete above.
 * A write operation was just completed. Removed inodes can
 * now be freed and associated block pointers may be committed.
 * Note that this routine is always called from interrupt level
 * with further splbio interrupts blocked.
 */
static int 
handle_written_filepage(pagedep, bp)
	struct pagedep *pagedep;
	struct buf *bp;		/* buffer containing the written page */
{
	struct dirrem *dirrem;
	struct diradd *dap, *nextdap;
	struct direct *ep;
	int i, chgs;

	if ((pagedep->pd_state & IOSTARTED) == 0)
		panic("handle_written_filepage: not started");
	pagedep->pd_state &= ~IOSTARTED;
	/*
	 * Process any directory removals that have been committed.
	 */
	while ((dirrem = LIST_FIRST(&pagedep->pd_dirremhd)) != NULL) {
		LIST_REMOVE(dirrem, dm_next);
		dirrem->dm_dirinum = pagedep->pd_ino;
		add_to_worklist(&dirrem->dm_list);
	}
	/*
	 * Free any directory additions that have been committed.
	 * If it is a newly allocated block, we have to wait until
	 * the on-disk directory inode claims the new block.
	 */
	if ((pagedep->pd_state & NEWBLOCK) == 0)
		while ((dap = LIST_FIRST(&pagedep->pd_pendinghd)) != NULL)
			free_diradd(dap);
	/*
	 * Uncommitted directory entries must be restored.
	 */
	for (chgs = 0, i = 0; i < DAHASHSZ; i++) {
		for (dap = LIST_FIRST(&pagedep->pd_diraddhd[i]); dap;
		     dap = nextdap) {
			nextdap = LIST_NEXT(dap, da_pdlist);
			if (dap->da_state & ATTACHED)
				panic("handle_written_filepage: attached");
			ep = (struct direct *)
			    ((char *)bp->b_data + dap->da_offset);
			ep->d_ino = dap->da_newinum;
			dap->da_state &= ~UNDONE;
			dap->da_state |= ATTACHED;
			chgs = 1;
			/*
			 * If the inode referenced by the directory has
			 * been written out, then the dependency can be
			 * moved to the pending list.
			 */
			if ((dap->da_state & ALLCOMPLETE) == ALLCOMPLETE) {
				LIST_REMOVE(dap, da_pdlist);
				LIST_INSERT_HEAD(&pagedep->pd_pendinghd, dap,
				    da_pdlist);
			}
		}
	}
	/*
	 * If there were any rollbacks in the directory, then it must be
	 * marked dirty so that its will eventually get written back in
	 * its correct form.
	 */
	if (chgs) {
		if ((bp->b_flags & B_DELWRI) == 0)
			stat_dir_entry++;
		bdirty(bp);
		return (1);
	}
	/*
	 * If we are not waiting for a new directory block to be
	 * claimed by its inode, then the pagedep will be freed.
	 * Otherwise it will remain to track any new entries on
	 * the page in case they are fsync'ed.
	 */
	if ((pagedep->pd_state & NEWBLOCK) == 0) {
		LIST_REMOVE(pagedep, pd_hash);
		WORKITEM_FREE(pagedep, D_PAGEDEP);
	}
	return (0);
}

/*
 * Writing back in-core inode structures.
 * 
 * The filesystem only accesses an inode's contents when it occupies an
 * "in-core" inode structure.  These "in-core" structures are separate from
 * the page frames used to cache inode blocks.  Only the latter are
 * transferred to/from the disk.  So, when the updated contents of the
 * "in-core" inode structure are copied to the corresponding in-memory inode
 * block, the dependencies are also transferred.  The following procedure is
 * called when copying a dirty "in-core" inode to a cached inode block.
 */

/*
 * Called when an inode is loaded from disk. If the effective link count
 * differed from the actual link count when it was last flushed, then we
 * need to ensure that the correct effective link count is put back.
 */
void 
softdep_load_inodeblock(ip)
	struct inode *ip;	/* the "in_core" copy of the inode */
{
	struct inodedep *inodedep;

	/*
	 * Check for alternate nlink count.
	 */
	ip->i_effnlink = ip->i_nlink;
	ACQUIRE_LOCK(&lk);
	if (inodedep_lookup(UFSTOVFS(ip->i_ump),
	    ip->i_number, 0, &inodedep) == 0) {
		FREE_LOCK(&lk);
		return;
	}
	ip->i_effnlink -= inodedep->id_nlinkdelta;
	if (inodedep->id_state & SPACECOUNTED)
		ip->i_flag |= IN_SPACECOUNTED;
	FREE_LOCK(&lk);
}

/*
 * This routine is called just before the "in-core" inode
 * information is to be copied to the in-memory inode block.
 * Recall that an inode block contains several inodes. If
 * the force flag is set, then the dependencies will be
 * cleared so that the update can always be made. Note that
 * the buffer is locked when this routine is called, so we
 * will never be in the middle of writing the inode block 
 * to disk.
 */
void 
softdep_update_inodeblock(ip, bp, waitfor)
	struct inode *ip;	/* the "in_core" copy of the inode */
	struct buf *bp;		/* the buffer containing the inode block */
	int waitfor;		/* nonzero => update must be allowed */
{
	struct inodedep *inodedep;
	struct worklist *wk;
	struct mount *mp;
	struct buf *ibp;
	int error;

	/*
	 * If the effective link count is not equal to the actual link
	 * count, then we must track the difference in an inodedep while
	 * the inode is (potentially) tossed out of the cache. Otherwise,
	 * if there is no existing inodedep, then there are no dependencies
	 * to track.
	 */
	mp = UFSTOVFS(ip->i_ump);
	ACQUIRE_LOCK(&lk);
	if (inodedep_lookup(mp, ip->i_number, 0, &inodedep) == 0) {
		FREE_LOCK(&lk);
		if (ip->i_effnlink != ip->i_nlink)
			panic("softdep_update_inodeblock: bad link count");
		return;
	}
	if (inodedep->id_nlinkdelta != ip->i_nlink - ip->i_effnlink)
		panic("softdep_update_inodeblock: bad delta");
	/*
	 * Changes have been initiated. Anything depending on these
	 * changes cannot occur until this inode has been written.
	 */
	inodedep->id_state &= ~COMPLETE;
	if ((inodedep->id_state & ONWORKLIST) == 0)
		WORKLIST_INSERT(&bp->b_dep, &inodedep->id_list);
	/*
	 * Any new dependencies associated with the incore inode must 
	 * now be moved to the list associated with the buffer holding
	 * the in-memory copy of the inode. Once merged process any
	 * allocdirects that are completed by the merger.
	 */
	merge_inode_lists(&inodedep->id_newinoupdt, &inodedep->id_inoupdt);
	if (!TAILQ_EMPTY(&inodedep->id_inoupdt))
		handle_allocdirect_partdone(TAILQ_FIRST(&inodedep->id_inoupdt));
	merge_inode_lists(&inodedep->id_newextupdt, &inodedep->id_extupdt);
	if (!TAILQ_EMPTY(&inodedep->id_extupdt))
		handle_allocdirect_partdone(TAILQ_FIRST(&inodedep->id_extupdt));
	/*
	 * Now that the inode has been pushed into the buffer, the
	 * operations dependent on the inode being written to disk
	 * can be moved to the id_bufwait so that they will be
	 * processed when the buffer I/O completes.
	 */
	while ((wk = LIST_FIRST(&inodedep->id_inowait)) != NULL) {
		WORKLIST_REMOVE(wk);
		WORKLIST_INSERT(&inodedep->id_bufwait, wk);
	}
	/*
	 * Newly allocated inodes cannot be written until the bitmap
	 * that allocates them have been written (indicated by
	 * DEPCOMPLETE being set in id_state). If we are doing a
	 * forced sync (e.g., an fsync on a file), we force the bitmap
	 * to be written so that the update can be done.
	 */
	if (waitfor == 0) {
		FREE_LOCK(&lk);
		return;
	}
retry:
	if ((inodedep->id_state & DEPCOMPLETE) != 0) {
		FREE_LOCK(&lk);
		return;
	}
	ibp = inodedep->id_buf;
	ibp = getdirtybuf(ibp, &lk, MNT_WAIT);
	if (ibp == NULL) {
		/*
		 * If ibp came back as NULL, the dependency could have been
		 * freed while we slept.  Look it up again, and check to see
		 * that it has completed.
		 */
		if (inodedep_lookup(mp, ip->i_number, 0, &inodedep) != 0)
			goto retry;
		FREE_LOCK(&lk);
		return;
	}
	FREE_LOCK(&lk);
	if ((error = bwrite(ibp)) != 0)
		softdep_error("softdep_update_inodeblock: bwrite", error);
}

/*
 * Merge the a new inode dependency list (such as id_newinoupdt) into an
 * old inode dependency list (such as id_inoupdt). This routine must be
 * called with splbio interrupts blocked.
 */
static void
merge_inode_lists(newlisthead, oldlisthead)
	struct allocdirectlst *newlisthead;
	struct allocdirectlst *oldlisthead;
{
	struct allocdirect *listadp, *newadp;

	newadp = TAILQ_FIRST(newlisthead);
	for (listadp = TAILQ_FIRST(oldlisthead); listadp && newadp;) {
		if (listadp->ad_lbn < newadp->ad_lbn) {
			listadp = TAILQ_NEXT(listadp, ad_next);
			continue;
		}
		TAILQ_REMOVE(newlisthead, newadp, ad_next);
		TAILQ_INSERT_BEFORE(listadp, newadp, ad_next);
		if (listadp->ad_lbn == newadp->ad_lbn) {
			allocdirect_merge(oldlisthead, newadp,
			    listadp);
			listadp = newadp;
		}
		newadp = TAILQ_FIRST(newlisthead);
	}
	while ((newadp = TAILQ_FIRST(newlisthead)) != NULL) {
		TAILQ_REMOVE(newlisthead, newadp, ad_next);
		TAILQ_INSERT_TAIL(oldlisthead, newadp, ad_next);
	}
}

/*
 * If we are doing an fsync, then we must ensure that any directory
 * entries for the inode have been written after the inode gets to disk.
 */
int
softdep_fsync(vp)
	struct vnode *vp;	/* the "in_core" copy of the inode */
{
	struct inodedep *inodedep;
	struct pagedep *pagedep;
	struct worklist *wk;
	struct diradd *dap;
	struct mount *mp;
	struct vnode *pvp;
	struct inode *ip;
	struct buf *bp;
	struct fs *fs;
	struct thread *td = curthread;
	int error, flushparent, pagedep_new_block;
	ino_t parentino;
	ufs_lbn_t lbn;

	ip = VTOI(vp);
	fs = ip->i_fs;
	mp = vp->v_mount;
	ACQUIRE_LOCK(&lk);
	if (inodedep_lookup(mp, ip->i_number, 0, &inodedep) == 0) {
		FREE_LOCK(&lk);
		return (0);
	}
	if (!LIST_EMPTY(&inodedep->id_inowait) ||
	    !LIST_EMPTY(&inodedep->id_bufwait) ||
	    !TAILQ_EMPTY(&inodedep->id_extupdt) ||
	    !TAILQ_EMPTY(&inodedep->id_newextupdt) ||
	    !TAILQ_EMPTY(&inodedep->id_inoupdt) ||
	    !TAILQ_EMPTY(&inodedep->id_newinoupdt))
		panic("softdep_fsync: pending ops");
	for (error = 0, flushparent = 0; ; ) {
		if ((wk = LIST_FIRST(&inodedep->id_pendinghd)) == NULL)
			break;
		if (wk->wk_type != D_DIRADD)
			panic("softdep_fsync: Unexpected type %s",
			    TYPENAME(wk->wk_type));
		dap = WK_DIRADD(wk);
		/*
		 * Flush our parent if this directory entry has a MKDIR_PARENT
		 * dependency or is contained in a newly allocated block.
		 */
		if (dap->da_state & DIRCHG)
			pagedep = dap->da_previous->dm_pagedep;
		else
			pagedep = dap->da_pagedep;
		parentino = pagedep->pd_ino;
		lbn = pagedep->pd_lbn;
		if ((dap->da_state & (MKDIR_BODY | COMPLETE)) != COMPLETE)
			panic("softdep_fsync: dirty");
		if ((dap->da_state & MKDIR_PARENT) ||
		    (pagedep->pd_state & NEWBLOCK))
			flushparent = 1;
		else
			flushparent = 0;
		/*
		 * If we are being fsync'ed as part of vgone'ing this vnode,
		 * then we will not be able to release and recover the
		 * vnode below, so we just have to give up on writing its
		 * directory entry out. It will eventually be written, just
		 * not now, but then the user was not asking to have it
		 * written, so we are not breaking any promises.
		 */
		if (vp->v_iflag & VI_DOOMED)
			break;
		/*
		 * We prevent deadlock by always fetching inodes from the
		 * root, moving down the directory tree. Thus, when fetching
		 * our parent directory, we first try to get the lock. If
		 * that fails, we must unlock ourselves before requesting
		 * the lock on our parent. See the comment in ufs_lookup
		 * for details on possible races.
		 */
		FREE_LOCK(&lk);
		if (ffs_vget(mp, parentino, LK_NOWAIT | LK_EXCLUSIVE, &pvp)) {
			VOP_UNLOCK(vp, 0, td);
			error = ffs_vget(mp, parentino, LK_EXCLUSIVE, &pvp);
			vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
			if (error != 0)
				return (error);
		}
		/*
		 * All MKDIR_PARENT dependencies and all the NEWBLOCK pagedeps
		 * that are contained in direct blocks will be resolved by 
		 * doing a ffs_update. Pagedeps contained in indirect blocks
		 * may require a complete sync'ing of the directory. So, we
		 * try the cheap and fast ffs_update first, and if that fails,
		 * then we do the slower ffs_syncvnode of the directory.
		 */
		if (flushparent) {
			int locked;

			if ((error = ffs_update(pvp, 1)) != 0) {
				vput(pvp);
				return (error);
			}
			ACQUIRE_LOCK(&lk);
			locked = 1;
			if (inodedep_lookup(mp, ip->i_number, 0, &inodedep) != 0) {
				if ((wk = LIST_FIRST(&inodedep->id_pendinghd)) != NULL) {
					if (wk->wk_type != D_DIRADD)
						panic("softdep_fsync: Unexpected type %s",
						      TYPENAME(wk->wk_type));
					dap = WK_DIRADD(wk);
					if (dap->da_state & DIRCHG)
						pagedep = dap->da_previous->dm_pagedep;
					else
						pagedep = dap->da_pagedep;
					pagedep_new_block = pagedep->pd_state & NEWBLOCK;
					FREE_LOCK(&lk);
					locked = 0;
					if (pagedep_new_block &&
					    (error = ffs_syncvnode(pvp, MNT_WAIT))) {
						vput(pvp);
						return (error);
					}
				}
			}
			if (locked)
				FREE_LOCK(&lk);
		}
		/*
		 * Flush directory page containing the inode's name.
		 */
		error = bread(pvp, lbn, blksize(fs, VTOI(pvp), lbn), td->td_ucred,
		    &bp);
		if (error == 0)
			error = bwrite(bp);
		else
			brelse(bp);
		vput(pvp);
		if (error != 0)
			return (error);
		ACQUIRE_LOCK(&lk);
		if (inodedep_lookup(mp, ip->i_number, 0, &inodedep) == 0)
			break;
	}
	FREE_LOCK(&lk);
	return (0);
}

/*
 * Flush all the dirty bitmaps associated with the block device
 * before flushing the rest of the dirty blocks so as to reduce
 * the number of dependencies that will have to be rolled back.
 */
void
softdep_fsync_mountdev(vp)
	struct vnode *vp;
{
	struct buf *bp, *nbp;
	struct worklist *wk;

	if (!vn_isdisk(vp, NULL))
		panic("softdep_fsync_mountdev: vnode not a disk");
restart:
	ACQUIRE_LOCK(&lk);
	VI_LOCK(vp);
	TAILQ_FOREACH_SAFE(bp, &vp->v_bufobj.bo_dirty.bv_hd, b_bobufs, nbp) {
		/* 
		 * If it is already scheduled, skip to the next buffer.
		 */
		if (BUF_LOCK(bp, LK_EXCLUSIVE | LK_NOWAIT, NULL))
			continue;

		if ((bp->b_flags & B_DELWRI) == 0)
			panic("softdep_fsync_mountdev: not dirty");
		/*
		 * We are only interested in bitmaps with outstanding
		 * dependencies.
		 */
		if ((wk = LIST_FIRST(&bp->b_dep)) == NULL ||
		    wk->wk_type != D_BMSAFEMAP ||
		    (bp->b_vflags & BV_BKGRDINPROG)) {
			BUF_UNLOCK(bp);
			continue;
		}
		VI_UNLOCK(vp);
		FREE_LOCK(&lk);
		bremfree(bp);
		(void) bawrite(bp);
		goto restart;
	}
	FREE_LOCK(&lk);
	drain_output(vp);
	VI_UNLOCK(vp);
}

/*
 * This routine is called when we are trying to synchronously flush a
 * file. This routine must eliminate any filesystem metadata dependencies
 * so that the syncing routine can succeed by pushing the dirty blocks
 * associated with the file. If any I/O errors occur, they are returned.
 */
int
softdep_sync_metadata(struct vnode *vp)
{
	struct pagedep *pagedep;
	struct allocdirect *adp;
	struct allocindir *aip;
	struct buf *bp, *nbp;
	struct worklist *wk;
	int i, error, waitfor;

	if (!DOINGSOFTDEP(vp))
		return (0);
	/*
	 * Ensure that any direct block dependencies have been cleared.
	 */
	ACQUIRE_LOCK(&lk);
	if ((error = flush_inodedep_deps(vp->v_mount, VTOI(vp)->i_number))) {
		FREE_LOCK(&lk);
		return (error);
	}
	FREE_LOCK(&lk);
	/*
	 * For most files, the only metadata dependencies are the
	 * cylinder group maps that allocate their inode or blocks.
	 * The block allocation dependencies can be found by traversing
	 * the dependency lists for any buffers that remain on their
	 * dirty buffer list. The inode allocation dependency will
	 * be resolved when the inode is updated with MNT_WAIT.
	 * This work is done in two passes. The first pass grabs most
	 * of the buffers and begins asynchronously writing them. The
	 * only way to wait for these asynchronous writes is to sleep
	 * on the filesystem vnode which may stay busy for a long time
	 * if the filesystem is active. So, instead, we make a second
	 * pass over the dependencies blocking on each write. In the
	 * usual case we will be blocking against a write that we
	 * initiated, so when it is done the dependency will have been
	 * resolved. Thus the second pass is expected to end quickly.
	 */
	waitfor = MNT_NOWAIT;

top:
	/*
	 * We must wait for any I/O in progress to finish so that
	 * all potential buffers on the dirty list will be visible.
	 */
	VI_LOCK(vp);
	drain_output(vp);
	while ((bp = TAILQ_FIRST(&vp->v_bufobj.bo_dirty.bv_hd)) != NULL) {
		bp = getdirtybuf(bp, VI_MTX(vp), MNT_WAIT);
		if (bp)
			break;
	}
	VI_UNLOCK(vp);
	if (bp == NULL)
		return (0);
loop:
	/* While syncing snapshots, we must allow recursive lookups */
	bp->b_lock.lk_flags |= LK_CANRECURSE;
	ACQUIRE_LOCK(&lk);
	/*
	 * As we hold the buffer locked, none of its dependencies
	 * will disappear.
	 */
	LIST_FOREACH(wk, &bp->b_dep, wk_list) {
		switch (wk->wk_type) {

		case D_ALLOCDIRECT:
			adp = WK_ALLOCDIRECT(wk);
			if (adp->ad_state & DEPCOMPLETE)
				continue;
			nbp = adp->ad_buf;
			nbp = getdirtybuf(nbp, &lk, waitfor);
			if (nbp == NULL)
				continue;
			FREE_LOCK(&lk);
			if (waitfor == MNT_NOWAIT) {
				bawrite(nbp);
			} else if ((error = bwrite(nbp)) != 0) {
				break;
			}
			ACQUIRE_LOCK(&lk);
			continue;

		case D_ALLOCINDIR:
			aip = WK_ALLOCINDIR(wk);
			if (aip->ai_state & DEPCOMPLETE)
				continue;
			nbp = aip->ai_buf;
			nbp = getdirtybuf(nbp, &lk, waitfor);
			if (nbp == NULL)
				continue;
			FREE_LOCK(&lk);
			if (waitfor == MNT_NOWAIT) {
				bawrite(nbp);
			} else if ((error = bwrite(nbp)) != 0) {
				break;
			}
			ACQUIRE_LOCK(&lk);
			continue;

		case D_INDIRDEP:
		restart:

			LIST_FOREACH(aip, &WK_INDIRDEP(wk)->ir_deplisthd, ai_next) {
				if (aip->ai_state & DEPCOMPLETE)
					continue;
				nbp = aip->ai_buf;
				nbp = getdirtybuf(nbp, &lk, MNT_WAIT);
				if (nbp == NULL)
					goto restart;
				FREE_LOCK(&lk);
				if ((error = bwrite(nbp)) != 0) {
					goto loop_end;
				}
				ACQUIRE_LOCK(&lk);
				goto restart;
			}
			continue;

		case D_INODEDEP:
			if ((error = flush_inodedep_deps(wk->wk_mp,
			    WK_INODEDEP(wk)->id_ino)) != 0) {
				FREE_LOCK(&lk);
				break;
			}
			continue;

		case D_PAGEDEP:
			/*
			 * We are trying to sync a directory that may
			 * have dependencies on both its own metadata
			 * and/or dependencies on the inodes of any
			 * recently allocated files. We walk its diradd
			 * lists pushing out the associated inode.
			 */
			pagedep = WK_PAGEDEP(wk);
			for (i = 0; i < DAHASHSZ; i++) {
				if (LIST_FIRST(&pagedep->pd_diraddhd[i]) == 0)
					continue;
				if ((error =
				    flush_pagedep_deps(vp, wk->wk_mp,
						&pagedep->pd_diraddhd[i]))) {
					FREE_LOCK(&lk);
					goto loop_end;
				}
			}
			continue;

		case D_MKDIR:
			/*
			 * This case should never happen if the vnode has
			 * been properly sync'ed. However, if this function
			 * is used at a place where the vnode has not yet
			 * been sync'ed, this dependency can show up. So,
			 * rather than panic, just flush it.
			 */
			nbp = WK_MKDIR(wk)->md_buf;
			nbp = getdirtybuf(nbp, &lk, waitfor);
			if (nbp == NULL)
				continue;
			FREE_LOCK(&lk);
			if (waitfor == MNT_NOWAIT) {
				bawrite(nbp);
			} else if ((error = bwrite(nbp)) != 0) {
				break;
			}
			ACQUIRE_LOCK(&lk);
			continue;

		case D_BMSAFEMAP:
			/*
			 * This case should never happen if the vnode has
			 * been properly sync'ed. However, if this function
			 * is used at a place where the vnode has not yet
			 * been sync'ed, this dependency can show up. So,
			 * rather than panic, just flush it.
			 */
			nbp = WK_BMSAFEMAP(wk)->sm_buf;
			nbp = getdirtybuf(nbp, &lk, waitfor);
			if (nbp == NULL)
				continue;
			FREE_LOCK(&lk);
			if (waitfor == MNT_NOWAIT) {
				bawrite(nbp);
			} else if ((error = bwrite(nbp)) != 0) {
				break;
			}
			ACQUIRE_LOCK(&lk);
			continue;

		default:
			panic("softdep_sync_metadata: Unknown type %s",
			    TYPENAME(wk->wk_type));
			/* NOTREACHED */
		}
	loop_end:
		/* We reach here only in error and unlocked */
		if (error == 0)
			panic("softdep_sync_metadata: zero error");
		bp->b_lock.lk_flags &= ~LK_CANRECURSE;
		bawrite(bp);
		return (error);
	}
	FREE_LOCK(&lk);
	VI_LOCK(vp);
	while ((nbp = TAILQ_NEXT(bp, b_bobufs)) != NULL) {
		nbp = getdirtybuf(nbp, VI_MTX(vp), MNT_WAIT);
		if (nbp)
			break;
	}
	VI_UNLOCK(vp);
	bp->b_lock.lk_flags &= ~LK_CANRECURSE;
	bawrite(bp);
	if (nbp != NULL) {
		bp = nbp;
		goto loop;
	}
	/*
	 * The brief unlock is to allow any pent up dependency
	 * processing to be done. Then proceed with the second pass.
	 */
	if (waitfor == MNT_NOWAIT) {
		waitfor = MNT_WAIT;
		goto top;
	}

	/*
	 * If we have managed to get rid of all the dirty buffers,
	 * then we are done. For certain directories and block
	 * devices, we may need to do further work.
	 *
	 * We must wait for any I/O in progress to finish so that
	 * all potential buffers on the dirty list will be visible.
	 */
	VI_LOCK(vp);
	drain_output(vp);
	VI_UNLOCK(vp);
	return (0);
}

/*
 * Flush the dependencies associated with an inodedep.
 * Called with splbio blocked.
 */
static int
flush_inodedep_deps(mp, ino)
	struct mount *mp;
	ino_t ino;
{
	struct inodedep *inodedep;
	int error, waitfor;

	/*
	 * This work is done in two passes. The first pass grabs most
	 * of the buffers and begins asynchronously writing them. The
	 * only way to wait for these asynchronous writes is to sleep
	 * on the filesystem vnode which may stay busy for a long time
	 * if the filesystem is active. So, instead, we make a second
	 * pass over the dependencies blocking on each write. In the
	 * usual case we will be blocking against a write that we
	 * initiated, so when it is done the dependency will have been
	 * resolved. Thus the second pass is expected to end quickly.
	 * We give a brief window at the top of the loop to allow
	 * any pending I/O to complete.
	 */
	for (error = 0, waitfor = MNT_NOWAIT; ; ) {
		if (error)
			return (error);
		FREE_LOCK(&lk);
		ACQUIRE_LOCK(&lk);
		if (inodedep_lookup(mp, ino, 0, &inodedep) == 0)
			return (0);
		if (flush_deplist(&inodedep->id_inoupdt, waitfor, &error) ||
		    flush_deplist(&inodedep->id_newinoupdt, waitfor, &error) ||
		    flush_deplist(&inodedep->id_extupdt, waitfor, &error) ||
		    flush_deplist(&inodedep->id_newextupdt, waitfor, &error))
			continue;
		/*
		 * If pass2, we are done, otherwise do pass 2.
		 */
		if (waitfor == MNT_WAIT)
			break;
		waitfor = MNT_WAIT;
	}
	/*
	 * Try freeing inodedep in case all dependencies have been removed.
	 */
	if (inodedep_lookup(mp, ino, 0, &inodedep) != 0)
		(void) free_inodedep(inodedep);
	return (0);
}

/*
 * Flush an inode dependency list.
 * Called with splbio blocked.
 */
static int
flush_deplist(listhead, waitfor, errorp)
	struct allocdirectlst *listhead;
	int waitfor;
	int *errorp;
{
	struct allocdirect *adp;
	struct buf *bp;

	mtx_assert(&lk, MA_OWNED);
	TAILQ_FOREACH(adp, listhead, ad_next) {
		if (adp->ad_state & DEPCOMPLETE)
			continue;
		bp = adp->ad_buf;
		bp = getdirtybuf(bp, &lk, waitfor);
		if (bp == NULL) {
			if (waitfor == MNT_NOWAIT)
				continue;
			return (1);
		}
		FREE_LOCK(&lk);
		if (waitfor == MNT_NOWAIT) {
			bawrite(bp);
		} else if ((*errorp = bwrite(bp)) != 0) {
			ACQUIRE_LOCK(&lk);
			return (1);
		}
		ACQUIRE_LOCK(&lk);
		return (1);
	}
	return (0);
}

/*
 * Eliminate a pagedep dependency by flushing out all its diradd dependencies.
 * Called with splbio blocked.
 */
static int
flush_pagedep_deps(pvp, mp, diraddhdp)
	struct vnode *pvp;
	struct mount *mp;
	struct diraddhd *diraddhdp;
{
	struct inodedep *inodedep;
	struct ufsmount *ump;
	struct diradd *dap;
	struct vnode *vp;
	int error = 0;
	struct buf *bp;
	ino_t inum;
	struct worklist *wk;

	ump = VFSTOUFS(mp);
	while ((dap = LIST_FIRST(diraddhdp)) != NULL) {
		/*
		 * Flush ourselves if this directory entry
		 * has a MKDIR_PARENT dependency.
		 */
		if (dap->da_state & MKDIR_PARENT) {
			FREE_LOCK(&lk);
			if ((error = ffs_update(pvp, 1)) != 0)
				break;
			ACQUIRE_LOCK(&lk);
			/*
			 * If that cleared dependencies, go on to next.
			 */
			if (dap != LIST_FIRST(diraddhdp))
				continue;
			if (dap->da_state & MKDIR_PARENT)
				panic("flush_pagedep_deps: MKDIR_PARENT");
		}
		/*
		 * A newly allocated directory must have its "." and
		 * ".." entries written out before its name can be
		 * committed in its parent. We do not want or need
		 * the full semantics of a synchronous ffs_syncvnode as
		 * that may end up here again, once for each directory
		 * level in the filesystem. Instead, we push the blocks
		 * and wait for them to clear. We have to fsync twice
		 * because the first call may choose to defer blocks
		 * that still have dependencies, but deferral will
		 * happen at most once.
		 */
		inum = dap->da_newinum;
		if (dap->da_state & MKDIR_BODY) {
			FREE_LOCK(&lk);
			if ((error = ffs_vget(mp, inum, LK_EXCLUSIVE, &vp)))
				break;
			if ((error=ffs_syncvnode(vp, MNT_NOWAIT)) ||
			    (error=ffs_syncvnode(vp, MNT_NOWAIT))) {
				vput(vp);
				break;
			}
			VI_LOCK(vp);
			drain_output(vp);
			/*
			 * If first block is still dirty with a D_MKDIR
			 * dependency then it needs to be written now.
			 */
			for (;;) {
				error = 0;
				bp = gbincore(&vp->v_bufobj, 0);
				if (bp == NULL)
					break;	/* First block not present */
				error = BUF_LOCK(bp,
						 LK_EXCLUSIVE |
						 LK_SLEEPFAIL |
						 LK_INTERLOCK,
						 VI_MTX(vp));
				VI_LOCK(vp);
				if (error == ENOLCK)
					continue;	/* Slept, retry */
				if (error != 0)
					break;		/* Failed */
				if ((bp->b_flags & B_DELWRI) == 0) {
					BUF_UNLOCK(bp);
					break;	/* Buffer not dirty */
				}
				for (wk = LIST_FIRST(&bp->b_dep);
				     wk != NULL;
				     wk = LIST_NEXT(wk, wk_list))
					if (wk->wk_type == D_MKDIR)
						break;
				if (wk == NULL)
					BUF_UNLOCK(bp);	/* Dependency gone */
				else {
					/*
					 * D_MKDIR dependency remains,
					 * must write buffer to stable
					 * storage.
					 */
					VI_UNLOCK(vp);
					bremfree(bp);
					error = bwrite(bp);
					VI_LOCK(vp);
				}
				break;
			}
			VI_UNLOCK(vp);
			vput(vp);
			if (error != 0)
				break;	/* Flushing of first block failed */
			ACQUIRE_LOCK(&lk);
			/*
			 * If that cleared dependencies, go on to next.
			 */
			if (dap != LIST_FIRST(diraddhdp))
				continue;
			if (dap->da_state & MKDIR_BODY)
				panic("flush_pagedep_deps: MKDIR_BODY");
		}
		/*
		 * Flush the inode on which the directory entry depends.
		 * Having accounted for MKDIR_PARENT and MKDIR_BODY above,
		 * the only remaining dependency is that the updated inode
		 * count must get pushed to disk. The inode has already
		 * been pushed into its inode buffer (via VOP_UPDATE) at
		 * the time of the reference count change. So we need only
		 * locate that buffer, ensure that there will be no rollback
		 * caused by a bitmap dependency, then write the inode buffer.
		 */
retry:
		if (inodedep_lookup(UFSTOVFS(ump), inum, 0, &inodedep) == 0)
			panic("flush_pagedep_deps: lost inode");
		/*
		 * If the inode still has bitmap dependencies,
		 * push them to disk.
		 */
		if ((inodedep->id_state & DEPCOMPLETE) == 0) {
			bp = inodedep->id_buf;
			bp = getdirtybuf(bp, &lk, MNT_WAIT);
			if (bp == NULL)
				goto retry;
			FREE_LOCK(&lk);
			if ((error = bwrite(bp)) != 0)
				break;
			ACQUIRE_LOCK(&lk);
			if (dap != LIST_FIRST(diraddhdp))
				continue;
		}
		/*
		 * If the inode is still sitting in a buffer waiting
		 * to be written, push it to disk.
		 */
		FREE_LOCK(&lk);
		if ((error = bread(ump->um_devvp,
		    fsbtodb(ump->um_fs, ino_to_fsba(ump->um_fs, inum)),
		    (int)ump->um_fs->fs_bsize, NOCRED, &bp)) != 0) {
			brelse(bp);
			break;
		}
		if ((error = bwrite(bp)) != 0)
			break;
		ACQUIRE_LOCK(&lk);
		/*
		 * If we have failed to get rid of all the dependencies
		 * then something is seriously wrong.
		 */
		if (dap == LIST_FIRST(diraddhdp))
			panic("flush_pagedep_deps: flush failed");
	}
	if (error)
		ACQUIRE_LOCK(&lk);
	return (error);
}

/*
 * A large burst of file addition or deletion activity can drive the
 * memory load excessively high. First attempt to slow things down
 * using the techniques below. If that fails, this routine requests
 * the offending operations to fall back to running synchronously
 * until the memory load returns to a reasonable level.
 */
int
softdep_slowdown(vp)
	struct vnode *vp;
{
	int max_softdeps_hard;

	ACQUIRE_LOCK(&lk);
	max_softdeps_hard = max_softdeps * 11 / 10;
	if (num_dirrem < max_softdeps_hard / 2 &&
	    num_inodedep < max_softdeps_hard &&
	    VFSTOUFS(vp->v_mount)->um_numindirdeps < maxindirdeps) {
		FREE_LOCK(&lk);
  		return (0);
	}
	if (VFSTOUFS(vp->v_mount)->um_numindirdeps >= maxindirdeps)
		softdep_speedup();
	stat_sync_limit_hit += 1;
	FREE_LOCK(&lk);
	return (1);
}

/*
 * Called by the allocation routines when they are about to fail
 * in the hope that we can free up some disk space.
 * 
 * First check to see if the work list has anything on it. If it has,
 * clean up entries until we successfully free some space. Because this
 * process holds inodes locked, we cannot handle any remove requests
 * that might block on a locked inode as that could lead to deadlock.
 * If the worklist yields no free space, encourage the syncer daemon
 * to help us. In no event will we try for longer than tickdelay seconds.
 */
int
softdep_request_cleanup(fs, vp)
	struct fs *fs;
	struct vnode *vp;
{
	struct ufsmount *ump;
	long starttime;
	ufs2_daddr_t needed;
	int error;

	ump = VTOI(vp)->i_ump;
	mtx_assert(UFS_MTX(ump), MA_OWNED);
	needed = fs->fs_cstotal.cs_nbfree + fs->fs_contigsumsize;
	starttime = time_second + tickdelay;
	/*
	 * If we are being called because of a process doing a
	 * copy-on-write, then it is not safe to update the vnode
	 * as we may recurse into the copy-on-write routine.
	 */
	if (!(curthread->td_pflags & TDP_COWINPROGRESS)) {
		UFS_UNLOCK(ump);
		error = ffs_update(vp, 1);
		UFS_LOCK(ump);
		if (error != 0)
			return (0);
	}
	while (fs->fs_pendingblocks > 0 && fs->fs_cstotal.cs_nbfree <= needed) {
		if (time_second > starttime)
			return (0);
		UFS_UNLOCK(ump);
		ACQUIRE_LOCK(&lk);
		if (ump->softdep_on_worklist > 0 &&
		    process_worklist_item(UFSTOVFS(ump), LK_NOWAIT) != -1) {
			stat_worklist_push += 1;
			FREE_LOCK(&lk);
			UFS_LOCK(ump);
			continue;
		}
		request_cleanup(UFSTOVFS(ump), FLUSH_REMOVE_WAIT);
		FREE_LOCK(&lk);
		UFS_LOCK(ump);
	}
	return (1);
}

/*
 * If memory utilization has gotten too high, deliberately slow things
 * down and speed up the I/O processing.
 */
extern struct thread *syncertd;
static int
request_cleanup(mp, resource)
	struct mount *mp;
	int resource;
{
	struct thread *td = curthread;
	struct ufsmount *ump;

	mtx_assert(&lk, MA_OWNED);
	/*
	 * We never hold up the filesystem syncer or buf daemon.
	 */
	if (td->td_pflags & (TDP_SOFTDEP|TDP_NORUNNINGBUF))
		return (0);
	ump = VFSTOUFS(mp);
	/*
	 * First check to see if the work list has gotten backlogged.
	 * If it has, co-opt this process to help clean up two entries.
	 * Because this process may hold inodes locked, we cannot
	 * handle any remove requests that might block on a locked
	 * inode as that could lead to deadlock.  We set TDP_SOFTDEP
	 * to avoid recursively processing the worklist.
	 */
	if (ump->softdep_on_worklist > max_softdeps / 10) {
		td->td_pflags |= TDP_SOFTDEP;
		process_worklist_item(mp, LK_NOWAIT);
		process_worklist_item(mp, LK_NOWAIT);
		td->td_pflags &= ~TDP_SOFTDEP;
		stat_worklist_push += 2;
		return(1);
	}
	/*
	 * Next, we attempt to speed up the syncer process. If that
	 * is successful, then we allow the process to continue.
	 */
	if (softdep_speedup() && resource != FLUSH_REMOVE_WAIT)
		return(0);
	/*
	 * If we are resource constrained on inode dependencies, try
	 * flushing some dirty inodes. Otherwise, we are constrained
	 * by file deletions, so try accelerating flushes of directories
	 * with removal dependencies. We would like to do the cleanup
	 * here, but we probably hold an inode locked at this point and 
	 * that might deadlock against one that we try to clean. So,
	 * the best that we can do is request the syncer daemon to do
	 * the cleanup for us.
	 */
	switch (resource) {

	case FLUSH_INODES:
		stat_ino_limit_push += 1;
		req_clear_inodedeps += 1;
		stat_countp = &stat_ino_limit_hit;
		break;

	case FLUSH_REMOVE:
	case FLUSH_REMOVE_WAIT:
		stat_blk_limit_push += 1;
		req_clear_remove += 1;
		stat_countp = &stat_blk_limit_hit;
		break;

	default:
		panic("request_cleanup: unknown type");
	}
	/*
	 * Hopefully the syncer daemon will catch up and awaken us.
	 * We wait at most tickdelay before proceeding in any case.
	 */
	proc_waiting += 1;
	if (handle.callout == NULL)
		handle = timeout(pause_timer, 0, tickdelay > 2 ? tickdelay : 2);
	msleep((caddr_t)&proc_waiting, &lk, PPAUSE, "softupdate", 0);
	proc_waiting -= 1;
	return (1);
}

/*
 * Awaken processes pausing in request_cleanup and clear proc_waiting
 * to indicate that there is no longer a timer running.
 */
static void
pause_timer(arg)
	void *arg;
{

	ACQUIRE_LOCK(&lk);
	*stat_countp += 1;
	wakeup_one(&proc_waiting);
	if (proc_waiting > 0)
		handle = timeout(pause_timer, 0, tickdelay > 2 ? tickdelay : 2);
	else
		handle.callout = NULL;
	FREE_LOCK(&lk);
}

/*
 * Flush out a directory with at least one removal dependency in an effort to
 * reduce the number of dirrem, freefile, and freeblks dependency structures.
 */
static void
clear_remove(td)
	struct thread *td;
{
	struct pagedep_hashhead *pagedephd;
	struct pagedep *pagedep;
	static int next = 0;
	struct mount *mp;
	struct vnode *vp;
	int error, cnt;
	ino_t ino;

	mtx_assert(&lk, MA_OWNED);

	for (cnt = 0; cnt < pagedep_hash; cnt++) {
		pagedephd = &pagedep_hashtbl[next++];
		if (next >= pagedep_hash)
			next = 0;
		LIST_FOREACH(pagedep, pagedephd, pd_hash) {
			if (LIST_EMPTY(&pagedep->pd_dirremhd))
				continue;
			mp = pagedep->pd_list.wk_mp;
			ino = pagedep->pd_ino;
			if (vn_start_write(NULL, &mp, V_NOWAIT) != 0)
				continue;
			FREE_LOCK(&lk);
			if ((error = ffs_vget(mp, ino, LK_EXCLUSIVE, &vp))) {
				softdep_error("clear_remove: vget", error);
				vn_finished_write(mp);
				ACQUIRE_LOCK(&lk);
				return;
			}
			if ((error = ffs_syncvnode(vp, MNT_NOWAIT)))
				softdep_error("clear_remove: fsync", error);
			VI_LOCK(vp);
			drain_output(vp);
			VI_UNLOCK(vp);
			vput(vp);
			vn_finished_write(mp);
			ACQUIRE_LOCK(&lk);
			return;
		}
	}
}

/*
 * Clear out a block of dirty inodes in an effort to reduce
 * the number of inodedep dependency structures.
 */
static void
clear_inodedeps(td)
	struct thread *td;
{
	struct inodedep_hashhead *inodedephd;
	struct inodedep *inodedep;
	static int next = 0;
	struct mount *mp;
	struct vnode *vp;
	struct fs *fs;
	int error, cnt;
	ino_t firstino, lastino, ino;

	mtx_assert(&lk, MA_OWNED);
	/*
	 * Pick a random inode dependency to be cleared.
	 * We will then gather up all the inodes in its block 
	 * that have dependencies and flush them out.
	 */
	for (cnt = 0; cnt < inodedep_hash; cnt++) {
		inodedephd = &inodedep_hashtbl[next++];
		if (next >= inodedep_hash)
			next = 0;
		if ((inodedep = LIST_FIRST(inodedephd)) != NULL)
			break;
	}
	if (inodedep == NULL)
		return;
	fs = inodedep->id_fs;
	mp = inodedep->id_list.wk_mp;
	/*
	 * Find the last inode in the block with dependencies.
	 */
	firstino = inodedep->id_ino & ~(INOPB(fs) - 1);
	for (lastino = firstino + INOPB(fs) - 1; lastino > firstino; lastino--)
		if (inodedep_lookup(mp, lastino, 0, &inodedep) != 0)
			break;
	/*
	 * Asynchronously push all but the last inode with dependencies.
	 * Synchronously push the last inode with dependencies to ensure
	 * that the inode block gets written to free up the inodedeps.
	 */
	for (ino = firstino; ino <= lastino; ino++) {
		if (inodedep_lookup(mp, ino, 0, &inodedep) == 0)
			continue;
		if (vn_start_write(NULL, &mp, V_NOWAIT) != 0)
			continue;
		FREE_LOCK(&lk);
		if ((error = ffs_vget(mp, ino, LK_EXCLUSIVE, &vp)) != 0) {
			softdep_error("clear_inodedeps: vget", error);
			vn_finished_write(mp);
			ACQUIRE_LOCK(&lk);
			return;
		}
		if (ino == lastino) {
			if ((error = ffs_syncvnode(vp, MNT_WAIT)))
				softdep_error("clear_inodedeps: fsync1", error);
		} else {
			if ((error = ffs_syncvnode(vp, MNT_NOWAIT)))
				softdep_error("clear_inodedeps: fsync2", error);
			VI_LOCK(vp);
			drain_output(vp);
			VI_UNLOCK(vp);
		}
		vput(vp);
		vn_finished_write(mp);
		ACQUIRE_LOCK(&lk);
	}
}

/*
 * Function to determine if the buffer has outstanding dependencies
 * that will cause a roll-back if the buffer is written. If wantcount
 * is set, return number of dependencies, otherwise just yes or no.
 */
static int
softdep_count_dependencies(bp, wantcount)
	struct buf *bp;
	int wantcount;
{
	struct worklist *wk;
	struct inodedep *inodedep;
	struct indirdep *indirdep;
	struct allocindir *aip;
	struct pagedep *pagedep;
	struct diradd *dap;
	int i, retval;

	retval = 0;
	ACQUIRE_LOCK(&lk);
	LIST_FOREACH(wk, &bp->b_dep, wk_list) {
		switch (wk->wk_type) {

		case D_INODEDEP:
			inodedep = WK_INODEDEP(wk);
			if ((inodedep->id_state & DEPCOMPLETE) == 0) {
				/* bitmap allocation dependency */
				retval += 1;
				if (!wantcount)
					goto out;
			}
			if (TAILQ_FIRST(&inodedep->id_inoupdt)) {
				/* direct block pointer dependency */
				retval += 1;
				if (!wantcount)
					goto out;
			}
			if (TAILQ_FIRST(&inodedep->id_extupdt)) {
				/* direct block pointer dependency */
				retval += 1;
				if (!wantcount)
					goto out;
			}
			continue;

		case D_INDIRDEP:
			indirdep = WK_INDIRDEP(wk);

			LIST_FOREACH(aip, &indirdep->ir_deplisthd, ai_next) {
				/* indirect block pointer dependency */
				retval += 1;
				if (!wantcount)
					goto out;
			}
			continue;

		case D_PAGEDEP:
			pagedep = WK_PAGEDEP(wk);
			for (i = 0; i < DAHASHSZ; i++) {

				LIST_FOREACH(dap, &pagedep->pd_diraddhd[i], da_pdlist) {
					/* directory entry dependency */
					retval += 1;
					if (!wantcount)
						goto out;
				}
			}
			continue;

		case D_BMSAFEMAP:
		case D_ALLOCDIRECT:
		case D_ALLOCINDIR:
		case D_MKDIR:
			/* never a dependency on these blocks */
			continue;

		default:
			panic("softdep_check_for_rollback: Unexpected type %s",
			    TYPENAME(wk->wk_type));
			/* NOTREACHED */
		}
	}
out:
	FREE_LOCK(&lk);
	return retval;
}

/*
 * Acquire exclusive access to a buffer.
 * Must be called with a locked mtx parameter.
 * Return acquired buffer or NULL on failure.
 */
static struct buf *
getdirtybuf(bp, mtx, waitfor)
	struct buf *bp;
	struct mtx *mtx;
	int waitfor;
{
	int error;

	mtx_assert(mtx, MA_OWNED);
	if (BUF_LOCK(bp, LK_EXCLUSIVE | LK_NOWAIT, NULL) != 0) {
		if (waitfor != MNT_WAIT)
			return (NULL);
		error = BUF_LOCK(bp,
		    LK_EXCLUSIVE | LK_SLEEPFAIL | LK_INTERLOCK, mtx);
		/*
		 * Even if we sucessfully acquire bp here, we have dropped
		 * mtx, which may violates our guarantee.
		 */
		if (error == 0)
			BUF_UNLOCK(bp);
		else if (error != ENOLCK)
			panic("getdirtybuf: inconsistent lock: %d", error);
		mtx_lock(mtx);
		return (NULL);
	}
	if ((bp->b_vflags & BV_BKGRDINPROG) != 0) {
		if (mtx == &lk && waitfor == MNT_WAIT) {
			mtx_unlock(mtx);
			BO_LOCK(bp->b_bufobj);
			BUF_UNLOCK(bp);
			if ((bp->b_vflags & BV_BKGRDINPROG) != 0) {
				bp->b_vflags |= BV_BKGRDWAIT;
				msleep(&bp->b_xflags, BO_MTX(bp->b_bufobj),
				       PRIBIO | PDROP, "getbuf", 0);
			} else
				BO_UNLOCK(bp->b_bufobj);
			mtx_lock(mtx);
			return (NULL);
		}
		BUF_UNLOCK(bp);
		if (waitfor != MNT_WAIT)
			return (NULL);
		/*
		 * The mtx argument must be bp->b_vp's mutex in
		 * this case.
		 */
#ifdef	DEBUG_VFS_LOCKS
		if (bp->b_vp->v_type != VCHR)
			ASSERT_VI_LOCKED(bp->b_vp, "getdirtybuf");
#endif
		bp->b_vflags |= BV_BKGRDWAIT;
		msleep(&bp->b_xflags, mtx, PRIBIO, "getbuf", 0);
		return (NULL);
	}
	if ((bp->b_flags & B_DELWRI) == 0) {
		BUF_UNLOCK(bp);
		return (NULL);
	}
	bremfree(bp);
	return (bp);
}


/*
 * Check if it is safe to suspend the file system now.  On entry,
 * the vnode interlock for devvp should be held.  Return 0 with
 * the mount interlock held if the file system can be suspended now,
 * otherwise return EAGAIN with the mount interlock held.
 */
int
softdep_check_suspend(struct mount *mp,
		      struct vnode *devvp,
		      int softdep_deps,
		      int softdep_accdeps,
		      int secondary_writes,
		      int secondary_accwrites)
{
	struct bufobj *bo;
	struct ufsmount *ump;
	int error;

	ASSERT_VI_LOCKED(devvp, "softdep_check_suspend");
	ump = VFSTOUFS(mp);
	bo = &devvp->v_bufobj;

	for (;;) {
		if (!TRY_ACQUIRE_LOCK(&lk)) {
			VI_UNLOCK(devvp);
			ACQUIRE_LOCK(&lk);
			FREE_LOCK(&lk);
			VI_LOCK(devvp);
			continue;
		}
		if (!MNT_ITRYLOCK(mp)) {
			FREE_LOCK(&lk);
			VI_UNLOCK(devvp);
			MNT_ILOCK(mp);
			MNT_IUNLOCK(mp);
			VI_LOCK(devvp);
			continue;
		}
		if (mp->mnt_secondary_writes != 0) {
			FREE_LOCK(&lk);
			VI_UNLOCK(devvp);
			msleep(&mp->mnt_secondary_writes,
			       MNT_MTX(mp),
			       (PUSER - 1) | PDROP, "secwr", 0);
			VI_LOCK(devvp);
			continue;
		}
		break;
	}

	/*
	 * Reasons for needing more work before suspend:
	 * - Dirty buffers on devvp.
	 * - Softdep activity occurred after start of vnode sync loop
	 * - Secondary writes occurred after start of vnode sync loop
	 */
	error = 0;
	if (bo->bo_numoutput > 0 ||
	    bo->bo_dirty.bv_cnt > 0 ||
	    softdep_deps != 0 ||
	    ump->softdep_deps != 0 ||
	    softdep_accdeps != ump->softdep_accdeps ||
	    secondary_writes != 0 ||
	    mp->mnt_secondary_writes != 0 ||
	    secondary_accwrites != mp->mnt_secondary_accwrites)
		error = EAGAIN;
	FREE_LOCK(&lk);
	VI_UNLOCK(devvp);
	return (error);
}


/*
 * Get the number of dependency structures for the file system, both
 * the current number and the total number allocated.  These will
 * later be used to detect that softdep processing has occurred.
 */
void
softdep_get_depcounts(struct mount *mp,
		      int *softdep_depsp,
		      int *softdep_accdepsp)
{
	struct ufsmount *ump;

	ump = VFSTOUFS(mp);
	ACQUIRE_LOCK(&lk);
	*softdep_depsp = ump->softdep_deps;
	*softdep_accdepsp = ump->softdep_accdeps;
	FREE_LOCK(&lk);
}

/*
 * Wait for pending output on a vnode to complete.
 * Must be called with vnode lock and interlock locked.
 *
 * XXX: Should just be a call to bufobj_wwait().
 */
static void
drain_output(vp)
	struct vnode *vp;
{
	ASSERT_VOP_LOCKED(vp, "drain_output");
	ASSERT_VI_LOCKED(vp, "drain_output");

	while (vp->v_bufobj.bo_numoutput) {
		vp->v_bufobj.bo_flag |= BO_WWAIT;
		msleep((caddr_t)&vp->v_bufobj.bo_numoutput,
		    VI_MTX(vp), PRIBIO + 1, "drainvp", 0);
	}
}

/*
 * Called whenever a buffer that is being invalidated or reallocated
 * contains dependencies. This should only happen if an I/O error has
 * occurred. The routine is called with the buffer locked.
 */ 
static void
softdep_deallocate_dependencies(bp)
	struct buf *bp;
{

	if ((bp->b_ioflags & BIO_ERROR) == 0)
		panic("softdep_deallocate_dependencies: dangling deps");
	softdep_error(bp->b_vp->v_mount->mnt_stat.f_mntonname, bp->b_error);
	panic("softdep_deallocate_dependencies: unrecovered I/O error");
}

/*
 * Function to handle asynchronous write errors in the filesystem.
 */
static void
softdep_error(func, error)
	char *func;
	int error;
{

	/* XXX should do something better! */
	printf("%s: got error %d while accessing filesystem\n", func, error);
}

#endif /* SOFTUPDATES */
