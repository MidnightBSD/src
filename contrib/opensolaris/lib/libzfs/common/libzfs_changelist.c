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

#include <libintl.h>
#include <libuutil.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zone.h>

#include <libzfs.h>

#include "libzfs_impl.h"

/*
 * Structure to keep track of dataset state.  Before changing the 'sharenfs' or
 * 'mountpoint' property, we record whether the filesystem was previously
 * mounted/shared.  This prior state dictates whether we remount/reshare the
 * dataset after the property has been changed.
 *
 * The interface consists of the following sequence of functions:
 *
 * 	changelist_gather()
 * 	changelist_prefix()
 * 	< change property >
 * 	changelist_postfix()
 * 	changelist_free()
 *
 * Other interfaces:
 *
 * changelist_remove() - remove a node from a gathered list
 * changelist_rename() - renames all datasets appropriately when doing a rename
 * changelist_unshare() - unshares all the nodes in a given changelist
 * changelist_haszonedchild() - check if there is any child exported to
 *				a local zone
 */
typedef struct prop_changenode {
	zfs_handle_t		*cn_handle;
	int			cn_shared;
	int			cn_mounted;
	int			cn_zoned;
	uu_list_node_t		cn_listnode;
} prop_changenode_t;

struct prop_changelist {
	zfs_prop_t		cl_prop;
	zfs_prop_t		cl_realprop;
	uu_list_pool_t		*cl_pool;
	uu_list_t		*cl_list;
	boolean_t		cl_waslegacy;
	boolean_t		cl_allchildren;
	boolean_t		cl_alldependents;
	int			cl_flags;
	boolean_t		cl_haszonedchild;
	boolean_t		cl_sorted;
};

/*
 * If the property is 'mountpoint', go through and unmount filesystems as
 * necessary.  We don't do the same for 'sharenfs', because we can just re-share
 * with different options without interrupting service.
 */
int
changelist_prefix(prop_changelist_t *clp)
{
	prop_changenode_t *cn;
	int ret = 0;

	if (clp->cl_prop != ZFS_PROP_MOUNTPOINT)
		return (0);

	for (cn = uu_list_first(clp->cl_list); cn != NULL;
	    cn = uu_list_next(clp->cl_list, cn)) {
		/*
		 * If we are in the global zone, but this dataset is exported
		 * to a local zone, do nothing.
		 */
		if (getzoneid() == GLOBAL_ZONEID && cn->cn_zoned)
			continue;

		if (ZFS_IS_VOLUME(cn->cn_handle)) {
			switch (clp->cl_realprop) {
			case ZFS_PROP_NAME:
				/*
				 * If this was a rename, unshare the zvol, and
				 * remove the /dev/zvol links.
				 */
				(void) zfs_unshare_iscsi(cn->cn_handle);

				if (zvol_remove_link(cn->cn_handle->zfs_hdl,
				    cn->cn_handle->zfs_name) != 0)
					ret = -1;
				break;

			case ZFS_PROP_VOLSIZE:
				/*
				 * If this was a change to the volume size, we
				 * need to unshare and reshare the volume.
				 */
				(void) zfs_unshare_iscsi(cn->cn_handle);
				break;
			}
		} else if (zfs_unmount(cn->cn_handle, NULL, clp->cl_flags) != 0)
			ret = -1;
	}

	return (ret);
}

/*
 * If the property is 'mountpoint' or 'sharenfs', go through and remount and/or
 * reshare the filesystems as necessary.  In changelist_gather() we recorded
 * whether the filesystem was previously shared or mounted.  The action we take
 * depends on the previous state, and whether the value was previously 'legacy'.
 * For non-legacy properties, we only remount/reshare the filesystem if it was
 * previously mounted/shared.  Otherwise, we always remount/reshare the
 * filesystem.
 */
int
changelist_postfix(prop_changelist_t *clp)
{
	prop_changenode_t *cn;
	char shareopts[ZFS_MAXPROPLEN];
	int ret = 0;

	/*
	 * If we're changing the mountpoint, attempt to destroy the underlying
	 * mountpoint.  All other datasets will have inherited from this dataset
	 * (in which case their mountpoints exist in the filesystem in the new
	 * location), or have explicit mountpoints set (in which case they won't
	 * be in the changelist).
	 */
	if ((cn = uu_list_last(clp->cl_list)) == NULL)
		return (0);

	if (clp->cl_prop == ZFS_PROP_MOUNTPOINT)
		remove_mountpoint(cn->cn_handle);

	/*
	 * We walk the datasets in reverse, because we want to mount any parent
	 * datasets before mounting the children.
	 */
	for (cn = uu_list_last(clp->cl_list); cn != NULL;
	    cn = uu_list_prev(clp->cl_list, cn)) {
		/*
		 * If we are in the global zone, but this dataset is exported
		 * to a local zone, do nothing.
		 */
		if (getzoneid() == GLOBAL_ZONEID && cn->cn_zoned)
			continue;

		zfs_refresh_properties(cn->cn_handle);

		if (ZFS_IS_VOLUME(cn->cn_handle)) {
			/*
			 * If we're doing a rename, recreate the /dev/zvol
			 * links.
			 */
			if (clp->cl_realprop == ZFS_PROP_NAME &&
			    zvol_create_link(cn->cn_handle->zfs_hdl,
			    cn->cn_handle->zfs_name) != 0) {
				ret = -1;
			} else if (cn->cn_shared ||
			    clp->cl_prop == ZFS_PROP_SHAREISCSI) {
				if (zfs_prop_get(cn->cn_handle,
				    ZFS_PROP_SHAREISCSI, shareopts,
				    sizeof (shareopts), NULL, NULL, 0,
				    B_FALSE) == 0 &&
				    strcmp(shareopts, "off") == 0) {
					ret = zfs_unshare_iscsi(cn->cn_handle);
				} else {
					ret = zfs_share_iscsi(cn->cn_handle);
				}
			}

			continue;
		}

		if ((clp->cl_waslegacy || cn->cn_mounted) &&
		    !zfs_is_mounted(cn->cn_handle, NULL) &&
		    zfs_mount(cn->cn_handle, NULL, 0) != 0)
			ret = -1;

		/*
		 * We always re-share even if the filesystem is currently
		 * shared, so that we can adopt any new options.
		 */
		if (cn->cn_shared ||
		    (clp->cl_prop == ZFS_PROP_SHARENFS && clp->cl_waslegacy)) {
			if (zfs_prop_get(cn->cn_handle, ZFS_PROP_SHARENFS,
			    shareopts, sizeof (shareopts), NULL, NULL, 0,
			    B_FALSE) == 0 && strcmp(shareopts, "off") == 0) {
				ret = zfs_unshare_nfs(cn->cn_handle, NULL);
			} else {
				ret = zfs_share_nfs(cn->cn_handle);
			}
		}
	}

	return (ret);
}

/*
 * Is this "dataset" a child of "parent"?
 */
static boolean_t
isa_child_of(const char *dataset, const char *parent)
{
	int len;

	len = strlen(parent);

	if (strncmp(dataset, parent, len) == 0 &&
	    (dataset[len] == '@' || dataset[len] == '/' ||
	    dataset[len] == '\0'))
		return (B_TRUE);
	else
		return (B_FALSE);

}

/*
 * If we rename a filesystem, child filesystem handles are no longer valid
 * since we identify each dataset by its name in the ZFS namespace.  As a
 * result, we have to go through and fix up all the names appropriately.  We
 * could do this automatically if libzfs kept track of all open handles, but
 * this is a lot less work.
 */
void
changelist_rename(prop_changelist_t *clp, const char *src, const char *dst)
{
	prop_changenode_t *cn;
	char newname[ZFS_MAXNAMELEN];

	for (cn = uu_list_first(clp->cl_list); cn != NULL;
	    cn = uu_list_next(clp->cl_list, cn)) {
		/*
		 * Do not rename a clone that's not in the source hierarchy.
		 */
		if (!isa_child_of(cn->cn_handle->zfs_name, src))
			continue;

		/*
		 * Destroy the previous mountpoint if needed.
		 */
		remove_mountpoint(cn->cn_handle);

		(void) strlcpy(newname, dst, sizeof (newname));
		(void) strcat(newname, cn->cn_handle->zfs_name + strlen(src));

		(void) strlcpy(cn->cn_handle->zfs_name, newname,
		    sizeof (cn->cn_handle->zfs_name));
	}
}

/*
 * Given a gathered changelist for the 'sharenfs' property, unshare all the
 * datasets in the list.
 */
int
changelist_unshare(prop_changelist_t *clp)
{
	prop_changenode_t *cn;
	int ret = 0;

	if (clp->cl_prop != ZFS_PROP_SHARENFS)
		return (0);

	for (cn = uu_list_first(clp->cl_list); cn != NULL;
	    cn = uu_list_next(clp->cl_list, cn)) {
		if (zfs_unshare_nfs(cn->cn_handle, NULL) != 0)
			ret = -1;
	}

	return (ret);
}

/*
 * Check if there is any child exported to a local zone in a given changelist.
 * This information has already been recorded while gathering the changelist
 * via changelist_gather().
 */
int
changelist_haszonedchild(prop_changelist_t *clp)
{
	return (clp->cl_haszonedchild);
}

/*
 * Remove a node from a gathered list.
 */
void
changelist_remove(zfs_handle_t *zhp, prop_changelist_t *clp)
{
	prop_changenode_t *cn;

	for (cn = uu_list_first(clp->cl_list); cn != NULL;
	    cn = uu_list_next(clp->cl_list, cn)) {

		if (strcmp(cn->cn_handle->zfs_name, zhp->zfs_name) == 0) {
			uu_list_remove(clp->cl_list, cn);
			zfs_close(cn->cn_handle);
			free(cn);
			return;
		}
	}
}

/*
 * Release any memory associated with a changelist.
 */
void
changelist_free(prop_changelist_t *clp)
{
	prop_changenode_t *cn;
	void *cookie;

	if (clp->cl_list) {
		cookie = NULL;
		while ((cn = uu_list_teardown(clp->cl_list, &cookie)) != NULL) {
			zfs_close(cn->cn_handle);
			free(cn);
		}

		uu_list_destroy(clp->cl_list);
	}
	if (clp->cl_pool)
		uu_list_pool_destroy(clp->cl_pool);

	free(clp);
}

static int
change_one(zfs_handle_t *zhp, void *data)
{
	prop_changelist_t *clp = data;
	char property[ZFS_MAXPROPLEN];
	char where[64];
	prop_changenode_t *cn;
	zfs_source_t sourcetype;

	/*
	 * We only want to unmount/unshare those filesystems that may inherit
	 * from the target filesystem.  If we find any filesystem with a
	 * locally set mountpoint, we ignore any children since changing the
	 * property will not affect them.  If this is a rename, we iterate
	 * over all children regardless, since we need them unmounted in
	 * order to do the rename.  Also, if this is a volume and we're doing
	 * a rename, then always add it to the changelist.
	 */

	if (!(ZFS_IS_VOLUME(zhp) && clp->cl_realprop == ZFS_PROP_NAME) &&
	    zfs_prop_get(zhp, clp->cl_prop, property,
	    sizeof (property), &sourcetype, where, sizeof (where),
	    B_FALSE) != 0) {
		zfs_close(zhp);
		return (0);
	}

	if (clp->cl_alldependents || clp->cl_allchildren ||
	    sourcetype == ZFS_SRC_DEFAULT || sourcetype == ZFS_SRC_INHERITED) {
		if ((cn = zfs_alloc(zfs_get_handle(zhp),
		    sizeof (prop_changenode_t))) == NULL) {
			zfs_close(zhp);
			return (-1);
		}

		cn->cn_handle = zhp;
		cn->cn_mounted = zfs_is_mounted(zhp, NULL);
		cn->cn_shared = zfs_is_shared(zhp);
		cn->cn_zoned = zfs_prop_get_int(zhp, ZFS_PROP_ZONED);

		/* Indicate if any child is exported to a local zone. */
		if (getzoneid() == GLOBAL_ZONEID && cn->cn_zoned)
			clp->cl_haszonedchild = B_TRUE;

		uu_list_node_init(cn, &cn->cn_listnode, clp->cl_pool);

		if (clp->cl_sorted) {
			uu_list_index_t idx;

			(void) uu_list_find(clp->cl_list, cn, NULL,
			    &idx);
			uu_list_insert(clp->cl_list, cn, idx);
		} else {
			ASSERT(!clp->cl_alldependents);
			verify(uu_list_insert_before(clp->cl_list,
			    uu_list_first(clp->cl_list), cn) == 0);
		}

		if (!clp->cl_alldependents)
			return (zfs_iter_children(zhp, change_one, data));
	} else {
		zfs_close(zhp);
	}

	return (0);
}

/*ARGSUSED*/
static int
compare_mountpoints(const void *a, const void *b, void *unused)
{
	const prop_changenode_t *ca = a;
	const prop_changenode_t *cb = b;

	char mounta[MAXPATHLEN];
	char mountb[MAXPATHLEN];

	boolean_t hasmounta, hasmountb;

	/*
	 * When unsharing or unmounting filesystems, we need to do it in
	 * mountpoint order.  This allows the user to have a mountpoint
	 * hierarchy that is different from the dataset hierarchy, and still
	 * allow it to be changed.  However, if either dataset doesn't have a
	 * mountpoint (because it is a volume or a snapshot), we place it at the
	 * end of the list, because it doesn't affect our change at all.
	 */
	hasmounta = (zfs_prop_get(ca->cn_handle, ZFS_PROP_MOUNTPOINT, mounta,
	    sizeof (mounta), NULL, NULL, 0, B_FALSE) == 0);
	hasmountb = (zfs_prop_get(cb->cn_handle, ZFS_PROP_MOUNTPOINT, mountb,
	    sizeof (mountb), NULL, NULL, 0, B_FALSE) == 0);

	if (!hasmounta && hasmountb)
		return (-1);
	else if (hasmounta && !hasmountb)
		return (1);
	else if (!hasmounta && !hasmountb)
		return (0);
	else
		return (strcmp(mountb, mounta));
}

/*
 * Given a ZFS handle and a property, construct a complete list of datasets
 * that need to be modified as part of this process.  For anything but the
 * 'mountpoint' and 'sharenfs' properties, this just returns an empty list.
 * Otherwise, we iterate over all children and look for any datasets that
 * inherit the property.  For each such dataset, we add it to the list and
 * mark whether it was shared beforehand.
 */
prop_changelist_t *
changelist_gather(zfs_handle_t *zhp, zfs_prop_t prop, int flags)
{
	prop_changelist_t *clp;
	prop_changenode_t *cn;
	zfs_handle_t *temp;
	char property[ZFS_MAXPROPLEN];
	uu_compare_fn_t *compare = NULL;

	if ((clp = zfs_alloc(zhp->zfs_hdl, sizeof (prop_changelist_t))) == NULL)
		return (NULL);

	/*
	 * For mountpoint-related tasks, we want to sort everything by
	 * mountpoint, so that we mount and unmount them in the appropriate
	 * order, regardless of their position in the hierarchy.
	 */
	if (prop == ZFS_PROP_NAME || prop == ZFS_PROP_ZONED ||
	    prop == ZFS_PROP_MOUNTPOINT || prop == ZFS_PROP_SHARENFS) {
		compare = compare_mountpoints;
		clp->cl_sorted = B_TRUE;
	}

	clp->cl_pool = uu_list_pool_create("changelist_pool",
	    sizeof (prop_changenode_t),
	    offsetof(prop_changenode_t, cn_listnode),
	    compare, 0);
	if (clp->cl_pool == NULL) {
		assert(uu_error() == UU_ERROR_NO_MEMORY);
		(void) zfs_error(zhp->zfs_hdl, EZFS_NOMEM, "internal error");
		changelist_free(clp);
		return (NULL);
	}

	clp->cl_list = uu_list_create(clp->cl_pool, NULL,
	    clp->cl_sorted ? UU_LIST_SORTED : 0);
	clp->cl_flags = flags;

	if (clp->cl_list == NULL) {
		assert(uu_error() == UU_ERROR_NO_MEMORY);
		(void) zfs_error(zhp->zfs_hdl, EZFS_NOMEM, "internal error");
		changelist_free(clp);
		return (NULL);
	}

	/*
	 * If this is a rename or the 'zoned' property, we pretend we're
	 * changing the mountpoint and flag it so we can catch all children in
	 * change_one().
	 *
	 * Flag cl_alldependents to catch all children plus the dependents
	 * (clones) that are not in the hierarchy.
	 */
	if (prop == ZFS_PROP_NAME) {
		clp->cl_prop = ZFS_PROP_MOUNTPOINT;
		clp->cl_alldependents = B_TRUE;
	} else if (prop == ZFS_PROP_ZONED) {
		clp->cl_prop = ZFS_PROP_MOUNTPOINT;
		clp->cl_allchildren = B_TRUE;
	} else if (prop == ZFS_PROP_CANMOUNT) {
		clp->cl_prop = ZFS_PROP_MOUNTPOINT;
	} else if (prop == ZFS_PROP_VOLSIZE) {
		clp->cl_prop = ZFS_PROP_MOUNTPOINT;
	} else {
		clp->cl_prop = prop;
	}
	clp->cl_realprop = prop;

	if (clp->cl_prop != ZFS_PROP_MOUNTPOINT &&
	    clp->cl_prop != ZFS_PROP_SHARENFS &&
	    clp->cl_prop != ZFS_PROP_SHAREISCSI)
		return (clp);

	if (clp->cl_alldependents) {
		if (zfs_iter_dependents(zhp, B_TRUE, change_one, clp) != 0) {
			changelist_free(clp);
			return (NULL);
		}
	} else if (zfs_iter_children(zhp, change_one, clp) != 0) {
		changelist_free(clp);
		return (NULL);
	}

	/*
	 * We have to re-open ourselves because we auto-close all the handles
	 * and can't tell the difference.
	 */
	if ((temp = zfs_open(zhp->zfs_hdl, zfs_get_name(zhp),
	    ZFS_TYPE_ANY)) == NULL) {
		changelist_free(clp);
		return (NULL);
	}

	/*
	 * Always add ourself to the list.  We add ourselves to the end so that
	 * we're the last to be unmounted.
	 */
	if ((cn = zfs_alloc(zhp->zfs_hdl,
	    sizeof (prop_changenode_t))) == NULL) {
		zfs_close(temp);
		changelist_free(clp);
		return (NULL);
	}

	cn->cn_handle = temp;
	cn->cn_mounted = zfs_is_mounted(temp, NULL);
	cn->cn_shared = zfs_is_shared(temp);
	cn->cn_zoned = zfs_prop_get_int(zhp, ZFS_PROP_ZONED);

	uu_list_node_init(cn, &cn->cn_listnode, clp->cl_pool);
	if (clp->cl_sorted) {
		uu_list_index_t idx;
		(void) uu_list_find(clp->cl_list, cn, NULL, &idx);
		uu_list_insert(clp->cl_list, cn, idx);
	} else {
		verify(uu_list_insert_after(clp->cl_list,
		    uu_list_last(clp->cl_list), cn) == 0);
	}

	/*
	 * If the property was previously 'legacy' or 'none', record this fact,
	 * as the behavior of changelist_postfix() will be different.
	 */
	if (zfs_prop_get(zhp, prop, property, sizeof (property),
	    NULL, NULL, 0, B_FALSE) == 0 &&
	    (strcmp(property, "legacy") == 0 || strcmp(property, "none") == 0 ||
	    strcmp(property, "off") == 0))
		clp->cl_waslegacy = B_TRUE;

	return (clp);
}
