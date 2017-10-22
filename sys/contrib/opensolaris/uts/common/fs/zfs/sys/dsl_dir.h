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

#ifndef	_SYS_DSL_DIR_H
#define	_SYS_DSL_DIR_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/dmu.h>
#include <sys/dsl_pool.h>
#include <sys/dsl_synctask.h>
#include <sys/refcount.h>
#include <sys/zfs_context.h>

#ifdef	__cplusplus
extern "C" {
#endif

struct dsl_dataset;

typedef struct dsl_dir_phys {
	uint64_t dd_creation_time; /* not actually used */
	uint64_t dd_head_dataset_obj;
	uint64_t dd_parent_obj;
	uint64_t dd_clone_parent_obj;
	uint64_t dd_child_dir_zapobj;
	/*
	 * how much space our children are accounting for; for leaf
	 * datasets, == physical space used by fs + snaps
	 */
	uint64_t dd_used_bytes;
	uint64_t dd_compressed_bytes;
	uint64_t dd_uncompressed_bytes;
	/* Administrative quota setting */
	uint64_t dd_quota;
	/* Administrative reservation setting */
	uint64_t dd_reserved;
	uint64_t dd_props_zapobj;
	uint64_t dd_pad[21]; /* pad out to 256 bytes for good measure */
} dsl_dir_phys_t;

struct dsl_dir {
	/* These are immutable; no lock needed: */
	uint64_t dd_object;
	dsl_dir_phys_t *dd_phys;
	dmu_buf_t *dd_dbuf;
	dsl_pool_t *dd_pool;

	/* protected by lock on pool's dp_dirty_dirs list */
	txg_node_t dd_dirty_link;

	/* protected by dp_config_rwlock */
	dsl_dir_t *dd_parent;

	/* Protected by dd_lock */
	kmutex_t dd_lock;
	list_t dd_prop_cbs; /* list of dsl_prop_cb_record_t's */

	/* Accounting */
	/* reflects any changes to dd_phys->dd_used_bytes made this syncing */
	int64_t dd_used_bytes;
	/* gross estimate of space used by in-flight tx's */
	uint64_t dd_tempreserved[TXG_SIZE];
	/* amount of space we expect to write; == amount of dirty data */
	int64_t dd_space_towrite[TXG_SIZE];

	/* protected by dd_lock; keep at end of struct for better locality */
	char dd_myname[MAXNAMELEN];
};

void dsl_dir_close(dsl_dir_t *dd, void *tag);
int dsl_dir_open(const char *name, void *tag, dsl_dir_t **, const char **tail);
int dsl_dir_open_spa(spa_t *spa, const char *name, void *tag, dsl_dir_t **,
    const char **tailp);
int dsl_dir_open_obj(dsl_pool_t *dp, uint64_t ddobj,
    const char *tail, void *tag, dsl_dir_t **);
void dsl_dir_name(dsl_dir_t *dd, char *buf);
int dsl_dir_namelen(dsl_dir_t *dd);
int dsl_dir_is_private(dsl_dir_t *dd);
uint64_t dsl_dir_create_sync(dsl_dir_t *pds, const char *name, dmu_tx_t *tx);
void dsl_dir_create_root(objset_t *mos, uint64_t *ddobjp, dmu_tx_t *tx);
dsl_checkfunc_t dsl_dir_destroy_check;
dsl_syncfunc_t dsl_dir_destroy_sync;
void dsl_dir_stats(dsl_dir_t *dd, nvlist_t *nv);
uint64_t dsl_dir_space_available(dsl_dir_t *dd,
    dsl_dir_t *ancestor, int64_t delta, int ondiskonly);
void dsl_dir_dirty(dsl_dir_t *dd, dmu_tx_t *tx);
void dsl_dir_sync(dsl_dir_t *dd, dmu_tx_t *tx);
int dsl_dir_tempreserve_space(dsl_dir_t *dd, uint64_t mem,
    uint64_t asize, uint64_t fsize, void **tr_cookiep, dmu_tx_t *tx);
void dsl_dir_tempreserve_clear(void *tr_cookie, dmu_tx_t *tx);
void dsl_dir_willuse_space(dsl_dir_t *dd, int64_t space, dmu_tx_t *tx);
void dsl_dir_diduse_space(dsl_dir_t *dd,
    int64_t used, int64_t compressed, int64_t uncompressed, dmu_tx_t *tx);
int dsl_dir_set_quota(const char *ddname, uint64_t quota);
int dsl_dir_set_reservation(const char *ddname, uint64_t reservation);
int dsl_dir_rename(dsl_dir_t *dd, const char *newname);
int dsl_dir_transfer_possible(dsl_dir_t *sdd, dsl_dir_t *tdd, uint64_t space);

/* internal reserved dir name */
#define	MOS_DIR_NAME "$MOS"

#ifdef ZFS_DEBUG
#define	dprintf_dd(dd, fmt, ...) do { \
	if (zfs_flags & ZFS_DEBUG_DPRINTF) { \
	char *__ds_name = kmem_alloc(MAXNAMELEN + strlen(MOS_DIR_NAME) + 1, \
	    KM_SLEEP); \
	dsl_dir_name(dd, __ds_name); \
	dprintf("dd=%s " fmt, __ds_name, __VA_ARGS__); \
	kmem_free(__ds_name, MAXNAMELEN + strlen(MOS_DIR_NAME) + 1); \
	} \
_NOTE(CONSTCOND) } while (0)
#else
#define	dprintf_dd(dd, fmt, ...)
#endif

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_DSL_DIR_H */
