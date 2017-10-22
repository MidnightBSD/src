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

/*
 * This file contains all the routines used when modifying on-disk SPA state.
 * This includes opening, importing, destroying, exporting a pool, and syncing a
 * pool.
 */

#include <sys/zfs_context.h>
#include <sys/fm/fs/zfs.h>
#include <sys/spa_impl.h>
#include <sys/zio.h>
#include <sys/zio_checksum.h>
#include <sys/zio_compress.h>
#include <sys/dmu.h>
#include <sys/dmu_tx.h>
#include <sys/zap.h>
#include <sys/zil.h>
#include <sys/vdev_impl.h>
#include <sys/metaslab.h>
#include <sys/uberblock_impl.h>
#include <sys/txg.h>
#include <sys/avl.h>
#include <sys/dmu_traverse.h>
#include <sys/dmu_objset.h>
#include <sys/unique.h>
#include <sys/dsl_pool.h>
#include <sys/dsl_dataset.h>
#include <sys/dsl_dir.h>
#include <sys/dsl_prop.h>
#include <sys/dsl_synctask.h>
#include <sys/fs/zfs.h>
#include <sys/callb.h>
#include <sys/sunddi.h>

int zio_taskq_threads = 0;
SYSCTL_DECL(_vfs_zfs);
SYSCTL_NODE(_vfs_zfs, OID_AUTO, zio, CTLFLAG_RW, 0, "ZFS ZIO");
TUNABLE_INT("vfs.zfs.zio.taskq_threads", &zio_taskq_threads);
SYSCTL_INT(_vfs_zfs_zio, OID_AUTO, taskq_threads, CTLFLAG_RW,
    &zio_taskq_threads, 0, "Number of ZIO threads per ZIO type");


/*
 * ==========================================================================
 * SPA state manipulation (open/create/destroy/import/export)
 * ==========================================================================
 */

static int
spa_error_entry_compare(const void *a, const void *b)
{
	spa_error_entry_t *sa = (spa_error_entry_t *)a;
	spa_error_entry_t *sb = (spa_error_entry_t *)b;
	int ret;

	ret = bcmp(&sa->se_bookmark, &sb->se_bookmark,
	    sizeof (zbookmark_t));

	if (ret < 0)
		return (-1);
	else if (ret > 0)
		return (1);
	else
		return (0);
}

/*
 * Utility function which retrieves copies of the current logs and
 * re-initializes them in the process.
 */
void
spa_get_errlists(spa_t *spa, avl_tree_t *last, avl_tree_t *scrub)
{
	ASSERT(MUTEX_HELD(&spa->spa_errlist_lock));

	bcopy(&spa->spa_errlist_last, last, sizeof (avl_tree_t));
	bcopy(&spa->spa_errlist_scrub, scrub, sizeof (avl_tree_t));

	avl_create(&spa->spa_errlist_scrub,
	    spa_error_entry_compare, sizeof (spa_error_entry_t),
	    offsetof(spa_error_entry_t, se_avl));
	avl_create(&spa->spa_errlist_last,
	    spa_error_entry_compare, sizeof (spa_error_entry_t),
	    offsetof(spa_error_entry_t, se_avl));
}

/*
 * Activate an uninitialized pool.
 */
static void
spa_activate(spa_t *spa)
{
	int t;
	int nthreads = zio_taskq_threads;
	char name[32];

	ASSERT(spa->spa_state == POOL_STATE_UNINITIALIZED);

	spa->spa_state = POOL_STATE_ACTIVE;

	spa->spa_normal_class = metaslab_class_create();

	if (nthreads == 0)
		nthreads = max_ncpus;
	for (t = 0; t < ZIO_TYPES; t++) {
		snprintf(name, sizeof(name), "spa_zio_issue %d", t);
		spa->spa_zio_issue_taskq[t] = taskq_create(name, nthreads,
		    maxclsyspri, 50, INT_MAX, TASKQ_PREPOPULATE);
		snprintf(name, sizeof(name), "spa_zio_intr %d", t);
		spa->spa_zio_intr_taskq[t] = taskq_create(name, nthreads,
		    maxclsyspri, 50, INT_MAX, TASKQ_PREPOPULATE);
	}

	rw_init(&spa->spa_traverse_lock, NULL, RW_DEFAULT, NULL);

	mutex_init(&spa->spa_uberblock_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&spa->spa_errlog_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&spa->spa_errlist_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&spa->spa_config_lock.scl_lock, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&spa->spa_config_lock.scl_cv, NULL, CV_DEFAULT, NULL);
	mutex_init(&spa->spa_sync_bplist.bpl_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&spa->spa_history_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&spa->spa_props_lock, NULL, MUTEX_DEFAULT, NULL);

	list_create(&spa->spa_dirty_list, sizeof (vdev_t),
	    offsetof(vdev_t, vdev_dirty_node));

	txg_list_create(&spa->spa_vdev_txg_list,
	    offsetof(struct vdev, vdev_txg_node));

	avl_create(&spa->spa_errlist_scrub,
	    spa_error_entry_compare, sizeof (spa_error_entry_t),
	    offsetof(spa_error_entry_t, se_avl));
	avl_create(&spa->spa_errlist_last,
	    spa_error_entry_compare, sizeof (spa_error_entry_t),
	    offsetof(spa_error_entry_t, se_avl));
}

/*
 * Opposite of spa_activate().
 */
static void
spa_deactivate(spa_t *spa)
{
	int t;

	ASSERT(spa->spa_sync_on == B_FALSE);
	ASSERT(spa->spa_dsl_pool == NULL);
	ASSERT(spa->spa_root_vdev == NULL);

	ASSERT(spa->spa_state != POOL_STATE_UNINITIALIZED);

	txg_list_destroy(&spa->spa_vdev_txg_list);

	list_destroy(&spa->spa_dirty_list);

	for (t = 0; t < ZIO_TYPES; t++) {
		taskq_destroy(spa->spa_zio_issue_taskq[t]);
		taskq_destroy(spa->spa_zio_intr_taskq[t]);
		spa->spa_zio_issue_taskq[t] = NULL;
		spa->spa_zio_intr_taskq[t] = NULL;
	}

	metaslab_class_destroy(spa->spa_normal_class);
	spa->spa_normal_class = NULL;

	/*
	 * If this was part of an import or the open otherwise failed, we may
	 * still have errors left in the queues.  Empty them just in case.
	 */
	spa_errlog_drain(spa);

	avl_destroy(&spa->spa_errlist_scrub);
	avl_destroy(&spa->spa_errlist_last);

	rw_destroy(&spa->spa_traverse_lock);
	mutex_destroy(&spa->spa_uberblock_lock);
	mutex_destroy(&spa->spa_errlog_lock);
	mutex_destroy(&spa->spa_errlist_lock);
	mutex_destroy(&spa->spa_config_lock.scl_lock);
	cv_destroy(&spa->spa_config_lock.scl_cv);
	mutex_destroy(&spa->spa_sync_bplist.bpl_lock);
	mutex_destroy(&spa->spa_history_lock);
	mutex_destroy(&spa->spa_props_lock);

	spa->spa_state = POOL_STATE_UNINITIALIZED;
}

/*
 * Verify a pool configuration, and construct the vdev tree appropriately.  This
 * will create all the necessary vdevs in the appropriate layout, with each vdev
 * in the CLOSED state.  This will prep the pool before open/creation/import.
 * All vdev validation is done by the vdev_alloc() routine.
 */
static int
spa_config_parse(spa_t *spa, vdev_t **vdp, nvlist_t *nv, vdev_t *parent,
    uint_t id, int atype)
{
	nvlist_t **child;
	uint_t c, children;
	int error;

	if ((error = vdev_alloc(spa, vdp, nv, parent, id, atype)) != 0)
		return (error);

	if ((*vdp)->vdev_ops->vdev_op_leaf)
		return (0);

	if (nvlist_lookup_nvlist_array(nv, ZPOOL_CONFIG_CHILDREN,
	    &child, &children) != 0) {
		vdev_free(*vdp);
		*vdp = NULL;
		return (EINVAL);
	}

	for (c = 0; c < children; c++) {
		vdev_t *vd;
		if ((error = spa_config_parse(spa, &vd, child[c], *vdp, c,
		    atype)) != 0) {
			vdev_free(*vdp);
			*vdp = NULL;
			return (error);
		}
	}

	ASSERT(*vdp != NULL);

	return (0);
}

/*
 * Opposite of spa_load().
 */
static void
spa_unload(spa_t *spa)
{
	int i;

	/*
	 * Stop async tasks.
	 */
	spa_async_suspend(spa);

	/*
	 * Stop syncing.
	 */
	if (spa->spa_sync_on) {
		txg_sync_stop(spa->spa_dsl_pool);
		spa->spa_sync_on = B_FALSE;
	}

	/*
	 * Wait for any outstanding prefetch I/O to complete.
	 */
	spa_config_enter(spa, RW_WRITER, FTAG);
	spa_config_exit(spa, FTAG);

	/*
	 * Close the dsl pool.
	 */
	if (spa->spa_dsl_pool) {
		dsl_pool_close(spa->spa_dsl_pool);
		spa->spa_dsl_pool = NULL;
	}

	/*
	 * Close all vdevs.
	 */
	if (spa->spa_root_vdev)
		vdev_free(spa->spa_root_vdev);
	ASSERT(spa->spa_root_vdev == NULL);

	for (i = 0; i < spa->spa_nspares; i++)
		vdev_free(spa->spa_spares[i]);
	if (spa->spa_spares) {
		kmem_free(spa->spa_spares, spa->spa_nspares * sizeof (void *));
		spa->spa_spares = NULL;
	}
	if (spa->spa_sparelist) {
		nvlist_free(spa->spa_sparelist);
		spa->spa_sparelist = NULL;
	}

	spa->spa_async_suspended = 0;
}

/*
 * Load (or re-load) the current list of vdevs describing the active spares for
 * this pool.  When this is called, we have some form of basic information in
 * 'spa_sparelist'.  We parse this into vdevs, try to open them, and then
 * re-generate a more complete list including status information.
 */
static void
spa_load_spares(spa_t *spa)
{
	nvlist_t **spares;
	uint_t nspares;
	int i;
	vdev_t *vd, *tvd;

	/*
	 * First, close and free any existing spare vdevs.
	 */
	for (i = 0; i < spa->spa_nspares; i++) {
		vd = spa->spa_spares[i];

		/* Undo the call to spa_activate() below */
		if ((tvd = spa_lookup_by_guid(spa, vd->vdev_guid)) != NULL &&
		    tvd->vdev_isspare)
			spa_spare_remove(tvd);
		vdev_close(vd);
		vdev_free(vd);
	}

	if (spa->spa_spares)
		kmem_free(spa->spa_spares, spa->spa_nspares * sizeof (void *));

	if (spa->spa_sparelist == NULL)
		nspares = 0;
	else
		VERIFY(nvlist_lookup_nvlist_array(spa->spa_sparelist,
		    ZPOOL_CONFIG_SPARES, &spares, &nspares) == 0);

	spa->spa_nspares = (int)nspares;
	spa->spa_spares = NULL;

	if (nspares == 0)
		return;

	/*
	 * Construct the array of vdevs, opening them to get status in the
	 * process.   For each spare, there is potentially two different vdev_t
	 * structures associated with it: one in the list of spares (used only
	 * for basic validation purposes) and one in the active vdev
	 * configuration (if it's spared in).  During this phase we open and
	 * validate each vdev on the spare list.  If the vdev also exists in the
	 * active configuration, then we also mark this vdev as an active spare.
	 */
	spa->spa_spares = kmem_alloc(nspares * sizeof (void *), KM_SLEEP);
	for (i = 0; i < spa->spa_nspares; i++) {
		VERIFY(spa_config_parse(spa, &vd, spares[i], NULL, 0,
		    VDEV_ALLOC_SPARE) == 0);
		ASSERT(vd != NULL);

		spa->spa_spares[i] = vd;

		if ((tvd = spa_lookup_by_guid(spa, vd->vdev_guid)) != NULL) {
			if (!tvd->vdev_isspare)
				spa_spare_add(tvd);

			/*
			 * We only mark the spare active if we were successfully
			 * able to load the vdev.  Otherwise, importing a pool
			 * with a bad active spare would result in strange
			 * behavior, because multiple pool would think the spare
			 * is actively in use.
			 *
			 * There is a vulnerability here to an equally bizarre
			 * circumstance, where a dead active spare is later
			 * brought back to life (onlined or otherwise).  Given
			 * the rarity of this scenario, and the extra complexity
			 * it adds, we ignore the possibility.
			 */
			if (!vdev_is_dead(tvd))
				spa_spare_activate(tvd);
		}

		if (vdev_open(vd) != 0)
			continue;

		vd->vdev_top = vd;
		(void) vdev_validate_spare(vd);
	}

	/*
	 * Recompute the stashed list of spares, with status information
	 * this time.
	 */
	VERIFY(nvlist_remove(spa->spa_sparelist, ZPOOL_CONFIG_SPARES,
	    DATA_TYPE_NVLIST_ARRAY) == 0);

	spares = kmem_alloc(spa->spa_nspares * sizeof (void *), KM_SLEEP);
	for (i = 0; i < spa->spa_nspares; i++)
		spares[i] = vdev_config_generate(spa, spa->spa_spares[i],
		    B_TRUE, B_TRUE);
	VERIFY(nvlist_add_nvlist_array(spa->spa_sparelist, ZPOOL_CONFIG_SPARES,
	    spares, spa->spa_nspares) == 0);
	for (i = 0; i < spa->spa_nspares; i++)
		nvlist_free(spares[i]);
	kmem_free(spares, spa->spa_nspares * sizeof (void *));
}

static int
load_nvlist(spa_t *spa, uint64_t obj, nvlist_t **value)
{
	dmu_buf_t *db;
	char *packed = NULL;
	size_t nvsize = 0;
	int error;
	*value = NULL;

	VERIFY(0 == dmu_bonus_hold(spa->spa_meta_objset, obj, FTAG, &db));
	nvsize = *(uint64_t *)db->db_data;
	dmu_buf_rele(db, FTAG);

	packed = kmem_alloc(nvsize, KM_SLEEP);
	error = dmu_read(spa->spa_meta_objset, obj, 0, nvsize, packed);
	if (error == 0)
		error = nvlist_unpack(packed, nvsize, value, 0);
	kmem_free(packed, nvsize);

	return (error);
}

/*
 * Load an existing storage pool, using the pool's builtin spa_config as a
 * source of configuration information.
 */
static int
spa_load(spa_t *spa, nvlist_t *config, spa_load_state_t state, int mosconfig)
{
	int error = 0;
	nvlist_t *nvroot = NULL;
	vdev_t *rvd;
	uberblock_t *ub = &spa->spa_uberblock;
	uint64_t config_cache_txg = spa->spa_config_txg;
	uint64_t pool_guid;
	uint64_t version;
	zio_t *zio;

	spa->spa_load_state = state;

	if (nvlist_lookup_nvlist(config, ZPOOL_CONFIG_VDEV_TREE, &nvroot) ||
	    nvlist_lookup_uint64(config, ZPOOL_CONFIG_POOL_GUID, &pool_guid)) {
		error = EINVAL;
		goto out;
	}

	/*
	 * Versioning wasn't explicitly added to the label until later, so if
	 * it's not present treat it as the initial version.
	 */
	if (nvlist_lookup_uint64(config, ZPOOL_CONFIG_VERSION, &version) != 0)
		version = ZFS_VERSION_INITIAL;

	(void) nvlist_lookup_uint64(config, ZPOOL_CONFIG_POOL_TXG,
	    &spa->spa_config_txg);

	if ((state == SPA_LOAD_IMPORT || state == SPA_LOAD_TRYIMPORT) &&
	    spa_guid_exists(pool_guid, 0)) {
		error = EEXIST;
		goto out;
	}

	spa->spa_load_guid = pool_guid;

	/*
	 * Parse the configuration into a vdev tree.  We explicitly set the
	 * value that will be returned by spa_version() since parsing the
	 * configuration requires knowing the version number.
	 */
	spa_config_enter(spa, RW_WRITER, FTAG);
	spa->spa_ubsync.ub_version = version;
	error = spa_config_parse(spa, &rvd, nvroot, NULL, 0, VDEV_ALLOC_LOAD);
	spa_config_exit(spa, FTAG);

	if (error != 0)
		goto out;

	ASSERT(spa->spa_root_vdev == rvd);
	ASSERT(spa_guid(spa) == pool_guid);

	/*
	 * Try to open all vdevs, loading each label in the process.
	 */
	error = vdev_open(rvd);
	if (error != 0)
		goto out;

	/*
	 * Validate the labels for all leaf vdevs.  We need to grab the config
	 * lock because all label I/O is done with the ZIO_FLAG_CONFIG_HELD
	 * flag.
	 */
	spa_config_enter(spa, RW_READER, FTAG);
	error = vdev_validate(rvd);
	spa_config_exit(spa, FTAG);

	if (error != 0)
		goto out;

	if (rvd->vdev_state <= VDEV_STATE_CANT_OPEN) {
		error = ENXIO;
		goto out;
	}

	/*
	 * Find the best uberblock.
	 */
	bzero(ub, sizeof (uberblock_t));

	zio = zio_root(spa, NULL, NULL,
	    ZIO_FLAG_CANFAIL | ZIO_FLAG_SPECULATIVE);
	vdev_uberblock_load(zio, rvd, ub);
	error = zio_wait(zio);

	/*
	 * If we weren't able to find a single valid uberblock, return failure.
	 */
	if (ub->ub_txg == 0) {
		vdev_set_state(rvd, B_TRUE, VDEV_STATE_CANT_OPEN,
		    VDEV_AUX_CORRUPT_DATA);
		error = ENXIO;
		goto out;
	}

	/*
	 * If the pool is newer than the code, we can't open it.
	 */
	if (ub->ub_version > ZFS_VERSION) {
		vdev_set_state(rvd, B_TRUE, VDEV_STATE_CANT_OPEN,
		    VDEV_AUX_VERSION_NEWER);
		error = ENOTSUP;
		goto out;
	}

	/*
	 * If the vdev guid sum doesn't match the uberblock, we have an
	 * incomplete configuration.
	 */
	if (rvd->vdev_guid_sum != ub->ub_guid_sum && mosconfig) {
		vdev_set_state(rvd, B_TRUE, VDEV_STATE_CANT_OPEN,
		    VDEV_AUX_BAD_GUID_SUM);
		error = ENXIO;
		goto out;
	}

	/*
	 * Initialize internal SPA structures.
	 */
	spa->spa_state = POOL_STATE_ACTIVE;
	spa->spa_ubsync = spa->spa_uberblock;
	spa->spa_first_txg = spa_last_synced_txg(spa) + 1;
	error = dsl_pool_open(spa, spa->spa_first_txg, &spa->spa_dsl_pool);
	if (error) {
		vdev_set_state(rvd, B_TRUE, VDEV_STATE_CANT_OPEN,
		    VDEV_AUX_CORRUPT_DATA);
		goto out;
	}
	spa->spa_meta_objset = spa->spa_dsl_pool->dp_meta_objset;

	if (zap_lookup(spa->spa_meta_objset,
	    DMU_POOL_DIRECTORY_OBJECT, DMU_POOL_CONFIG,
	    sizeof (uint64_t), 1, &spa->spa_config_object) != 0) {
		vdev_set_state(rvd, B_TRUE, VDEV_STATE_CANT_OPEN,
		    VDEV_AUX_CORRUPT_DATA);
		error = EIO;
		goto out;
	}

	if (!mosconfig) {
		nvlist_t *newconfig;
		uint64_t hostid;

		if (load_nvlist(spa, spa->spa_config_object, &newconfig) != 0) {
			vdev_set_state(rvd, B_TRUE, VDEV_STATE_CANT_OPEN,
			    VDEV_AUX_CORRUPT_DATA);
			error = EIO;
			goto out;
		}

		/*
		 * hostid is set after the root file system is mounted, so
		 * ignore the check until it's done.
		 */
		if (nvlist_lookup_uint64(newconfig, ZPOOL_CONFIG_HOSTID,
		    &hostid) == 0 && root_mounted()) {
			char *hostname;
			unsigned long myhostid = 0;

			VERIFY(nvlist_lookup_string(newconfig,
			    ZPOOL_CONFIG_HOSTNAME, &hostname) == 0);

			(void) ddi_strtoul(hw_serial, NULL, 10, &myhostid);
			if ((unsigned long)hostid != myhostid) {
				cmn_err(CE_WARN, "pool '%s' could not be "
				    "loaded as it was last accessed by "
				    "another system (host: %s hostid: 0x%lx).  "
				    "See: http://www.sun.com/msg/ZFS-8000-EY",
				    spa->spa_name, hostname,
				    (unsigned long)hostid);
				error = EBADF;
				goto out;
			}
		}

		spa_config_set(spa, newconfig);
		spa_unload(spa);
		spa_deactivate(spa);
		spa_activate(spa);

		return (spa_load(spa, newconfig, state, B_TRUE));
	}

	if (zap_lookup(spa->spa_meta_objset,
	    DMU_POOL_DIRECTORY_OBJECT, DMU_POOL_SYNC_BPLIST,
	    sizeof (uint64_t), 1, &spa->spa_sync_bplist_obj) != 0) {
		vdev_set_state(rvd, B_TRUE, VDEV_STATE_CANT_OPEN,
		    VDEV_AUX_CORRUPT_DATA);
		error = EIO;
		goto out;
	}

	/*
	 * Load the bit that tells us to use the new accounting function
	 * (raid-z deflation).  If we have an older pool, this will not
	 * be present.
	 */
	error = zap_lookup(spa->spa_meta_objset,
	    DMU_POOL_DIRECTORY_OBJECT, DMU_POOL_DEFLATE,
	    sizeof (uint64_t), 1, &spa->spa_deflate);
	if (error != 0 && error != ENOENT) {
		vdev_set_state(rvd, B_TRUE, VDEV_STATE_CANT_OPEN,
		    VDEV_AUX_CORRUPT_DATA);
		error = EIO;
		goto out;
	}

	/*
	 * Load the persistent error log.  If we have an older pool, this will
	 * not be present.
	 */
	error = zap_lookup(spa->spa_meta_objset,
	    DMU_POOL_DIRECTORY_OBJECT, DMU_POOL_ERRLOG_LAST,
	    sizeof (uint64_t), 1, &spa->spa_errlog_last);
	if (error != 0 && error != ENOENT) {
		vdev_set_state(rvd, B_TRUE, VDEV_STATE_CANT_OPEN,
		    VDEV_AUX_CORRUPT_DATA);
		error = EIO;
		goto out;
	}

	error = zap_lookup(spa->spa_meta_objset,
	    DMU_POOL_DIRECTORY_OBJECT, DMU_POOL_ERRLOG_SCRUB,
	    sizeof (uint64_t), 1, &spa->spa_errlog_scrub);
	if (error != 0 && error != ENOENT) {
		vdev_set_state(rvd, B_TRUE, VDEV_STATE_CANT_OPEN,
		    VDEV_AUX_CORRUPT_DATA);
		error = EIO;
		goto out;
	}

	/*
	 * Load the history object.  If we have an older pool, this
	 * will not be present.
	 */
	error = zap_lookup(spa->spa_meta_objset,
	    DMU_POOL_DIRECTORY_OBJECT, DMU_POOL_HISTORY,
	    sizeof (uint64_t), 1, &spa->spa_history);
	if (error != 0 && error != ENOENT) {
		vdev_set_state(rvd, B_TRUE, VDEV_STATE_CANT_OPEN,
		    VDEV_AUX_CORRUPT_DATA);
		error = EIO;
		goto out;
	}

	/*
	 * Load any hot spares for this pool.
	 */
	error = zap_lookup(spa->spa_meta_objset, DMU_POOL_DIRECTORY_OBJECT,
	    DMU_POOL_SPARES, sizeof (uint64_t), 1, &spa->spa_spares_object);
	if (error != 0 && error != ENOENT) {
		vdev_set_state(rvd, B_TRUE, VDEV_STATE_CANT_OPEN,
		    VDEV_AUX_CORRUPT_DATA);
		error = EIO;
		goto out;
	}
	if (error == 0) {
		ASSERT(spa_version(spa) >= ZFS_VERSION_SPARES);
		if (load_nvlist(spa, spa->spa_spares_object,
		    &spa->spa_sparelist) != 0) {
			vdev_set_state(rvd, B_TRUE, VDEV_STATE_CANT_OPEN,
			    VDEV_AUX_CORRUPT_DATA);
			error = EIO;
			goto out;
		}

		spa_config_enter(spa, RW_WRITER, FTAG);
		spa_load_spares(spa);
		spa_config_exit(spa, FTAG);
	}

	error = zap_lookup(spa->spa_meta_objset, DMU_POOL_DIRECTORY_OBJECT,
	    DMU_POOL_PROPS, sizeof (uint64_t), 1, &spa->spa_pool_props_object);

	if (error && error != ENOENT) {
		vdev_set_state(rvd, B_TRUE, VDEV_STATE_CANT_OPEN,
		    VDEV_AUX_CORRUPT_DATA);
		error = EIO;
		goto out;
	}

	if (error == 0) {
		(void) zap_lookup(spa->spa_meta_objset,
		    spa->spa_pool_props_object,
		    zpool_prop_to_name(ZFS_PROP_BOOTFS),
		    sizeof (uint64_t), 1, &spa->spa_bootfs);
	}

	/*
	 * Load the vdev state for all toplevel vdevs.
	 */
	vdev_load(rvd);

	/*
	 * Propagate the leaf DTLs we just loaded all the way up the tree.
	 */
	spa_config_enter(spa, RW_WRITER, FTAG);
	vdev_dtl_reassess(rvd, 0, 0, B_FALSE);
	spa_config_exit(spa, FTAG);

	/*
	 * Check the state of the root vdev.  If it can't be opened, it
	 * indicates one or more toplevel vdevs are faulted.
	 */
	if (rvd->vdev_state <= VDEV_STATE_CANT_OPEN) {
		error = ENXIO;
		goto out;
	}

	if ((spa_mode & FWRITE) && state != SPA_LOAD_TRYIMPORT) {
		dmu_tx_t *tx;
		int need_update = B_FALSE;
		int c;

		/*
		 * Claim log blocks that haven't been committed yet.
		 * This must all happen in a single txg.
		 */
		tx = dmu_tx_create_assigned(spa_get_dsl(spa),
		    spa_first_txg(spa));
		(void) dmu_objset_find(spa->spa_name,
		    zil_claim, tx, DS_FIND_CHILDREN);
		dmu_tx_commit(tx);

		spa->spa_sync_on = B_TRUE;
		txg_sync_start(spa->spa_dsl_pool);

		/*
		 * Wait for all claims to sync.
		 */
		txg_wait_synced(spa->spa_dsl_pool, 0);

		/*
		 * If the config cache is stale, or we have uninitialized
		 * metaslabs (see spa_vdev_add()), then update the config.
		 */
		if (config_cache_txg != spa->spa_config_txg ||
		    state == SPA_LOAD_IMPORT)
			need_update = B_TRUE;

		for (c = 0; c < rvd->vdev_children; c++)
			if (rvd->vdev_child[c]->vdev_ms_array == 0)
				need_update = B_TRUE;

		/*
		 * Update the config cache asychronously in case we're the
		 * root pool, in which case the config cache isn't writable yet.
		 */
		if (need_update)
			spa_async_request(spa, SPA_ASYNC_CONFIG_UPDATE);
	}

	error = 0;
out:
	if (error && error != EBADF)
		zfs_ereport_post(FM_EREPORT_ZFS_POOL, spa, NULL, NULL, 0, 0);
	spa->spa_load_state = SPA_LOAD_NONE;
	spa->spa_ena = 0;

	return (error);
}

/*
 * Pool Open/Import
 *
 * The import case is identical to an open except that the configuration is sent
 * down from userland, instead of grabbed from the configuration cache.  For the
 * case of an open, the pool configuration will exist in the
 * POOL_STATE_UNITIALIZED state.
 *
 * The stats information (gen/count/ustats) is used to gather vdev statistics at
 * the same time open the pool, without having to keep around the spa_t in some
 * ambiguous state.
 */
static int
spa_open_common(const char *pool, spa_t **spapp, void *tag, nvlist_t **config)
{
	spa_t *spa;
	int error;
	int loaded = B_FALSE;
	int locked = B_FALSE;

	*spapp = NULL;

	/*
	 * As disgusting as this is, we need to support recursive calls to this
	 * function because dsl_dir_open() is called during spa_load(), and ends
	 * up calling spa_open() again.  The real fix is to figure out how to
	 * avoid dsl_dir_open() calling this in the first place.
	 */
	if (mutex_owner(&spa_namespace_lock) != curthread) {
		mutex_enter(&spa_namespace_lock);
		locked = B_TRUE;
	}

	if ((spa = spa_lookup(pool)) == NULL) {
		if (locked)
			mutex_exit(&spa_namespace_lock);
		return (ENOENT);
	}
	if (spa->spa_state == POOL_STATE_UNINITIALIZED) {

		spa_activate(spa);

		error = spa_load(spa, spa->spa_config, SPA_LOAD_OPEN, B_FALSE);

		if (error == EBADF) {
			/*
			 * If vdev_validate() returns failure (indicated by
			 * EBADF), it indicates that one of the vdevs indicates
			 * that the pool has been exported or destroyed.  If
			 * this is the case, the config cache is out of sync and
			 * we should remove the pool from the namespace.
			 */
			zfs_post_ok(spa, NULL);
			spa_unload(spa);
			spa_deactivate(spa);
			spa_remove(spa);
			spa_config_sync();
			if (locked)
				mutex_exit(&spa_namespace_lock);
			return (ENOENT);
		}

		if (error) {
			/*
			 * We can't open the pool, but we still have useful
			 * information: the state of each vdev after the
			 * attempted vdev_open().  Return this to the user.
			 */
			if (config != NULL && spa->spa_root_vdev != NULL) {
				spa_config_enter(spa, RW_READER, FTAG);
				*config = spa_config_generate(spa, NULL, -1ULL,
				    B_TRUE);
				spa_config_exit(spa, FTAG);
			}
			spa_unload(spa);
			spa_deactivate(spa);
			spa->spa_last_open_failed = B_TRUE;
			if (locked)
				mutex_exit(&spa_namespace_lock);
			*spapp = NULL;
			return (error);
		} else {
			zfs_post_ok(spa, NULL);
			spa->spa_last_open_failed = B_FALSE;
		}

		loaded = B_TRUE;
	}

	spa_open_ref(spa, tag);
	if (locked)
		mutex_exit(&spa_namespace_lock);

	*spapp = spa;

	if (config != NULL) {
		spa_config_enter(spa, RW_READER, FTAG);
		*config = spa_config_generate(spa, NULL, -1ULL, B_TRUE);
		spa_config_exit(spa, FTAG);
	}

	/*
	 * If we just loaded the pool, resilver anything that's out of date.
	 */
	if (loaded && (spa_mode & FWRITE))
		VERIFY(spa_scrub(spa, POOL_SCRUB_RESILVER, B_TRUE) == 0);

	return (0);
}

int
spa_open(const char *name, spa_t **spapp, void *tag)
{
	return (spa_open_common(name, spapp, tag, NULL));
}

/*
 * Lookup the given spa_t, incrementing the inject count in the process,
 * preventing it from being exported or destroyed.
 */
spa_t *
spa_inject_addref(char *name)
{
	spa_t *spa;

	mutex_enter(&spa_namespace_lock);
	if ((spa = spa_lookup(name)) == NULL) {
		mutex_exit(&spa_namespace_lock);
		return (NULL);
	}
	spa->spa_inject_ref++;
	mutex_exit(&spa_namespace_lock);

	return (spa);
}

void
spa_inject_delref(spa_t *spa)
{
	mutex_enter(&spa_namespace_lock);
	spa->spa_inject_ref--;
	mutex_exit(&spa_namespace_lock);
}

static void
spa_add_spares(spa_t *spa, nvlist_t *config)
{
	nvlist_t **spares;
	uint_t i, nspares;
	nvlist_t *nvroot;
	uint64_t guid;
	vdev_stat_t *vs;
	uint_t vsc;
	uint64_t pool;

	if (spa->spa_nspares == 0)
		return;

	VERIFY(nvlist_lookup_nvlist(config,
	    ZPOOL_CONFIG_VDEV_TREE, &nvroot) == 0);
	VERIFY(nvlist_lookup_nvlist_array(spa->spa_sparelist,
	    ZPOOL_CONFIG_SPARES, &spares, &nspares) == 0);
	if (nspares != 0) {
		VERIFY(nvlist_add_nvlist_array(nvroot,
		    ZPOOL_CONFIG_SPARES, spares, nspares) == 0);
		VERIFY(nvlist_lookup_nvlist_array(nvroot,
		    ZPOOL_CONFIG_SPARES, &spares, &nspares) == 0);

		/*
		 * Go through and find any spares which have since been
		 * repurposed as an active spare.  If this is the case, update
		 * their status appropriately.
		 */
		for (i = 0; i < nspares; i++) {
			VERIFY(nvlist_lookup_uint64(spares[i],
			    ZPOOL_CONFIG_GUID, &guid) == 0);
			if (spa_spare_exists(guid, &pool) && pool != 0ULL) {
				VERIFY(nvlist_lookup_uint64_array(
				    spares[i], ZPOOL_CONFIG_STATS,
				    (uint64_t **)&vs, &vsc) == 0);
				vs->vs_state = VDEV_STATE_CANT_OPEN;
				vs->vs_aux = VDEV_AUX_SPARED;
			}
		}
	}
}

int
spa_get_stats(const char *name, nvlist_t **config, char *altroot, size_t buflen)
{
	int error;
	spa_t *spa;

	*config = NULL;
	error = spa_open_common(name, &spa, FTAG, config);

	if (spa && *config != NULL) {
		VERIFY(nvlist_add_uint64(*config, ZPOOL_CONFIG_ERRCOUNT,
		    spa_get_errlog_size(spa)) == 0);

		spa_add_spares(spa, *config);
	}

	/*
	 * We want to get the alternate root even for faulted pools, so we cheat
	 * and call spa_lookup() directly.
	 */
	if (altroot) {
		if (spa == NULL) {
			mutex_enter(&spa_namespace_lock);
			spa = spa_lookup(name);
			if (spa)
				spa_altroot(spa, altroot, buflen);
			else
				altroot[0] = '\0';
			spa = NULL;
			mutex_exit(&spa_namespace_lock);
		} else {
			spa_altroot(spa, altroot, buflen);
		}
	}

	if (spa != NULL)
		spa_close(spa, FTAG);

	return (error);
}

/*
 * Validate that the 'spares' array is well formed.  We must have an array of
 * nvlists, each which describes a valid leaf vdev.  If this is an import (mode
 * is VDEV_ALLOC_SPARE), then we allow corrupted spares to be specified, as long
 * as they are well-formed.
 */
static int
spa_validate_spares(spa_t *spa, nvlist_t *nvroot, uint64_t crtxg, int mode)
{
	nvlist_t **spares;
	uint_t i, nspares;
	vdev_t *vd;
	int error;

	/*
	 * It's acceptable to have no spares specified.
	 */
	if (nvlist_lookup_nvlist_array(nvroot, ZPOOL_CONFIG_SPARES,
	    &spares, &nspares) != 0)
		return (0);

	if (nspares == 0)
		return (EINVAL);

	/*
	 * Make sure the pool is formatted with a version that supports hot
	 * spares.
	 */
	if (spa_version(spa) < ZFS_VERSION_SPARES)
		return (ENOTSUP);

	/*
	 * Set the pending spare list so we correctly handle device in-use
	 * checking.
	 */
	spa->spa_pending_spares = spares;
	spa->spa_pending_nspares = nspares;

	for (i = 0; i < nspares; i++) {
		if ((error = spa_config_parse(spa, &vd, spares[i], NULL, 0,
		    mode)) != 0)
			goto out;

		if (!vd->vdev_ops->vdev_op_leaf) {
			vdev_free(vd);
			error = EINVAL;
			goto out;
		}

		vd->vdev_top = vd;

		if ((error = vdev_open(vd)) == 0 &&
		    (error = vdev_label_init(vd, crtxg,
		    VDEV_LABEL_SPARE)) == 0) {
			VERIFY(nvlist_add_uint64(spares[i], ZPOOL_CONFIG_GUID,
			    vd->vdev_guid) == 0);
		}

		vdev_free(vd);

		if (error && mode != VDEV_ALLOC_SPARE)
			goto out;
		else
			error = 0;
	}

out:
	spa->spa_pending_spares = NULL;
	spa->spa_pending_nspares = 0;
	return (error);
}

/*
 * Pool Creation
 */
int
spa_create(const char *pool, nvlist_t *nvroot, const char *altroot)
{
	spa_t *spa;
	vdev_t *rvd;
	dsl_pool_t *dp;
	dmu_tx_t *tx;
	int c, error = 0;
	uint64_t txg = TXG_INITIAL;
	nvlist_t **spares;
	uint_t nspares;

	/*
	 * If this pool already exists, return failure.
	 */
	mutex_enter(&spa_namespace_lock);
	if (spa_lookup(pool) != NULL) {
		mutex_exit(&spa_namespace_lock);
		return (EEXIST);
	}

	/*
	 * Allocate a new spa_t structure.
	 */
	spa = spa_add(pool, altroot);
	spa_activate(spa);

	spa->spa_uberblock.ub_txg = txg - 1;
	spa->spa_uberblock.ub_version = ZFS_VERSION;
	spa->spa_ubsync = spa->spa_uberblock;

	/*
	 * Create the root vdev.
	 */
	spa_config_enter(spa, RW_WRITER, FTAG);

	error = spa_config_parse(spa, &rvd, nvroot, NULL, 0, VDEV_ALLOC_ADD);

	ASSERT(error != 0 || rvd != NULL);
	ASSERT(error != 0 || spa->spa_root_vdev == rvd);

	if (error == 0 && rvd->vdev_children == 0)
		error = EINVAL;

	if (error == 0 &&
	    (error = vdev_create(rvd, txg, B_FALSE)) == 0 &&
	    (error = spa_validate_spares(spa, nvroot, txg,
	    VDEV_ALLOC_ADD)) == 0) {
		for (c = 0; c < rvd->vdev_children; c++)
			vdev_init(rvd->vdev_child[c], txg);
		vdev_config_dirty(rvd);
	}

	spa_config_exit(spa, FTAG);

	if (error != 0) {
		spa_unload(spa);
		spa_deactivate(spa);
		spa_remove(spa);
		mutex_exit(&spa_namespace_lock);
		return (error);
	}

	/*
	 * Get the list of spares, if specified.
	 */
	if (nvlist_lookup_nvlist_array(nvroot, ZPOOL_CONFIG_SPARES,
	    &spares, &nspares) == 0) {
		VERIFY(nvlist_alloc(&spa->spa_sparelist, NV_UNIQUE_NAME,
		    KM_SLEEP) == 0);
		VERIFY(nvlist_add_nvlist_array(spa->spa_sparelist,
		    ZPOOL_CONFIG_SPARES, spares, nspares) == 0);
		spa_config_enter(spa, RW_WRITER, FTAG);
		spa_load_spares(spa);
		spa_config_exit(spa, FTAG);
		spa->spa_sync_spares = B_TRUE;
	}

	spa->spa_dsl_pool = dp = dsl_pool_create(spa, txg);
	spa->spa_meta_objset = dp->dp_meta_objset;

	tx = dmu_tx_create_assigned(dp, txg);

	/*
	 * Create the pool config object.
	 */
	spa->spa_config_object = dmu_object_alloc(spa->spa_meta_objset,
	    DMU_OT_PACKED_NVLIST, 1 << 14,
	    DMU_OT_PACKED_NVLIST_SIZE, sizeof (uint64_t), tx);

	if (zap_add(spa->spa_meta_objset,
	    DMU_POOL_DIRECTORY_OBJECT, DMU_POOL_CONFIG,
	    sizeof (uint64_t), 1, &spa->spa_config_object, tx) != 0) {
		cmn_err(CE_PANIC, "failed to add pool config");
	}

	/* Newly created pools are always deflated. */
	spa->spa_deflate = TRUE;
	if (zap_add(spa->spa_meta_objset,
	    DMU_POOL_DIRECTORY_OBJECT, DMU_POOL_DEFLATE,
	    sizeof (uint64_t), 1, &spa->spa_deflate, tx) != 0) {
		cmn_err(CE_PANIC, "failed to add deflate");
	}

	/*
	 * Create the deferred-free bplist object.  Turn off compression
	 * because sync-to-convergence takes longer if the blocksize
	 * keeps changing.
	 */
	spa->spa_sync_bplist_obj = bplist_create(spa->spa_meta_objset,
	    1 << 14, tx);
	dmu_object_set_compress(spa->spa_meta_objset, spa->spa_sync_bplist_obj,
	    ZIO_COMPRESS_OFF, tx);

	if (zap_add(spa->spa_meta_objset,
	    DMU_POOL_DIRECTORY_OBJECT, DMU_POOL_SYNC_BPLIST,
	    sizeof (uint64_t), 1, &spa->spa_sync_bplist_obj, tx) != 0) {
		cmn_err(CE_PANIC, "failed to add bplist");
	}

	/*
	 * Create the pool's history object.
	 */
	spa_history_create_obj(spa, tx);

	dmu_tx_commit(tx);

	spa->spa_bootfs = zfs_prop_default_numeric(ZFS_PROP_BOOTFS);
	spa->spa_sync_on = B_TRUE;
	txg_sync_start(spa->spa_dsl_pool);

	/*
	 * We explicitly wait for the first transaction to complete so that our
	 * bean counters are appropriately updated.
	 */
	txg_wait_synced(spa->spa_dsl_pool, txg);

	spa_config_sync();

	mutex_exit(&spa_namespace_lock);

	return (0);
}

/*
 * Import the given pool into the system.  We set up the necessary spa_t and
 * then call spa_load() to do the dirty work.
 */
int
spa_import(const char *pool, nvlist_t *config, const char *altroot)
{
	spa_t *spa;
	int error;
	nvlist_t *nvroot;
	nvlist_t **spares;
	uint_t nspares;

	if (!(spa_mode & FWRITE))
		return (EROFS);

	/*
	 * If a pool with this name exists, return failure.
	 */
	mutex_enter(&spa_namespace_lock);
	if (spa_lookup(pool) != NULL) {
		mutex_exit(&spa_namespace_lock);
		return (EEXIST);
	}

	/*
	 * Create and initialize the spa structure.
	 */
	spa = spa_add(pool, altroot);
	spa_activate(spa);

	/*
	 * Pass off the heavy lifting to spa_load().
	 * Pass TRUE for mosconfig because the user-supplied config
	 * is actually the one to trust when doing an import.
	 */
	error = spa_load(spa, config, SPA_LOAD_IMPORT, B_TRUE);

	spa_config_enter(spa, RW_WRITER, FTAG);
	/*
	 * Toss any existing sparelist, as it doesn't have any validity anymore,
	 * and conflicts with spa_has_spare().
	 */
	if (spa->spa_sparelist) {
		nvlist_free(spa->spa_sparelist);
		spa->spa_sparelist = NULL;
		spa_load_spares(spa);
	}

	VERIFY(nvlist_lookup_nvlist(config, ZPOOL_CONFIG_VDEV_TREE,
	    &nvroot) == 0);
	if (error == 0)
		error = spa_validate_spares(spa, nvroot, -1ULL,
		    VDEV_ALLOC_SPARE);
	spa_config_exit(spa, FTAG);

	if (error != 0) {
		spa_unload(spa);
		spa_deactivate(spa);
		spa_remove(spa);
		mutex_exit(&spa_namespace_lock);
		return (error);
	}

	/*
	 * Override any spares as specified by the user, as these may have
	 * correct device names/devids, etc.
	 */
	if (nvlist_lookup_nvlist_array(nvroot, ZPOOL_CONFIG_SPARES,
	    &spares, &nspares) == 0) {
		if (spa->spa_sparelist)
			VERIFY(nvlist_remove(spa->spa_sparelist,
			    ZPOOL_CONFIG_SPARES, DATA_TYPE_NVLIST_ARRAY) == 0);
		else
			VERIFY(nvlist_alloc(&spa->spa_sparelist,
			    NV_UNIQUE_NAME, KM_SLEEP) == 0);
		VERIFY(nvlist_add_nvlist_array(spa->spa_sparelist,
		    ZPOOL_CONFIG_SPARES, spares, nspares) == 0);
		spa_config_enter(spa, RW_WRITER, FTAG);
		spa_load_spares(spa);
		spa_config_exit(spa, FTAG);
		spa->spa_sync_spares = B_TRUE;
	}

	/*
	 * Update the config cache to include the newly-imported pool.
	 */
	spa_config_update(spa, SPA_CONFIG_UPDATE_POOL);

	mutex_exit(&spa_namespace_lock);

	/*
	 * Resilver anything that's out of date.
	 */
	if (spa_mode & FWRITE)
		VERIFY(spa_scrub(spa, POOL_SCRUB_RESILVER, B_TRUE) == 0);

	return (0);
}

/*
 * This (illegal) pool name is used when temporarily importing a spa_t in order
 * to get the vdev stats associated with the imported devices.
 */
#define	TRYIMPORT_NAME	"$import"

nvlist_t *
spa_tryimport(nvlist_t *tryconfig)
{
	nvlist_t *config = NULL;
	char *poolname;
	spa_t *spa;
	uint64_t state;

	if (nvlist_lookup_string(tryconfig, ZPOOL_CONFIG_POOL_NAME, &poolname))
		return (NULL);

	if (nvlist_lookup_uint64(tryconfig, ZPOOL_CONFIG_POOL_STATE, &state))
		return (NULL);

	/*
	 * Create and initialize the spa structure.
	 */
	mutex_enter(&spa_namespace_lock);
	spa = spa_add(TRYIMPORT_NAME, NULL);
	spa_activate(spa);

	/*
	 * Pass off the heavy lifting to spa_load().
	 * Pass TRUE for mosconfig because the user-supplied config
	 * is actually the one to trust when doing an import.
	 */
	(void) spa_load(spa, tryconfig, SPA_LOAD_TRYIMPORT, B_TRUE);

	/*
	 * If 'tryconfig' was at least parsable, return the current config.
	 */
	if (spa->spa_root_vdev != NULL) {
		spa_config_enter(spa, RW_READER, FTAG);
		config = spa_config_generate(spa, NULL, -1ULL, B_TRUE);
		spa_config_exit(spa, FTAG);
		VERIFY(nvlist_add_string(config, ZPOOL_CONFIG_POOL_NAME,
		    poolname) == 0);
		VERIFY(nvlist_add_uint64(config, ZPOOL_CONFIG_POOL_STATE,
		    state) == 0);
		VERIFY(nvlist_add_uint64(config, ZPOOL_CONFIG_TIMESTAMP,
		    spa->spa_uberblock.ub_timestamp) == 0);

		/*
		 * Add the list of hot spares.
		 */
		spa_add_spares(spa, config);
	}

	spa_unload(spa);
	spa_deactivate(spa);
	spa_remove(spa);
	mutex_exit(&spa_namespace_lock);

	return (config);
}

/*
 * Pool export/destroy
 *
 * The act of destroying or exporting a pool is very simple.  We make sure there
 * is no more pending I/O and any references to the pool are gone.  Then, we
 * update the pool state and sync all the labels to disk, removing the
 * configuration from the cache afterwards.
 */
static int
spa_export_common(char *pool, int new_state, nvlist_t **oldconfig)
{
	spa_t *spa;

	if (oldconfig)
		*oldconfig = NULL;

	if (!(spa_mode & FWRITE))
		return (EROFS);

	mutex_enter(&spa_namespace_lock);
	if ((spa = spa_lookup(pool)) == NULL) {
		mutex_exit(&spa_namespace_lock);
		return (ENOENT);
	}

	/*
	 * Put a hold on the pool, drop the namespace lock, stop async tasks,
	 * reacquire the namespace lock, and see if we can export.
	 */
	spa_open_ref(spa, FTAG);
	mutex_exit(&spa_namespace_lock);
	spa_async_suspend(spa);
	mutex_enter(&spa_namespace_lock);
	spa_close(spa, FTAG);

	/*
	 * The pool will be in core if it's openable,
	 * in which case we can modify its state.
	 */
	if (spa->spa_state != POOL_STATE_UNINITIALIZED && spa->spa_sync_on) {
		/*
		 * Objsets may be open only because they're dirty, so we
		 * have to force it to sync before checking spa_refcnt.
		 */
		spa_scrub_suspend(spa);
		txg_wait_synced(spa->spa_dsl_pool, 0);

		/*
		 * A pool cannot be exported or destroyed if there are active
		 * references.  If we are resetting a pool, allow references by
		 * fault injection handlers.
		 */
		if (!spa_refcount_zero(spa) ||
		    (spa->spa_inject_ref != 0 &&
		    new_state != POOL_STATE_UNINITIALIZED)) {
			spa_scrub_resume(spa);
			spa_async_resume(spa);
			mutex_exit(&spa_namespace_lock);
			return (EBUSY);
		}

		spa_scrub_resume(spa);
		VERIFY(spa_scrub(spa, POOL_SCRUB_NONE, B_TRUE) == 0);

		/*
		 * We want this to be reflected on every label,
		 * so mark them all dirty.  spa_unload() will do the
		 * final sync that pushes these changes out.
		 */
		if (new_state != POOL_STATE_UNINITIALIZED) {
			spa_config_enter(spa, RW_WRITER, FTAG);
			spa->spa_state = new_state;
			spa->spa_final_txg = spa_last_synced_txg(spa) + 1;
			vdev_config_dirty(spa->spa_root_vdev);
			spa_config_exit(spa, FTAG);
		}
	}

	if (spa->spa_state != POOL_STATE_UNINITIALIZED) {
		spa_unload(spa);
		spa_deactivate(spa);
	}

	if (oldconfig && spa->spa_config)
		VERIFY(nvlist_dup(spa->spa_config, oldconfig, 0) == 0);

	if (new_state != POOL_STATE_UNINITIALIZED) {
		spa_remove(spa);
		spa_config_sync();
	}
	mutex_exit(&spa_namespace_lock);

	return (0);
}

/*
 * Destroy a storage pool.
 */
int
spa_destroy(char *pool)
{
	return (spa_export_common(pool, POOL_STATE_DESTROYED, NULL));
}

/*
 * Export a storage pool.
 */
int
spa_export(char *pool, nvlist_t **oldconfig)
{
	return (spa_export_common(pool, POOL_STATE_EXPORTED, oldconfig));
}

/*
 * Similar to spa_export(), this unloads the spa_t without actually removing it
 * from the namespace in any way.
 */
int
spa_reset(char *pool)
{
	return (spa_export_common(pool, POOL_STATE_UNINITIALIZED, NULL));
}


/*
 * ==========================================================================
 * Device manipulation
 * ==========================================================================
 */

/*
 * Add capacity to a storage pool.
 */
int
spa_vdev_add(spa_t *spa, nvlist_t *nvroot)
{
	uint64_t txg;
	int c, error;
	vdev_t *rvd = spa->spa_root_vdev;
	vdev_t *vd, *tvd;
	nvlist_t **spares;
	uint_t i, nspares;

	txg = spa_vdev_enter(spa);

	if ((error = spa_config_parse(spa, &vd, nvroot, NULL, 0,
	    VDEV_ALLOC_ADD)) != 0)
		return (spa_vdev_exit(spa, NULL, txg, error));

	spa->spa_pending_vdev = vd;

	if (nvlist_lookup_nvlist_array(nvroot, ZPOOL_CONFIG_SPARES,
	    &spares, &nspares) != 0)
		nspares = 0;

	if (vd->vdev_children == 0 && nspares == 0) {
		spa->spa_pending_vdev = NULL;
		return (spa_vdev_exit(spa, vd, txg, EINVAL));
	}

	if (vd->vdev_children != 0) {
		if ((error = vdev_create(vd, txg, B_FALSE)) != 0) {
			spa->spa_pending_vdev = NULL;
			return (spa_vdev_exit(spa, vd, txg, error));
		}
	}

	/*
	 * We must validate the spares after checking the children.  Otherwise,
	 * vdev_inuse() will blindly overwrite the spare.
	 */
	if ((error = spa_validate_spares(spa, nvroot, txg,
	    VDEV_ALLOC_ADD)) != 0) {
		spa->spa_pending_vdev = NULL;
		return (spa_vdev_exit(spa, vd, txg, error));
	}

	spa->spa_pending_vdev = NULL;

	/*
	 * Transfer each new top-level vdev from vd to rvd.
	 */
	for (c = 0; c < vd->vdev_children; c++) {
		tvd = vd->vdev_child[c];
		vdev_remove_child(vd, tvd);
		tvd->vdev_id = rvd->vdev_children;
		vdev_add_child(rvd, tvd);
		vdev_config_dirty(tvd);
	}

	if (nspares != 0) {
		if (spa->spa_sparelist != NULL) {
			nvlist_t **oldspares;
			uint_t oldnspares;
			nvlist_t **newspares;

			VERIFY(nvlist_lookup_nvlist_array(spa->spa_sparelist,
			    ZPOOL_CONFIG_SPARES, &oldspares, &oldnspares) == 0);

			newspares = kmem_alloc(sizeof (void *) *
			    (nspares + oldnspares), KM_SLEEP);
			for (i = 0; i < oldnspares; i++)
				VERIFY(nvlist_dup(oldspares[i],
				    &newspares[i], KM_SLEEP) == 0);
			for (i = 0; i < nspares; i++)
				VERIFY(nvlist_dup(spares[i],
				    &newspares[i + oldnspares],
				    KM_SLEEP) == 0);

			VERIFY(nvlist_remove(spa->spa_sparelist,
			    ZPOOL_CONFIG_SPARES, DATA_TYPE_NVLIST_ARRAY) == 0);

			VERIFY(nvlist_add_nvlist_array(spa->spa_sparelist,
			    ZPOOL_CONFIG_SPARES, newspares,
			    nspares + oldnspares) == 0);
			for (i = 0; i < oldnspares + nspares; i++)
				nvlist_free(newspares[i]);
			kmem_free(newspares, (oldnspares + nspares) *
			    sizeof (void *));
		} else {
			VERIFY(nvlist_alloc(&spa->spa_sparelist,
			    NV_UNIQUE_NAME, KM_SLEEP) == 0);
			VERIFY(nvlist_add_nvlist_array(spa->spa_sparelist,
			    ZPOOL_CONFIG_SPARES, spares, nspares) == 0);
		}

		spa_load_spares(spa);
		spa->spa_sync_spares = B_TRUE;
	}

	/*
	 * We have to be careful when adding new vdevs to an existing pool.
	 * If other threads start allocating from these vdevs before we
	 * sync the config cache, and we lose power, then upon reboot we may
	 * fail to open the pool because there are DVAs that the config cache
	 * can't translate.  Therefore, we first add the vdevs without
	 * initializing metaslabs; sync the config cache (via spa_vdev_exit());
	 * and then let spa_config_update() initialize the new metaslabs.
	 *
	 * spa_load() checks for added-but-not-initialized vdevs, so that
	 * if we lose power at any point in this sequence, the remaining
	 * steps will be completed the next time we load the pool.
	 */
	(void) spa_vdev_exit(spa, vd, txg, 0);

	mutex_enter(&spa_namespace_lock);
	spa_config_update(spa, SPA_CONFIG_UPDATE_POOL);
	mutex_exit(&spa_namespace_lock);

	return (0);
}

/*
 * Attach a device to a mirror.  The arguments are the path to any device
 * in the mirror, and the nvroot for the new device.  If the path specifies
 * a device that is not mirrored, we automatically insert the mirror vdev.
 *
 * If 'replacing' is specified, the new device is intended to replace the
 * existing device; in this case the two devices are made into their own
 * mirror using the 'replacing' vdev, which is functionally idendical to
 * the mirror vdev (it actually reuses all the same ops) but has a few
 * extra rules: you can't attach to it after it's been created, and upon
 * completion of resilvering, the first disk (the one being replaced)
 * is automatically detached.
 */
int
spa_vdev_attach(spa_t *spa, uint64_t guid, nvlist_t *nvroot, int replacing)
{
	uint64_t txg, open_txg;
	int error;
	vdev_t *rvd = spa->spa_root_vdev;
	vdev_t *oldvd, *newvd, *newrootvd, *pvd, *tvd;
	vdev_ops_t *pvops;

	txg = spa_vdev_enter(spa);

	oldvd = vdev_lookup_by_guid(rvd, guid);

	if (oldvd == NULL)
		return (spa_vdev_exit(spa, NULL, txg, ENODEV));

	if (!oldvd->vdev_ops->vdev_op_leaf)
		return (spa_vdev_exit(spa, NULL, txg, ENOTSUP));

	pvd = oldvd->vdev_parent;

	if ((error = spa_config_parse(spa, &newrootvd, nvroot, NULL, 0,
	    VDEV_ALLOC_ADD)) != 0 || newrootvd->vdev_children != 1)
		return (spa_vdev_exit(spa, newrootvd, txg, EINVAL));

	newvd = newrootvd->vdev_child[0];

	if (!newvd->vdev_ops->vdev_op_leaf)
		return (spa_vdev_exit(spa, newrootvd, txg, EINVAL));

	if ((error = vdev_create(newrootvd, txg, replacing)) != 0)
		return (spa_vdev_exit(spa, newrootvd, txg, error));

	if (!replacing) {
		/*
		 * For attach, the only allowable parent is a mirror or the root
		 * vdev.
		 */
		if (pvd->vdev_ops != &vdev_mirror_ops &&
		    pvd->vdev_ops != &vdev_root_ops)
			return (spa_vdev_exit(spa, newrootvd, txg, ENOTSUP));

		pvops = &vdev_mirror_ops;
	} else {
		/*
		 * Active hot spares can only be replaced by inactive hot
		 * spares.
		 */
		if (pvd->vdev_ops == &vdev_spare_ops &&
		    pvd->vdev_child[1] == oldvd &&
		    !spa_has_spare(spa, newvd->vdev_guid))
			return (spa_vdev_exit(spa, newrootvd, txg, ENOTSUP));

		/*
		 * If the source is a hot spare, and the parent isn't already a
		 * spare, then we want to create a new hot spare.  Otherwise, we
		 * want to create a replacing vdev.  The user is not allowed to
		 * attach to a spared vdev child unless the 'isspare' state is
		 * the same (spare replaces spare, non-spare replaces
		 * non-spare).
		 */
		if (pvd->vdev_ops == &vdev_replacing_ops)
			return (spa_vdev_exit(spa, newrootvd, txg, ENOTSUP));
		else if (pvd->vdev_ops == &vdev_spare_ops &&
		    newvd->vdev_isspare != oldvd->vdev_isspare)
			return (spa_vdev_exit(spa, newrootvd, txg, ENOTSUP));
		else if (pvd->vdev_ops != &vdev_spare_ops &&
		    newvd->vdev_isspare)
			pvops = &vdev_spare_ops;
		else
			pvops = &vdev_replacing_ops;
	}

	/*
	 * Compare the new device size with the replaceable/attachable
	 * device size.
	 */
	if (newvd->vdev_psize < vdev_get_rsize(oldvd))
		return (spa_vdev_exit(spa, newrootvd, txg, EOVERFLOW));

	/*
	 * The new device cannot have a higher alignment requirement
	 * than the top-level vdev.
	 */
	if (newvd->vdev_ashift > oldvd->vdev_top->vdev_ashift)
		return (spa_vdev_exit(spa, newrootvd, txg, EDOM));

	/*
	 * If this is an in-place replacement, update oldvd's path and devid
	 * to make it distinguishable from newvd, and unopenable from now on.
	 */
	if (strcmp(oldvd->vdev_path, newvd->vdev_path) == 0) {
		spa_strfree(oldvd->vdev_path);
		oldvd->vdev_path = kmem_alloc(strlen(newvd->vdev_path) + 5,
		    KM_SLEEP);
		(void) sprintf(oldvd->vdev_path, "%s/%s",
		    newvd->vdev_path, "old");
		if (oldvd->vdev_devid != NULL) {
			spa_strfree(oldvd->vdev_devid);
			oldvd->vdev_devid = NULL;
		}
	}

	/*
	 * If the parent is not a mirror, or if we're replacing, insert the new
	 * mirror/replacing/spare vdev above oldvd.
	 */
	if (pvd->vdev_ops != pvops)
		pvd = vdev_add_parent(oldvd, pvops);

	ASSERT(pvd->vdev_top->vdev_parent == rvd);
	ASSERT(pvd->vdev_ops == pvops);
	ASSERT(oldvd->vdev_parent == pvd);

	/*
	 * Extract the new device from its root and add it to pvd.
	 */
	vdev_remove_child(newrootvd, newvd);
	newvd->vdev_id = pvd->vdev_children;
	vdev_add_child(pvd, newvd);

	/*
	 * If newvd is smaller than oldvd, but larger than its rsize,
	 * the addition of newvd may have decreased our parent's asize.
	 */
	pvd->vdev_asize = MIN(pvd->vdev_asize, newvd->vdev_asize);

	tvd = newvd->vdev_top;
	ASSERT(pvd->vdev_top == tvd);
	ASSERT(tvd->vdev_parent == rvd);

	vdev_config_dirty(tvd);

	/*
	 * Set newvd's DTL to [TXG_INITIAL, open_txg].  It will propagate
	 * upward when spa_vdev_exit() calls vdev_dtl_reassess().
	 */
	open_txg = txg + TXG_CONCURRENT_STATES - 1;

	mutex_enter(&newvd->vdev_dtl_lock);
	space_map_add(&newvd->vdev_dtl_map, TXG_INITIAL,
	    open_txg - TXG_INITIAL + 1);
	mutex_exit(&newvd->vdev_dtl_lock);

	if (newvd->vdev_isspare)
		spa_spare_activate(newvd);

	/*
	 * Mark newvd's DTL dirty in this txg.
	 */
	vdev_dirty(tvd, VDD_DTL, newvd, txg);

	(void) spa_vdev_exit(spa, newrootvd, open_txg, 0);

	/*
	 * Kick off a resilver to update newvd.
	 */
	VERIFY(spa_scrub(spa, POOL_SCRUB_RESILVER, B_TRUE) == 0);

	return (0);
}

/*
 * Detach a device from a mirror or replacing vdev.
 * If 'replace_done' is specified, only detach if the parent
 * is a replacing vdev.
 */
int
spa_vdev_detach(spa_t *spa, uint64_t guid, int replace_done)
{
	uint64_t txg;
	int c, t, error;
	vdev_t *rvd = spa->spa_root_vdev;
	vdev_t *vd, *pvd, *cvd, *tvd;
	boolean_t unspare = B_FALSE;
	uint64_t unspare_guid;

	txg = spa_vdev_enter(spa);

	vd = vdev_lookup_by_guid(rvd, guid);

	if (vd == NULL)
		return (spa_vdev_exit(spa, NULL, txg, ENODEV));

	if (!vd->vdev_ops->vdev_op_leaf)
		return (spa_vdev_exit(spa, NULL, txg, ENOTSUP));

	pvd = vd->vdev_parent;

	/*
	 * If replace_done is specified, only remove this device if it's
	 * the first child of a replacing vdev.  For the 'spare' vdev, either
	 * disk can be removed.
	 */
	if (replace_done) {
		if (pvd->vdev_ops == &vdev_replacing_ops) {
			if (vd->vdev_id != 0)
				return (spa_vdev_exit(spa, NULL, txg, ENOTSUP));
		} else if (pvd->vdev_ops != &vdev_spare_ops) {
			return (spa_vdev_exit(spa, NULL, txg, ENOTSUP));
		}
	}

	ASSERT(pvd->vdev_ops != &vdev_spare_ops ||
	    spa_version(spa) >= ZFS_VERSION_SPARES);

	/*
	 * Only mirror, replacing, and spare vdevs support detach.
	 */
	if (pvd->vdev_ops != &vdev_replacing_ops &&
	    pvd->vdev_ops != &vdev_mirror_ops &&
	    pvd->vdev_ops != &vdev_spare_ops)
		return (spa_vdev_exit(spa, NULL, txg, ENOTSUP));

	/*
	 * If there's only one replica, you can't detach it.
	 */
	if (pvd->vdev_children <= 1)
		return (spa_vdev_exit(spa, NULL, txg, EBUSY));

	/*
	 * If all siblings have non-empty DTLs, this device may have the only
	 * valid copy of the data, which means we cannot safely detach it.
	 *
	 * XXX -- as in the vdev_offline() case, we really want a more
	 * precise DTL check.
	 */
	for (c = 0; c < pvd->vdev_children; c++) {
		uint64_t dirty;

		cvd = pvd->vdev_child[c];
		if (cvd == vd)
			continue;
		if (vdev_is_dead(cvd))
			continue;
		mutex_enter(&cvd->vdev_dtl_lock);
		dirty = cvd->vdev_dtl_map.sm_space |
		    cvd->vdev_dtl_scrub.sm_space;
		mutex_exit(&cvd->vdev_dtl_lock);
		if (!dirty)
			break;
	}

	/*
	 * If we are a replacing or spare vdev, then we can always detach the
	 * latter child, as that is how one cancels the operation.
	 */
	if ((pvd->vdev_ops == &vdev_mirror_ops || vd->vdev_id != 1) &&
	    c == pvd->vdev_children)
		return (spa_vdev_exit(spa, NULL, txg, EBUSY));

	/*
	 * If we are detaching the original disk from a spare, then it implies
	 * that the spare should become a real disk, and be removed from the
	 * active spare list for the pool.
	 */
	if (pvd->vdev_ops == &vdev_spare_ops &&
	    vd->vdev_id == 0)
		unspare = B_TRUE;

	/*
	 * Erase the disk labels so the disk can be used for other things.
	 * This must be done after all other error cases are handled,
	 * but before we disembowel vd (so we can still do I/O to it).
	 * But if we can't do it, don't treat the error as fatal --
	 * it may be that the unwritability of the disk is the reason
	 * it's being detached!
	 */
	error = vdev_label_init(vd, 0, VDEV_LABEL_REMOVE);

	/*
	 * Remove vd from its parent and compact the parent's children.
	 */
	vdev_remove_child(pvd, vd);
	vdev_compact_children(pvd);

	/*
	 * Remember one of the remaining children so we can get tvd below.
	 */
	cvd = pvd->vdev_child[0];

	/*
	 * If we need to remove the remaining child from the list of hot spares,
	 * do it now, marking the vdev as no longer a spare in the process.  We
	 * must do this before vdev_remove_parent(), because that can change the
	 * GUID if it creates a new toplevel GUID.
	 */
	if (unspare) {
		ASSERT(cvd->vdev_isspare);
		spa_spare_remove(cvd);
		unspare_guid = cvd->vdev_guid;
	}

	/*
	 * If the parent mirror/replacing vdev only has one child,
	 * the parent is no longer needed.  Remove it from the tree.
	 */
	if (pvd->vdev_children == 1)
		vdev_remove_parent(cvd);

	/*
	 * We don't set tvd until now because the parent we just removed
	 * may have been the previous top-level vdev.
	 */
	tvd = cvd->vdev_top;
	ASSERT(tvd->vdev_parent == rvd);

	/*
	 * Reevaluate the parent vdev state.
	 */
	vdev_propagate_state(cvd->vdev_parent);

	/*
	 * If the device we just detached was smaller than the others, it may be
	 * possible to add metaslabs (i.e. grow the pool).  vdev_metaslab_init()
	 * can't fail because the existing metaslabs are already in core, so
	 * there's nothing to read from disk.
	 */
	VERIFY(vdev_metaslab_init(tvd, txg) == 0);

	vdev_config_dirty(tvd);

	/*
	 * Mark vd's DTL as dirty in this txg.  vdev_dtl_sync() will see that
	 * vd->vdev_detached is set and free vd's DTL object in syncing context.
	 * But first make sure we're not on any *other* txg's DTL list, to
	 * prevent vd from being accessed after it's freed.
	 */
	for (t = 0; t < TXG_SIZE; t++)
		(void) txg_list_remove_this(&tvd->vdev_dtl_list, vd, t);
	vd->vdev_detached = B_TRUE;
	vdev_dirty(tvd, VDD_DTL, vd, txg);

	error = spa_vdev_exit(spa, vd, txg, 0);

	/*
	 * If this was the removal of the original device in a hot spare vdev,
	 * then we want to go through and remove the device from the hot spare
	 * list of every other pool.
	 */
	if (unspare) {
		spa = NULL;
		mutex_enter(&spa_namespace_lock);
		while ((spa = spa_next(spa)) != NULL) {
			if (spa->spa_state != POOL_STATE_ACTIVE)
				continue;

			(void) spa_vdev_remove(spa, unspare_guid, B_TRUE);
		}
		mutex_exit(&spa_namespace_lock);
	}

	return (error);
}

/*
 * Remove a device from the pool.  Currently, this supports removing only hot
 * spares.
 */
int
spa_vdev_remove(spa_t *spa, uint64_t guid, boolean_t unspare)
{
	vdev_t *vd;
	nvlist_t **spares, *nv, **newspares;
	uint_t i, j, nspares;
	int ret = 0;

	spa_config_enter(spa, RW_WRITER, FTAG);

	vd = spa_lookup_by_guid(spa, guid);

	nv = NULL;
	if (spa->spa_spares != NULL &&
	    nvlist_lookup_nvlist_array(spa->spa_sparelist, ZPOOL_CONFIG_SPARES,
	    &spares, &nspares) == 0) {
		for (i = 0; i < nspares; i++) {
			uint64_t theguid;

			VERIFY(nvlist_lookup_uint64(spares[i],
			    ZPOOL_CONFIG_GUID, &theguid) == 0);
			if (theguid == guid) {
				nv = spares[i];
				break;
			}
		}
	}

	/*
	 * We only support removing a hot spare, and only if it's not currently
	 * in use in this pool.
	 */
	if (nv == NULL && vd == NULL) {
		ret = ENOENT;
		goto out;
	}

	if (nv == NULL && vd != NULL) {
		ret = ENOTSUP;
		goto out;
	}

	if (!unspare && nv != NULL && vd != NULL) {
		ret = EBUSY;
		goto out;
	}

	if (nspares == 1) {
		newspares = NULL;
	} else {
		newspares = kmem_alloc((nspares - 1) * sizeof (void *),
		    KM_SLEEP);
		for (i = 0, j = 0; i < nspares; i++) {
			if (spares[i] != nv)
				VERIFY(nvlist_dup(spares[i],
				    &newspares[j++], KM_SLEEP) == 0);
		}
	}

	VERIFY(nvlist_remove(spa->spa_sparelist, ZPOOL_CONFIG_SPARES,
	    DATA_TYPE_NVLIST_ARRAY) == 0);
	VERIFY(nvlist_add_nvlist_array(spa->spa_sparelist, ZPOOL_CONFIG_SPARES,
	    newspares, nspares - 1) == 0);
	for (i = 0; i < nspares - 1; i++)
		nvlist_free(newspares[i]);
	kmem_free(newspares, (nspares - 1) * sizeof (void *));
	spa_load_spares(spa);
	spa->spa_sync_spares = B_TRUE;

out:
	spa_config_exit(spa, FTAG);

	return (ret);
}

/*
 * Find any device that's done replacing, so we can detach it.
 */
static vdev_t *
spa_vdev_replace_done_hunt(vdev_t *vd)
{
	vdev_t *newvd, *oldvd;
	int c;

	for (c = 0; c < vd->vdev_children; c++) {
		oldvd = spa_vdev_replace_done_hunt(vd->vdev_child[c]);
		if (oldvd != NULL)
			return (oldvd);
	}

	if (vd->vdev_ops == &vdev_replacing_ops && vd->vdev_children == 2) {
		oldvd = vd->vdev_child[0];
		newvd = vd->vdev_child[1];

		mutex_enter(&newvd->vdev_dtl_lock);
		if (newvd->vdev_dtl_map.sm_space == 0 &&
		    newvd->vdev_dtl_scrub.sm_space == 0) {
			mutex_exit(&newvd->vdev_dtl_lock);
			return (oldvd);
		}
		mutex_exit(&newvd->vdev_dtl_lock);
	}

	return (NULL);
}

static void
spa_vdev_replace_done(spa_t *spa)
{
	vdev_t *vd;
	vdev_t *pvd;
	uint64_t guid;
	uint64_t pguid = 0;

	spa_config_enter(spa, RW_READER, FTAG);

	while ((vd = spa_vdev_replace_done_hunt(spa->spa_root_vdev)) != NULL) {
		guid = vd->vdev_guid;
		/*
		 * If we have just finished replacing a hot spared device, then
		 * we need to detach the parent's first child (the original hot
		 * spare) as well.
		 */
		pvd = vd->vdev_parent;
		if (pvd->vdev_parent->vdev_ops == &vdev_spare_ops &&
		    pvd->vdev_id == 0) {
			ASSERT(pvd->vdev_ops == &vdev_replacing_ops);
			ASSERT(pvd->vdev_parent->vdev_children == 2);
			pguid = pvd->vdev_parent->vdev_child[1]->vdev_guid;
		}
		spa_config_exit(spa, FTAG);
		if (spa_vdev_detach(spa, guid, B_TRUE) != 0)
			return;
		if (pguid != 0 && spa_vdev_detach(spa, pguid, B_TRUE) != 0)
			return;
		spa_config_enter(spa, RW_READER, FTAG);
	}

	spa_config_exit(spa, FTAG);
}

/*
 * Update the stored path for this vdev.  Dirty the vdev configuration, relying
 * on spa_vdev_enter/exit() to synchronize the labels and cache.
 */
int
spa_vdev_setpath(spa_t *spa, uint64_t guid, const char *newpath)
{
	vdev_t *rvd, *vd;
	uint64_t txg;

	rvd = spa->spa_root_vdev;

	txg = spa_vdev_enter(spa);

	if ((vd = vdev_lookup_by_guid(rvd, guid)) == NULL) {
		/*
		 * Determine if this is a reference to a hot spare.  In that
		 * case, update the path as stored in the spare list.
		 */
		nvlist_t **spares;
		uint_t i, nspares;
		if (spa->spa_sparelist != NULL) {
			VERIFY(nvlist_lookup_nvlist_array(spa->spa_sparelist,
			    ZPOOL_CONFIG_SPARES, &spares, &nspares) == 0);
			for (i = 0; i < nspares; i++) {
				uint64_t theguid;
				VERIFY(nvlist_lookup_uint64(spares[i],
				    ZPOOL_CONFIG_GUID, &theguid) == 0);
				if (theguid == guid)
					break;
			}

			if (i == nspares)
				return (spa_vdev_exit(spa, NULL, txg, ENOENT));

			VERIFY(nvlist_add_string(spares[i],
			    ZPOOL_CONFIG_PATH, newpath) == 0);
			spa_load_spares(spa);
			spa->spa_sync_spares = B_TRUE;
			return (spa_vdev_exit(spa, NULL, txg, 0));
		} else {
			return (spa_vdev_exit(spa, NULL, txg, ENOENT));
		}
	}

	if (!vd->vdev_ops->vdev_op_leaf)
		return (spa_vdev_exit(spa, NULL, txg, ENOTSUP));

	spa_strfree(vd->vdev_path);
	vd->vdev_path = spa_strdup(newpath);

	vdev_config_dirty(vd->vdev_top);

	return (spa_vdev_exit(spa, NULL, txg, 0));
}

/*
 * ==========================================================================
 * SPA Scrubbing
 * ==========================================================================
 */

static void
spa_scrub_io_done(zio_t *zio)
{
	spa_t *spa = zio->io_spa;

	zio_data_buf_free(zio->io_data, zio->io_size);

	mutex_enter(&spa->spa_scrub_lock);
	if (zio->io_error && !(zio->io_flags & ZIO_FLAG_SPECULATIVE)) {
		vdev_t *vd = zio->io_vd ? zio->io_vd : spa->spa_root_vdev;
		spa->spa_scrub_errors++;
		mutex_enter(&vd->vdev_stat_lock);
		vd->vdev_stat.vs_scrub_errors++;
		mutex_exit(&vd->vdev_stat_lock);
	}

	if (--spa->spa_scrub_inflight < spa->spa_scrub_maxinflight)
		cv_broadcast(&spa->spa_scrub_io_cv);

	ASSERT(spa->spa_scrub_inflight >= 0);

	mutex_exit(&spa->spa_scrub_lock);
}

static void
spa_scrub_io_start(spa_t *spa, blkptr_t *bp, int priority, int flags,
    zbookmark_t *zb)
{
	size_t size = BP_GET_LSIZE(bp);
	void *data;

	mutex_enter(&spa->spa_scrub_lock);
	/*
	 * Do not give too much work to vdev(s).
	 */
	while (spa->spa_scrub_inflight >= spa->spa_scrub_maxinflight) {
		cv_wait(&spa->spa_scrub_io_cv, &spa->spa_scrub_lock);
	}
	spa->spa_scrub_inflight++;
	mutex_exit(&spa->spa_scrub_lock);

	data = zio_data_buf_alloc(size);

	if (zb->zb_level == -1 && BP_GET_TYPE(bp) != DMU_OT_OBJSET)
		flags |= ZIO_FLAG_SPECULATIVE;	/* intent log block */

	flags |= ZIO_FLAG_SCRUB_THREAD | ZIO_FLAG_CANFAIL;

	zio_nowait(zio_read(NULL, spa, bp, data, size,
	    spa_scrub_io_done, NULL, priority, flags, zb));
}

/* ARGSUSED */
static int
spa_scrub_cb(traverse_blk_cache_t *bc, spa_t *spa, void *a)
{
	blkptr_t *bp = &bc->bc_blkptr;
	vdev_t *vd = spa->spa_root_vdev;
	dva_t *dva = bp->blk_dva;
	int needs_resilver = B_FALSE;
	int d;

	if (bc->bc_errno) {
		/*
		 * We can't scrub this block, but we can continue to scrub
		 * the rest of the pool.  Note the error and move along.
		 */
		mutex_enter(&spa->spa_scrub_lock);
		spa->spa_scrub_errors++;
		mutex_exit(&spa->spa_scrub_lock);

		mutex_enter(&vd->vdev_stat_lock);
		vd->vdev_stat.vs_scrub_errors++;
		mutex_exit(&vd->vdev_stat_lock);

		return (ERESTART);
	}

	ASSERT(bp->blk_birth < spa->spa_scrub_maxtxg);

	for (d = 0; d < BP_GET_NDVAS(bp); d++) {
		vd = vdev_lookup_top(spa, DVA_GET_VDEV(&dva[d]));

		ASSERT(vd != NULL);

		/*
		 * Keep track of how much data we've examined so that
		 * zpool(1M) status can make useful progress reports.
		 */
		mutex_enter(&vd->vdev_stat_lock);
		vd->vdev_stat.vs_scrub_examined += DVA_GET_ASIZE(&dva[d]);
		mutex_exit(&vd->vdev_stat_lock);

		if (spa->spa_scrub_type == POOL_SCRUB_RESILVER) {
			if (DVA_GET_GANG(&dva[d])) {
				/*
				 * Gang members may be spread across multiple
				 * vdevs, so the best we can do is look at the
				 * pool-wide DTL.
				 * XXX -- it would be better to change our
				 * allocation policy to ensure that this can't
				 * happen.
				 */
				vd = spa->spa_root_vdev;
			}
			if (vdev_dtl_contains(&vd->vdev_dtl_map,
			    bp->blk_birth, 1))
				needs_resilver = B_TRUE;
		}
	}

	if (spa->spa_scrub_type == POOL_SCRUB_EVERYTHING)
		spa_scrub_io_start(spa, bp, ZIO_PRIORITY_SCRUB,
		    ZIO_FLAG_SCRUB, &bc->bc_bookmark);
	else if (needs_resilver)
		spa_scrub_io_start(spa, bp, ZIO_PRIORITY_RESILVER,
		    ZIO_FLAG_RESILVER, &bc->bc_bookmark);

	return (0);
}

static void
spa_scrub_thread(void *arg)
{
	spa_t *spa = arg;
	callb_cpr_t cprinfo;
	traverse_handle_t *th = spa->spa_scrub_th;
	vdev_t *rvd = spa->spa_root_vdev;
	pool_scrub_type_t scrub_type = spa->spa_scrub_type;
	int error = 0;
	boolean_t complete;

	CALLB_CPR_INIT(&cprinfo, &spa->spa_scrub_lock, callb_generic_cpr, FTAG);

	/*
	 * If we're restarting due to a snapshot create/delete,
	 * wait for that to complete.
	 */
	txg_wait_synced(spa_get_dsl(spa), 0);

	dprintf("start %s mintxg=%llu maxtxg=%llu\n",
	    scrub_type == POOL_SCRUB_RESILVER ? "resilver" : "scrub",
	    spa->spa_scrub_mintxg, spa->spa_scrub_maxtxg);

	spa_config_enter(spa, RW_WRITER, FTAG);
	vdev_reopen(rvd);		/* purge all vdev caches */
	vdev_config_dirty(rvd);		/* rewrite all disk labels */
	vdev_scrub_stat_update(rvd, scrub_type, B_FALSE);
	spa_config_exit(spa, FTAG);

	mutex_enter(&spa->spa_scrub_lock);
	spa->spa_scrub_errors = 0;
	spa->spa_scrub_active = 1;
	ASSERT(spa->spa_scrub_inflight == 0);

	while (!spa->spa_scrub_stop) {
		CALLB_CPR_SAFE_BEGIN(&cprinfo);
		while (spa->spa_scrub_suspended) {
			spa->spa_scrub_active = 0;
			cv_broadcast(&spa->spa_scrub_cv);
			cv_wait(&spa->spa_scrub_cv, &spa->spa_scrub_lock);
			spa->spa_scrub_active = 1;
		}
		CALLB_CPR_SAFE_END(&cprinfo, &spa->spa_scrub_lock);

		if (spa->spa_scrub_restart_txg != 0)
			break;

		mutex_exit(&spa->spa_scrub_lock);
		error = traverse_more(th);
		mutex_enter(&spa->spa_scrub_lock);
		if (error != EAGAIN)
			break;
	}

	while (spa->spa_scrub_inflight)
		cv_wait(&spa->spa_scrub_io_cv, &spa->spa_scrub_lock);

	spa->spa_scrub_active = 0;
	cv_broadcast(&spa->spa_scrub_cv);

	mutex_exit(&spa->spa_scrub_lock);

	spa_config_enter(spa, RW_WRITER, FTAG);

	mutex_enter(&spa->spa_scrub_lock);

	/*
	 * Note: we check spa_scrub_restart_txg under both spa_scrub_lock
	 * AND the spa config lock to synchronize with any config changes
	 * that revise the DTLs under spa_vdev_enter() / spa_vdev_exit().
	 */
	if (spa->spa_scrub_restart_txg != 0)
		error = ERESTART;

	if (spa->spa_scrub_stop)
		error = EINTR;

	/*
	 * Even if there were uncorrectable errors, we consider the scrub
	 * completed.  The downside is that if there is a transient error during
	 * a resilver, we won't resilver the data properly to the target.  But
	 * if the damage is permanent (more likely) we will resilver forever,
	 * which isn't really acceptable.  Since there is enough information for
	 * the user to know what has failed and why, this seems like a more
	 * tractable approach.
	 */
	complete = (error == 0);

	dprintf("end %s to maxtxg=%llu %s, traverse=%d, %llu errors, stop=%u\n",
	    scrub_type == POOL_SCRUB_RESILVER ? "resilver" : "scrub",
	    spa->spa_scrub_maxtxg, complete ? "done" : "FAILED",
	    error, spa->spa_scrub_errors, spa->spa_scrub_stop);

	mutex_exit(&spa->spa_scrub_lock);

	/*
	 * If the scrub/resilver completed, update all DTLs to reflect this.
	 * Whether it succeeded or not, vacate all temporary scrub DTLs.
	 */
	vdev_dtl_reassess(rvd, spa_last_synced_txg(spa) + 1,
	    complete ? spa->spa_scrub_maxtxg : 0, B_TRUE);
	vdev_scrub_stat_update(rvd, POOL_SCRUB_NONE, complete);
	spa_errlog_rotate(spa);

	spa_config_exit(spa, FTAG);

	mutex_enter(&spa->spa_scrub_lock);

	/*
	 * We may have finished replacing a device.
	 * Let the async thread assess this and handle the detach.
	 */
	spa_async_request(spa, SPA_ASYNC_REPLACE_DONE);

	/*
	 * If we were told to restart, our final act is to start a new scrub.
	 */
	if (error == ERESTART)
		spa_async_request(spa, scrub_type == POOL_SCRUB_RESILVER ?
		    SPA_ASYNC_RESILVER : SPA_ASYNC_SCRUB);

	spa->spa_scrub_type = POOL_SCRUB_NONE;
	spa->spa_scrub_active = 0;
	spa->spa_scrub_thread = NULL;
	cv_broadcast(&spa->spa_scrub_cv);
	CALLB_CPR_EXIT(&cprinfo);	/* drops &spa->spa_scrub_lock */
	thread_exit();
}

void
spa_scrub_suspend(spa_t *spa)
{
	mutex_enter(&spa->spa_scrub_lock);
	spa->spa_scrub_suspended++;
	while (spa->spa_scrub_active) {
		cv_broadcast(&spa->spa_scrub_cv);
		cv_wait(&spa->spa_scrub_cv, &spa->spa_scrub_lock);
	}
	while (spa->spa_scrub_inflight)
		cv_wait(&spa->spa_scrub_io_cv, &spa->spa_scrub_lock);
	mutex_exit(&spa->spa_scrub_lock);
}

void
spa_scrub_resume(spa_t *spa)
{
	mutex_enter(&spa->spa_scrub_lock);
	ASSERT(spa->spa_scrub_suspended != 0);
	if (--spa->spa_scrub_suspended == 0)
		cv_broadcast(&spa->spa_scrub_cv);
	mutex_exit(&spa->spa_scrub_lock);
}

void
spa_scrub_restart(spa_t *spa, uint64_t txg)
{
	/*
	 * Something happened (e.g. snapshot create/delete) that means
	 * we must restart any in-progress scrubs.  The itinerary will
	 * fix this properly.
	 */
	mutex_enter(&spa->spa_scrub_lock);
	spa->spa_scrub_restart_txg = txg;
	mutex_exit(&spa->spa_scrub_lock);
}

int
spa_scrub(spa_t *spa, pool_scrub_type_t type, boolean_t force)
{
	space_seg_t *ss;
	uint64_t mintxg, maxtxg;
	vdev_t *rvd = spa->spa_root_vdev;

	if ((uint_t)type >= POOL_SCRUB_TYPES)
		return (ENOTSUP);

	mutex_enter(&spa->spa_scrub_lock);

	/*
	 * If there's a scrub or resilver already in progress, stop it.
	 */
	while (spa->spa_scrub_thread != NULL) {
		/*
		 * Don't stop a resilver unless forced.
		 */
		if (spa->spa_scrub_type == POOL_SCRUB_RESILVER && !force) {
			mutex_exit(&spa->spa_scrub_lock);
			return (EBUSY);
		}
		spa->spa_scrub_stop = 1;
		cv_broadcast(&spa->spa_scrub_cv);
		cv_wait(&spa->spa_scrub_cv, &spa->spa_scrub_lock);
	}

	/*
	 * Terminate the previous traverse.
	 */
	if (spa->spa_scrub_th != NULL) {
		traverse_fini(spa->spa_scrub_th);
		spa->spa_scrub_th = NULL;
	}

	if (rvd == NULL) {
		ASSERT(spa->spa_scrub_stop == 0);
		ASSERT(spa->spa_scrub_type == type);
		ASSERT(spa->spa_scrub_restart_txg == 0);
		mutex_exit(&spa->spa_scrub_lock);
		return (0);
	}

	mintxg = TXG_INITIAL - 1;
	maxtxg = spa_last_synced_txg(spa) + 1;

	mutex_enter(&rvd->vdev_dtl_lock);

	if (rvd->vdev_dtl_map.sm_space == 0) {
		/*
		 * The pool-wide DTL is empty.
		 * If this is a resilver, there's nothing to do except
		 * check whether any in-progress replacements have completed.
		 */
		if (type == POOL_SCRUB_RESILVER) {
			type = POOL_SCRUB_NONE;
			spa_async_request(spa, SPA_ASYNC_REPLACE_DONE);
		}
	} else {
		/*
		 * The pool-wide DTL is non-empty.
		 * If this is a normal scrub, upgrade to a resilver instead.
		 */
		if (type == POOL_SCRUB_EVERYTHING)
			type = POOL_SCRUB_RESILVER;
	}

	if (type == POOL_SCRUB_RESILVER) {
		/*
		 * Determine the resilvering boundaries.
		 *
		 * Note: (mintxg, maxtxg) is an open interval,
		 * i.e. mintxg and maxtxg themselves are not included.
		 *
		 * Note: for maxtxg, we MIN with spa_last_synced_txg(spa) + 1
		 * so we don't claim to resilver a txg that's still changing.
		 */
		ss = avl_first(&rvd->vdev_dtl_map.sm_root);
		mintxg = ss->ss_start - 1;
		ss = avl_last(&rvd->vdev_dtl_map.sm_root);
		maxtxg = MIN(ss->ss_end, maxtxg);
	}

	mutex_exit(&rvd->vdev_dtl_lock);

	spa->spa_scrub_stop = 0;
	spa->spa_scrub_type = type;
	spa->spa_scrub_restart_txg = 0;

	if (type != POOL_SCRUB_NONE) {
		spa->spa_scrub_mintxg = mintxg;
		spa->spa_scrub_maxtxg = maxtxg;
		spa->spa_scrub_th = traverse_init(spa, spa_scrub_cb, NULL,
		    ADVANCE_PRE | ADVANCE_PRUNE | ADVANCE_ZIL,
		    ZIO_FLAG_CANFAIL);
		traverse_add_pool(spa->spa_scrub_th, mintxg, maxtxg);
		spa->spa_scrub_thread = thread_create(NULL, 0,
		    spa_scrub_thread, spa, 0, &p0, TS_RUN, minclsyspri);
	}

	mutex_exit(&spa->spa_scrub_lock);

	return (0);
}

/*
 * ==========================================================================
 * SPA async task processing
 * ==========================================================================
 */

static void
spa_async_reopen(spa_t *spa)
{
	vdev_t *rvd = spa->spa_root_vdev;
	vdev_t *tvd;
	int c;

	spa_config_enter(spa, RW_WRITER, FTAG);

	for (c = 0; c < rvd->vdev_children; c++) {
		tvd = rvd->vdev_child[c];
		if (tvd->vdev_reopen_wanted) {
			tvd->vdev_reopen_wanted = 0;
			vdev_reopen(tvd);
		}
	}

	spa_config_exit(spa, FTAG);
}

static void
spa_async_thread(void *arg)
{
	spa_t *spa = arg;
	int tasks;

	ASSERT(spa->spa_sync_on);

	mutex_enter(&spa->spa_async_lock);
	tasks = spa->spa_async_tasks;
	spa->spa_async_tasks = 0;
	mutex_exit(&spa->spa_async_lock);

	/*
	 * See if the config needs to be updated.
	 */
	if (tasks & SPA_ASYNC_CONFIG_UPDATE) {
		mutex_enter(&spa_namespace_lock);
		spa_config_update(spa, SPA_CONFIG_UPDATE_POOL);
		mutex_exit(&spa_namespace_lock);
	}

	/*
	 * See if any devices need to be reopened.
	 */
	if (tasks & SPA_ASYNC_REOPEN)
		spa_async_reopen(spa);

	/*
	 * If any devices are done replacing, detach them.
	 */
	if (tasks & SPA_ASYNC_REPLACE_DONE)
		spa_vdev_replace_done(spa);

	/*
	 * Kick off a scrub.
	 */
	if (tasks & SPA_ASYNC_SCRUB)
		VERIFY(spa_scrub(spa, POOL_SCRUB_EVERYTHING, B_TRUE) == 0);

	/*
	 * Kick off a resilver.
	 */
	if (tasks & SPA_ASYNC_RESILVER)
		VERIFY(spa_scrub(spa, POOL_SCRUB_RESILVER, B_TRUE) == 0);

	/*
	 * Let the world know that we're done.
	 */
	mutex_enter(&spa->spa_async_lock);
	spa->spa_async_thread = NULL;
	cv_broadcast(&spa->spa_async_cv);
	mutex_exit(&spa->spa_async_lock);
	thread_exit();
}

void
spa_async_suspend(spa_t *spa)
{
	mutex_enter(&spa->spa_async_lock);
	spa->spa_async_suspended++;
	while (spa->spa_async_thread != NULL)
		cv_wait(&spa->spa_async_cv, &spa->spa_async_lock);
	mutex_exit(&spa->spa_async_lock);
}

void
spa_async_resume(spa_t *spa)
{
	mutex_enter(&spa->spa_async_lock);
	ASSERT(spa->spa_async_suspended != 0);
	spa->spa_async_suspended--;
	mutex_exit(&spa->spa_async_lock);
}

static void
spa_async_dispatch(spa_t *spa)
{
	mutex_enter(&spa->spa_async_lock);
	if (spa->spa_async_tasks && !spa->spa_async_suspended &&
	    spa->spa_async_thread == NULL &&
	    rootdir != NULL && !vn_is_readonly(rootdir))
		spa->spa_async_thread = thread_create(NULL, 0,
		    spa_async_thread, spa, 0, &p0, TS_RUN, maxclsyspri);
	mutex_exit(&spa->spa_async_lock);
}

void
spa_async_request(spa_t *spa, int task)
{
	mutex_enter(&spa->spa_async_lock);
	spa->spa_async_tasks |= task;
	mutex_exit(&spa->spa_async_lock);
}

/*
 * ==========================================================================
 * SPA syncing routines
 * ==========================================================================
 */

static void
spa_sync_deferred_frees(spa_t *spa, uint64_t txg)
{
	bplist_t *bpl = &spa->spa_sync_bplist;
	dmu_tx_t *tx;
	blkptr_t blk;
	uint64_t itor = 0;
	zio_t *zio;
	int error;
	uint8_t c = 1;

	zio = zio_root(spa, NULL, NULL, ZIO_FLAG_CONFIG_HELD);

	while (bplist_iterate(bpl, &itor, &blk) == 0)
		zio_nowait(zio_free(zio, spa, txg, &blk, NULL, NULL));

	error = zio_wait(zio);
	ASSERT3U(error, ==, 0);

	tx = dmu_tx_create_assigned(spa->spa_dsl_pool, txg);
	bplist_vacate(bpl, tx);

	/*
	 * Pre-dirty the first block so we sync to convergence faster.
	 * (Usually only the first block is needed.)
	 */
	dmu_write(spa->spa_meta_objset, spa->spa_sync_bplist_obj, 0, 1, &c, tx);
	dmu_tx_commit(tx);
}

static void
spa_sync_nvlist(spa_t *spa, uint64_t obj, nvlist_t *nv, dmu_tx_t *tx)
{
	char *packed = NULL;
	size_t nvsize = 0;
	dmu_buf_t *db;

	VERIFY(nvlist_size(nv, &nvsize, NV_ENCODE_XDR) == 0);

	packed = kmem_alloc(nvsize, KM_SLEEP);

	VERIFY(nvlist_pack(nv, &packed, &nvsize, NV_ENCODE_XDR,
	    KM_SLEEP) == 0);

	dmu_write(spa->spa_meta_objset, obj, 0, nvsize, packed, tx);

	kmem_free(packed, nvsize);

	VERIFY(0 == dmu_bonus_hold(spa->spa_meta_objset, obj, FTAG, &db));
	dmu_buf_will_dirty(db, tx);
	*(uint64_t *)db->db_data = nvsize;
	dmu_buf_rele(db, FTAG);
}

static void
spa_sync_spares(spa_t *spa, dmu_tx_t *tx)
{
	nvlist_t *nvroot;
	nvlist_t **spares;
	int i;

	if (!spa->spa_sync_spares)
		return;

	/*
	 * Update the MOS nvlist describing the list of available spares.
	 * spa_validate_spares() will have already made sure this nvlist is
	 * valid and the vdevs are labelled appropriately.
	 */
	if (spa->spa_spares_object == 0) {
		spa->spa_spares_object = dmu_object_alloc(spa->spa_meta_objset,
		    DMU_OT_PACKED_NVLIST, 1 << 14,
		    DMU_OT_PACKED_NVLIST_SIZE, sizeof (uint64_t), tx);
		VERIFY(zap_update(spa->spa_meta_objset,
		    DMU_POOL_DIRECTORY_OBJECT, DMU_POOL_SPARES,
		    sizeof (uint64_t), 1, &spa->spa_spares_object, tx) == 0);
	}

	VERIFY(nvlist_alloc(&nvroot, NV_UNIQUE_NAME, KM_SLEEP) == 0);
	if (spa->spa_nspares == 0) {
		VERIFY(nvlist_add_nvlist_array(nvroot, ZPOOL_CONFIG_SPARES,
		    NULL, 0) == 0);
	} else {
		spares = kmem_alloc(spa->spa_nspares * sizeof (void *),
		    KM_SLEEP);
		for (i = 0; i < spa->spa_nspares; i++)
			spares[i] = vdev_config_generate(spa,
			    spa->spa_spares[i], B_FALSE, B_TRUE);
		VERIFY(nvlist_add_nvlist_array(nvroot, ZPOOL_CONFIG_SPARES,
		    spares, spa->spa_nspares) == 0);
		for (i = 0; i < spa->spa_nspares; i++)
			nvlist_free(spares[i]);
		kmem_free(spares, spa->spa_nspares * sizeof (void *));
	}

	spa_sync_nvlist(spa, spa->spa_spares_object, nvroot, tx);
	nvlist_free(nvroot);

	spa->spa_sync_spares = B_FALSE;
}

static void
spa_sync_config_object(spa_t *spa, dmu_tx_t *tx)
{
	nvlist_t *config;

	if (list_is_empty(&spa->spa_dirty_list))
		return;

	config = spa_config_generate(spa, NULL, dmu_tx_get_txg(tx), B_FALSE);

	if (spa->spa_config_syncing)
		nvlist_free(spa->spa_config_syncing);
	spa->spa_config_syncing = config;

	spa_sync_nvlist(spa, spa->spa_config_object, config, tx);
}

static void
spa_sync_props(void *arg1, void *arg2, dmu_tx_t *tx)
{
	spa_t *spa = arg1;
	nvlist_t *nvp = arg2;
	nvpair_t *nvpair;
	objset_t *mos = spa->spa_meta_objset;
	uint64_t zapobj;

	mutex_enter(&spa->spa_props_lock);
	if (spa->spa_pool_props_object == 0) {
		zapobj = zap_create(mos, DMU_OT_POOL_PROPS, DMU_OT_NONE, 0, tx);
		VERIFY(zapobj > 0);

		spa->spa_pool_props_object = zapobj;

		VERIFY(zap_update(mos, DMU_POOL_DIRECTORY_OBJECT,
		    DMU_POOL_PROPS, 8, 1,
		    &spa->spa_pool_props_object, tx) == 0);
	}
	mutex_exit(&spa->spa_props_lock);

	nvpair = NULL;
	while ((nvpair = nvlist_next_nvpair(nvp, nvpair))) {
		switch (zpool_name_to_prop(nvpair_name(nvpair))) {
		case ZFS_PROP_BOOTFS:
			VERIFY(nvlist_lookup_uint64(nvp,
			    nvpair_name(nvpair), &spa->spa_bootfs) == 0);
			VERIFY(zap_update(mos,
			    spa->spa_pool_props_object,
			    zpool_prop_to_name(ZFS_PROP_BOOTFS), 8, 1,
			    &spa->spa_bootfs, tx) == 0);
			break;
		}
	}
}

/*
 * Sync the specified transaction group.  New blocks may be dirtied as
 * part of the process, so we iterate until it converges.
 */
void
spa_sync(spa_t *spa, uint64_t txg)
{
	dsl_pool_t *dp = spa->spa_dsl_pool;
	objset_t *mos = spa->spa_meta_objset;
	bplist_t *bpl = &spa->spa_sync_bplist;
	vdev_t *rvd = spa->spa_root_vdev;
	vdev_t *vd;
	dmu_tx_t *tx;
	int dirty_vdevs;

	/*
	 * Lock out configuration changes.
	 */
	spa_config_enter(spa, RW_READER, FTAG);

	spa->spa_syncing_txg = txg;
	spa->spa_sync_pass = 0;

	VERIFY(0 == bplist_open(bpl, mos, spa->spa_sync_bplist_obj));

	tx = dmu_tx_create_assigned(dp, txg);

	/*
	 * If we are upgrading to ZFS_VERSION_RAIDZ_DEFLATE this txg,
	 * set spa_deflate if we have no raid-z vdevs.
	 */
	if (spa->spa_ubsync.ub_version < ZFS_VERSION_RAIDZ_DEFLATE &&
	    spa->spa_uberblock.ub_version >= ZFS_VERSION_RAIDZ_DEFLATE) {
		int i;

		for (i = 0; i < rvd->vdev_children; i++) {
			vd = rvd->vdev_child[i];
			if (vd->vdev_deflate_ratio != SPA_MINBLOCKSIZE)
				break;
		}
		if (i == rvd->vdev_children) {
			spa->spa_deflate = TRUE;
			VERIFY(0 == zap_add(spa->spa_meta_objset,
			    DMU_POOL_DIRECTORY_OBJECT, DMU_POOL_DEFLATE,
			    sizeof (uint64_t), 1, &spa->spa_deflate, tx));
		}
	}

	/*
	 * If anything has changed in this txg, push the deferred frees
	 * from the previous txg.  If not, leave them alone so that we
	 * don't generate work on an otherwise idle system.
	 */
	if (!txg_list_empty(&dp->dp_dirty_datasets, txg) ||
	    !txg_list_empty(&dp->dp_dirty_dirs, txg) ||
	    !txg_list_empty(&dp->dp_sync_tasks, txg))
		spa_sync_deferred_frees(spa, txg);

	/*
	 * Iterate to convergence.
	 */
	do {
		spa->spa_sync_pass++;

		spa_sync_config_object(spa, tx);
		spa_sync_spares(spa, tx);
		spa_errlog_sync(spa, txg);
		dsl_pool_sync(dp, txg);

		dirty_vdevs = 0;
		while (vd = txg_list_remove(&spa->spa_vdev_txg_list, txg)) {
			vdev_sync(vd, txg);
			dirty_vdevs++;
		}

		bplist_sync(bpl, tx);
	} while (dirty_vdevs);

	bplist_close(bpl);

	dprintf("txg %llu passes %d\n", txg, spa->spa_sync_pass);

	/*
	 * Rewrite the vdev configuration (which includes the uberblock)
	 * to commit the transaction group.
	 *
	 * If there are any dirty vdevs, sync the uberblock to all vdevs.
	 * Otherwise, pick a random top-level vdev that's known to be
	 * visible in the config cache (see spa_vdev_add() for details).
	 * If the write fails, try the next vdev until we're tried them all.
	 */
	if (!list_is_empty(&spa->spa_dirty_list)) {
		VERIFY(vdev_config_sync(rvd, txg) == 0);
	} else {
		int children = rvd->vdev_children;
		int c0 = spa_get_random(children);
		int c;

		for (c = 0; c < children; c++) {
			vd = rvd->vdev_child[(c0 + c) % children];
			if (vd->vdev_ms_array == 0)
				continue;
			if (vdev_config_sync(vd, txg) == 0)
				break;
		}
		if (c == children)
			VERIFY(vdev_config_sync(rvd, txg) == 0);
	}

	dmu_tx_commit(tx);

	/*
	 * Clear the dirty config list.
	 */
	while ((vd = list_head(&spa->spa_dirty_list)) != NULL)
		vdev_config_clean(vd);

	/*
	 * Now that the new config has synced transactionally,
	 * let it become visible to the config cache.
	 */
	if (spa->spa_config_syncing != NULL) {
		spa_config_set(spa, spa->spa_config_syncing);
		spa->spa_config_txg = txg;
		spa->spa_config_syncing = NULL;
	}

	/*
	 * Make a stable copy of the fully synced uberblock.
	 * We use this as the root for pool traversals.
	 */
	spa->spa_traverse_wanted = 1;	/* tells traverse_more() to stop */

	spa_scrub_suspend(spa);		/* stop scrubbing and finish I/Os */

	rw_enter(&spa->spa_traverse_lock, RW_WRITER);
	spa->spa_traverse_wanted = 0;
	spa->spa_ubsync = spa->spa_uberblock;
	rw_exit(&spa->spa_traverse_lock);

	spa_scrub_resume(spa);		/* resume scrub with new ubsync */

	/*
	 * Clean up the ZIL records for the synced txg.
	 */
	dsl_pool_zil_clean(dp);

	/*
	 * Update usable space statistics.
	 */
	while (vd = txg_list_remove(&spa->spa_vdev_txg_list, TXG_CLEAN(txg)))
		vdev_sync_done(vd, txg);

	/*
	 * It had better be the case that we didn't dirty anything
	 * since vdev_config_sync().
	 */
	ASSERT(txg_list_empty(&dp->dp_dirty_datasets, txg));
	ASSERT(txg_list_empty(&dp->dp_dirty_dirs, txg));
	ASSERT(txg_list_empty(&spa->spa_vdev_txg_list, txg));
	ASSERT(bpl->bpl_queue == NULL);

	spa_config_exit(spa, FTAG);

	/*
	 * If any async tasks have been requested, kick them off.
	 */
	spa_async_dispatch(spa);
}

/*
 * Sync all pools.  We don't want to hold the namespace lock across these
 * operations, so we take a reference on the spa_t and drop the lock during the
 * sync.
 */
void
spa_sync_allpools(void)
{
	spa_t *spa = NULL;
	mutex_enter(&spa_namespace_lock);
	while ((spa = spa_next(spa)) != NULL) {
		if (spa_state(spa) != POOL_STATE_ACTIVE)
			continue;
		spa_open_ref(spa, FTAG);
		mutex_exit(&spa_namespace_lock);
		txg_wait_synced(spa_get_dsl(spa), 0);
		mutex_enter(&spa_namespace_lock);
		spa_close(spa, FTAG);
	}
	mutex_exit(&spa_namespace_lock);
}

/*
 * ==========================================================================
 * Miscellaneous routines
 * ==========================================================================
 */

/*
 * Remove all pools in the system.
 */
void
spa_evict_all(void)
{
	spa_t *spa;

	/*
	 * Remove all cached state.  All pools should be closed now,
	 * so every spa in the AVL tree should be unreferenced.
	 */
	mutex_enter(&spa_namespace_lock);
	while ((spa = spa_next(NULL)) != NULL) {
		/*
		 * Stop async tasks.  The async thread may need to detach
		 * a device that's been replaced, which requires grabbing
		 * spa_namespace_lock, so we must drop it here.
		 */
		spa_open_ref(spa, FTAG);
		mutex_exit(&spa_namespace_lock);
		spa_async_suspend(spa);
		VERIFY(spa_scrub(spa, POOL_SCRUB_NONE, B_TRUE) == 0);
		mutex_enter(&spa_namespace_lock);
		spa_close(spa, FTAG);

		if (spa->spa_state != POOL_STATE_UNINITIALIZED) {
			spa_unload(spa);
			spa_deactivate(spa);
		}
		spa_remove(spa);
	}
	mutex_exit(&spa_namespace_lock);
}

vdev_t *
spa_lookup_by_guid(spa_t *spa, uint64_t guid)
{
	return (vdev_lookup_by_guid(spa->spa_root_vdev, guid));
}

void
spa_upgrade(spa_t *spa)
{
	spa_config_enter(spa, RW_WRITER, FTAG);

	/*
	 * This should only be called for a non-faulted pool, and since a
	 * future version would result in an unopenable pool, this shouldn't be
	 * possible.
	 */
	ASSERT(spa->spa_uberblock.ub_version <= ZFS_VERSION);

	spa->spa_uberblock.ub_version = ZFS_VERSION;
	vdev_config_dirty(spa->spa_root_vdev);

	spa_config_exit(spa, FTAG);

	txg_wait_synced(spa_get_dsl(spa), 0);
}

boolean_t
spa_has_spare(spa_t *spa, uint64_t guid)
{
	int i;
	uint64_t spareguid;

	for (i = 0; i < spa->spa_nspares; i++)
		if (spa->spa_spares[i]->vdev_guid == guid)
			return (B_TRUE);

	for (i = 0; i < spa->spa_pending_nspares; i++) {
		if (nvlist_lookup_uint64(spa->spa_pending_spares[i],
		    ZPOOL_CONFIG_GUID, &spareguid) == 0 &&
		    spareguid == guid)
			return (B_TRUE);
	}

	return (B_FALSE);
}

int
spa_set_props(spa_t *spa, nvlist_t *nvp)
{
	return (dsl_sync_task_do(spa_get_dsl(spa), NULL, spa_sync_props,
	    spa, nvp, 3));
}

int
spa_get_props(spa_t *spa, nvlist_t **nvp)
{
	zap_cursor_t zc;
	zap_attribute_t za;
	objset_t *mos = spa->spa_meta_objset;
	zfs_source_t src;
	zfs_prop_t prop;
	nvlist_t *propval;
	uint64_t value;
	int err;

	VERIFY(nvlist_alloc(nvp, NV_UNIQUE_NAME, KM_SLEEP) == 0);

	mutex_enter(&spa->spa_props_lock);
	/* If no props object, then just return empty nvlist */
	if (spa->spa_pool_props_object == 0) {
		mutex_exit(&spa->spa_props_lock);
		return (0);
	}

	for (zap_cursor_init(&zc, mos, spa->spa_pool_props_object);
	    (err = zap_cursor_retrieve(&zc, &za)) == 0;
	    zap_cursor_advance(&zc)) {

		if ((prop = zpool_name_to_prop(za.za_name)) == ZFS_PROP_INVAL)
			continue;

		VERIFY(nvlist_alloc(&propval, NV_UNIQUE_NAME, KM_SLEEP) == 0);
		switch (za.za_integer_length) {
		case 8:
			if (zfs_prop_default_numeric(prop) ==
			    za.za_first_integer)
				src = ZFS_SRC_DEFAULT;
			else
				src = ZFS_SRC_LOCAL;
			value = za.za_first_integer;

			if (prop == ZFS_PROP_BOOTFS) {
				dsl_pool_t *dp;
				dsl_dataset_t *ds = NULL;
				char strval[MAXPATHLEN];

				dp = spa_get_dsl(spa);
				rw_enter(&dp->dp_config_rwlock, RW_READER);
				if ((err = dsl_dataset_open_obj(dp,
				    za.za_first_integer, NULL, DS_MODE_NONE,
				    FTAG, &ds)) != 0) {
					rw_exit(&dp->dp_config_rwlock);
					break;
				}
				dsl_dataset_name(ds, strval);
				dsl_dataset_close(ds, DS_MODE_NONE, FTAG);
				rw_exit(&dp->dp_config_rwlock);

				VERIFY(nvlist_add_uint64(propval,
				    ZFS_PROP_SOURCE, src) == 0);
				VERIFY(nvlist_add_string(propval,
				    ZFS_PROP_VALUE, strval) == 0);
			} else {
				VERIFY(nvlist_add_uint64(propval,
				    ZFS_PROP_SOURCE, src) == 0);
				VERIFY(nvlist_add_uint64(propval,
				    ZFS_PROP_VALUE, value) == 0);
			}
			VERIFY(nvlist_add_nvlist(*nvp, za.za_name,
			    propval) == 0);
			break;
		}
		nvlist_free(propval);
	}
	zap_cursor_fini(&zc);
	mutex_exit(&spa->spa_props_lock);
	if (err && err != ENOENT) {
		nvlist_free(*nvp);
		return (err);
	}

	return (0);
}

/*
 * If the bootfs property value is dsobj, clear it.
 */
void
spa_clear_bootfs(spa_t *spa, uint64_t dsobj, dmu_tx_t *tx)
{
	if (spa->spa_bootfs == dsobj && spa->spa_pool_props_object != 0) {
		VERIFY(zap_remove(spa->spa_meta_objset,
		    spa->spa_pool_props_object,
		    zpool_prop_to_name(ZFS_PROP_BOOTFS), tx) == 0);
		spa->spa_bootfs = 0;
	}
}
