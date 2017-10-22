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
 * This file contains the functions which analyze the status of a pool.  This
 * include both the status of an active pool, as well as the status exported
 * pools.  Returns one of the ZPOOL_STATUS_* defines describing the status of
 * the pool.  This status is independent (to a certain degree) from the state of
 * the pool.  A pool's state descsribes only whether or not it is capable of
 * providing the necessary fault tolerance for data.  The status describes the
 * overall status of devices.  A pool that is online can still have a device
 * that is experiencing errors.
 *
 * Only a subset of the possible faults can be detected using 'zpool status',
 * and not all possible errors correspond to a FMA message ID.  The explanation
 * is left up to the caller, depending on whether it is a live pool or an
 * import.
 */

#include <libzfs.h>
#include <string.h>
#include <unistd.h>
#include "libzfs_impl.h"

/*
 * Message ID table.  This must be kep in sync with the ZPOOL_STATUS_* defines
 * in libzfs.h.  Note that there are some status results which go past the end
 * of this table, and hence have no associated message ID.
 */
static char *zfs_msgid_table[] = {
	"ZFS-8000-14",
	"ZFS-8000-2Q",
	"ZFS-8000-3C",
	"ZFS-8000-4J",
	"ZFS-8000-5E",
	"ZFS-8000-6X",
	"ZFS-8000-72",
	"ZFS-8000-8A",
	"ZFS-8000-9P",
	"ZFS-8000-A5",
	"ZFS-8000-EY"
};

/*
 * If the pool is active, a certain class of static errors is overridden by the
 * faults as analayzed by FMA.  These faults have separate knowledge articles,
 * and the article referred to by 'zpool status' must match that indicated by
 * the syslog error message.  We override missing data as well as corrupt pool.
 */
static char *zfs_msgid_table_active[] = {
	"ZFS-8000-14",
	"ZFS-8000-D3",		/* overridden */
	"ZFS-8000-D3",		/* overridden */
	"ZFS-8000-4J",
	"ZFS-8000-5E",
	"ZFS-8000-6X",
	"ZFS-8000-CS",		/* overridden */
	"ZFS-8000-8A",
	"ZFS-8000-9P",
	"ZFS-8000-CS",		/* overridden */
};

#define	NMSGID	(sizeof (zfs_msgid_table) / sizeof (zfs_msgid_table[0]))

/* ARGSUSED */
static int
vdev_missing(uint64_t state, uint64_t aux, uint64_t errs)
{
	return (state == VDEV_STATE_CANT_OPEN &&
	    aux == VDEV_AUX_OPEN_FAILED);
}

/* ARGSUSED */
static int
vdev_errors(uint64_t state, uint64_t aux, uint64_t errs)
{
	return (errs != 0);
}

/* ARGSUSED */
static int
vdev_broken(uint64_t state, uint64_t aux, uint64_t errs)
{
	return (state == VDEV_STATE_CANT_OPEN);
}

/* ARGSUSED */
static int
vdev_offlined(uint64_t state, uint64_t aux, uint64_t errs)
{
	return (state == VDEV_STATE_OFFLINE);
}

/*
 * Detect if any leaf devices that have seen errors or could not be opened.
 */
static boolean_t
find_vdev_problem(nvlist_t *vdev, int (*func)(uint64_t, uint64_t, uint64_t))
{
	nvlist_t **child;
	vdev_stat_t *vs;
	uint_t c, children;
	char *type;

	/*
	 * Ignore problems within a 'replacing' vdev, since we're presumably in
	 * the process of repairing any such errors, and don't want to call them
	 * out again.  We'll pick up the fact that a resilver is happening
	 * later.
	 */
	verify(nvlist_lookup_string(vdev, ZPOOL_CONFIG_TYPE, &type) == 0);
	if (strcmp(type, VDEV_TYPE_REPLACING) == 0)
		return (B_FALSE);

	if (nvlist_lookup_nvlist_array(vdev, ZPOOL_CONFIG_CHILDREN, &child,
	    &children) == 0) {
		for (c = 0; c < children; c++)
			if (find_vdev_problem(child[c], func))
				return (B_TRUE);
	} else {
		verify(nvlist_lookup_uint64_array(vdev, ZPOOL_CONFIG_STATS,
		    (uint64_t **)&vs, &c) == 0);

		if (func(vs->vs_state, vs->vs_aux,
		    vs->vs_read_errors +
		    vs->vs_write_errors +
		    vs->vs_checksum_errors))
			return (B_TRUE);
	}

	return (B_FALSE);
}

/*
 * Active pool health status.
 *
 * To determine the status for a pool, we make several passes over the config,
 * picking the most egregious error we find.  In order of importance, we do the
 * following:
 *
 *	- Check for a complete and valid configuration
 *	- Look for any missing devices in a non-replicated config
 *	- Check for any data errors
 *	- Check for any missing devices in a replicated config
 *	- Look for any devices showing errors
 *	- Check for any resilvering devices
 *
 * There can obviously be multiple errors within a single pool, so this routine
 * only picks the most damaging of all the current errors to report.
 */
static zpool_status_t
check_status(nvlist_t *config, boolean_t isimport)
{
	nvlist_t *nvroot;
	vdev_stat_t *vs;
	uint_t vsc;
	uint64_t nerr;
	uint64_t version;
	uint64_t stateval;
	uint64_t hostid = 0;

	verify(nvlist_lookup_uint64(config, ZPOOL_CONFIG_VERSION,
	    &version) == 0);
	verify(nvlist_lookup_nvlist(config, ZPOOL_CONFIG_VDEV_TREE,
	    &nvroot) == 0);
	verify(nvlist_lookup_uint64_array(nvroot, ZPOOL_CONFIG_STATS,
	    (uint64_t **)&vs, &vsc) == 0);
	verify(nvlist_lookup_uint64(config, ZPOOL_CONFIG_POOL_STATE,
	    &stateval) == 0);
	(void) nvlist_lookup_uint64(config, ZPOOL_CONFIG_HOSTID, &hostid);

	/*
	 * Pool last accessed by another system.
	 */
	if (hostid != 0 && (unsigned long)hostid != gethostid() &&
	    stateval == POOL_STATE_ACTIVE)
		return (ZPOOL_STATUS_HOSTID_MISMATCH);

	/*
	 * Newer on-disk version.
	 */
	if (vs->vs_state == VDEV_STATE_CANT_OPEN &&
	    vs->vs_aux == VDEV_AUX_VERSION_NEWER)
		return (ZPOOL_STATUS_VERSION_NEWER);

	/*
	 * Check that the config is complete.
	 */
	if (vs->vs_state == VDEV_STATE_CANT_OPEN &&
	    vs->vs_aux == VDEV_AUX_BAD_GUID_SUM)
		return (ZPOOL_STATUS_BAD_GUID_SUM);

	/*
	 * Missing devices in non-replicated config.
	 */
	if (vs->vs_state == VDEV_STATE_CANT_OPEN &&
	    find_vdev_problem(nvroot, vdev_missing))
		return (ZPOOL_STATUS_MISSING_DEV_NR);

	if (vs->vs_state == VDEV_STATE_CANT_OPEN &&
	    find_vdev_problem(nvroot, vdev_broken))
		return (ZPOOL_STATUS_CORRUPT_LABEL_NR);

	/*
	 * Corrupted pool metadata
	 */
	if (vs->vs_state == VDEV_STATE_CANT_OPEN &&
	    vs->vs_aux == VDEV_AUX_CORRUPT_DATA)
		return (ZPOOL_STATUS_CORRUPT_POOL);

	/*
	 * Persistent data errors.
	 */
	if (!isimport) {
		if (nvlist_lookup_uint64(config, ZPOOL_CONFIG_ERRCOUNT,
		    &nerr) == 0 && nerr != 0)
			return (ZPOOL_STATUS_CORRUPT_DATA);
	}

	/*
	 * Missing devices in a replicated config.
	 */
	if (find_vdev_problem(nvroot, vdev_missing))
		return (ZPOOL_STATUS_MISSING_DEV_R);
	if (find_vdev_problem(nvroot, vdev_broken))
		return (ZPOOL_STATUS_CORRUPT_LABEL_R);

	/*
	 * Devices with errors
	 */
	if (!isimport && find_vdev_problem(nvroot, vdev_errors))
		return (ZPOOL_STATUS_FAILING_DEV);

	/*
	 * Offlined devices
	 */
	if (find_vdev_problem(nvroot, vdev_offlined))
		return (ZPOOL_STATUS_OFFLINE_DEV);

	/*
	 * Currently resilvering
	 */
	if (!vs->vs_scrub_complete && vs->vs_scrub_type == POOL_SCRUB_RESILVER)
		return (ZPOOL_STATUS_RESILVERING);

	/*
	 * Outdated, but usable, version
	 */
	if (version < ZFS_VERSION)
		return (ZPOOL_STATUS_VERSION_OLDER);

	return (ZPOOL_STATUS_OK);
}

zpool_status_t
zpool_get_status(zpool_handle_t *zhp, char **msgid)
{
	zpool_status_t ret = check_status(zhp->zpool_config, B_FALSE);

	if (ret >= NMSGID)
		*msgid = NULL;
	else
		*msgid = zfs_msgid_table_active[ret];

	return (ret);
}

zpool_status_t
zpool_import_status(nvlist_t *config, char **msgid)
{
	zpool_status_t ret = check_status(config, B_TRUE);

	if (ret >= NMSGID)
		*msgid = NULL;
	else
		*msgid = zfs_msgid_table[ret];

	return (ret);
}
