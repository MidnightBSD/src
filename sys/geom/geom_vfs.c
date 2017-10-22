/*-
 * Copyright (c) 2004 Poul-Henning Kamp
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/7.0.0/sys/geom/geom_vfs.c 166193 2007-01-23 10:01:19Z kib $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/mount.h>	/* XXX Temporary for VFS_LOCK_GIANT */

#include <geom/geom.h>
#include <geom/geom_vfs.h>

/*
 * subroutines for use by filesystems.
 *
 * XXX: should maybe live somewhere else ?
 */
#include <sys/buf.h>

static struct buf_ops __g_vfs_bufops = {
	.bop_name =	"GEOM_VFS",
	.bop_write =	bufwrite,
	.bop_strategy =	g_vfs_strategy,	
	.bop_sync =	bufsync,	
	.bop_bdflush =	bufbdflush
};

struct buf_ops *g_vfs_bufops = &__g_vfs_bufops;

static g_orphan_t g_vfs_orphan;

static struct g_class g_vfs_class = {
	.name =		"VFS",
	.version =	G_VERSION,
	.orphan =	g_vfs_orphan,
};

DECLARE_GEOM_CLASS(g_vfs_class, g_vfs);

static void
g_vfs_done(struct bio *bip)
{
	struct buf *bp;
	int vfslocked;

	if (bip->bio_error) {
		printf("g_vfs_done():");
		g_print_bio(bip);
		printf("error = %d\n", bip->bio_error);
	}
	bp = bip->bio_caller2;
	bp->b_error = bip->bio_error;
	bp->b_ioflags = bip->bio_flags;
	if (bip->bio_error)
		bp->b_ioflags |= BIO_ERROR;
	bp->b_resid = bp->b_bcount - bip->bio_completed;
	g_destroy_bio(bip);
	vfslocked = VFS_LOCK_GIANT(((struct mount *)NULL));
	bufdone(bp);
	VFS_UNLOCK_GIANT(vfslocked);
}

void
g_vfs_strategy(struct bufobj *bo, struct buf *bp)
{
	struct g_consumer *cp;
	struct bio *bip;

	cp = bo->bo_private;
	G_VALID_CONSUMER(cp);

	bip = g_alloc_bio();
	bip->bio_cmd = bp->b_iocmd;
	bip->bio_offset = bp->b_iooffset;
	bip->bio_data = bp->b_data;
	bip->bio_done = g_vfs_done;
	bip->bio_caller2 = bp;
	bip->bio_length = bp->b_bcount;
	g_io_request(bip, cp);
}

static void
g_vfs_orphan(struct g_consumer *cp)
{

	/*
	 * Don't do anything here yet.
	 *
	 * Ideally we should detach the consumer already now, but that
	 * leads to a locking requirement in the I/O path to see if we have
	 * a consumer or not.  Considering how ugly things are going to get
	 * anyway as none of our filesystems are graceful about i/o errors,
	 * this is not important right now.
	 *
	 * Down the road, this is the place where we could give the user
	 * a "Abort, Retry or Ignore" option to replace the media again.
	 */
}

int
g_vfs_open(struct vnode *vp, struct g_consumer **cpp, const char *fsname, int wr)
{
	struct g_geom *gp;
	struct g_provider *pp;
	struct g_consumer *cp;
	struct bufobj *bo;
	int vfslocked;
	int error;

	g_topology_assert();

	*cpp = NULL;
	pp = g_dev_getprovider(vp->v_rdev);
	if (pp == NULL)
		return (ENOENT);
	gp = g_new_geomf(&g_vfs_class, "%s.%s", fsname, pp->name);
	cp = g_new_consumer(gp);
	g_attach(cp, pp);
	error = g_access(cp, 1, wr, 1);
	if (error) {
		g_wither_geom(gp, ENXIO);
		return (error);
	}
	vfslocked = VFS_LOCK_GIANT(vp->v_mount);
	vnode_create_vobject(vp, pp->mediasize, curthread);
	VFS_UNLOCK_GIANT(vfslocked);
	*cpp = cp;
	bo = &vp->v_bufobj;
	bo->bo_ops = g_vfs_bufops;
	bo->bo_private = cp;
	bo->bo_bsize = pp->sectorsize;
	gp->softc = bo;

	return (error);
}

void
g_vfs_close(struct g_consumer *cp, struct thread *td)
{
	struct g_geom *gp;
	struct bufobj *bo;

	g_topology_assert();

	gp = cp->geom;
	bo = gp->softc;
	bufobj_invalbuf(bo, V_SAVE, td, 0, 0);
	g_wither_geom_close(gp, ENXIO);
}
