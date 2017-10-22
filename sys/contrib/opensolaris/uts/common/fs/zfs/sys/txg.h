/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
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
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _SYS_TXG_H
#define	_SYS_TXG_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/spa.h>
#include <sys/zfs_context.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	TXG_CONCURRENT_STATES	3	/* open, quiescing, syncing	*/
#define	TXG_SIZE		4		/* next power of 2	*/
#define	TXG_MASK		(TXG_SIZE - 1)	/* mask for size	*/
#define	TXG_INITIAL		TXG_SIZE	/* initial txg 		*/
#define	TXG_IDX			(txg & TXG_MASK)

#define	TXG_WAIT		1ULL
#define	TXG_NOWAIT		2ULL

typedef struct tx_cpu tx_cpu_t;

typedef struct txg_handle {
	tx_cpu_t	*th_cpu;
	uint64_t	th_txg;
} txg_handle_t;

typedef struct txg_node {
	struct txg_node	*tn_next[TXG_SIZE];
	uint8_t		tn_member[TXG_SIZE];
} txg_node_t;

typedef struct txg_list {
	kmutex_t	tl_lock;
	size_t		tl_offset;
	txg_node_t	*tl_head[TXG_SIZE];
} txg_list_t;

struct dsl_pool;

extern void txg_init(struct dsl_pool *dp, uint64_t txg);
extern void txg_fini(struct dsl_pool *dp);
extern void txg_sync_start(struct dsl_pool *dp);
extern void txg_sync_stop(struct dsl_pool *dp);
extern uint64_t txg_hold_open(struct dsl_pool *dp, txg_handle_t *txghp);
extern void txg_rele_to_quiesce(txg_handle_t *txghp);
extern void txg_rele_to_sync(txg_handle_t *txghp);
extern void txg_suspend(struct dsl_pool *dp);
extern void txg_resume(struct dsl_pool *dp);

/*
 * Wait until the given transaction group has finished syncing.
 * Try to make this happen as soon as possible (eg. kick off any
 * necessary syncs immediately).  If txg==0, wait for the currently open
 * txg to finish syncing.
 */
extern void txg_wait_synced(struct dsl_pool *dp, uint64_t txg);

/*
 * Wait until the given transaction group, or one after it, is
 * the open transaction group.  Try to make this happen as soon
 * as possible (eg. kick off any necessary syncs immediately).
 * If txg == 0, wait for the next open txg.
 */
extern void txg_wait_open(struct dsl_pool *dp, uint64_t txg);

/*
 * Returns TRUE if we are "backed up" waiting for the syncing
 * transaction to complete; otherwise returns FALSE.
 */
extern int txg_stalled(struct dsl_pool *dp);

/*
 * Per-txg object lists.
 */

#define	TXG_CLEAN(txg)	((txg) - 1)

extern void txg_list_create(txg_list_t *tl, size_t offset);
extern void txg_list_destroy(txg_list_t *tl);
extern int txg_list_empty(txg_list_t *tl, uint64_t txg);
extern int txg_list_add(txg_list_t *tl, void *p, uint64_t txg);
extern void *txg_list_remove(txg_list_t *tl, uint64_t txg);
extern void *txg_list_remove_this(txg_list_t *tl, void *p, uint64_t txg);
extern int txg_list_member(txg_list_t *tl, void *p, uint64_t txg);
extern void *txg_list_head(txg_list_t *tl, uint64_t txg);
extern void *txg_list_next(txg_list_t *tl, void *p, uint64_t txg);

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_TXG_H */
