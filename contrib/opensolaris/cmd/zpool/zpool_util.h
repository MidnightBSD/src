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

#ifndef	ZPOOL_UTIL_H
#define	ZPOOL_UTIL_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <libnvpair.h>
#include <libzfs.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Basic utility functions
 */
void *safe_malloc(size_t);
char *safe_strdup(const char *);
void zpool_no_memory(void);

/*
 * Virtual device functions
 */
nvlist_t *make_root_vdev(nvlist_t *poolconfig, int force, int check_rep,
    boolean_t isreplace, int argc, char **argv);

/*
 * Pool list functions
 */
int for_each_pool(int, char **, boolean_t unavail, zpool_proplist_t **,
    zpool_iter_f, void *);

typedef struct zpool_list zpool_list_t;

zpool_list_t *pool_list_get(int, char **, zpool_proplist_t **, int *);
void pool_list_update(zpool_list_t *);
int pool_list_iter(zpool_list_t *, int unavail, zpool_iter_f, void *);
void pool_list_free(zpool_list_t *);
int pool_list_count(zpool_list_t *);
void pool_list_remove(zpool_list_t *, zpool_handle_t *);

libzfs_handle_t *g_zfs;

#ifdef	__cplusplus
}
#endif

#endif	/* ZPOOL_UTIL_H */
