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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"


/*
 * This file contains the top half of the zfs directory structure
 * implementation. The bottom half is in zap_leaf.c.
 *
 * The zdir is an extendable hash data structure. There is a table of
 * pointers to buckets (zap_t->zd_data->zd_leafs). The buckets are
 * each a constant size and hold a variable number of directory entries.
 * The buckets (aka "leaf nodes") are implemented in zap_leaf.c.
 *
 * The pointer table holds a power of 2 number of pointers.
 * (1<<zap_t->zd_data->zd_phys->zd_prefix_len).  The bucket pointed to
 * by the pointer at index i in the table holds entries whose hash value
 * has a zd_prefix_len - bit prefix
 */

#include <sys/spa.h>
#include <sys/dmu.h>
#include <sys/zfs_context.h>
#include <sys/zap.h>
#include <sys/refcount.h>
#include <sys/zap_impl.h>
#include <sys/zap_leaf.h>
#include <sys/zfs_znode.h>

int fzap_default_block_shift = 14; /* 16k blocksize */

static void zap_leaf_pageout(dmu_buf_t *db, void *vl);
static uint64_t zap_allocate_blocks(zap_t *zap, int nblocks);


void
fzap_byteswap(void *vbuf, size_t size)
{
	uint64_t block_type;

	block_type = *(uint64_t *)vbuf;

	if (block_type == ZBT_LEAF || block_type == BSWAP_64(ZBT_LEAF))
		zap_leaf_byteswap(vbuf, size);
	else {
		/* it's a ptrtbl block */
		byteswap_uint64_array(vbuf, size);
	}
}

void
fzap_upgrade(zap_t *zap, dmu_tx_t *tx)
{
	dmu_buf_t *db;
	zap_leaf_t *l;
	int i;
	zap_phys_t *zp;

	ASSERT(RW_WRITE_HELD(&zap->zap_rwlock));
	zap->zap_ismicro = FALSE;

	(void) dmu_buf_update_user(zap->zap_dbuf, zap, zap,
	    &zap->zap_f.zap_phys, zap_evict);

	mutex_init(&zap->zap_f.zap_num_entries_mtx, NULL, MUTEX_DEFAULT, 0);
	zap->zap_f.zap_block_shift = highbit(zap->zap_dbuf->db_size) - 1;

	zp = zap->zap_f.zap_phys;
	/*
	 * explicitly zero it since it might be coming from an
	 * initialized microzap
	 */
	bzero(zap->zap_dbuf->db_data, zap->zap_dbuf->db_size);
	zp->zap_block_type = ZBT_HEADER;
	zp->zap_magic = ZAP_MAGIC;

	zp->zap_ptrtbl.zt_shift = ZAP_EMBEDDED_PTRTBL_SHIFT(zap);

	zp->zap_freeblk = 2;		/* block 1 will be the first leaf */
	zp->zap_num_leafs = 1;
	zp->zap_num_entries = 0;
	zp->zap_salt = zap->zap_salt;

	/* block 1 will be the first leaf */
	for (i = 0; i < (1<<zp->zap_ptrtbl.zt_shift); i++)
		ZAP_EMBEDDED_PTRTBL_ENT(zap, i) = 1;

	/*
	 * set up block 1 - the first leaf
	 */
	VERIFY(0 == dmu_buf_hold(zap->zap_objset, zap->zap_object,
	    1<<FZAP_BLOCK_SHIFT(zap), FTAG, &db));
	dmu_buf_will_dirty(db, tx);

	l = kmem_zalloc(sizeof (zap_leaf_t), KM_SLEEP);
	l->l_dbuf = db;
	l->l_phys = db->db_data;

	zap_leaf_init(l);

	kmem_free(l, sizeof (zap_leaf_t));
	dmu_buf_rele(db, FTAG);
}

static int
zap_tryupgradedir(zap_t *zap, dmu_tx_t *tx)
{
	if (RW_WRITE_HELD(&zap->zap_rwlock))
		return (1);
	if (rw_tryupgrade(&zap->zap_rwlock)) {
		dmu_buf_will_dirty(zap->zap_dbuf, tx);
		return (1);
	}
	return (0);
}

/*
 * Generic routines for dealing with the pointer & cookie tables.
 */

static int
zap_table_grow(zap_t *zap, zap_table_phys_t *tbl,
    void (*transfer_func)(const uint64_t *src, uint64_t *dst, int n),
    dmu_tx_t *tx)
{
	uint64_t b, newblk;
	dmu_buf_t *db_old, *db_new;
	int err;
	int bs = FZAP_BLOCK_SHIFT(zap);
	int hepb = 1<<(bs-4);
	/* hepb = half the number of entries in a block */

	ASSERT(RW_WRITE_HELD(&zap->zap_rwlock));
	ASSERT(tbl->zt_blk != 0);
	ASSERT(tbl->zt_numblks > 0);

	if (tbl->zt_nextblk != 0) {
		newblk = tbl->zt_nextblk;
	} else {
		newblk = zap_allocate_blocks(zap, tbl->zt_numblks * 2);
		tbl->zt_nextblk = newblk;
		ASSERT3U(tbl->zt_blks_copied, ==, 0);
		dmu_prefetch(zap->zap_objset, zap->zap_object,
		    tbl->zt_blk << bs, tbl->zt_numblks << bs);
	}

	/*
	 * Copy the ptrtbl from the old to new location.
	 */

	b = tbl->zt_blks_copied;
	err = dmu_buf_hold(zap->zap_objset, zap->zap_object,
	    (tbl->zt_blk + b) << bs, FTAG, &db_old);
	if (err)
		return (err);

	/* first half of entries in old[b] go to new[2*b+0] */
	VERIFY(0 == dmu_buf_hold(zap->zap_objset, zap->zap_object,
	    (newblk + 2*b+0) << bs, FTAG, &db_new));
	dmu_buf_will_dirty(db_new, tx);
	transfer_func(db_old->db_data, db_new->db_data, hepb);
	dmu_buf_rele(db_new, FTAG);

	/* second half of entries in old[b] go to new[2*b+1] */
	VERIFY(0 == dmu_buf_hold(zap->zap_objset, zap->zap_object,
	    (newblk + 2*b+1) << bs, FTAG, &db_new));
	dmu_buf_will_dirty(db_new, tx);
	transfer_func((uint64_t *)db_old->db_data + hepb,
	    db_new->db_data, hepb);
	dmu_buf_rele(db_new, FTAG);

	dmu_buf_rele(db_old, FTAG);

	tbl->zt_blks_copied++;

	dprintf("copied block %llu of %llu\n",
	    tbl->zt_blks_copied, tbl->zt_numblks);

	if (tbl->zt_blks_copied == tbl->zt_numblks) {
		(void) dmu_free_range(zap->zap_objset, zap->zap_object,
		    tbl->zt_blk << bs, tbl->zt_numblks << bs, tx);

		tbl->zt_blk = newblk;
		tbl->zt_numblks *= 2;
		tbl->zt_shift++;
		tbl->zt_nextblk = 0;
		tbl->zt_blks_copied = 0;

		dprintf("finished; numblocks now %llu (%lluk entries)\n",
		    tbl->zt_numblks, 1<<(tbl->zt_shift-10));
	}

	return (0);
}

static int
zap_table_store(zap_t *zap, zap_table_phys_t *tbl, uint64_t idx, uint64_t val,
    dmu_tx_t *tx)
{
	int err;
	uint64_t blk, off;
	int bs = FZAP_BLOCK_SHIFT(zap);
	dmu_buf_t *db;

	ASSERT(RW_LOCK_HELD(&zap->zap_rwlock));
	ASSERT(tbl->zt_blk != 0);

	dprintf("storing %llx at index %llx\n", val, idx);

	blk = idx >> (bs-3);
	off = idx & ((1<<(bs-3))-1);

	err = dmu_buf_hold(zap->zap_objset, zap->zap_object,
	    (tbl->zt_blk + blk) << bs, FTAG, &db);
	if (err)
		return (err);
	dmu_buf_will_dirty(db, tx);

	if (tbl->zt_nextblk != 0) {
		uint64_t idx2 = idx * 2;
		uint64_t blk2 = idx2 >> (bs-3);
		uint64_t off2 = idx2 & ((1<<(bs-3))-1);
		dmu_buf_t *db2;

		err = dmu_buf_hold(zap->zap_objset, zap->zap_object,
		    (tbl->zt_nextblk + blk2) << bs, FTAG, &db2);
		if (err) {
			dmu_buf_rele(db, FTAG);
			return (err);
		}
		dmu_buf_will_dirty(db2, tx);
		((uint64_t *)db2->db_data)[off2] = val;
		((uint64_t *)db2->db_data)[off2+1] = val;
		dmu_buf_rele(db2, FTAG);
	}

	((uint64_t *)db->db_data)[off] = val;
	dmu_buf_rele(db, FTAG);

	return (0);
}

static int
zap_table_load(zap_t *zap, zap_table_phys_t *tbl, uint64_t idx, uint64_t *valp)
{
	uint64_t blk, off;
	int err;
	dmu_buf_t *db;
	int bs = FZAP_BLOCK_SHIFT(zap);

	ASSERT(RW_LOCK_HELD(&zap->zap_rwlock));

	blk = idx >> (bs-3);
	off = idx & ((1<<(bs-3))-1);

	err = dmu_buf_hold(zap->zap_objset, zap->zap_object,
	    (tbl->zt_blk + blk) << bs, FTAG, &db);
	if (err)
		return (err);
	*valp = ((uint64_t *)db->db_data)[off];
	dmu_buf_rele(db, FTAG);

	if (tbl->zt_nextblk != 0) {
		/*
		 * read the nextblk for the sake of i/o error checking,
		 * so that zap_table_load() will catch errors for
		 * zap_table_store.
		 */
		blk = (idx*2) >> (bs-3);

		err = dmu_buf_hold(zap->zap_objset, zap->zap_object,
		    (tbl->zt_nextblk + blk) << bs, FTAG, &db);
		dmu_buf_rele(db, FTAG);
	}
	return (err);
}

/*
 * Routines for growing the ptrtbl.
 */

static void
zap_ptrtbl_transfer(const uint64_t *src, uint64_t *dst, int n)
{
	int i;
	for (i = 0; i < n; i++) {
		uint64_t lb = src[i];
		dst[2*i+0] = lb;
		dst[2*i+1] = lb;
	}
}

static int
zap_grow_ptrtbl(zap_t *zap, dmu_tx_t *tx)
{
	/* In case things go horribly wrong. */
	if (zap->zap_f.zap_phys->zap_ptrtbl.zt_shift >= ZAP_HASHBITS-2)
		return (ENOSPC);

	if (zap->zap_f.zap_phys->zap_ptrtbl.zt_numblks == 0) {
		/*
		 * We are outgrowing the "embedded" ptrtbl (the one
		 * stored in the header block).  Give it its own entire
		 * block, which will double the size of the ptrtbl.
		 */
		uint64_t newblk;
		dmu_buf_t *db_new;
		int err;

		ASSERT3U(zap->zap_f.zap_phys->zap_ptrtbl.zt_shift, ==,
		    ZAP_EMBEDDED_PTRTBL_SHIFT(zap));
		ASSERT3U(zap->zap_f.zap_phys->zap_ptrtbl.zt_blk, ==, 0);

		newblk = zap_allocate_blocks(zap, 1);
		err = dmu_buf_hold(zap->zap_objset, zap->zap_object,
		    newblk << FZAP_BLOCK_SHIFT(zap), FTAG, &db_new);
		if (err)
			return (err);
		dmu_buf_will_dirty(db_new, tx);
		zap_ptrtbl_transfer(&ZAP_EMBEDDED_PTRTBL_ENT(zap, 0),
		    db_new->db_data, 1 << ZAP_EMBEDDED_PTRTBL_SHIFT(zap));
		dmu_buf_rele(db_new, FTAG);

		zap->zap_f.zap_phys->zap_ptrtbl.zt_blk = newblk;
		zap->zap_f.zap_phys->zap_ptrtbl.zt_numblks = 1;
		zap->zap_f.zap_phys->zap_ptrtbl.zt_shift++;

		ASSERT3U(1ULL << zap->zap_f.zap_phys->zap_ptrtbl.zt_shift, ==,
		    zap->zap_f.zap_phys->zap_ptrtbl.zt_numblks <<
		    (FZAP_BLOCK_SHIFT(zap)-3));

		return (0);
	} else {
		return (zap_table_grow(zap, &zap->zap_f.zap_phys->zap_ptrtbl,
		    zap_ptrtbl_transfer, tx));
	}
}

static void
zap_increment_num_entries(zap_t *zap, int delta, dmu_tx_t *tx)
{
	dmu_buf_will_dirty(zap->zap_dbuf, tx);
	mutex_enter(&zap->zap_f.zap_num_entries_mtx);
	ASSERT(delta > 0 || zap->zap_f.zap_phys->zap_num_entries >= -delta);
	zap->zap_f.zap_phys->zap_num_entries += delta;
	mutex_exit(&zap->zap_f.zap_num_entries_mtx);
}

static uint64_t
zap_allocate_blocks(zap_t *zap, int nblocks)
{
	uint64_t newblk;
	ASSERT(RW_WRITE_HELD(&zap->zap_rwlock));
	newblk = zap->zap_f.zap_phys->zap_freeblk;
	zap->zap_f.zap_phys->zap_freeblk += nblocks;
	return (newblk);
}

static zap_leaf_t *
zap_create_leaf(zap_t *zap, dmu_tx_t *tx)
{
	void *winner;
	zap_leaf_t *l = kmem_alloc(sizeof (zap_leaf_t), KM_SLEEP);

	ASSERT(RW_WRITE_HELD(&zap->zap_rwlock));

	rw_init(&l->l_rwlock, NULL, RW_DEFAULT, 0);
	rw_enter(&l->l_rwlock, RW_WRITER);
	l->l_blkid = zap_allocate_blocks(zap, 1);
	l->l_dbuf = NULL;
	l->l_phys = NULL;

	VERIFY(0 == dmu_buf_hold(zap->zap_objset, zap->zap_object,
	    l->l_blkid << FZAP_BLOCK_SHIFT(zap), NULL, &l->l_dbuf));
	winner = dmu_buf_set_user(l->l_dbuf, l, &l->l_phys, zap_leaf_pageout);
	ASSERT(winner == NULL);
	dmu_buf_will_dirty(l->l_dbuf, tx);

	zap_leaf_init(l);

	zap->zap_f.zap_phys->zap_num_leafs++;

	return (l);
}

int
fzap_count(zap_t *zap, uint64_t *count)
{
	ASSERT(!zap->zap_ismicro);
	mutex_enter(&zap->zap_f.zap_num_entries_mtx); /* unnecessary */
	*count = zap->zap_f.zap_phys->zap_num_entries;
	mutex_exit(&zap->zap_f.zap_num_entries_mtx);
	return (0);
}

/*
 * Routines for obtaining zap_leaf_t's
 */

void
zap_put_leaf(zap_leaf_t *l)
{
	rw_exit(&l->l_rwlock);
	dmu_buf_rele(l->l_dbuf, NULL);
}

_NOTE(ARGSUSED(0))
static void
zap_leaf_pageout(dmu_buf_t *db, void *vl)
{
	zap_leaf_t *l = vl;

	rw_destroy(&l->l_rwlock);
	kmem_free(l, sizeof (zap_leaf_t));
}

static zap_leaf_t *
zap_open_leaf(uint64_t blkid, dmu_buf_t *db)
{
	zap_leaf_t *l, *winner;

	ASSERT(blkid != 0);

	l = kmem_alloc(sizeof (zap_leaf_t), KM_SLEEP);
	rw_init(&l->l_rwlock, NULL, RW_DEFAULT, 0);
	rw_enter(&l->l_rwlock, RW_WRITER);
	l->l_blkid = blkid;
	l->l_bs = highbit(db->db_size)-1;
	l->l_dbuf = db;
	l->l_phys = NULL;

	winner = dmu_buf_set_user(db, l, &l->l_phys, zap_leaf_pageout);

	rw_exit(&l->l_rwlock);
	if (winner != NULL) {
		/* someone else set it first */
		zap_leaf_pageout(NULL, l);
		l = winner;
	}

	/*
	 * lhr_pad was previously used for the next leaf in the leaf
	 * chain.  There should be no chained leafs (as we have removed
	 * support for them).
	 */
	ASSERT3U(l->l_phys->l_hdr.lh_pad1, ==, 0);

	/*
	 * There should be more hash entries than there can be
	 * chunks to put in the hash table
	 */
	ASSERT3U(ZAP_LEAF_HASH_NUMENTRIES(l), >, ZAP_LEAF_NUMCHUNKS(l) / 3);

	/* The chunks should begin at the end of the hash table */
	ASSERT3P(&ZAP_LEAF_CHUNK(l, 0), ==,
	    &l->l_phys->l_hash[ZAP_LEAF_HASH_NUMENTRIES(l)]);

	/* The chunks should end at the end of the block */
	ASSERT3U((uintptr_t)&ZAP_LEAF_CHUNK(l, ZAP_LEAF_NUMCHUNKS(l)) -
	    (uintptr_t)l->l_phys, ==, l->l_dbuf->db_size);

	return (l);
}

static int
zap_get_leaf_byblk(zap_t *zap, uint64_t blkid, dmu_tx_t *tx, krw_t lt,
    zap_leaf_t **lp)
{
	dmu_buf_t *db;
	zap_leaf_t *l;
	int bs = FZAP_BLOCK_SHIFT(zap);
	int err;

	ASSERT(RW_LOCK_HELD(&zap->zap_rwlock));

	err = dmu_buf_hold(zap->zap_objset, zap->zap_object,
	    blkid << bs, NULL, &db);
	if (err)
		return (err);

	ASSERT3U(db->db_object, ==, zap->zap_object);
	ASSERT3U(db->db_offset, ==, blkid << bs);
	ASSERT3U(db->db_size, ==, 1 << bs);
	ASSERT(blkid != 0);

	l = dmu_buf_get_user(db);

	if (l == NULL)
		l = zap_open_leaf(blkid, db);

	rw_enter(&l->l_rwlock, lt);
	/*
	 * Must lock before dirtying, otherwise l->l_phys could change,
	 * causing ASSERT below to fail.
	 */
	if (lt == RW_WRITER)
		dmu_buf_will_dirty(db, tx);
	ASSERT3U(l->l_blkid, ==, blkid);
	ASSERT3P(l->l_dbuf, ==, db);
	ASSERT3P(l->l_phys, ==, l->l_dbuf->db_data);
	ASSERT3U(l->l_phys->l_hdr.lh_block_type, ==, ZBT_LEAF);
	ASSERT3U(l->l_phys->l_hdr.lh_magic, ==, ZAP_LEAF_MAGIC);

	*lp = l;
	return (0);
}

static int
zap_idx_to_blk(zap_t *zap, uint64_t idx, uint64_t *valp)
{
	ASSERT(RW_LOCK_HELD(&zap->zap_rwlock));

	if (zap->zap_f.zap_phys->zap_ptrtbl.zt_numblks == 0) {
		ASSERT3U(idx, <,
		    (1ULL << zap->zap_f.zap_phys->zap_ptrtbl.zt_shift));
		*valp = ZAP_EMBEDDED_PTRTBL_ENT(zap, idx);
		return (0);
	} else {
		return (zap_table_load(zap, &zap->zap_f.zap_phys->zap_ptrtbl,
		    idx, valp));
	}
}

static int
zap_set_idx_to_blk(zap_t *zap, uint64_t idx, uint64_t blk, dmu_tx_t *tx)
{
	ASSERT(tx != NULL);
	ASSERT(RW_WRITE_HELD(&zap->zap_rwlock));

	if (zap->zap_f.zap_phys->zap_ptrtbl.zt_blk == 0) {
		ZAP_EMBEDDED_PTRTBL_ENT(zap, idx) = blk;
		return (0);
	} else {
		return (zap_table_store(zap, &zap->zap_f.zap_phys->zap_ptrtbl,
		    idx, blk, tx));
	}
}

static int
zap_deref_leaf(zap_t *zap, uint64_t h, dmu_tx_t *tx, krw_t lt, zap_leaf_t **lp)
{
	uint64_t idx, blk;
	int err;

	ASSERT(zap->zap_dbuf == NULL ||
	    zap->zap_f.zap_phys == zap->zap_dbuf->db_data);
	ASSERT3U(zap->zap_f.zap_phys->zap_magic, ==, ZAP_MAGIC);
	idx = ZAP_HASH_IDX(h, zap->zap_f.zap_phys->zap_ptrtbl.zt_shift);
	err = zap_idx_to_blk(zap, idx, &blk);
	if (err != 0)
		return (err);
	err = zap_get_leaf_byblk(zap, blk, tx, lt, lp);

	ASSERT(err || ZAP_HASH_IDX(h, (*lp)->l_phys->l_hdr.lh_prefix_len) ==
	    (*lp)->l_phys->l_hdr.lh_prefix);
	return (err);
}

static int
zap_expand_leaf(zap_t *zap, zap_leaf_t *l, uint64_t hash, dmu_tx_t *tx,
    zap_leaf_t **lp)
{
	zap_leaf_t *nl;
	int prefix_diff, i, err;
	uint64_t sibling;
	int old_prefix_len = l->l_phys->l_hdr.lh_prefix_len;

	ASSERT3U(old_prefix_len, <=, zap->zap_f.zap_phys->zap_ptrtbl.zt_shift);
	ASSERT(RW_LOCK_HELD(&zap->zap_rwlock));

	ASSERT3U(ZAP_HASH_IDX(hash, old_prefix_len), ==,
	    l->l_phys->l_hdr.lh_prefix);

	if (zap_tryupgradedir(zap, tx) == 0 ||
	    old_prefix_len == zap->zap_f.zap_phys->zap_ptrtbl.zt_shift) {
		/* We failed to upgrade, or need to grow the pointer table */
		objset_t *os = zap->zap_objset;
		uint64_t object = zap->zap_object;

		zap_put_leaf(l);
		zap_unlockdir(zap);
		err = zap_lockdir(os, object, tx, RW_WRITER, FALSE, &zap);
		if (err)
			return (err);
		ASSERT(!zap->zap_ismicro);

		while (old_prefix_len ==
		    zap->zap_f.zap_phys->zap_ptrtbl.zt_shift) {
			err = zap_grow_ptrtbl(zap, tx);
			if (err)
				return (err);
		}

		err = zap_deref_leaf(zap, hash, tx, RW_WRITER, &l);
		if (err)
			return (err);

		if (l->l_phys->l_hdr.lh_prefix_len != old_prefix_len) {
			/* it split while our locks were down */
			*lp = l;
			return (0);
		}
	}
	ASSERT(RW_WRITE_HELD(&zap->zap_rwlock));
	ASSERT3U(old_prefix_len, <, zap->zap_f.zap_phys->zap_ptrtbl.zt_shift);
	ASSERT3U(ZAP_HASH_IDX(hash, old_prefix_len), ==,
	    l->l_phys->l_hdr.lh_prefix);

	prefix_diff = zap->zap_f.zap_phys->zap_ptrtbl.zt_shift -
	    (old_prefix_len + 1);
	sibling = (ZAP_HASH_IDX(hash, old_prefix_len + 1) | 1) << prefix_diff;

	/* check for i/o errors before doing zap_leaf_split */
	for (i = 0; i < (1ULL<<prefix_diff); i++) {
		uint64_t blk;
		err = zap_idx_to_blk(zap, sibling+i, &blk);
		if (err)
			return (err);
		ASSERT3U(blk, ==, l->l_blkid);
	}

	nl = zap_create_leaf(zap, tx);
	zap_leaf_split(l, nl);

	/* set sibling pointers */
	for (i = 0; i < (1ULL<<prefix_diff); i++) {
		err = zap_set_idx_to_blk(zap, sibling+i, nl->l_blkid, tx);
		ASSERT3U(err, ==, 0); /* we checked for i/o errors above */
	}

	if (hash & (1ULL << (64 - l->l_phys->l_hdr.lh_prefix_len))) {
		/* we want the sibling */
		zap_put_leaf(l);
		*lp = nl;
	} else {
		zap_put_leaf(nl);
		*lp = l;
	}

	return (0);
}

static void
zap_put_leaf_maybe_grow_ptrtbl(zap_t *zap, zap_leaf_t *l, dmu_tx_t *tx)
{
	int shift = zap->zap_f.zap_phys->zap_ptrtbl.zt_shift;
	int leaffull = (l->l_phys->l_hdr.lh_prefix_len == shift &&
	    l->l_phys->l_hdr.lh_nfree < ZAP_LEAF_LOW_WATER);

	zap_put_leaf(l);

	if (leaffull || zap->zap_f.zap_phys->zap_ptrtbl.zt_nextblk) {
		int err;

		/*
		 * We are in the middle of growing the pointer table, or
		 * this leaf will soon make us grow it.
		 */
		if (zap_tryupgradedir(zap, tx) == 0) {
			objset_t *os = zap->zap_objset;
			uint64_t zapobj = zap->zap_object;

			zap_unlockdir(zap);
			err = zap_lockdir(os, zapobj, tx,
			    RW_WRITER, FALSE, &zap);
			if (err)
				return;
		}

		/* could have finished growing while our locks were down */
		if (zap->zap_f.zap_phys->zap_ptrtbl.zt_shift == shift)
			(void) zap_grow_ptrtbl(zap, tx);
	}
}


static int
fzap_checksize(const char *name, uint64_t integer_size, uint64_t num_integers)
{
	if (name && strlen(name) > ZAP_MAXNAMELEN)
		return (E2BIG);

	/* Only integer sizes supported by C */
	switch (integer_size) {
	case 1:
	case 2:
	case 4:
	case 8:
		break;
	default:
		return (EINVAL);
	}

	if (integer_size * num_integers > ZAP_MAXVALUELEN)
		return (E2BIG);

	return (0);
}

/*
 * Routines for maniplulating attributes.
 */
int
fzap_lookup(zap_t *zap, const char *name,
    uint64_t integer_size, uint64_t num_integers, void *buf)
{
	zap_leaf_t *l;
	int err;
	uint64_t hash;
	zap_entry_handle_t zeh;

	err = fzap_checksize(name, integer_size, num_integers);
	if (err != 0)
		return (err);

	hash = zap_hash(zap, name);
	err = zap_deref_leaf(zap, hash, NULL, RW_READER, &l);
	if (err != 0)
		return (err);
	err = zap_leaf_lookup(l, name, hash, &zeh);
	if (err == 0)
		err = zap_entry_read(&zeh, integer_size, num_integers, buf);

	zap_put_leaf(l);
	return (err);
}

int
fzap_add_cd(zap_t *zap, const char *name,
    uint64_t integer_size, uint64_t num_integers,
    const void *val, uint32_t cd, dmu_tx_t *tx)
{
	zap_leaf_t *l;
	uint64_t hash;
	int err;
	zap_entry_handle_t zeh;

	ASSERT(RW_LOCK_HELD(&zap->zap_rwlock));
	ASSERT(!zap->zap_ismicro);
	ASSERT(fzap_checksize(name, integer_size, num_integers) == 0);

	hash = zap_hash(zap, name);
	err = zap_deref_leaf(zap, hash, tx, RW_WRITER, &l);
	if (err != 0)
		return (err);
retry:
	err = zap_leaf_lookup(l, name, hash, &zeh);
	if (err == 0) {
		err = EEXIST;
		goto out;
	}
	if (err != ENOENT)
		goto out;

	err = zap_entry_create(l, name, hash, cd,
	    integer_size, num_integers, val, &zeh);

	if (err == 0) {
		zap_increment_num_entries(zap, 1, tx);
	} else if (err == EAGAIN) {
		err = zap_expand_leaf(zap, l, hash, tx, &l);
		if (err == 0)
			goto retry;
	}

out:
	zap_put_leaf_maybe_grow_ptrtbl(zap, l, tx);
	return (err);
}

int
fzap_add(zap_t *zap, const char *name,
    uint64_t integer_size, uint64_t num_integers,
    const void *val, dmu_tx_t *tx)
{
	int err = fzap_checksize(name, integer_size, num_integers);
	if (err != 0)
		return (err);

	return (fzap_add_cd(zap, name, integer_size, num_integers,
	    val, ZAP_MAXCD, tx));
}

int
fzap_update(zap_t *zap, const char *name,
    int integer_size, uint64_t num_integers, const void *val, dmu_tx_t *tx)
{
	zap_leaf_t *l;
	uint64_t hash;
	int err, create;
	zap_entry_handle_t zeh;

	ASSERT(RW_LOCK_HELD(&zap->zap_rwlock));
	err = fzap_checksize(name, integer_size, num_integers);
	if (err != 0)
		return (err);

	hash = zap_hash(zap, name);
	err = zap_deref_leaf(zap, hash, tx, RW_WRITER, &l);
	if (err != 0)
		return (err);
retry:
	err = zap_leaf_lookup(l, name, hash, &zeh);
	create = (err == ENOENT);
	ASSERT(err == 0 || err == ENOENT);

	/* XXX If this leaf is chained, split it if we can. */

	if (create) {
		err = zap_entry_create(l, name, hash, ZAP_MAXCD,
		    integer_size, num_integers, val, &zeh);
		if (err == 0)
			zap_increment_num_entries(zap, 1, tx);
	} else {
		err = zap_entry_update(&zeh, integer_size, num_integers, val);
	}

	if (err == EAGAIN) {
		err = zap_expand_leaf(zap, l, hash, tx, &l);
		if (err == 0)
			goto retry;
	}

	zap_put_leaf_maybe_grow_ptrtbl(zap, l, tx);
	return (err);
}

int
fzap_length(zap_t *zap, const char *name,
    uint64_t *integer_size, uint64_t *num_integers)
{
	zap_leaf_t *l;
	int err;
	uint64_t hash;
	zap_entry_handle_t zeh;

	hash = zap_hash(zap, name);
	err = zap_deref_leaf(zap, hash, NULL, RW_READER, &l);
	if (err != 0)
		return (err);
	err = zap_leaf_lookup(l, name, hash, &zeh);
	if (err != 0)
		goto out;

	if (integer_size)
		*integer_size = zeh.zeh_integer_size;
	if (num_integers)
		*num_integers = zeh.zeh_num_integers;
out:
	zap_put_leaf(l);
	return (err);
}

int
fzap_remove(zap_t *zap, const char *name, dmu_tx_t *tx)
{
	zap_leaf_t *l;
	uint64_t hash;
	int err;
	zap_entry_handle_t zeh;

	hash = zap_hash(zap, name);
	err = zap_deref_leaf(zap, hash, tx, RW_WRITER, &l);
	if (err != 0)
		return (err);
	err = zap_leaf_lookup(l, name, hash, &zeh);
	if (err == 0) {
		zap_entry_remove(&zeh);
		zap_increment_num_entries(zap, -1, tx);
	}
	zap_put_leaf(l);
	dprintf("fzap_remove: ds=%p obj=%llu name=%s err=%d\n",
	    zap->zap_objset, zap->zap_object, name, err);
	return (err);
}

int
zap_value_search(objset_t *os, uint64_t zapobj, uint64_t value, char *name)
{
	zap_cursor_t zc;
	zap_attribute_t *za;
	int err;

	za = kmem_alloc(sizeof (zap_attribute_t), KM_SLEEP);
	for (zap_cursor_init(&zc, os, zapobj);
	    (err = zap_cursor_retrieve(&zc, za)) == 0;
	    zap_cursor_advance(&zc)) {
		if (ZFS_DIRENT_OBJ(za->za_first_integer) == value) {
			(void) strcpy(name, za->za_name);
			break;
		}
	}
	zap_cursor_fini(&zc);
	kmem_free(za, sizeof (zap_attribute_t));
	return (err);
}


/*
 * Routines for iterating over the attributes.
 */

int
fzap_cursor_retrieve(zap_t *zap, zap_cursor_t *zc, zap_attribute_t *za)
{
	int err = ENOENT;
	zap_entry_handle_t zeh;
	zap_leaf_t *l;

	/* retrieve the next entry at or after zc_hash/zc_cd */
	/* if no entry, return ENOENT */

	if (zc->zc_leaf &&
	    (ZAP_HASH_IDX(zc->zc_hash,
	    zc->zc_leaf->l_phys->l_hdr.lh_prefix_len) !=
	    zc->zc_leaf->l_phys->l_hdr.lh_prefix)) {
		rw_enter(&zc->zc_leaf->l_rwlock, RW_READER);
		zap_put_leaf(zc->zc_leaf);
		zc->zc_leaf = NULL;
	}

again:
	if (zc->zc_leaf == NULL) {
		err = zap_deref_leaf(zap, zc->zc_hash, NULL, RW_READER,
		    &zc->zc_leaf);
		if (err != 0)
			return (err);
	} else {
		rw_enter(&zc->zc_leaf->l_rwlock, RW_READER);
	}
	l = zc->zc_leaf;

	err = zap_leaf_lookup_closest(l, zc->zc_hash, zc->zc_cd, &zeh);

	if (err == ENOENT) {
		uint64_t nocare =
		    (1ULL << (64 - l->l_phys->l_hdr.lh_prefix_len)) - 1;
		zc->zc_hash = (zc->zc_hash & ~nocare) + nocare + 1;
		zc->zc_cd = 0;
		if (l->l_phys->l_hdr.lh_prefix_len == 0 || zc->zc_hash == 0) {
			zc->zc_hash = -1ULL;
		} else {
			zap_put_leaf(zc->zc_leaf);
			zc->zc_leaf = NULL;
			goto again;
		}
	}

	if (err == 0) {
		zc->zc_hash = zeh.zeh_hash;
		zc->zc_cd = zeh.zeh_cd;
		za->za_integer_length = zeh.zeh_integer_size;
		za->za_num_integers = zeh.zeh_num_integers;
		if (zeh.zeh_num_integers == 0) {
			za->za_first_integer = 0;
		} else {
			err = zap_entry_read(&zeh, 8, 1, &za->za_first_integer);
			ASSERT(err == 0 || err == EOVERFLOW);
		}
		err = zap_entry_read_name(&zeh,
		    sizeof (za->za_name), za->za_name);
		ASSERT(err == 0);
	}
	rw_exit(&zc->zc_leaf->l_rwlock);
	return (err);
}


static void
zap_stats_ptrtbl(zap_t *zap, uint64_t *tbl, int len, zap_stats_t *zs)
{
	int i, err;
	uint64_t lastblk = 0;

	/*
	 * NB: if a leaf has more pointers than an entire ptrtbl block
	 * can hold, then it'll be accounted for more than once, since
	 * we won't have lastblk.
	 */
	for (i = 0; i < len; i++) {
		zap_leaf_t *l;

		if (tbl[i] == lastblk)
			continue;
		lastblk = tbl[i];

		err = zap_get_leaf_byblk(zap, tbl[i], NULL, RW_READER, &l);
		if (err == 0) {
			zap_leaf_stats(zap, l, zs);
			zap_put_leaf(l);
		}
	}
}

void
fzap_get_stats(zap_t *zap, zap_stats_t *zs)
{
	int bs = FZAP_BLOCK_SHIFT(zap);
	zs->zs_blocksize = 1ULL << bs;

	/*
	 * Set zap_phys_t fields
	 */
	zs->zs_num_leafs = zap->zap_f.zap_phys->zap_num_leafs;
	zs->zs_num_entries = zap->zap_f.zap_phys->zap_num_entries;
	zs->zs_num_blocks = zap->zap_f.zap_phys->zap_freeblk;
	zs->zs_block_type = zap->zap_f.zap_phys->zap_block_type;
	zs->zs_magic = zap->zap_f.zap_phys->zap_magic;
	zs->zs_salt = zap->zap_f.zap_phys->zap_salt;

	/*
	 * Set zap_ptrtbl fields
	 */
	zs->zs_ptrtbl_len = 1ULL << zap->zap_f.zap_phys->zap_ptrtbl.zt_shift;
	zs->zs_ptrtbl_nextblk = zap->zap_f.zap_phys->zap_ptrtbl.zt_nextblk;
	zs->zs_ptrtbl_blks_copied =
	    zap->zap_f.zap_phys->zap_ptrtbl.zt_blks_copied;
	zs->zs_ptrtbl_zt_blk = zap->zap_f.zap_phys->zap_ptrtbl.zt_blk;
	zs->zs_ptrtbl_zt_numblks = zap->zap_f.zap_phys->zap_ptrtbl.zt_numblks;
	zs->zs_ptrtbl_zt_shift = zap->zap_f.zap_phys->zap_ptrtbl.zt_shift;

	if (zap->zap_f.zap_phys->zap_ptrtbl.zt_numblks == 0) {
		/* the ptrtbl is entirely in the header block. */
		zap_stats_ptrtbl(zap, &ZAP_EMBEDDED_PTRTBL_ENT(zap, 0),
		    1 << ZAP_EMBEDDED_PTRTBL_SHIFT(zap), zs);
	} else {
		int b;

		dmu_prefetch(zap->zap_objset, zap->zap_object,
		    zap->zap_f.zap_phys->zap_ptrtbl.zt_blk << bs,
		    zap->zap_f.zap_phys->zap_ptrtbl.zt_numblks << bs);

		for (b = 0; b < zap->zap_f.zap_phys->zap_ptrtbl.zt_numblks;
		    b++) {
			dmu_buf_t *db;
			int err;

			err = dmu_buf_hold(zap->zap_objset, zap->zap_object,
			    (zap->zap_f.zap_phys->zap_ptrtbl.zt_blk + b) << bs,
			    FTAG, &db);
			if (err == 0) {
				zap_stats_ptrtbl(zap, db->db_data,
				    1<<(bs-3), zs);
				dmu_buf_rele(db, FTAG);
			}
		}
	}
}
