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
 * Pool import support functions.
 *
 * To import a pool, we rely on reading the configuration information from the
 * ZFS label of each device.  If we successfully read the label, then we
 * organize the configuration information in the following hierarchy:
 *
 * 	pool guid -> toplevel vdev guid -> label txg
 *
 * Duplicate entries matching this same tuple will be discarded.  Once we have
 * examined every device, we pick the best label txg config for each toplevel
 * vdev.  We then arrange these toplevel vdevs into a complete pool config, and
 * update any paths that have changed.  Finally, we attempt to import the pool
 * using our derived config, and record the results.
 */

#include <devid.h>
#include <dirent.h>
#include <errno.h>
#include <libintl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <libgeom.h>

#include <sys/vdev_impl.h>

#include "libzfs.h"
#include "libzfs_impl.h"

/*
 * Intermediate structures used to gather configuration information.
 */
typedef struct config_entry {
	uint64_t		ce_txg;
	nvlist_t		*ce_config;
	struct config_entry	*ce_next;
} config_entry_t;

typedef struct vdev_entry {
	uint64_t		ve_guid;
	config_entry_t		*ve_configs;
	struct vdev_entry	*ve_next;
} vdev_entry_t;

typedef struct pool_entry {
	uint64_t		pe_guid;
	vdev_entry_t		*pe_vdevs;
	struct pool_entry	*pe_next;
} pool_entry_t;

typedef struct name_entry {
	char			*ne_name;
	uint64_t		ne_guid;
	struct name_entry	*ne_next;
} name_entry_t;

typedef struct pool_list {
	pool_entry_t		*pools;
	name_entry_t		*names;
} pool_list_t;

static char *
get_devid(const char *path)
{
	int fd;
	ddi_devid_t devid;
	char *minor, *ret;

	if ((fd = open(path, O_RDONLY)) < 0)
		return (NULL);

	minor = NULL;
	ret = NULL;
	if (devid_get(fd, &devid) == 0) {
		if (devid_get_minor_name(fd, &minor) == 0)
			ret = devid_str_encode(devid, minor);
		if (minor != NULL)
			devid_str_free(minor);
		devid_free(devid);
	}
	(void) close(fd);

	return (ret);
}

/*
 * Go through and fix up any path and/or devid information for the given vdev
 * configuration.
 */
static int
fix_paths(nvlist_t *nv, name_entry_t *names)
{
	nvlist_t **child;
	uint_t c, children;
	uint64_t guid;
	name_entry_t *ne, *best;
	char *path, *devid;
	int matched;

	if (nvlist_lookup_nvlist_array(nv, ZPOOL_CONFIG_CHILDREN,
	    &child, &children) == 0) {
		for (c = 0; c < children; c++)
			if (fix_paths(child[c], names) != 0)
				return (-1);
		return (0);
	}

	/*
	 * This is a leaf (file or disk) vdev.  In either case, go through
	 * the name list and see if we find a matching guid.  If so, replace
	 * the path and see if we can calculate a new devid.
	 *
	 * There may be multiple names associated with a particular guid, in
	 * which case we have overlapping slices or multiple paths to the same
	 * disk.  If this is the case, then we want to pick the path that is
	 * the most similar to the original, where "most similar" is the number
	 * of matching characters starting from the end of the path.  This will
	 * preserve slice numbers even if the disks have been reorganized, and
	 * will also catch preferred disk names if multiple paths exist.
	 */
	verify(nvlist_lookup_uint64(nv, ZPOOL_CONFIG_GUID, &guid) == 0);
	if (nvlist_lookup_string(nv, ZPOOL_CONFIG_PATH, &path) != 0)
		path = NULL;

	matched = 0;
	best = NULL;
	for (ne = names; ne != NULL; ne = ne->ne_next) {
		if (ne->ne_guid == guid) {
			const char *src, *dst;
			int count;

			if (path == NULL) {
				best = ne;
				break;
			}

			src = ne->ne_name + strlen(ne->ne_name) - 1;
			dst = path + strlen(path) - 1;
			for (count = 0; src >= ne->ne_name && dst >= path;
			    src--, dst--, count++)
				if (*src != *dst)
					break;

			/*
			 * At this point, 'count' is the number of characters
			 * matched from the end.
			 */
			if (count > matched || best == NULL) {
				best = ne;
				matched = count;
			}
		}
	}

	if (best == NULL)
		return (0);

	if (nvlist_add_string(nv, ZPOOL_CONFIG_PATH, best->ne_name) != 0)
		return (-1);

	if ((devid = get_devid(best->ne_name)) == NULL) {
		(void) nvlist_remove_all(nv, ZPOOL_CONFIG_DEVID);
	} else {
		if (nvlist_add_string(nv, ZPOOL_CONFIG_DEVID, devid) != 0)
			return (-1);
		devid_str_free(devid);
	}

	return (0);
}

/*
 * Add the given configuration to the list of known devices.
 */
static int
add_config(libzfs_handle_t *hdl, pool_list_t *pl, const char *path,
    nvlist_t *config)
{
	uint64_t pool_guid, vdev_guid, top_guid, txg, state;
	pool_entry_t *pe;
	vdev_entry_t *ve;
	config_entry_t *ce;
	name_entry_t *ne;

	/*
	 * If this is a hot spare not currently in use, add it to the list of
	 * names to translate, but don't do anything else.
	 */
	if (nvlist_lookup_uint64(config, ZPOOL_CONFIG_POOL_STATE,
	    &state) == 0 && state == POOL_STATE_SPARE &&
	    nvlist_lookup_uint64(config, ZPOOL_CONFIG_GUID, &vdev_guid) == 0) {
		if ((ne = zfs_alloc(hdl, sizeof (name_entry_t))) == NULL)
			return (-1);

		if ((ne->ne_name = zfs_strdup(hdl, path)) == NULL) {
			free(ne);
			return (-1);
		}
		ne->ne_guid = vdev_guid;
		ne->ne_next = pl->names;
		pl->names = ne;
		return (0);
	}

	/*
	 * If we have a valid config but cannot read any of these fields, then
	 * it means we have a half-initialized label.  In vdev_label_init()
	 * we write a label with txg == 0 so that we can identify the device
	 * in case the user refers to the same disk later on.  If we fail to
	 * create the pool, we'll be left with a label in this state
	 * which should not be considered part of a valid pool.
	 */
	if (nvlist_lookup_uint64(config, ZPOOL_CONFIG_POOL_GUID,
	    &pool_guid) != 0 ||
	    nvlist_lookup_uint64(config, ZPOOL_CONFIG_GUID,
	    &vdev_guid) != 0 ||
	    nvlist_lookup_uint64(config, ZPOOL_CONFIG_TOP_GUID,
	    &top_guid) != 0 ||
	    nvlist_lookup_uint64(config, ZPOOL_CONFIG_POOL_TXG,
	    &txg) != 0 || txg == 0) {
		nvlist_free(config);
		return (0);
	}

	/*
	 * First, see if we know about this pool.  If not, then add it to the
	 * list of known pools.
	 */
	for (pe = pl->pools; pe != NULL; pe = pe->pe_next) {
		if (pe->pe_guid == pool_guid)
			break;
	}

	if (pe == NULL) {
		if ((pe = zfs_alloc(hdl, sizeof (pool_entry_t))) == NULL) {
			nvlist_free(config);
			return (-1);
		}
		pe->pe_guid = pool_guid;
		pe->pe_next = pl->pools;
		pl->pools = pe;
	}

	/*
	 * Second, see if we know about this toplevel vdev.  Add it if its
	 * missing.
	 */
	for (ve = pe->pe_vdevs; ve != NULL; ve = ve->ve_next) {
		if (ve->ve_guid == top_guid)
			break;
	}

	if (ve == NULL) {
		if ((ve = zfs_alloc(hdl, sizeof (vdev_entry_t))) == NULL) {
			nvlist_free(config);
			return (-1);
		}
		ve->ve_guid = top_guid;
		ve->ve_next = pe->pe_vdevs;
		pe->pe_vdevs = ve;
	}

	/*
	 * Third, see if we have a config with a matching transaction group.  If
	 * so, then we do nothing.  Otherwise, add it to the list of known
	 * configs.
	 */
	for (ce = ve->ve_configs; ce != NULL; ce = ce->ce_next) {
		if (ce->ce_txg == txg)
			break;
	}

	if (ce == NULL) {
		if ((ce = zfs_alloc(hdl, sizeof (config_entry_t))) == NULL) {
			nvlist_free(config);
			return (-1);
		}
		ce->ce_txg = txg;
		ce->ce_config = config;
		ce->ce_next = ve->ve_configs;
		ve->ve_configs = ce;
	} else {
		nvlist_free(config);
	}

	/*
	 * At this point we've successfully added our config to the list of
	 * known configs.  The last thing to do is add the vdev guid -> path
	 * mappings so that we can fix up the configuration as necessary before
	 * doing the import.
	 */
	if ((ne = zfs_alloc(hdl, sizeof (name_entry_t))) == NULL)
		return (-1);

	if ((ne->ne_name = zfs_strdup(hdl, path)) == NULL) {
		free(ne);
		return (-1);
	}

	ne->ne_guid = vdev_guid;
	ne->ne_next = pl->names;
	pl->names = ne;

	return (0);
}

/*
 * Returns true if the named pool matches the given GUID.
 */
static int
pool_active(libzfs_handle_t *hdl, const char *name, uint64_t guid,
    boolean_t *isactive)
{
	zpool_handle_t *zhp;
	uint64_t theguid;

	if (zpool_open_silent(hdl, name, &zhp) != 0)
		return (-1);

	if (zhp == NULL) {
		*isactive = B_FALSE;
		return (0);
	}

	verify(nvlist_lookup_uint64(zhp->zpool_config, ZPOOL_CONFIG_POOL_GUID,
	    &theguid) == 0);

	zpool_close(zhp);

	*isactive = (theguid == guid);
	return (0);
}

/*
 * Convert our list of pools into the definitive set of configurations.  We
 * start by picking the best config for each toplevel vdev.  Once that's done,
 * we assemble the toplevel vdevs into a full config for the pool.  We make a
 * pass to fix up any incorrect paths, and then add it to the main list to
 * return to the user.
 */
static nvlist_t *
get_configs(libzfs_handle_t *hdl, pool_list_t *pl)
{
	pool_entry_t *pe;
	vdev_entry_t *ve;
	config_entry_t *ce;
	nvlist_t *ret = NULL, *config = NULL, *tmp, *nvtop, *nvroot;
	nvlist_t **spares;
	uint_t i, nspares;
	boolean_t config_seen;
	uint64_t best_txg;
	char *name, *hostname;
	zfs_cmd_t zc = { 0 };
	uint64_t version, guid;
	size_t len;
	int err;
	uint_t children = 0;
	nvlist_t **child = NULL;
	uint_t c;
	boolean_t isactive;
	uint64_t hostid;

	if (nvlist_alloc(&ret, 0, 0) != 0)
		goto nomem;

	for (pe = pl->pools; pe != NULL; pe = pe->pe_next) {
		uint64_t id;

		if (nvlist_alloc(&config, NV_UNIQUE_NAME, 0) != 0)
			goto nomem;
		config_seen = B_FALSE;

		/*
		 * Iterate over all toplevel vdevs.  Grab the pool configuration
		 * from the first one we find, and then go through the rest and
		 * add them as necessary to the 'vdevs' member of the config.
		 */
		for (ve = pe->pe_vdevs; ve != NULL; ve = ve->ve_next) {

			/*
			 * Determine the best configuration for this vdev by
			 * selecting the config with the latest transaction
			 * group.
			 */
			best_txg = 0;
			for (ce = ve->ve_configs; ce != NULL;
			    ce = ce->ce_next) {

				if (ce->ce_txg > best_txg) {
					tmp = ce->ce_config;
					best_txg = ce->ce_txg;
				}
			}

			if (!config_seen) {
				/*
				 * Copy the relevant pieces of data to the pool
				 * configuration:
				 *
				 *	version
				 * 	pool guid
				 * 	name
				 * 	pool state
				 *	hostid (if available)
				 *	hostname (if available)
				 */
				uint64_t state;

				verify(nvlist_lookup_uint64(tmp,
				    ZPOOL_CONFIG_VERSION, &version) == 0);
				if (nvlist_add_uint64(config,
				    ZPOOL_CONFIG_VERSION, version) != 0)
					goto nomem;
				verify(nvlist_lookup_uint64(tmp,
				    ZPOOL_CONFIG_POOL_GUID, &guid) == 0);
				if (nvlist_add_uint64(config,
				    ZPOOL_CONFIG_POOL_GUID, guid) != 0)
					goto nomem;
				verify(nvlist_lookup_string(tmp,
				    ZPOOL_CONFIG_POOL_NAME, &name) == 0);
				if (nvlist_add_string(config,
				    ZPOOL_CONFIG_POOL_NAME, name) != 0)
					goto nomem;
				verify(nvlist_lookup_uint64(tmp,
				    ZPOOL_CONFIG_POOL_STATE, &state) == 0);
				if (nvlist_add_uint64(config,
				    ZPOOL_CONFIG_POOL_STATE, state) != 0)
					goto nomem;
				hostid = 0;
				if (nvlist_lookup_uint64(tmp,
				    ZPOOL_CONFIG_HOSTID, &hostid) == 0) {
					if (nvlist_add_uint64(config,
					    ZPOOL_CONFIG_HOSTID, hostid) != 0)
						goto nomem;
					verify(nvlist_lookup_string(tmp,
					    ZPOOL_CONFIG_HOSTNAME,
					    &hostname) == 0);
					if (nvlist_add_string(config,
					    ZPOOL_CONFIG_HOSTNAME,
					    hostname) != 0)
						goto nomem;
				}

				config_seen = B_TRUE;
			}

			/*
			 * Add this top-level vdev to the child array.
			 */
			verify(nvlist_lookup_nvlist(tmp,
			    ZPOOL_CONFIG_VDEV_TREE, &nvtop) == 0);
			verify(nvlist_lookup_uint64(nvtop, ZPOOL_CONFIG_ID,
			    &id) == 0);
			if (id >= children) {
				nvlist_t **newchild;

				newchild = zfs_alloc(hdl, (id + 1) *
				    sizeof (nvlist_t *));
				if (newchild == NULL)
					goto nomem;

				for (c = 0; c < children; c++)
					newchild[c] = child[c];

				free(child);
				child = newchild;
				children = id + 1;
			}
			if (nvlist_dup(nvtop, &child[id], 0) != 0)
				goto nomem;

		}

		verify(nvlist_lookup_uint64(config, ZPOOL_CONFIG_POOL_GUID,
		    &guid) == 0);

		/*
		 * Look for any missing top-level vdevs.  If this is the case,
		 * create a faked up 'missing' vdev as a placeholder.  We cannot
		 * simply compress the child array, because the kernel performs
		 * certain checks to make sure the vdev IDs match their location
		 * in the configuration.
		 */
		for (c = 0; c < children; c++)
			if (child[c] == NULL) {
				nvlist_t *missing;
				if (nvlist_alloc(&missing, NV_UNIQUE_NAME,
				    0) != 0)
					goto nomem;
				if (nvlist_add_string(missing,
				    ZPOOL_CONFIG_TYPE,
				    VDEV_TYPE_MISSING) != 0 ||
				    nvlist_add_uint64(missing,
				    ZPOOL_CONFIG_ID, c) != 0 ||
				    nvlist_add_uint64(missing,
				    ZPOOL_CONFIG_GUID, 0ULL) != 0) {
					nvlist_free(missing);
					goto nomem;
				}
				child[c] = missing;
			}

		/*
		 * Put all of this pool's top-level vdevs into a root vdev.
		 */
		if (nvlist_alloc(&nvroot, NV_UNIQUE_NAME, 0) != 0)
			goto nomem;
		if (nvlist_add_string(nvroot, ZPOOL_CONFIG_TYPE,
		    VDEV_TYPE_ROOT) != 0 ||
		    nvlist_add_uint64(nvroot, ZPOOL_CONFIG_ID, 0ULL) != 0 ||
		    nvlist_add_uint64(nvroot, ZPOOL_CONFIG_GUID, guid) != 0 ||
		    nvlist_add_nvlist_array(nvroot, ZPOOL_CONFIG_CHILDREN,
		    child, children) != 0) {
			nvlist_free(nvroot);
			goto nomem;
		}

		for (c = 0; c < children; c++)
			nvlist_free(child[c]);
		free(child);
		children = 0;
		child = NULL;

		/*
		 * Go through and fix up any paths and/or devids based on our
		 * known list of vdev GUID -> path mappings.
		 */
		if (fix_paths(nvroot, pl->names) != 0) {
			nvlist_free(nvroot);
			goto nomem;
		}

		/*
		 * Add the root vdev to this pool's configuration.
		 */
		if (nvlist_add_nvlist(config, ZPOOL_CONFIG_VDEV_TREE,
		    nvroot) != 0) {
			nvlist_free(nvroot);
			goto nomem;
		}
		nvlist_free(nvroot);

		/*
		 * Determine if this pool is currently active, in which case we
		 * can't actually import it.
		 */
		verify(nvlist_lookup_string(config, ZPOOL_CONFIG_POOL_NAME,
		    &name) == 0);
		verify(nvlist_lookup_uint64(config, ZPOOL_CONFIG_POOL_GUID,
		    &guid) == 0);

		if (pool_active(hdl, name, guid, &isactive) != 0)
			goto error;

		if (isactive) {
			nvlist_free(config);
			config = NULL;
			continue;
		}

		/*
		 * Try to do the import in order to get vdev state.
		 */
		if (zcmd_write_src_nvlist(hdl, &zc, config, &len) != 0)
			goto error;

		nvlist_free(config);
		config = NULL;

		if (zcmd_alloc_dst_nvlist(hdl, &zc, len * 2) != 0) {
			zcmd_free_nvlists(&zc);
			goto error;
		}

		while ((err = ioctl(hdl->libzfs_fd, ZFS_IOC_POOL_TRYIMPORT,
		    &zc)) != 0 && errno == ENOMEM) {
			if (zcmd_expand_dst_nvlist(hdl, &zc) != 0) {
				zcmd_free_nvlists(&zc);
				goto error;
			}
		}

		if (err) {
			(void) zpool_standard_error(hdl, errno,
			    dgettext(TEXT_DOMAIN, "cannot discover pools"));
			zcmd_free_nvlists(&zc);
			goto error;
		}

		if (zcmd_read_dst_nvlist(hdl, &zc, &config) != 0) {
			zcmd_free_nvlists(&zc);
			goto error;
		}

		zcmd_free_nvlists(&zc);

		/*
		 * Go through and update the paths for spares, now that we have
		 * them.
		 */
		verify(nvlist_lookup_nvlist(config, ZPOOL_CONFIG_VDEV_TREE,
		    &nvroot) == 0);
		if (nvlist_lookup_nvlist_array(nvroot, ZPOOL_CONFIG_SPARES,
		    &spares, &nspares) == 0) {
			for (i = 0; i < nspares; i++) {
				if (fix_paths(spares[i], pl->names) != 0)
					goto nomem;
			}
		}

		/*
		 * Restore the original information read from the actual label.
		 */
		(void) nvlist_remove(config, ZPOOL_CONFIG_HOSTID,
		    DATA_TYPE_UINT64);
		(void) nvlist_remove(config, ZPOOL_CONFIG_HOSTNAME,
		    DATA_TYPE_STRING);
		if (hostid != 0) {
			verify(nvlist_add_uint64(config, ZPOOL_CONFIG_HOSTID,
			    hostid) == 0);
			verify(nvlist_add_string(config, ZPOOL_CONFIG_HOSTNAME,
			    hostname) == 0);
		}

		/*
		 * Add this pool to the list of configs.
		 */
		verify(nvlist_lookup_string(config, ZPOOL_CONFIG_POOL_NAME,
		    &name) == 0);
		if (nvlist_add_nvlist(ret, name, config) != 0)
			goto nomem;

		nvlist_free(config);
		config = NULL;
	}

	return (ret);

nomem:
	(void) no_memory(hdl);
error:
	nvlist_free(config);
	nvlist_free(ret);
	for (c = 0; c < children; c++)
		nvlist_free(child[c]);
	free(child);

	return (NULL);
}

/*
 * Return the offset of the given label.
 */
static uint64_t
label_offset(size_t size, int l)
{
	return (l * sizeof (vdev_label_t) + (l < VDEV_LABELS / 2 ?
	    0 : size - VDEV_LABELS * sizeof (vdev_label_t)));
}

/*
 * Given a file descriptor, read the label information and return an nvlist
 * describing the configuration, if there is one.
 */
int
zpool_read_label(int fd, nvlist_t **config)
{
	struct stat64 statbuf;
	int l;
	vdev_label_t *label;
	uint64_t state, txg;

	*config = NULL;

	if (fstat64(fd, &statbuf) == -1)
		return (0);

	if ((label = malloc(sizeof (vdev_label_t))) == NULL)
		return (-1);

	for (l = 0; l < VDEV_LABELS; l++) {
		if (pread(fd, label, sizeof (vdev_label_t),
		    label_offset(statbuf.st_size, l)) != sizeof (vdev_label_t))
			continue;

		if (nvlist_unpack(label->vl_vdev_phys.vp_nvlist,
		    sizeof (label->vl_vdev_phys.vp_nvlist), config, 0) != 0)
			continue;

		if (nvlist_lookup_uint64(*config, ZPOOL_CONFIG_POOL_STATE,
		    &state) != 0 || state > POOL_STATE_SPARE) {
			nvlist_free(*config);
			continue;
		}

		if (state != POOL_STATE_SPARE &&
		    (nvlist_lookup_uint64(*config, ZPOOL_CONFIG_POOL_TXG,
		    &txg) != 0 || txg == 0)) {
			nvlist_free(*config);
			continue;
		}

		free(label);
		return (0);
	}

	free(label);
	*config = NULL;
	return (0);
}

/*
 * Given a list of directories to search, find all pools stored on disk.  This
 * includes partial pools which are not available to import.  If no args are
 * given (argc is 0), then the default directory (/dev) is searched.
 */
nvlist_t *
zpool_find_import(libzfs_handle_t *hdl, int argc, char **argv)
{
	int i;
	char path[MAXPATHLEN];
	nvlist_t *ret = NULL, *config;
	int fd;
	pool_list_t pools = { 0 };
	pool_entry_t *pe, *penext;
	vdev_entry_t *ve, *venext;
	config_entry_t *ce, *cenext;
	name_entry_t *ne, *nenext;
	struct gmesh mesh;
	struct gclass *mp;
	struct ggeom *gp;
	struct gprovider *pp;

	/*
	 * Go through and read the label configuration information from every
	 * possible device, organizing the information according to pool GUID
	 * and toplevel GUID.
	 */

	fd = geom_gettree(&mesh);
	assert(fd == 0);

	LIST_FOREACH(mp, &mesh.lg_class, lg_class) {
        	LIST_FOREACH(gp, &mp->lg_geom, lg_geom) {
			LIST_FOREACH(pp, &gp->lg_provider, lg_provider) {

				(void) snprintf(path, sizeof (path), "%s%s",
				    _PATH_DEV, pp->lg_name);

				if ((fd = open64(path, O_RDONLY)) < 0)
					continue;

				if ((zpool_read_label(fd, &config)) != 0) {
					(void) no_memory(hdl);
					goto error;
				}

				(void) close(fd);

				if (config == NULL)
					continue;

				if (add_config(hdl, &pools, path, config) != 0)
					goto error;
			}
		}
	}

	geom_deletetree(&mesh);

	ret = get_configs(hdl, &pools);

error:
	for (pe = pools.pools; pe != NULL; pe = penext) {
		penext = pe->pe_next;
		for (ve = pe->pe_vdevs; ve != NULL; ve = venext) {
			venext = ve->ve_next;
			for (ce = ve->ve_configs; ce != NULL; ce = cenext) {
				cenext = ce->ce_next;
				if (ce->ce_config)
					nvlist_free(ce->ce_config);
				free(ce);
			}
			free(ve);
		}
		free(pe);
	}

	for (ne = pools.names; ne != NULL; ne = nenext) {
		nenext = ne->ne_next;
		if (ne->ne_name)
			free(ne->ne_name);
		free(ne);
	}

	return (ret);
}

boolean_t
find_guid(nvlist_t *nv, uint64_t guid)
{
	uint64_t tmp;
	nvlist_t **child;
	uint_t c, children;

	verify(nvlist_lookup_uint64(nv, ZPOOL_CONFIG_GUID, &tmp) == 0);
	if (tmp == guid)
		return (B_TRUE);

	if (nvlist_lookup_nvlist_array(nv, ZPOOL_CONFIG_CHILDREN,
	    &child, &children) == 0) {
		for (c = 0; c < children; c++)
			if (find_guid(child[c], guid))
				return (B_TRUE);
	}

	return (B_FALSE);
}

typedef struct spare_cbdata {
	uint64_t	cb_guid;
	zpool_handle_t	*cb_zhp;
} spare_cbdata_t;

static int
find_spare(zpool_handle_t *zhp, void *data)
{
	spare_cbdata_t *cbp = data;
	nvlist_t **spares;
	uint_t i, nspares;
	uint64_t guid;
	nvlist_t *nvroot;

	verify(nvlist_lookup_nvlist(zhp->zpool_config, ZPOOL_CONFIG_VDEV_TREE,
	    &nvroot) == 0);

	if (nvlist_lookup_nvlist_array(nvroot, ZPOOL_CONFIG_SPARES,
	    &spares, &nspares) == 0) {
		for (i = 0; i < nspares; i++) {
			verify(nvlist_lookup_uint64(spares[i],
			    ZPOOL_CONFIG_GUID, &guid) == 0);
			if (guid == cbp->cb_guid) {
				cbp->cb_zhp = zhp;
				return (1);
			}
		}
	}

	zpool_close(zhp);
	return (0);
}

/*
 * Determines if the pool is in use.  If so, it returns true and the state of
 * the pool as well as the name of the pool.  Both strings are allocated and
 * must be freed by the caller.
 */
int
zpool_in_use(libzfs_handle_t *hdl, int fd, pool_state_t *state, char **namestr,
    boolean_t *inuse)
{
	nvlist_t *config;
	char *name;
	boolean_t ret;
	uint64_t guid, vdev_guid;
	zpool_handle_t *zhp;
	nvlist_t *pool_config;
	uint64_t stateval, isspare;
	spare_cbdata_t cb = { 0 };
	boolean_t isactive;

	*inuse = B_FALSE;

	if (zpool_read_label(fd, &config) != 0) {
		(void) no_memory(hdl);
		return (-1);
	}

	if (config == NULL)
		return (0);

	verify(nvlist_lookup_uint64(config, ZPOOL_CONFIG_POOL_STATE,
	    &stateval) == 0);
	verify(nvlist_lookup_uint64(config, ZPOOL_CONFIG_GUID,
	    &vdev_guid) == 0);

	if (stateval != POOL_STATE_SPARE) {
		verify(nvlist_lookup_string(config, ZPOOL_CONFIG_POOL_NAME,
		    &name) == 0);
		verify(nvlist_lookup_uint64(config, ZPOOL_CONFIG_POOL_GUID,
		    &guid) == 0);
	}

	switch (stateval) {
	case POOL_STATE_EXPORTED:
		ret = B_TRUE;
		break;

	case POOL_STATE_ACTIVE:
		/*
		 * For an active pool, we have to determine if it's really part
		 * of a currently active pool (in which case the pool will exist
		 * and the guid will be the same), or whether it's part of an
		 * active pool that was disconnected without being explicitly
		 * exported.
		 */
		if (pool_active(hdl, name, guid, &isactive) != 0) {
			nvlist_free(config);
			return (-1);
		}

		if (isactive) {
			/*
			 * Because the device may have been removed while
			 * offlined, we only report it as active if the vdev is
			 * still present in the config.  Otherwise, pretend like
			 * it's not in use.
			 */
			if ((zhp = zpool_open_canfail(hdl, name)) != NULL &&
			    (pool_config = zpool_get_config(zhp, NULL))
			    != NULL) {
				nvlist_t *nvroot;

				verify(nvlist_lookup_nvlist(pool_config,
				    ZPOOL_CONFIG_VDEV_TREE, &nvroot) == 0);
				ret = find_guid(nvroot, vdev_guid);
			} else {
				ret = B_FALSE;
			}

			/*
			 * If this is an active spare within another pool, we
			 * treat it like an unused hot spare.  This allows the
			 * user to create a pool with a hot spare that currently
			 * in use within another pool.  Since we return B_TRUE,
			 * libdiskmgt will continue to prevent generic consumers
			 * from using the device.
			 */
			if (ret && nvlist_lookup_uint64(config,
			    ZPOOL_CONFIG_IS_SPARE, &isspare) == 0 && isspare)
				stateval = POOL_STATE_SPARE;

			if (zhp != NULL)
				zpool_close(zhp);
		} else {
			stateval = POOL_STATE_POTENTIALLY_ACTIVE;
			ret = B_TRUE;
		}
		break;

	case POOL_STATE_SPARE:
		/*
		 * For a hot spare, it can be either definitively in use, or
		 * potentially active.  To determine if it's in use, we iterate
		 * over all pools in the system and search for one with a spare
		 * with a matching guid.
		 *
		 * Due to the shared nature of spares, we don't actually report
		 * the potentially active case as in use.  This means the user
		 * can freely create pools on the hot spares of exported pools,
		 * but to do otherwise makes the resulting code complicated, and
		 * we end up having to deal with this case anyway.
		 */
		cb.cb_zhp = NULL;
		cb.cb_guid = vdev_guid;
		if (zpool_iter(hdl, find_spare, &cb) == 1) {
			name = (char *)zpool_get_name(cb.cb_zhp);
			ret = TRUE;
		} else {
			ret = FALSE;
		}
		break;

	default:
		ret = B_FALSE;
	}


	if (ret) {
		if ((*namestr = zfs_strdup(hdl, name)) == NULL) {
			nvlist_free(config);
			return (-1);
		}
		*state = (pool_state_t)stateval;
	}

	if (cb.cb_zhp)
		zpool_close(cb.cb_zhp);

	nvlist_free(config);
	*inuse = ret;
	return (0);
}
