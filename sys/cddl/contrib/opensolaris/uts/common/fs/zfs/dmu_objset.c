/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/zfs_context.h>
#include <sys/dmu_objset.h>
#include <sys/dsl_dir.h>
#include <sys/dsl_dataset.h>
#include <sys/dsl_prop.h>
#include <sys/dsl_pool.h>
#include <sys/dsl_synctask.h>
#include <sys/dnode.h>
#include <sys/dbuf.h>
#include <sys/zvol.h>
#include <sys/dmu_tx.h>
#include <sys/zio_checksum.h>
#include <sys/zap.h>
#include <sys/zil.h>
#include <sys/dmu_impl.h>


spa_t *
dmu_objset_spa(objset_t *os)
{
	return (os->os->os_spa);
}

zilog_t *
dmu_objset_zil(objset_t *os)
{
	return (os->os->os_zil);
}

dsl_pool_t *
dmu_objset_pool(objset_t *os)
{
	dsl_dataset_t *ds;

	if ((ds = os->os->os_dsl_dataset) != NULL && ds->ds_dir)
		return (ds->ds_dir->dd_pool);
	else
		return (spa_get_dsl(os->os->os_spa));
}

dsl_dataset_t *
dmu_objset_ds(objset_t *os)
{
	return (os->os->os_dsl_dataset);
}

dmu_objset_type_t
dmu_objset_type(objset_t *os)
{
	return (os->os->os_phys->os_type);
}

void
dmu_objset_name(objset_t *os, char *buf)
{
	dsl_dataset_name(os->os->os_dsl_dataset, buf);
}

uint64_t
dmu_objset_id(objset_t *os)
{
	dsl_dataset_t *ds = os->os->os_dsl_dataset;

	return (ds ? ds->ds_object : 0);
}

static void
checksum_changed_cb(void *arg, uint64_t newval)
{
	objset_impl_t *osi = arg;

	/*
	 * Inheritance should have been done by now.
	 */
	ASSERT(newval != ZIO_CHECKSUM_INHERIT);

	osi->os_checksum = zio_checksum_select(newval, ZIO_CHECKSUM_ON_VALUE);
}

static void
compression_changed_cb(void *arg, uint64_t newval)
{
	objset_impl_t *osi = arg;

	/*
	 * Inheritance and range checking should have been done by now.
	 */
	ASSERT(newval != ZIO_COMPRESS_INHERIT);

	osi->os_compress = zio_compress_select(newval, ZIO_COMPRESS_ON_VALUE);
}

static void
copies_changed_cb(void *arg, uint64_t newval)
{
	objset_impl_t *osi = arg;

	/*
	 * Inheritance and range checking should have been done by now.
	 */
	ASSERT(newval > 0);
	ASSERT(newval <= spa_max_replication(osi->os_spa));

	osi->os_copies = newval;
}

void
dmu_objset_byteswap(void *buf, size_t size)
{
	objset_phys_t *osp = buf;

	ASSERT(size == sizeof (objset_phys_t));
	dnode_byteswap(&osp->os_meta_dnode);
	byteswap_uint64_array(&osp->os_zil_header, sizeof (zil_header_t));
	osp->os_type = BSWAP_64(osp->os_type);
}

int
dmu_objset_open_impl(spa_t *spa, dsl_dataset_t *ds, blkptr_t *bp,
    objset_impl_t **osip)
{
	objset_impl_t *winner, *osi;
	int i, err, checksum;

	osi = kmem_zalloc(sizeof (objset_impl_t), KM_SLEEP);
	osi->os.os = osi;
	osi->os_dsl_dataset = ds;
	osi->os_spa = spa;
	osi->os_rootbp = bp;
	if (!BP_IS_HOLE(osi->os_rootbp)) {
		uint32_t aflags = ARC_WAIT;
		zbookmark_t zb;
		zb.zb_objset = ds ? ds->ds_object : 0;
		zb.zb_object = 0;
		zb.zb_level = -1;
		zb.zb_blkid = 0;

		dprintf_bp(osi->os_rootbp, "reading %s", "");
		err = arc_read(NULL, spa, osi->os_rootbp,
		    dmu_ot[DMU_OT_OBJSET].ot_byteswap,
		    arc_getbuf_func, &osi->os_phys_buf,
		    ZIO_PRIORITY_SYNC_READ, ZIO_FLAG_CANFAIL, &aflags, &zb);
		if (err) {
			kmem_free(osi, sizeof (objset_impl_t));
			return (err);
		}
		osi->os_phys = osi->os_phys_buf->b_data;
		arc_release(osi->os_phys_buf, &osi->os_phys_buf);
	} else {
		osi->os_phys_buf = arc_buf_alloc(spa, sizeof (objset_phys_t),
		    &osi->os_phys_buf, ARC_BUFC_METADATA);
		osi->os_phys = osi->os_phys_buf->b_data;
		bzero(osi->os_phys, sizeof (objset_phys_t));
	}

	/*
	 * Note: the changed_cb will be called once before the register
	 * func returns, thus changing the checksum/compression from the
	 * default (fletcher2/off).  Snapshots don't need to know, and
	 * registering would complicate clone promotion.
	 */
	if (ds && ds->ds_phys->ds_num_children == 0) {
		err = dsl_prop_register(ds, "checksum",
		    checksum_changed_cb, osi);
		if (err == 0)
			err = dsl_prop_register(ds, "compression",
			    compression_changed_cb, osi);
		if (err == 0)
			err = dsl_prop_register(ds, "copies",
			    copies_changed_cb, osi);
		if (err) {
			VERIFY(arc_buf_remove_ref(osi->os_phys_buf,
			    &osi->os_phys_buf) == 1);
			kmem_free(osi, sizeof (objset_impl_t));
			return (err);
		}
	} else if (ds == NULL) {
		/* It's the meta-objset. */
		osi->os_checksum = ZIO_CHECKSUM_FLETCHER_4;
		osi->os_compress = ZIO_COMPRESS_LZJB;
		osi->os_copies = spa_max_replication(spa);
	}

	osi->os_zil = zil_alloc(&osi->os, &osi->os_phys->os_zil_header);

	/*
	 * Metadata always gets compressed and checksummed.
	 * If the data checksum is multi-bit correctable, and it's not
	 * a ZBT-style checksum, then it's suitable for metadata as well.
	 * Otherwise, the metadata checksum defaults to fletcher4.
	 */
	checksum = osi->os_checksum;

	if (zio_checksum_table[checksum].ci_correctable &&
	    !zio_checksum_table[checksum].ci_zbt)
		osi->os_md_checksum = checksum;
	else
		osi->os_md_checksum = ZIO_CHECKSUM_FLETCHER_4;
	osi->os_md_compress = ZIO_COMPRESS_LZJB;

	for (i = 0; i < TXG_SIZE; i++) {
		list_create(&osi->os_dirty_dnodes[i], sizeof (dnode_t),
		    offsetof(dnode_t, dn_dirty_link[i]));
		list_create(&osi->os_free_dnodes[i], sizeof (dnode_t),
		    offsetof(dnode_t, dn_dirty_link[i]));
	}
	list_create(&osi->os_dnodes, sizeof (dnode_t),
	    offsetof(dnode_t, dn_link));
	list_create(&osi->os_downgraded_dbufs, sizeof (dmu_buf_impl_t),
	    offsetof(dmu_buf_impl_t, db_link));

	mutex_init(&osi->os_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&osi->os_obj_lock, NULL, MUTEX_DEFAULT, NULL);

	osi->os_meta_dnode = dnode_special_open(osi,
	    &osi->os_phys->os_meta_dnode, DMU_META_DNODE_OBJECT);

	if (ds != NULL) {
		winner = dsl_dataset_set_user_ptr(ds, osi, dmu_objset_evict);
		if (winner) {
			dmu_objset_evict(ds, osi);
			osi = winner;
		}
	}

	*osip = osi;
	return (0);
}

/* called from zpl */
int
dmu_objset_open(const char *name, dmu_objset_type_t type, int mode,
    objset_t **osp)
{
	dsl_dataset_t *ds;
	int err;
	objset_t *os;
	objset_impl_t *osi;

	os = kmem_alloc(sizeof (objset_t), KM_SLEEP);
	err = dsl_dataset_open(name, mode, os, &ds);
	if (err) {
		kmem_free(os, sizeof (objset_t));
		return (err);
	}

	osi = dsl_dataset_get_user_ptr(ds);
	if (osi == NULL) {
		err = dmu_objset_open_impl(dsl_dataset_get_spa(ds),
		    ds, &ds->ds_phys->ds_bp, &osi);
		if (err) {
			dsl_dataset_close(ds, mode, os);
			kmem_free(os, sizeof (objset_t));
			return (err);
		}
	}

	os->os = osi;
	os->os_mode = mode;

	if (type != DMU_OST_ANY && type != os->os->os_phys->os_type) {
		dmu_objset_close(os);
		return (EINVAL);
	}
	*osp = os;
	return (0);
}

void
dmu_objset_close(objset_t *os)
{
	dsl_dataset_close(os->os->os_dsl_dataset, os->os_mode, os);
	kmem_free(os, sizeof (objset_t));
}

int
dmu_objset_evict_dbufs(objset_t *os, int try)
{
	objset_impl_t *osi = os->os;
	dnode_t *dn;

	mutex_enter(&osi->os_lock);

	/* process the mdn last, since the other dnodes have holds on it */
	list_remove(&osi->os_dnodes, osi->os_meta_dnode);
	list_insert_tail(&osi->os_dnodes, osi->os_meta_dnode);

	/*
	 * Find the first dnode with holds.  We have to do this dance
	 * because dnode_add_ref() only works if you already have a
	 * hold.  If there are no holds then it has no dbufs so OK to
	 * skip.
	 */
	for (dn = list_head(&osi->os_dnodes);
	    dn && refcount_is_zero(&dn->dn_holds);
	    dn = list_next(&osi->os_dnodes, dn))
		continue;
	if (dn)
		dnode_add_ref(dn, FTAG);

	while (dn) {
		dnode_t *next_dn = dn;

		do {
			next_dn = list_next(&osi->os_dnodes, next_dn);
		} while (next_dn && refcount_is_zero(&next_dn->dn_holds));
		if (next_dn)
			dnode_add_ref(next_dn, FTAG);

		mutex_exit(&osi->os_lock);
		if (dnode_evict_dbufs(dn, try)) {
			dnode_rele(dn, FTAG);
			if (next_dn)
				dnode_rele(next_dn, FTAG);
			return (1);
		}
		dnode_rele(dn, FTAG);
		mutex_enter(&osi->os_lock);
		dn = next_dn;
	}
	mutex_exit(&osi->os_lock);
	return (0);
}

void
dmu_objset_evict(dsl_dataset_t *ds, void *arg)
{
	objset_impl_t *osi = arg;
	objset_t os;
	int i;

	for (i = 0; i < TXG_SIZE; i++) {
		ASSERT(list_head(&osi->os_dirty_dnodes[i]) == NULL);
		ASSERT(list_head(&osi->os_free_dnodes[i]) == NULL);
	}

	if (ds && ds->ds_phys->ds_num_children == 0) {
		VERIFY(0 == dsl_prop_unregister(ds, "checksum",
		    checksum_changed_cb, osi));
		VERIFY(0 == dsl_prop_unregister(ds, "compression",
		    compression_changed_cb, osi));
		VERIFY(0 == dsl_prop_unregister(ds, "copies",
		    copies_changed_cb, osi));
	}

	/*
	 * We should need only a single pass over the dnode list, since
	 * nothing can be added to the list at this point.
	 */
	os.os = osi;
	(void) dmu_objset_evict_dbufs(&os, 0);

	ASSERT3P(list_head(&osi->os_dnodes), ==, osi->os_meta_dnode);
	ASSERT3P(list_tail(&osi->os_dnodes), ==, osi->os_meta_dnode);
	ASSERT3P(list_head(&osi->os_meta_dnode->dn_dbufs), ==, NULL);

	dnode_special_close(osi->os_meta_dnode);
	zil_free(osi->os_zil);

	VERIFY(arc_buf_remove_ref(osi->os_phys_buf, &osi->os_phys_buf) == 1);
	mutex_destroy(&osi->os_lock);
	mutex_destroy(&osi->os_obj_lock);
	kmem_free(osi, sizeof (objset_impl_t));
}

/* called from dsl for meta-objset */
objset_impl_t *
dmu_objset_create_impl(spa_t *spa, dsl_dataset_t *ds, blkptr_t *bp,
    dmu_objset_type_t type, dmu_tx_t *tx)
{
	objset_impl_t *osi;
	dnode_t *mdn;

	ASSERT(dmu_tx_is_syncing(tx));
	VERIFY(0 == dmu_objset_open_impl(spa, ds, bp, &osi));
	mdn = osi->os_meta_dnode;

	dnode_allocate(mdn, DMU_OT_DNODE, 1 << DNODE_BLOCK_SHIFT,
	    DN_MAX_INDBLKSHIFT, DMU_OT_NONE, 0, tx);

	/*
	 * We don't want to have to increase the meta-dnode's nlevels
	 * later, because then we could do it in quescing context while
	 * we are also accessing it in open context.
	 *
	 * This precaution is not necessary for the MOS (ds == NULL),
	 * because the MOS is only updated in syncing context.
	 * This is most fortunate: the MOS is the only objset that
	 * needs to be synced multiple times as spa_sync() iterates
	 * to convergence, so minimizing its dn_nlevels matters.
	 */
	if (ds != NULL) {
		int levels = 1;

		/*
		 * Determine the number of levels necessary for the meta-dnode
		 * to contain DN_MAX_OBJECT dnodes.
		 */
		while ((uint64_t)mdn->dn_nblkptr << (mdn->dn_datablkshift +
		    (levels - 1) * (mdn->dn_indblkshift - SPA_BLKPTRSHIFT)) <
		    DN_MAX_OBJECT * sizeof (dnode_phys_t))
			levels++;

		mdn->dn_next_nlevels[tx->tx_txg & TXG_MASK] =
		    mdn->dn_nlevels = levels;
	}

	ASSERT(type != DMU_OST_NONE);
	ASSERT(type != DMU_OST_ANY);
	ASSERT(type < DMU_OST_NUMTYPES);
	osi->os_phys->os_type = type;

	dsl_dataset_dirty(ds, tx);

	return (osi);
}

struct oscarg {
	void (*userfunc)(objset_t *os, void *arg, dmu_tx_t *tx);
	void *userarg;
	dsl_dataset_t *clone_parent;
	const char *lastname;
	dmu_objset_type_t type;
};

/* ARGSUSED */
static int
dmu_objset_create_check(void *arg1, void *arg2, dmu_tx_t *tx)
{
	dsl_dir_t *dd = arg1;
	struct oscarg *oa = arg2;
	objset_t *mos = dd->dd_pool->dp_meta_objset;
	int err;
	uint64_t ddobj;

	err = zap_lookup(mos, dd->dd_phys->dd_child_dir_zapobj,
	    oa->lastname, sizeof (uint64_t), 1, &ddobj);
	if (err != ENOENT)
		return (err ? err : EEXIST);

	if (oa->clone_parent != NULL) {
		/*
		 * You can't clone across pools.
		 */
		if (oa->clone_parent->ds_dir->dd_pool != dd->dd_pool)
			return (EXDEV);

		/*
		 * You can only clone snapshots, not the head datasets.
		 */
		if (oa->clone_parent->ds_phys->ds_num_children == 0)
			return (EINVAL);
	}
	return (0);
}

static void
dmu_objset_create_sync(void *arg1, void *arg2, dmu_tx_t *tx)
{
	dsl_dir_t *dd = arg1;
	struct oscarg *oa = arg2;
	dsl_dataset_t *ds;
	blkptr_t *bp;
	uint64_t dsobj;

	ASSERT(dmu_tx_is_syncing(tx));

	dsobj = dsl_dataset_create_sync(dd, oa->lastname,
	    oa->clone_parent, tx);

	VERIFY(0 == dsl_dataset_open_obj(dd->dd_pool, dsobj, NULL,
	    DS_MODE_STANDARD | DS_MODE_READONLY, FTAG, &ds));
	bp = dsl_dataset_get_blkptr(ds);
	if (BP_IS_HOLE(bp)) {
		objset_impl_t *osi;

		/* This is an empty dmu_objset; not a clone. */
		osi = dmu_objset_create_impl(dsl_dataset_get_spa(ds),
		    ds, bp, oa->type, tx);

		if (oa->userfunc)
			oa->userfunc(&osi->os, oa->userarg, tx);
	}
	dsl_dataset_close(ds, DS_MODE_STANDARD | DS_MODE_READONLY, FTAG);
}

int
dmu_objset_create(const char *name, dmu_objset_type_t type,
    objset_t *clone_parent,
    void (*func)(objset_t *os, void *arg, dmu_tx_t *tx), void *arg)
{
	dsl_dir_t *pdd;
	const char *tail;
	int err = 0;
	struct oscarg oa = { 0 };

	ASSERT(strchr(name, '@') == NULL);
	err = dsl_dir_open(name, FTAG, &pdd, &tail);
	if (err)
		return (err);
	if (tail == NULL) {
		dsl_dir_close(pdd, FTAG);
		return (EEXIST);
	}

	dprintf("name=%s\n", name);

	oa.userfunc = func;
	oa.userarg = arg;
	oa.lastname = tail;
	oa.type = type;
	if (clone_parent != NULL) {
		/*
		 * You can't clone to a different type.
		 */
		if (clone_parent->os->os_phys->os_type != type) {
			dsl_dir_close(pdd, FTAG);
			return (EINVAL);
		}
		oa.clone_parent = clone_parent->os->os_dsl_dataset;
	}
	err = dsl_sync_task_do(pdd->dd_pool, dmu_objset_create_check,
	    dmu_objset_create_sync, pdd, &oa, 5);
	dsl_dir_close(pdd, FTAG);
	return (err);
}

int
dmu_objset_destroy(const char *name)
{
	objset_t *os;
	int error;

	/*
	 * If it looks like we'll be able to destroy it, and there's
	 * an unplayed replay log sitting around, destroy the log.
	 * It would be nicer to do this in dsl_dataset_destroy_sync(),
	 * but the replay log objset is modified in open context.
	 */
	error = dmu_objset_open(name, DMU_OST_ANY, DS_MODE_EXCLUSIVE, &os);
	if (error == 0) {
		zil_destroy(dmu_objset_zil(os), B_FALSE);
		dmu_objset_close(os);
	}

	return (dsl_dataset_destroy(name));
}

int
dmu_objset_rollback(const char *name)
{
	int err;
	objset_t *os;

	err = dmu_objset_open(name, DMU_OST_ANY,
	    DS_MODE_EXCLUSIVE | DS_MODE_INCONSISTENT, &os);
	if (err == 0) {
		err = zil_suspend(dmu_objset_zil(os));
		if (err == 0)
			zil_resume(dmu_objset_zil(os));
		if (err == 0) {
			/* XXX uncache everything? */
			err = dsl_dataset_rollback(os->os->os_dsl_dataset);
		}
		dmu_objset_close(os);
	}
	return (err);
}

struct snaparg {
	dsl_sync_task_group_t *dstg;
	char *snapname;
	char failed[MAXPATHLEN];
};

static int
dmu_objset_snapshot_one(char *name, void *arg)
{
	struct snaparg *sn = arg;
	objset_t *os;
	dmu_objset_stats_t stat;
	int err;

	(void) strcpy(sn->failed, name);

	err = dmu_objset_open(name, DMU_OST_ANY, DS_MODE_STANDARD, &os);
	if (err != 0)
		return (err);

	/*
	 * If the objset is in an inconsistent state, return busy.
	 */
	dmu_objset_fast_stat(os, &stat);
	if (stat.dds_inconsistent) {
		dmu_objset_close(os);
		return (EBUSY);
	}

	/*
	 * NB: we need to wait for all in-flight changes to get to disk,
	 * so that we snapshot those changes.  zil_suspend does this as
	 * a side effect.
	 */
	err = zil_suspend(dmu_objset_zil(os));
	if (err == 0) {
		dsl_sync_task_create(sn->dstg, dsl_dataset_snapshot_check,
		    dsl_dataset_snapshot_sync, os, sn->snapname, 3);
	} else {
		dmu_objset_close(os);
	}

	return (err);
}

int
dmu_objset_snapshot(char *fsname, char *snapname, boolean_t recursive)
{
	dsl_sync_task_t *dst;
	struct snaparg sn = { 0 };
	char *cp;
	spa_t *spa;
	int err;

	(void) strcpy(sn.failed, fsname);

	cp = strchr(fsname, '/');
	if (cp) {
		*cp = '\0';
		err = spa_open(fsname, &spa, FTAG);
		*cp = '/';
	} else {
		err = spa_open(fsname, &spa, FTAG);
	}
	if (err)
		return (err);

	sn.dstg = dsl_sync_task_group_create(spa_get_dsl(spa));
	sn.snapname = snapname;

	if (recursive) {
		err = dmu_objset_find(fsname,
		    dmu_objset_snapshot_one, &sn, DS_FIND_CHILDREN);
	} else {
		err = dmu_objset_snapshot_one(fsname, &sn);
	}

	if (err)
		goto out;

	err = dsl_sync_task_group_wait(sn.dstg);

	for (dst = list_head(&sn.dstg->dstg_tasks); dst;
	    dst = list_next(&sn.dstg->dstg_tasks, dst)) {
		objset_t *os = dst->dst_arg1;
		if (dst->dst_err)
			dmu_objset_name(os, sn.failed);
		zil_resume(dmu_objset_zil(os));
		dmu_objset_close(os);
	}
out:
	if (err)
		(void) strcpy(fsname, sn.failed);
	dsl_sync_task_group_destroy(sn.dstg);
	spa_close(spa, FTAG);
	return (err);
}

static void
dmu_objset_sync_dnodes(list_t *list, dmu_tx_t *tx)
{
	dnode_t *dn;

	while (dn = list_head(list)) {
		ASSERT(dn->dn_object != DMU_META_DNODE_OBJECT);
		ASSERT(dn->dn_dbuf->db_data_pending);
		/*
		 * Initialize dn_zio outside dnode_sync()
		 * to accomodate meta-dnode
		 */
		dn->dn_zio = dn->dn_dbuf->db_data_pending->dr_zio;
		ASSERT(dn->dn_zio);

		ASSERT3U(dn->dn_nlevels, <=, DN_MAX_LEVELS);
		list_remove(list, dn);
		dnode_sync(dn, tx);
	}
}

/* ARGSUSED */
static void
ready(zio_t *zio, arc_buf_t *abuf, void *arg)
{
	objset_impl_t *os = arg;
	blkptr_t *bp = os->os_rootbp;
	dnode_phys_t *dnp = &os->os_phys->os_meta_dnode;
	int i;

	/*
	 * Update rootbp fill count.
	 */
	bp->blk_fill = 1;	/* count the meta-dnode */
	for (i = 0; i < dnp->dn_nblkptr; i++)
		bp->blk_fill += dnp->dn_blkptr[i].blk_fill;
}

/* ARGSUSED */
static void
killer(zio_t *zio, arc_buf_t *abuf, void *arg)
{
	objset_impl_t *os = arg;

	ASSERT3U(zio->io_error, ==, 0);

	BP_SET_TYPE(zio->io_bp, DMU_OT_OBJSET);
	BP_SET_LEVEL(zio->io_bp, 0);

	if (!DVA_EQUAL(BP_IDENTITY(zio->io_bp),
	    BP_IDENTITY(&zio->io_bp_orig))) {
		if (zio->io_bp_orig.blk_birth == os->os_synctx->tx_txg)
			dsl_dataset_block_kill(os->os_dsl_dataset,
			    &zio->io_bp_orig, NULL, os->os_synctx);
		dsl_dataset_block_born(os->os_dsl_dataset, zio->io_bp,
		    os->os_synctx);
	}
	arc_release(os->os_phys_buf, &os->os_phys_buf);
}

/* called from dsl */
void
dmu_objset_sync(objset_impl_t *os, zio_t *pio, dmu_tx_t *tx)
{
	int txgoff;
	zbookmark_t zb;
	zio_t *zio;
	list_t *list;
	dbuf_dirty_record_t *dr;
	int zio_flags;

	dprintf_ds(os->os_dsl_dataset, "txg=%llu\n", tx->tx_txg);

	ASSERT(dmu_tx_is_syncing(tx));
	/* XXX the write_done callback should really give us the tx... */
	os->os_synctx = tx;

	if (os->os_dsl_dataset == NULL) {
		/*
		 * This is the MOS.  If we have upgraded,
		 * spa_max_replication() could change, so reset
		 * os_copies here.
		 */
		os->os_copies = spa_max_replication(os->os_spa);
	}

	/*
	 * Create the root block IO
	 */
	zb.zb_objset = os->os_dsl_dataset ? os->os_dsl_dataset->ds_object : 0;
	zb.zb_object = 0;
	zb.zb_level = -1;
	zb.zb_blkid = 0;
	zio_flags = ZIO_FLAG_MUSTSUCCEED;
	if (dmu_ot[DMU_OT_OBJSET].ot_metadata || zb.zb_level != 0)
		zio_flags |= ZIO_FLAG_METADATA;
	if (BP_IS_OLDER(os->os_rootbp, tx->tx_txg))
		dsl_dataset_block_kill(os->os_dsl_dataset,
		    os->os_rootbp, pio, tx);
	zio = arc_write(pio, os->os_spa, os->os_md_checksum,
	    os->os_md_compress,
	    dmu_get_replication_level(os, &zb, DMU_OT_OBJSET),
	    tx->tx_txg, os->os_rootbp, os->os_phys_buf, ready, killer, os,
	    ZIO_PRIORITY_ASYNC_WRITE, zio_flags, &zb);

	/*
	 * Sync meta-dnode - the parent IO for the sync is the root block
	 */
	os->os_meta_dnode->dn_zio = zio;
	dnode_sync(os->os_meta_dnode, tx);

	txgoff = tx->tx_txg & TXG_MASK;

	dmu_objset_sync_dnodes(&os->os_free_dnodes[txgoff], tx);
	dmu_objset_sync_dnodes(&os->os_dirty_dnodes[txgoff], tx);

	list = &os->os_meta_dnode->dn_dirty_records[txgoff];
	while (dr = list_head(list)) {
		ASSERT(dr->dr_dbuf->db_level == 0);
		list_remove(list, dr);
		if (dr->dr_zio)
			zio_nowait(dr->dr_zio);
	}
	/*
	 * Free intent log blocks up to this tx.
	 */
	zil_sync(os->os_zil, tx);
	zio_nowait(zio);
}

void
dmu_objset_space(objset_t *os, uint64_t *refdbytesp, uint64_t *availbytesp,
    uint64_t *usedobjsp, uint64_t *availobjsp)
{
	dsl_dataset_space(os->os->os_dsl_dataset, refdbytesp, availbytesp,
	    usedobjsp, availobjsp);
}

uint64_t
dmu_objset_fsid_guid(objset_t *os)
{
	return (dsl_dataset_fsid_guid(os->os->os_dsl_dataset));
}

void
dmu_objset_fast_stat(objset_t *os, dmu_objset_stats_t *stat)
{
	stat->dds_type = os->os->os_phys->os_type;
	if (os->os->os_dsl_dataset)
		dsl_dataset_fast_stat(os->os->os_dsl_dataset, stat);
}

void
dmu_objset_stats(objset_t *os, nvlist_t *nv)
{
	ASSERT(os->os->os_dsl_dataset ||
	    os->os->os_phys->os_type == DMU_OST_META);

	if (os->os->os_dsl_dataset != NULL)
		dsl_dataset_stats(os->os->os_dsl_dataset, nv);

	dsl_prop_nvlist_add_uint64(nv, ZFS_PROP_TYPE,
	    os->os->os_phys->os_type);
}

int
dmu_objset_is_snapshot(objset_t *os)
{
	if (os->os->os_dsl_dataset != NULL)
		return (dsl_dataset_is_snapshot(os->os->os_dsl_dataset));
	else
		return (B_FALSE);
}

int
dmu_snapshot_list_next(objset_t *os, int namelen, char *name,
    uint64_t *idp, uint64_t *offp)
{
	dsl_dataset_t *ds = os->os->os_dsl_dataset;
	zap_cursor_t cursor;
	zap_attribute_t attr;

	if (ds->ds_phys->ds_snapnames_zapobj == 0)
		return (ENOENT);

	zap_cursor_init_serialized(&cursor,
	    ds->ds_dir->dd_pool->dp_meta_objset,
	    ds->ds_phys->ds_snapnames_zapobj, *offp);

	if (zap_cursor_retrieve(&cursor, &attr) != 0) {
		zap_cursor_fini(&cursor);
		return (ENOENT);
	}

	if (strlen(attr.za_name) + 1 > namelen) {
		zap_cursor_fini(&cursor);
		return (ENAMETOOLONG);
	}

	(void) strcpy(name, attr.za_name);
	if (idp)
		*idp = attr.za_first_integer;
	zap_cursor_advance(&cursor);
	*offp = zap_cursor_serialize(&cursor);
	zap_cursor_fini(&cursor);

	return (0);
}

int
dmu_dir_list_next(objset_t *os, int namelen, char *name,
    uint64_t *idp, uint64_t *offp)
{
	dsl_dir_t *dd = os->os->os_dsl_dataset->ds_dir;
	zap_cursor_t cursor;
	zap_attribute_t attr;

	/* there is no next dir on a snapshot! */
	if (os->os->os_dsl_dataset->ds_object !=
	    dd->dd_phys->dd_head_dataset_obj)
		return (ENOENT);

	zap_cursor_init_serialized(&cursor,
	    dd->dd_pool->dp_meta_objset,
	    dd->dd_phys->dd_child_dir_zapobj, *offp);

	if (zap_cursor_retrieve(&cursor, &attr) != 0) {
		zap_cursor_fini(&cursor);
		return (ENOENT);
	}

	if (strlen(attr.za_name) + 1 > namelen) {
		zap_cursor_fini(&cursor);
		return (ENAMETOOLONG);
	}

	(void) strcpy(name, attr.za_name);
	if (idp)
		*idp = attr.za_first_integer;
	zap_cursor_advance(&cursor);
	*offp = zap_cursor_serialize(&cursor);
	zap_cursor_fini(&cursor);

	return (0);
}

/*
 * Find all objsets under name, and for each, call 'func(child_name, arg)'.
 */
int
dmu_objset_find(char *name, int func(char *, void *), void *arg, int flags)
{
	dsl_dir_t *dd;
	objset_t *os;
	uint64_t snapobj;
	zap_cursor_t zc;
	zap_attribute_t *attr;
	char *child;
	int do_self, err;

	err = dsl_dir_open(name, FTAG, &dd, NULL);
	if (err)
		return (err);

	/* NB: the $MOS dir doesn't have a head dataset */
	do_self = (dd->dd_phys->dd_head_dataset_obj != 0);
	attr = kmem_alloc(sizeof (zap_attribute_t), KM_SLEEP);

	/*
	 * Iterate over all children.
	 */
	if (flags & DS_FIND_CHILDREN) {
		for (zap_cursor_init(&zc, dd->dd_pool->dp_meta_objset,
		    dd->dd_phys->dd_child_dir_zapobj);
		    zap_cursor_retrieve(&zc, attr) == 0;
		    (void) zap_cursor_advance(&zc)) {
			ASSERT(attr->za_integer_length == sizeof (uint64_t));
			ASSERT(attr->za_num_integers == 1);

			/*
			 * No separating '/' because parent's name ends in /.
			 */
			child = kmem_alloc(MAXPATHLEN, KM_SLEEP);
			/* XXX could probably just use name here */
			dsl_dir_name(dd, child);
			(void) strcat(child, "/");
			(void) strcat(child, attr->za_name);
			err = dmu_objset_find(child, func, arg, flags);
			kmem_free(child, MAXPATHLEN);
			if (err)
				break;
		}
		zap_cursor_fini(&zc);

		if (err) {
			dsl_dir_close(dd, FTAG);
			kmem_free(attr, sizeof (zap_attribute_t));
			return (err);
		}
	}

	/*
	 * Iterate over all snapshots.
	 */
	if ((flags & DS_FIND_SNAPSHOTS) &&
	    dmu_objset_open(name, DMU_OST_ANY,
	    DS_MODE_STANDARD | DS_MODE_READONLY, &os) == 0) {

		snapobj = os->os->os_dsl_dataset->ds_phys->ds_snapnames_zapobj;
		dmu_objset_close(os);

		for (zap_cursor_init(&zc, dd->dd_pool->dp_meta_objset, snapobj);
		    zap_cursor_retrieve(&zc, attr) == 0;
		    (void) zap_cursor_advance(&zc)) {
			ASSERT(attr->za_integer_length == sizeof (uint64_t));
			ASSERT(attr->za_num_integers == 1);

			child = kmem_alloc(MAXPATHLEN, KM_SLEEP);
			/* XXX could probably just use name here */
			dsl_dir_name(dd, child);
			(void) strcat(child, "@");
			(void) strcat(child, attr->za_name);
			err = func(child, arg);
			kmem_free(child, MAXPATHLEN);
			if (err)
				break;
		}
		zap_cursor_fini(&zc);
	}

	dsl_dir_close(dd, FTAG);
	kmem_free(attr, sizeof (zap_attribute_t));

	if (err)
		return (err);

	/*
	 * Apply to self if appropriate.
	 */
	if (do_self)
		err = func(name, arg);
	return (err);
}
