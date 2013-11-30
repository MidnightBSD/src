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

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <libgen.h>
#include <libintl.h>
#include <libuutil.h>
#include <locale.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <zone.h>
#include <sys/mntent.h>
#include <sys/mnttab.h>
#include <sys/mount.h>
#include <sys/stat.h>

#include <libzfs.h>

#include "zfs_iter.h"
#include "zfs_util.h"

libzfs_handle_t *g_zfs;

static FILE *mnttab_file;

static int zfs_do_clone(int argc, char **argv);
static int zfs_do_create(int argc, char **argv);
static int zfs_do_destroy(int argc, char **argv);
static int zfs_do_get(int argc, char **argv);
static int zfs_do_inherit(int argc, char **argv);
static int zfs_do_list(int argc, char **argv);
static int zfs_do_mount(int argc, char **argv);
static int zfs_do_rename(int argc, char **argv);
static int zfs_do_rollback(int argc, char **argv);
static int zfs_do_set(int argc, char **argv);
static int zfs_do_snapshot(int argc, char **argv);
static int zfs_do_unmount(int argc, char **argv);
static int zfs_do_share(int argc, char **argv);
static int zfs_do_unshare(int argc, char **argv);
static int zfs_do_send(int argc, char **argv);
static int zfs_do_receive(int argc, char **argv);
static int zfs_do_promote(int argc, char **argv);
static int zfs_do_jail(int argc, char **argv);
static int zfs_do_unjail(int argc, char **argv);

/*
 * These libumem hooks provide a reasonable set of defaults for the allocator's
 * debugging facilities.
 */
const char *
_umem_debug_init(void)
{
	return ("default,verbose"); /* $UMEM_DEBUG setting */
}

const char *
_umem_logging_init(void)
{
	return ("fail,contents"); /* $UMEM_LOGGING setting */
}

typedef enum {
	HELP_CLONE,
	HELP_CREATE,
	HELP_DESTROY,
	HELP_GET,
	HELP_INHERIT,
	HELP_JAIL,
	HELP_UNJAIL,
	HELP_LIST,
	HELP_MOUNT,
	HELP_PROMOTE,
	HELP_RECEIVE,
	HELP_RENAME,
	HELP_ROLLBACK,
	HELP_SEND,
	HELP_SET,
	HELP_SHARE,
	HELP_SNAPSHOT,
	HELP_UNMOUNT,
	HELP_UNSHARE
} zfs_help_t;

typedef struct zfs_command {
	const char	*name;
	int		(*func)(int argc, char **argv);
	zfs_help_t	usage;
} zfs_command_t;

/*
 * Master command table.  Each ZFS command has a name, associated function, and
 * usage message.  The usage messages need to be internationalized, so we have
 * to have a function to return the usage message based on a command index.
 *
 * These commands are organized according to how they are displayed in the usage
 * message.  An empty command (one with a NULL name) indicates an empty line in
 * the generic usage message.
 */
static zfs_command_t command_table[] = {
	{ "create",	zfs_do_create,		HELP_CREATE		},
	{ "destroy",	zfs_do_destroy,		HELP_DESTROY		},
	{ NULL },
	{ "snapshot",	zfs_do_snapshot,	HELP_SNAPSHOT		},
	{ "rollback",	zfs_do_rollback,	HELP_ROLLBACK		},
	{ "clone",	zfs_do_clone,		HELP_CLONE		},
	{ "promote",	zfs_do_promote,		HELP_PROMOTE		},
	{ "rename",	zfs_do_rename,		HELP_RENAME		},
	{ NULL },
	{ "list",	zfs_do_list,		HELP_LIST		},
	{ NULL },
	{ "set",	zfs_do_set,		HELP_SET		},
	{ "get", 	zfs_do_get,		HELP_GET		},
	{ "inherit",	zfs_do_inherit,		HELP_INHERIT		},
	{ NULL },
	{ "mount",	zfs_do_mount,		HELP_MOUNT		},
	{ NULL },
	{ "unmount",	zfs_do_unmount,		HELP_UNMOUNT		},
	{ NULL },
	{ "share",	zfs_do_share,		HELP_SHARE		},
	{ NULL },
	{ "unshare",	zfs_do_unshare,		HELP_UNSHARE		},
	{ NULL },
	{ "send",	zfs_do_send,		HELP_SEND		},
	{ "receive",	zfs_do_receive,		HELP_RECEIVE		},
	{ NULL },
	{ "jail",	zfs_do_jail,		HELP_JAIL		},
	{ "unjail",	zfs_do_unjail,		HELP_UNJAIL		},
};

#define	NCOMMAND	(sizeof (command_table) / sizeof (command_table[0]))

zfs_command_t *current_command;

static const char *
get_usage(zfs_help_t idx)
{
	switch (idx) {
	case HELP_CLONE:
		return (gettext("\tclone <snapshot> <filesystem|volume>\n"));
	case HELP_CREATE:
		return (gettext("\tcreate [[-o property=value] ... ] "
		    "<filesystem>\n"
		    "\tcreate [-s] [-b blocksize] [[-o property=value] ...]\n"
		    "\t    -V <size> <volume>\n"));
	case HELP_DESTROY:
		return (gettext("\tdestroy [-rRf] "
		    "<filesystem|volume|snapshot>\n"));
	case HELP_GET:
		return (gettext("\tget [-rHp] [-o field[,field]...] "
		    "[-s source[,source]...]\n"
		    "\t    <all | property[,property]...> "
		    "[filesystem|volume|snapshot] ...\n"));
	case HELP_INHERIT:
		return (gettext("\tinherit [-r] <property> "
		    "<filesystem|volume> ...\n"));
	case HELP_JAIL:
		return (gettext("\tjail <jailid> <filesystem>\n"));
	case HELP_UNJAIL:
		return (gettext("\tunjail <jailid> <filesystem>\n"));
	case HELP_LIST:
		return (gettext("\tlist [-rH] [-o property[,property]...] "
		    "[-t type[,type]...]\n"
		    "\t    [-s property [-s property]...]"
		    " [-S property [-S property]...]\n"
		    "\t    [filesystem|volume|snapshot] ...\n"));
	case HELP_MOUNT:
		return (gettext("\tmount\n"
		    "\tmount [-o opts] [-O] -a\n"
		    "\tmount [-o opts] [-O] <filesystem>\n"));
	case HELP_PROMOTE:
		return (gettext("\tpromote <clone filesystem>\n"));
	case HELP_RECEIVE:
		return (gettext("\treceive [-vnF] <filesystem|volume|"
		"snapshot>\n"
		"\treceive [-vnF] -d <filesystem>\n"));
	case HELP_RENAME:
		return (gettext("\trename <filesystem|volume|snapshot> "
		    "<filesystem|volume|snapshot>\n"
		    "\trename -r <snapshot> <snapshot>"));
	case HELP_ROLLBACK:
		return (gettext("\trollback [-rRf] <snapshot>\n"));
	case HELP_SEND:
		return (gettext("\tsend [-i <snapshot>] <snapshot>\n"));
	case HELP_SET:
		return (gettext("\tset <property=value> "
		    "<filesystem|volume> ...\n"));
	case HELP_SHARE:
		return (gettext("\tshare -a\n"
		    "\tshare <filesystem>\n"));
	case HELP_SNAPSHOT:
		return (gettext("\tsnapshot [-r] "
		    "<filesystem@name|volume@name>\n"));
	case HELP_UNMOUNT:
		return (gettext("\tunmount [-f] -a\n"
		    "\tunmount [-f] <filesystem|mountpoint>\n"));
	case HELP_UNSHARE:
		return (gettext("\tunshare [-f] -a\n"
		    "\tunshare [-f] <filesystem|mountpoint>\n"));
	}

	abort();
	/* NOTREACHED */
}

/*
 * Utility function to guarantee malloc() success.
 */
void *
safe_malloc(size_t size)
{
	void *data;

	if ((data = calloc(1, size)) == NULL) {
		(void) fprintf(stderr, "internal error: out of memory\n");
		exit(1);
	}

	return (data);
}

/*
 * Callback routinue that will print out information for each of the
 * the properties.
 */
static zfs_prop_t
usage_prop_cb(zfs_prop_t prop, void *cb)
{
	FILE *fp = cb;

	(void) fprintf(fp, "\t%-13s  ", zfs_prop_to_name(prop));

	if (zfs_prop_readonly(prop))
		(void) fprintf(fp, "  NO    ");
	else
		(void) fprintf(fp, " YES    ");

	if (zfs_prop_inheritable(prop))
		(void) fprintf(fp, "  YES   ");
	else
		(void) fprintf(fp, "   NO   ");

	if (zfs_prop_values(prop) == NULL)
		(void) fprintf(fp, "-\n");
	else
		(void) fprintf(fp, "%s\n", zfs_prop_values(prop));

	return (ZFS_PROP_CONT);
}

/*
 * Display usage message.  If we're inside a command, display only the usage for
 * that command.  Otherwise, iterate over the entire command table and display
 * a complete usage message.
 */
static void
usage(boolean_t requested)
{
	int i;
	boolean_t show_properties = B_FALSE;
	FILE *fp = requested ? stdout : stderr;

	if (current_command == NULL) {

		(void) fprintf(fp, gettext("usage: zfs command args ...\n"));
		(void) fprintf(fp,
		    gettext("where 'command' is one of the following:\n\n"));

		for (i = 0; i < NCOMMAND; i++) {
			if (command_table[i].name == NULL)
				(void) fprintf(fp, "\n");
			else
				(void) fprintf(fp, "%s",
				    get_usage(command_table[i].usage));
		}

		(void) fprintf(fp, gettext("\nEach dataset is of the form: "
		    "pool/[dataset/]*dataset[@name]\n"));
	} else {
		(void) fprintf(fp, gettext("usage:\n"));
		(void) fprintf(fp, "%s", get_usage(current_command->usage));
	}

	if (current_command != NULL &&
	    (strcmp(current_command->name, "set") == 0 ||
	    strcmp(current_command->name, "get") == 0 ||
	    strcmp(current_command->name, "inherit") == 0 ||
	    strcmp(current_command->name, "list") == 0))
		show_properties = B_TRUE;

	if (show_properties) {

		(void) fprintf(fp,
		    gettext("\nThe following properties are supported:\n"));

		(void) fprintf(fp, "\n\t%-13s  %s  %s   %s\n\n",
		    "PROPERTY", "EDIT", "INHERIT", "VALUES");

		/* Iterate over all properties */
		(void) zfs_prop_iter(usage_prop_cb, fp, B_FALSE);

		(void) fprintf(fp, gettext("\nSizes are specified in bytes "
		    "with standard units such as K, M, G, etc.\n"));
		(void) fprintf(fp, gettext("\n\nUser-defined properties can "
		    "be specified by using a name containing a colon (:).\n"));
	} else {
		/*
		 * TRANSLATION NOTE:
		 * "zfs set|get" must not be localised this is the
		 * command name and arguments.
		 */
		(void) fprintf(fp,
		    gettext("\nFor the property list, run: zfs set|get\n"));
	}

	/*
	 * See comments at end of main().
	 */
	if (getenv("ZFS_ABORT") != NULL) {
		(void) printf("dumping core by request\n");
		abort();
	}

	exit(requested ? 0 : 2);
}

/*
 * zfs clone <fs, snap, vol> fs
 *
 * Given an existing dataset, create a writable copy whose initial contents
 * are the same as the source.  The newly created dataset maintains a
 * dependency on the original; the original cannot be destroyed so long as
 * the clone exists.
 */
static int
zfs_do_clone(int argc, char **argv)
{
	zfs_handle_t *zhp;
	int ret;

	/* check options */
	if (argc > 1 && argv[1][0] == '-') {
		(void) fprintf(stderr, gettext("invalid option '%c'\n"),
		    argv[1][1]);
		usage(B_FALSE);
	}

	/* check number of arguments */
	if (argc < 2) {
		(void) fprintf(stderr, gettext("missing source dataset "
		    "argument\n"));
		usage(B_FALSE);
	}
	if (argc < 3) {
		(void) fprintf(stderr, gettext("missing target dataset "
		    "argument\n"));
		usage(B_FALSE);
	}
	if (argc > 3) {
		(void) fprintf(stderr, gettext("too many arguments\n"));
		usage(B_FALSE);
	}

	/* open the source dataset */
	if ((zhp = zfs_open(g_zfs, argv[1], ZFS_TYPE_SNAPSHOT)) == NULL)
		return (1);

	/* pass to libzfs */
	ret = zfs_clone(zhp, argv[2], NULL);

	/* create the mountpoint if necessary */
	if (ret == 0) {
		zfs_handle_t *clone = zfs_open(g_zfs, argv[2], ZFS_TYPE_ANY);
		if (clone != NULL) {
			if ((ret = zfs_mount(clone, NULL, 0)) == 0)
				ret = zfs_share(clone);
			zfs_close(clone);
		}
		zpool_log_history(g_zfs, argc, argv, argv[2], B_FALSE, B_FALSE);
	}

	zfs_close(zhp);

	return (ret == 0 ? 0 : 1);
}

/*
 * zfs create [-o prop=value] ... fs
 * zfs create [-s] [-b blocksize] [-o prop=value] ... -V vol size
 *
 * Create a new dataset.  This command can be used to create filesystems
 * and volumes.  Snapshot creation is handled by 'zfs snapshot'.
 * For volumes, the user must specify a size to be used.
 *
 * The '-s' flag applies only to volumes, and indicates that we should not try
 * to set the reservation for this volume.  By default we set a reservation
 * equal to the size for any volume.
 */
static int
zfs_do_create(int argc, char **argv)
{
	zfs_type_t type = ZFS_TYPE_FILESYSTEM;
	zfs_handle_t *zhp = NULL;
	uint64_t volsize;
	int c;
	boolean_t noreserve = B_FALSE;
	int ret = 1;
	nvlist_t *props = NULL;
	uint64_t intval;
	char *propname;
	char *propval = NULL;
	char *strval;

	if (nvlist_alloc(&props, NV_UNIQUE_NAME, 0) != 0) {
		(void) fprintf(stderr, gettext("internal error: "
		    "out of memory\n"));
		return (1);
	}

	/* check options */
	while ((c = getopt(argc, argv, ":V:b:so:")) != -1) {
		switch (c) {
		case 'V':
			type = ZFS_TYPE_VOLUME;
			if (zfs_nicestrtonum(g_zfs, optarg, &intval) != 0) {
				(void) fprintf(stderr, gettext("bad volume "
				    "size '%s': %s\n"), optarg,
				    libzfs_error_description(g_zfs));
				goto error;
			}

			if (nvlist_add_uint64(props,
			    zfs_prop_to_name(ZFS_PROP_VOLSIZE),
			    intval) != 0) {
				(void) fprintf(stderr, gettext("internal "
				    "error: out of memory\n"));
				goto error;
			}
			volsize = intval;
			break;
		case 'b':
			if (zfs_nicestrtonum(g_zfs, optarg, &intval) != 0) {
				(void) fprintf(stderr, gettext("bad volume "
				    "block size '%s': %s\n"), optarg,
				    libzfs_error_description(g_zfs));
				goto error;
			}

			if (nvlist_add_uint64(props,
			    zfs_prop_to_name(ZFS_PROP_VOLBLOCKSIZE),
			    intval) != 0) {
				(void) fprintf(stderr, gettext("internal "
				    "error: out of memory\n"));
				goto error;
			}
			break;
		case 'o':
			propname = optarg;
			if ((propval = strchr(propname, '=')) == NULL) {
				(void) fprintf(stderr, gettext("missing "
				    "'=' for -o option\n"));
				goto error;
			}
			*propval = '\0';
			propval++;
			if (nvlist_lookup_string(props, propname,
			    &strval) == 0) {
				(void) fprintf(stderr, gettext("property '%s' "
				    "specified multiple times\n"), propname);
				goto error;
			}
			if (nvlist_add_string(props, propname, propval) != 0) {
				(void) fprintf(stderr, gettext("internal "
				    "error: out of memory\n"));
				goto error;
			}
			break;
		case 's':
			noreserve = B_TRUE;
			break;
		case ':':
			(void) fprintf(stderr, gettext("missing size "
			    "argument\n"));
			goto badusage;
			break;
		case '?':
			(void) fprintf(stderr, gettext("invalid option '%c'\n"),
			    optopt);
			goto badusage;
		}
	}

	if (noreserve && type != ZFS_TYPE_VOLUME) {
		(void) fprintf(stderr, gettext("'-s' can only be used when "
		    "creating a volume\n"));
		goto badusage;
	}

	argc -= optind;
	argv += optind;

	/* check number of arguments */
	if (argc == 0) {
		(void) fprintf(stderr, gettext("missing %s argument\n"),
		    zfs_type_to_name(type));
		goto badusage;
	}
	if (argc > 1) {
		(void) fprintf(stderr, gettext("too many arguments\n"));
		goto badusage;
	}

	if (type == ZFS_TYPE_VOLUME && !noreserve &&
	    nvlist_lookup_string(props, zfs_prop_to_name(ZFS_PROP_RESERVATION),
	    &strval) != 0) {
		if (nvlist_add_uint64(props,
		    zfs_prop_to_name(ZFS_PROP_RESERVATION),
		    volsize) != 0) {
			(void) fprintf(stderr, gettext("internal "
			    "error: out of memory\n"));
			nvlist_free(props);
			return (1);
		}
	}

	/* pass to libzfs */
	if (zfs_create(g_zfs, argv[0], type, props) != 0)
		goto error;

	if (propval != NULL)
		*(propval - 1) = '=';
	zpool_log_history(g_zfs, argc + optind, argv - optind, argv[0],
	    B_FALSE, B_FALSE);

	if ((zhp = zfs_open(g_zfs, argv[0], ZFS_TYPE_ANY)) == NULL)
		goto error;

	/*
	 * Mount and/or share the new filesystem as appropriate.  We provide a
	 * verbose error message to let the user know that their filesystem was
	 * in fact created, even if we failed to mount or share it.
	 */
	if (zfs_mount(zhp, NULL, 0) != 0) {
		(void) fprintf(stderr, gettext("filesystem successfully "
		    "created, but not mounted\n"));
		ret = 1;
	} else if (zfs_share(zhp) != 0) {
		(void) fprintf(stderr, gettext("filesystem successfully "
		    "created, but not shared\n"));
		ret = 1;
	} else {
		ret = 0;
	}

error:
	if (zhp)
		zfs_close(zhp);
	nvlist_free(props);
	return (ret);
badusage:
	nvlist_free(props);
	usage(B_FALSE);
	return (2);
}

/*
 * zfs destroy [-rf] <fs, snap, vol>
 *
 * 	-r	Recursively destroy all children
 * 	-R	Recursively destroy all dependents, including clones
 * 	-f	Force unmounting of any dependents
 *
 * Destroys the given dataset.  By default, it will unmount any filesystems,
 * and refuse to destroy a dataset that has any dependents.  A dependent can
 * either be a child, or a clone of a child.
 */
typedef struct destroy_cbdata {
	boolean_t	cb_first;
	int		cb_force;
	int		cb_recurse;
	int		cb_error;
	int		cb_needforce;
	int		cb_doclones;
	boolean_t	cb_closezhp;
	zfs_handle_t	*cb_target;
	char		*cb_snapname;
} destroy_cbdata_t;

/*
 * Check for any dependents based on the '-r' or '-R' flags.
 */
static int
destroy_check_dependent(zfs_handle_t *zhp, void *data)
{
	destroy_cbdata_t *cbp = data;
	const char *tname = zfs_get_name(cbp->cb_target);
	const char *name = zfs_get_name(zhp);

	if (strncmp(tname, name, strlen(tname)) == 0 &&
	    (name[strlen(tname)] == '/' || name[strlen(tname)] == '@')) {
		/*
		 * This is a direct descendant, not a clone somewhere else in
		 * the hierarchy.
		 */
		if (cbp->cb_recurse)
			goto out;

		if (cbp->cb_first) {
			(void) fprintf(stderr, gettext("cannot destroy '%s': "
			    "%s has children\n"),
			    zfs_get_name(cbp->cb_target),
			    zfs_type_to_name(zfs_get_type(cbp->cb_target)));
			(void) fprintf(stderr, gettext("use '-r' to destroy "
			    "the following datasets:\n"));
			cbp->cb_first = B_FALSE;
			cbp->cb_error = 1;
		}

		(void) fprintf(stderr, "%s\n", zfs_get_name(zhp));
	} else {
		/*
		 * This is a clone.  We only want to report this if the '-r'
		 * wasn't specified, or the target is a snapshot.
		 */
		if (!cbp->cb_recurse &&
		    zfs_get_type(cbp->cb_target) != ZFS_TYPE_SNAPSHOT)
			goto out;

		if (cbp->cb_first) {
			(void) fprintf(stderr, gettext("cannot destroy '%s': "
			    "%s has dependent clones\n"),
			    zfs_get_name(cbp->cb_target),
			    zfs_type_to_name(zfs_get_type(cbp->cb_target)));
			(void) fprintf(stderr, gettext("use '-R' to destroy "
			    "the following datasets:\n"));
			cbp->cb_first = B_FALSE;
			cbp->cb_error = 1;
		}

		(void) fprintf(stderr, "%s\n", zfs_get_name(zhp));
	}

out:
	zfs_close(zhp);
	return (0);
}

static int
destroy_callback(zfs_handle_t *zhp, void *data)
{
	destroy_cbdata_t *cbp = data;

	/*
	 * Ignore pools (which we've already flagged as an error before getting
	 * here.
	 */
	if (strchr(zfs_get_name(zhp), '/') == NULL &&
	    zfs_get_type(zhp) == ZFS_TYPE_FILESYSTEM) {
		zfs_close(zhp);
		return (0);
	}

	/*
	 * Bail out on the first error.
	 */
	if (zfs_unmount(zhp, NULL, cbp->cb_force ? MS_FORCE : 0) != 0 ||
	    zfs_destroy(zhp) != 0) {
		zfs_close(zhp);
		return (-1);
	}

	zfs_close(zhp);
	return (0);
}

static int
destroy_snap_clones(zfs_handle_t *zhp, void *arg)
{
	destroy_cbdata_t *cbp = arg;
	char thissnap[MAXPATHLEN];
	zfs_handle_t *szhp;
	boolean_t closezhp = cbp->cb_closezhp;
	int rv;

	(void) snprintf(thissnap, sizeof (thissnap),
	    "%s@%s", zfs_get_name(zhp), cbp->cb_snapname);

	libzfs_print_on_error(g_zfs, B_FALSE);
	szhp = zfs_open(g_zfs, thissnap, ZFS_TYPE_SNAPSHOT);
	libzfs_print_on_error(g_zfs, B_TRUE);
	if (szhp) {
		/*
		 * Destroy any clones of this snapshot
		 */
		if (zfs_iter_dependents(szhp, B_FALSE, destroy_callback,
		    cbp) != 0) {
			zfs_close(szhp);
			if (closezhp)
				zfs_close(zhp);
			return (-1);
		}
		zfs_close(szhp);
	}

	cbp->cb_closezhp = B_TRUE;
	rv = zfs_iter_filesystems(zhp, destroy_snap_clones, arg);
	if (closezhp)
		zfs_close(zhp);
	return (rv);
}

static int
zfs_do_destroy(int argc, char **argv)
{
	destroy_cbdata_t cb = { 0 };
	int c;
	zfs_handle_t *zhp;
	char *cp;

	/* check options */
	while ((c = getopt(argc, argv, "frR")) != -1) {
		switch (c) {
		case 'f':
			cb.cb_force = 1;
			break;
		case 'r':
			cb.cb_recurse = 1;
			break;
		case 'R':
			cb.cb_recurse = 1;
			cb.cb_doclones = 1;
			break;
		case '?':
		default:
			(void) fprintf(stderr, gettext("invalid option '%c'\n"),
			    optopt);
			usage(B_FALSE);
		}
	}

	argc -= optind;
	argv += optind;

	/* check number of arguments */
	if (argc == 0) {
		(void) fprintf(stderr, gettext("missing path argument\n"));
		usage(B_FALSE);
	}
	if (argc > 1) {
		(void) fprintf(stderr, gettext("too many arguments\n"));
		usage(B_FALSE);
	}

	/*
	 * If we are doing recursive destroy of a snapshot, then the
	 * named snapshot may not exist.  Go straight to libzfs.
	 */
	if (cb.cb_recurse && (cp = strchr(argv[0], '@'))) {
		int ret;

		*cp = '\0';
		if ((zhp = zfs_open(g_zfs, argv[0], ZFS_TYPE_ANY)) == NULL)
			return (1);
		*cp = '@';
		cp++;

		if (cb.cb_doclones) {
			cb.cb_snapname = cp;
			if (destroy_snap_clones(zhp, &cb) != 0) {
				zfs_close(zhp);
				return (1);
			}
		}

		ret = zfs_destroy_snaps(zhp, cp);
		zfs_close(zhp);
		if (ret) {
			(void) fprintf(stderr,
			    gettext("no snapshots destroyed\n"));
		} else {
			zpool_log_history(g_zfs, argc + optind, argv - optind,
			    argv[0], B_FALSE, B_FALSE);
		}
		return (ret != 0);
	}


	/* Open the given dataset */
	if ((zhp = zfs_open(g_zfs, argv[0], ZFS_TYPE_ANY)) == NULL)
		return (1);

	cb.cb_target = zhp;

	/*
	 * Perform an explicit check for pools before going any further.
	 */
	if (!cb.cb_recurse && strchr(zfs_get_name(zhp), '/') == NULL &&
	    zfs_get_type(zhp) == ZFS_TYPE_FILESYSTEM) {
		(void) fprintf(stderr, gettext("cannot destroy '%s': "
		    "operation does not apply to pools\n"),
		    zfs_get_name(zhp));
		(void) fprintf(stderr, gettext("use 'zfs destroy -r "
		    "%s' to destroy all datasets in the pool\n"),
		    zfs_get_name(zhp));
		(void) fprintf(stderr, gettext("use 'zpool destroy %s' "
		    "to destroy the pool itself\n"), zfs_get_name(zhp));
		zfs_close(zhp);
		return (1);
	}

	/*
	 * Check for any dependents and/or clones.
	 */
	cb.cb_first = B_TRUE;
	if (!cb.cb_doclones &&
	    zfs_iter_dependents(zhp, B_TRUE, destroy_check_dependent,
	    &cb) != 0) {
		zfs_close(zhp);
		return (1);
	}


	if (cb.cb_error ||
	    zfs_iter_dependents(zhp, B_FALSE, destroy_callback, &cb) != 0) {
		zfs_close(zhp);
		return (1);
	}

	/*
	 * Do the real thing.  The callback will close the handle regardless of
	 * whether it succeeds or not.
	 */
	if (destroy_callback(zhp, &cb) != 0)
		return (1);

	zpool_log_history(g_zfs, argc + optind, argv - optind, argv[0],
	    B_FALSE, B_FALSE);

	return (0);
}

/*
 * zfs get [-rHp] [-o field[,field]...] [-s source[,source]...]
 * 	< all | property[,property]... > < fs | snap | vol > ...
 *
 *	-r	recurse over any child datasets
 *	-H	scripted mode.  Headers are stripped, and fields are separated
 *		by tabs instead of spaces.
 *	-o	Set of fields to display.  One of "name,property,value,source".
 *		Default is all four.
 *	-s	Set of sources to allow.  One of
 *		"local,default,inherited,temporary,none".  Default is all
 *		five.
 *	-p	Display values in parsable (literal) format.
 *
 *  Prints properties for the given datasets.  The user can control which
 *  columns to display as well as which property types to allow.
 */

/*
 * Invoked to display the properties for a single dataset.
 */
static int
get_callback(zfs_handle_t *zhp, void *data)
{
	char buf[ZFS_MAXPROPLEN];
	zfs_source_t sourcetype;
	char source[ZFS_MAXNAMELEN];
	libzfs_get_cbdata_t *cbp = data;
	nvlist_t *userprop = zfs_get_user_props(zhp);
	zfs_proplist_t *pl = cbp->cb_proplist;
	nvlist_t *propval;
	char *strval;
	char *sourceval;

	for (; pl != NULL; pl = pl->pl_next) {
		/*
		 * Skip the special fake placeholder.  This will also skip over
		 * the name property when 'all' is specified.
		 */
		if (pl->pl_prop == ZFS_PROP_NAME &&
		    pl == cbp->cb_proplist)
			continue;

		if (pl->pl_prop != ZFS_PROP_INVAL) {
			if (zfs_prop_get(zhp, pl->pl_prop, buf,
			    sizeof (buf), &sourcetype, source,
			    sizeof (source),
			    cbp->cb_literal) != 0) {
				if (pl->pl_all)
					continue;
				if (!zfs_prop_valid_for_type(pl->pl_prop,
				    ZFS_TYPE_ANY)) {
					(void) fprintf(stderr,
					    gettext("No such property '%s'\n"),
					    zfs_prop_to_name(pl->pl_prop));
					continue;
				}
				sourcetype = ZFS_SRC_NONE;
				(void) strlcpy(buf, "-", sizeof (buf));
			}

			libzfs_print_one_property(zfs_get_name(zhp), cbp,
			    zfs_prop_to_name(pl->pl_prop),
			    buf, sourcetype, source);
		} else {
			if (nvlist_lookup_nvlist(userprop,
			    pl->pl_user_prop, &propval) != 0) {
				if (pl->pl_all)
					continue;
				sourcetype = ZFS_SRC_NONE;
				strval = "-";
			} else {
				verify(nvlist_lookup_string(propval,
				    ZFS_PROP_VALUE, &strval) == 0);
				verify(nvlist_lookup_string(propval,
				    ZFS_PROP_SOURCE, &sourceval) == 0);

				if (strcmp(sourceval,
				    zfs_get_name(zhp)) == 0) {
					sourcetype = ZFS_SRC_LOCAL;
				} else {
					sourcetype = ZFS_SRC_INHERITED;
					(void) strlcpy(source,
					    sourceval, sizeof (source));
				}
			}

			libzfs_print_one_property(zfs_get_name(zhp), cbp,
			    pl->pl_user_prop, strval, sourcetype,
			    source);
		}
	}

	return (0);
}

static int
zfs_do_get(int argc, char **argv)
{
	libzfs_get_cbdata_t cb = { 0 };
	boolean_t recurse = B_FALSE;
	int i, c;
	char *value, *fields;
	int ret;
	zfs_proplist_t fake_name = { 0 };

	/*
	 * Set up default columns and sources.
	 */
	cb.cb_sources = ZFS_SRC_ALL;
	cb.cb_columns[0] = GET_COL_NAME;
	cb.cb_columns[1] = GET_COL_PROPERTY;
	cb.cb_columns[2] = GET_COL_VALUE;
	cb.cb_columns[3] = GET_COL_SOURCE;

	/* check options */
	while ((c = getopt(argc, argv, ":o:s:rHp")) != -1) {
		switch (c) {
		case 'p':
			cb.cb_literal = B_TRUE;
			break;
		case 'r':
			recurse = B_TRUE;
			break;
		case 'H':
			cb.cb_scripted = B_TRUE;
			break;
		case ':':
			(void) fprintf(stderr, gettext("missing argument for "
			    "'%c' option\n"), optopt);
			usage(B_FALSE);
			break;
		case 'o':
			/*
			 * Process the set of columns to display.  We zero out
			 * the structure to give us a blank slate.
			 */
			bzero(&cb.cb_columns, sizeof (cb.cb_columns));
			i = 0;
			while (*optarg != '\0') {
				static char *col_subopts[] =
				    { "name", "property", "value", "source",
				    NULL };

				if (i == 4) {
					(void) fprintf(stderr, gettext("too "
					    "many fields given to -o "
					    "option\n"));
					usage(B_FALSE);
				}

				switch (getsubopt(&optarg, col_subopts,
				    &value)) {
				case 0:
					cb.cb_columns[i++] = GET_COL_NAME;
					break;
				case 1:
					cb.cb_columns[i++] = GET_COL_PROPERTY;
					break;
				case 2:
					cb.cb_columns[i++] = GET_COL_VALUE;
					break;
				case 3:
					cb.cb_columns[i++] = GET_COL_SOURCE;
					break;
				default:
					(void) fprintf(stderr,
					    gettext("invalid column name "
					    "'%s'\n"), value);
					usage(B_FALSE);
				}
			}
			break;

		case 's':
			cb.cb_sources = 0;
			while (*optarg != '\0') {
				static char *source_subopts[] = {
					"local", "default", "inherited",
					"temporary", "none", NULL };

				switch (getsubopt(&optarg, source_subopts,
				    &value)) {
				case 0:
					cb.cb_sources |= ZFS_SRC_LOCAL;
					break;
				case 1:
					cb.cb_sources |= ZFS_SRC_DEFAULT;
					break;
				case 2:
					cb.cb_sources |= ZFS_SRC_INHERITED;
					break;
				case 3:
					cb.cb_sources |= ZFS_SRC_TEMPORARY;
					break;
				case 4:
					cb.cb_sources |= ZFS_SRC_NONE;
					break;
				default:
					(void) fprintf(stderr,
					    gettext("invalid source "
					    "'%s'\n"), value);
					usage(B_FALSE);
				}
			}
			break;

		case '?':
			(void) fprintf(stderr, gettext("invalid option '%c'\n"),
			    optopt);
			usage(B_FALSE);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc < 1) {
		(void) fprintf(stderr, gettext("missing property "
		    "argument\n"));
		usage(B_FALSE);
	}

	fields = argv[0];

	if (zfs_get_proplist(g_zfs, fields, &cb.cb_proplist) != 0)
		usage(B_FALSE);

	argc--;
	argv++;

	/*
	 * As part of zfs_expand_proplist(), we keep track of the maximum column
	 * width for each property.  For the 'NAME' (and 'SOURCE') columns, we
	 * need to know the maximum name length.  However, the user likely did
	 * not specify 'name' as one of the properties to fetch, so we need to
	 * make sure we always include at least this property for
	 * print_get_headers() to work properly.
	 */
	if (cb.cb_proplist != NULL) {
		fake_name.pl_prop = ZFS_PROP_NAME;
		fake_name.pl_width = strlen(gettext("NAME"));
		fake_name.pl_next = cb.cb_proplist;
		cb.cb_proplist = &fake_name;
	}

	cb.cb_first = B_TRUE;

	/* run for each object */
	ret = zfs_for_each(argc, argv, recurse, ZFS_TYPE_ANY, NULL,
	    &cb.cb_proplist, get_callback, &cb, B_FALSE);

	if (cb.cb_proplist == &fake_name)
		zfs_free_proplist(fake_name.pl_next);
	else
		zfs_free_proplist(cb.cb_proplist);

	return (ret);
}

/*
 * inherit [-r] <property> <fs|vol> ...
 *
 * 	-r	Recurse over all children
 *
 * For each dataset specified on the command line, inherit the given property
 * from its parent.  Inheriting a property at the pool level will cause it to
 * use the default value.  The '-r' flag will recurse over all children, and is
 * useful for setting a property on a hierarchy-wide basis, regardless of any
 * local modifications for each dataset.
 */
typedef struct inherit_cbdata {
	char		*cb_propname;
	boolean_t	cb_any_successful;
} inherit_cbdata_t;

static int
inherit_callback(zfs_handle_t *zhp, void *data)
{
	inherit_cbdata_t *cbp = data;
	int ret;

	ret = zfs_prop_inherit(zhp, cbp->cb_propname);
	if (ret == 0)
		cbp->cb_any_successful = B_TRUE;
	return (ret != 0);
}

static int
zfs_do_inherit(int argc, char **argv)
{
	boolean_t recurse = B_FALSE;
	int c;
	zfs_prop_t prop;
	inherit_cbdata_t cb;
	int ret;

	/* check options */
	while ((c = getopt(argc, argv, "r")) != -1) {
		switch (c) {
		case 'r':
			recurse = B_TRUE;
			break;
		case '?':
		default:
			(void) fprintf(stderr, gettext("invalid option '%c'\n"),
			    optopt);
			usage(B_FALSE);
		}
	}

	argc -= optind;
	argv += optind;

	/* check number of arguments */
	if (argc < 1) {
		(void) fprintf(stderr, gettext("missing property argument\n"));
		usage(B_FALSE);
	}
	if (argc < 2) {
		(void) fprintf(stderr, gettext("missing dataset argument\n"));
		usage(B_FALSE);
	}

	cb.cb_propname = argv[0];
	argc--;
	argv++;

	if ((prop = zfs_name_to_prop(cb.cb_propname)) != ZFS_PROP_INVAL) {
		if (zfs_prop_readonly(prop)) {
			(void) fprintf(stderr, gettext(
			    "%s property is read-only\n"),
			    cb.cb_propname);
			return (1);
		}
		if (!zfs_prop_inheritable(prop)) {
			(void) fprintf(stderr, gettext("'%s' property cannot "
			    "be inherited\n"), cb.cb_propname);
			if (prop == ZFS_PROP_QUOTA ||
			    prop == ZFS_PROP_RESERVATION)
				(void) fprintf(stderr, gettext("use 'zfs set "
				    "%s=none' to clear\n"), cb.cb_propname);
			return (1);
		}
	} else if (!zfs_prop_user(cb.cb_propname)) {
		(void) fprintf(stderr, gettext(
		    "invalid property '%s'\n"),
		    cb.cb_propname);
		usage(B_FALSE);
	}

	cb.cb_any_successful = B_FALSE;

	ret = zfs_for_each(argc, argv, recurse,
	    ZFS_TYPE_FILESYSTEM | ZFS_TYPE_VOLUME, NULL, NULL,
	    inherit_callback, &cb, B_FALSE);

	if (cb.cb_any_successful) {
		zpool_log_history(g_zfs, argc + optind + 1, argv - optind - 1,
		    argv[0], B_FALSE, B_FALSE);
	}

	return (ret);
}

/*
 * list [-rH] [-o property[,property]...] [-t type[,type]...]
 *      [-s property [-s property]...] [-S property [-S property]...]
 *      <dataset> ...
 *
 * 	-r	Recurse over all children
 * 	-H	Scripted mode; elide headers and separate colums by tabs
 * 	-o	Control which fields to display.
 * 	-t	Control which object types to display.
 *	-s	Specify sort columns, descending order.
 *	-S	Specify sort columns, ascending order.
 *
 * When given no arguments, lists all filesystems in the system.
 * Otherwise, list the specified datasets, optionally recursing down them if
 * '-r' is specified.
 */
typedef struct list_cbdata {
	boolean_t	cb_first;
	boolean_t	cb_scripted;
	zfs_proplist_t	*cb_proplist;
} list_cbdata_t;

/*
 * Given a list of columns to display, output appropriate headers for each one.
 */
static void
print_header(zfs_proplist_t *pl)
{
	char headerbuf[ZFS_MAXPROPLEN];
	const char *header;
	int i;
	boolean_t first = B_TRUE;
	boolean_t right_justify;

	for (; pl != NULL; pl = pl->pl_next) {
		if (!first) {
			(void) printf("  ");
		} else {
			first = B_FALSE;
		}

		right_justify = B_FALSE;
		if (pl->pl_prop != ZFS_PROP_INVAL) {
			header = zfs_prop_column_name(pl->pl_prop);
			right_justify = zfs_prop_align_right(pl->pl_prop);
		} else {
			for (i = 0; pl->pl_user_prop[i] != '\0'; i++)
				headerbuf[i] = toupper(pl->pl_user_prop[i]);
			headerbuf[i] = '\0';
			header = headerbuf;
		}

		if (pl->pl_next == NULL && !right_justify)
			(void) printf("%s", header);
		else if (right_justify)
			(void) printf("%*s", pl->pl_width, header);
		else
			(void) printf("%-*s", pl->pl_width, header);
	}

	(void) printf("\n");
}

/*
 * Given a dataset and a list of fields, print out all the properties according
 * to the described layout.
 */
static void
print_dataset(zfs_handle_t *zhp, zfs_proplist_t *pl, int scripted)
{
	boolean_t first = B_TRUE;
	char property[ZFS_MAXPROPLEN];
	nvlist_t *userprops = zfs_get_user_props(zhp);
	nvlist_t *propval;
	char *propstr;
	boolean_t right_justify;
	int width;

	for (; pl != NULL; pl = pl->pl_next) {
		if (!first) {
			if (scripted)
				(void) printf("\t");
			else
				(void) printf("  ");
		} else {
			first = B_FALSE;
		}

		right_justify = B_FALSE;
		if (pl->pl_prop != ZFS_PROP_INVAL) {
			if (zfs_prop_get(zhp, pl->pl_prop, property,
			    sizeof (property), NULL, NULL, 0, B_FALSE) != 0)
				propstr = "-";
			else
				propstr = property;

			right_justify = zfs_prop_align_right(pl->pl_prop);
		} else {
			if (nvlist_lookup_nvlist(userprops,
			    pl->pl_user_prop, &propval) != 0)
				propstr = "-";
			else
				verify(nvlist_lookup_string(propval,
				    ZFS_PROP_VALUE, &propstr) == 0);
		}

		width = pl->pl_width;

		/*
		 * If this is being called in scripted mode, or if this is the
		 * last column and it is left-justified, don't include a width
		 * format specifier.
		 */
		if (scripted || (pl->pl_next == NULL && !right_justify))
			(void) printf("%s", propstr);
		else if (right_justify)
			(void) printf("%*s", width, propstr);
		else
			(void) printf("%-*s", width, propstr);
	}

	(void) printf("\n");
}

/*
 * Generic callback function to list a dataset or snapshot.
 */
static int
list_callback(zfs_handle_t *zhp, void *data)
{
	list_cbdata_t *cbp = data;

	if (cbp->cb_first) {
		if (!cbp->cb_scripted)
			print_header(cbp->cb_proplist);
		cbp->cb_first = B_FALSE;
	}

	print_dataset(zhp, cbp->cb_proplist, cbp->cb_scripted);

	return (0);
}

static int
zfs_do_list(int argc, char **argv)
{
	int c;
	boolean_t recurse = B_FALSE;
	boolean_t scripted = B_FALSE;
	static char default_fields[] =
	    "name,used,available,referenced,mountpoint";
	int types = ZFS_TYPE_ANY;
	char *fields = NULL;
	char *basic_fields = default_fields;
	list_cbdata_t cb = { 0 };
	char *value;
	int ret;
	char *type_subopts[] = { "filesystem", "volume", "snapshot", NULL };
	zfs_sort_column_t *sortcol = NULL;

	/* check options */
	while ((c = getopt(argc, argv, ":o:rt:Hs:S:")) != -1) {
		switch (c) {
		case 'o':
			fields = optarg;
			break;
		case 'r':
			recurse = B_TRUE;
			break;
		case 'H':
			scripted = B_TRUE;
			break;
		case 's':
			if (zfs_add_sort_column(&sortcol, optarg,
			    B_FALSE) != 0) {
				(void) fprintf(stderr,
				    gettext("invalid property '%s'\n"), optarg);
				usage(B_FALSE);
			}
			break;
		case 'S':
			if (zfs_add_sort_column(&sortcol, optarg,
			    B_TRUE) != 0) {
				(void) fprintf(stderr,
				    gettext("invalid property '%s'\n"), optarg);
				usage(B_FALSE);
			}
			break;
		case 't':
			types = 0;
			while (*optarg != '\0') {
				switch (getsubopt(&optarg, type_subopts,
				    &value)) {
				case 0:
					types |= ZFS_TYPE_FILESYSTEM;
					break;
				case 1:
					types |= ZFS_TYPE_VOLUME;
					break;
				case 2:
					types |= ZFS_TYPE_SNAPSHOT;
					break;
				default:
					(void) fprintf(stderr,
					    gettext("invalid type '%s'\n"),
					    value);
					usage(B_FALSE);
				}
			}
			break;
		case ':':
			(void) fprintf(stderr, gettext("missing argument for "
			    "'%c' option\n"), optopt);
			usage(B_FALSE);
			break;
		case '?':
			(void) fprintf(stderr, gettext("invalid option '%c'\n"),
			    optopt);
			usage(B_FALSE);
		}
	}

	argc -= optind;
	argv += optind;

	if (fields == NULL)
		fields = basic_fields;

	/*
	 * If the user specifies '-o all', the zfs_get_proplist() doesn't
	 * normally include the name of the dataset.  For 'zfs list', we always
	 * want this property to be first.
	 */
	if (zfs_get_proplist(g_zfs, fields, &cb.cb_proplist) != 0)
		usage(B_FALSE);

	cb.cb_scripted = scripted;
	cb.cb_first = B_TRUE;

	ret = zfs_for_each(argc, argv, recurse, types, sortcol, &cb.cb_proplist,
	    list_callback, &cb, B_TRUE);

	zfs_free_proplist(cb.cb_proplist);
	zfs_free_sort_columns(sortcol);

	if (ret == 0 && cb.cb_first)
		(void) printf(gettext("no datasets available\n"));

	return (ret);
}

/*
 * zfs rename [-r] <fs | snap | vol> <fs | snap | vol>
 *
 * Renames the given dataset to another of the same type.
 */
/* ARGSUSED */
static int
zfs_do_rename(int argc, char **argv)
{
	zfs_handle_t *zhp;
	int c;
	int ret;
	int recurse = 0;

	/* check options */
	while ((c = getopt(argc, argv, "r")) != -1) {
		switch (c) {
		case 'r':
			recurse = 1;
			break;
		case '?':
		default:
			(void) fprintf(stderr, gettext("invalid option '%c'\n"),
			    optopt);
			usage(B_FALSE);
		}
	}

	argc -= optind;
	argv += optind;

	/* check number of arguments */
	if (argc < 1) {
		(void) fprintf(stderr, gettext("missing source dataset "
		    "argument\n"));
		usage(B_FALSE);
	}
	if (argc < 2) {
		(void) fprintf(stderr, gettext("missing target dataset "
		    "argument\n"));
		usage(B_FALSE);
	}
	if (argc > 2) {
		(void) fprintf(stderr, gettext("too many arguments\n"));
		usage(B_FALSE);
	}

	if (recurse && strchr(argv[0], '@') == 0) {
		(void) fprintf(stderr, gettext("source dataset for recursive "
		    "rename must be a snapshot\n"));
		usage(B_FALSE);
	}

	if ((zhp = zfs_open(g_zfs, argv[0], ZFS_TYPE_ANY)) == NULL)
		return (1);

	ret = (zfs_rename(zhp, argv[1], recurse) != 0);

	if (!ret)
		zpool_log_history(g_zfs, argc + optind, argv - optind, argv[1],
		    B_FALSE, B_FALSE);

	zfs_close(zhp);
	return (ret);
}

/*
 * zfs promote <fs>
 *
 * Promotes the given clone fs to be the parent
 */
/* ARGSUSED */
static int
zfs_do_promote(int argc, char **argv)
{
	zfs_handle_t *zhp;
	int ret;

	/* check options */
	if (argc > 1 && argv[1][0] == '-') {
		(void) fprintf(stderr, gettext("invalid option '%c'\n"),
		    argv[1][1]);
		usage(B_FALSE);
	}

	/* check number of arguments */
	if (argc < 2) {
		(void) fprintf(stderr, gettext("missing clone filesystem"
		    " argument\n"));
		usage(B_FALSE);
	}
	if (argc > 2) {
		(void) fprintf(stderr, gettext("too many arguments\n"));
		usage(B_FALSE);
	}

	zhp = zfs_open(g_zfs, argv[1], ZFS_TYPE_FILESYSTEM | ZFS_TYPE_VOLUME);
	if (zhp == NULL)
		return (1);

	ret = (zfs_promote(zhp) != 0);

	if (!ret)
		zpool_log_history(g_zfs, argc, argv, argv[1], B_FALSE, B_FALSE);

	zfs_close(zhp);
	return (ret);
}

/*
 * zfs rollback [-rfR] <snapshot>
 *
 * 	-r	Delete any intervening snapshots before doing rollback
 * 	-R	Delete any snapshots and their clones
 * 	-f	Force unmount filesystems, even if they are in use.
 *
 * Given a filesystem, rollback to a specific snapshot, discarding any changes
 * since then and making it the active dataset.  If more recent snapshots exist,
 * the command will complain unless the '-r' flag is given.
 */
typedef struct rollback_cbdata {
	uint64_t	cb_create;
	boolean_t	cb_first;
	int		cb_doclones;
	char		*cb_target;
	int		cb_error;
	boolean_t	cb_recurse;
	boolean_t	cb_dependent;
} rollback_cbdata_t;

/*
 * Report any snapshots more recent than the one specified.  Used when '-r' is
 * not specified.  We reuse this same callback for the snapshot dependents - if
 * 'cb_dependent' is set, then this is a dependent and we should report it
 * without checking the transaction group.
 */
static int
rollback_check(zfs_handle_t *zhp, void *data)
{
	rollback_cbdata_t *cbp = data;

	if (cbp->cb_doclones) {
		zfs_close(zhp);
		return (0);
	}

	if (!cbp->cb_dependent) {
		if (strcmp(zfs_get_name(zhp), cbp->cb_target) != 0 &&
		    zfs_get_type(zhp) == ZFS_TYPE_SNAPSHOT &&
		    zfs_prop_get_int(zhp, ZFS_PROP_CREATETXG) >
		    cbp->cb_create) {

			if (cbp->cb_first && !cbp->cb_recurse) {
				(void) fprintf(stderr, gettext("cannot "
				    "rollback to '%s': more recent snapshots "
				    "exist\n"),
				    cbp->cb_target);
				(void) fprintf(stderr, gettext("use '-r' to "
				    "force deletion of the following "
				    "snapshots:\n"));
				cbp->cb_first = 0;
				cbp->cb_error = 1;
			}

			if (cbp->cb_recurse) {
				cbp->cb_dependent = B_TRUE;
				if (zfs_iter_dependents(zhp, B_TRUE,
				    rollback_check, cbp) != 0) {
					zfs_close(zhp);
					return (-1);
				}
				cbp->cb_dependent = B_FALSE;
			} else {
				(void) fprintf(stderr, "%s\n",
				    zfs_get_name(zhp));
			}
		}
	} else {
		if (cbp->cb_first && cbp->cb_recurse) {
			(void) fprintf(stderr, gettext("cannot rollback to "
			    "'%s': clones of previous snapshots exist\n"),
			    cbp->cb_target);
			(void) fprintf(stderr, gettext("use '-R' to "
			    "force deletion of the following clones and "
			    "dependents:\n"));
			cbp->cb_first = 0;
			cbp->cb_error = 1;
		}

		(void) fprintf(stderr, "%s\n", zfs_get_name(zhp));
	}

	zfs_close(zhp);
	return (0);
}

static int
zfs_do_rollback(int argc, char **argv)
{
	int ret;
	int c;
	rollback_cbdata_t cb = { 0 };
	zfs_handle_t *zhp, *snap;
	char parentname[ZFS_MAXNAMELEN];
	char *delim;
	int force = 0;

	/* check options */
	while ((c = getopt(argc, argv, "rfR")) != -1) {
		switch (c) {
		case 'f':
			force = 1;
			break;
		case 'r':
			cb.cb_recurse = 1;
			break;
		case 'R':
			cb.cb_recurse = 1;
			cb.cb_doclones = 1;
			break;
		case '?':
			(void) fprintf(stderr, gettext("invalid option '%c'\n"),
			    optopt);
			usage(B_FALSE);
		}
	}

	argc -= optind;
	argv += optind;

	/* check number of arguments */
	if (argc < 1) {
		(void) fprintf(stderr, gettext("missing dataset argument\n"));
		usage(B_FALSE);
	}
	if (argc > 1) {
		(void) fprintf(stderr, gettext("too many arguments\n"));
		usage(B_FALSE);
	}

	/* open the snapshot */
	if ((snap = zfs_open(g_zfs, argv[0], ZFS_TYPE_SNAPSHOT)) == NULL)
		return (1);

	/* open the parent dataset */
	(void) strlcpy(parentname, argv[0], sizeof (parentname));
	verify((delim = strrchr(parentname, '@')) != NULL);
	*delim = '\0';
	if ((zhp = zfs_open(g_zfs, parentname, ZFS_TYPE_ANY)) == NULL) {
		zfs_close(snap);
		return (1);
	}

	/*
	 * Check for more recent snapshots and/or clones based on the presence
	 * of '-r' and '-R'.
	 */
	cb.cb_target = argv[0];
	cb.cb_create = zfs_prop_get_int(snap, ZFS_PROP_CREATETXG);
	cb.cb_first = B_TRUE;
	cb.cb_error = 0;
	if ((ret = zfs_iter_children(zhp, rollback_check, &cb)) != 0)
		goto out;

	if ((ret = cb.cb_error) != 0)
		goto out;

	/*
	 * Rollback parent to the given snapshot.
	 */
	ret = zfs_rollback(zhp, snap, force);

	if (!ret) {
		zpool_log_history(g_zfs, argc + optind, argv - optind, argv[0],
		    B_FALSE, B_FALSE);
	}

out:
	zfs_close(snap);
	zfs_close(zhp);

	if (ret == 0)
		return (0);
	else
		return (1);
}

/*
 * zfs set property=value { fs | snap | vol } ...
 *
 * Sets the given property for all datasets specified on the command line.
 */
typedef struct set_cbdata {
	char		*cb_propname;
	char		*cb_value;
	boolean_t	cb_any_successful;
} set_cbdata_t;

static int
set_callback(zfs_handle_t *zhp, void *data)
{
	set_cbdata_t *cbp = data;

	if (zfs_prop_set(zhp, cbp->cb_propname, cbp->cb_value) != 0) {
		switch (libzfs_errno(g_zfs)) {
		case EZFS_MOUNTFAILED:
			(void) fprintf(stderr, gettext("property may be set "
			    "but unable to remount filesystem\n"));
			break;
		case EZFS_SHARENFSFAILED:
			(void) fprintf(stderr, gettext("property may be set "
			    "but unable to reshare filesystem\n"));
			break;
		}
		return (1);
	}
	cbp->cb_any_successful = B_TRUE;
	return (0);
}

static int
zfs_do_set(int argc, char **argv)
{
	set_cbdata_t cb;
	int ret;

	/* check for options */
	if (argc > 1 && argv[1][0] == '-') {
		(void) fprintf(stderr, gettext("invalid option '%c'\n"),
		    argv[1][1]);
		usage(B_FALSE);
	}

	/* check number of arguments */
	if (argc < 2) {
		(void) fprintf(stderr, gettext("missing property=value "
		    "argument\n"));
		usage(B_FALSE);
	}
	if (argc < 3) {
		(void) fprintf(stderr, gettext("missing dataset name\n"));
		usage(B_FALSE);
	}

	/* validate property=value argument */
	cb.cb_propname = argv[1];
	if ((cb.cb_value = strchr(cb.cb_propname, '=')) == NULL) {
		(void) fprintf(stderr, gettext("missing value in "
		    "property=value argument\n"));
		usage(B_FALSE);
	}

	*cb.cb_value = '\0';
	cb.cb_value++;
	cb.cb_any_successful = B_FALSE;

	if (*cb.cb_propname == '\0') {
		(void) fprintf(stderr,
		    gettext("missing property in property=value argument\n"));
		usage(B_FALSE);
	}

	ret = zfs_for_each(argc - 2, argv + 2, B_FALSE,
	    ZFS_TYPE_ANY, NULL, NULL, set_callback, &cb, B_FALSE);

	if (cb.cb_any_successful) {
		*(cb.cb_value - 1) = '=';
		zpool_log_history(g_zfs, argc, argv, argv[2], B_FALSE, B_FALSE);
	}

	return (ret);
}

/*
 * zfs snapshot [-r] <fs@snap>
 *
 * Creates a snapshot with the given name.  While functionally equivalent to
 * 'zfs create', it is a separate command to diffferentiate intent.
 */
static int
zfs_do_snapshot(int argc, char **argv)
{
	int recursive = B_FALSE;
	int ret;
	char c;

	/* check options */
	while ((c = getopt(argc, argv, ":r")) != -1) {
		switch (c) {
		case 'r':
			recursive = B_TRUE;
			break;
		case '?':
			(void) fprintf(stderr, gettext("invalid option '%c'\n"),
			    optopt);
			usage(B_FALSE);
		}
	}

	argc -= optind;
	argv += optind;

	/* check number of arguments */
	if (argc < 1) {
		(void) fprintf(stderr, gettext("missing snapshot argument\n"));
		usage(B_FALSE);
	}
	if (argc > 1) {
		(void) fprintf(stderr, gettext("too many arguments\n"));
		usage(B_FALSE);
	}

	ret = zfs_snapshot(g_zfs, argv[0], recursive);
	if (ret && recursive)
		(void) fprintf(stderr, gettext("no snapshots were created\n"));
	if (!ret) {
		zpool_log_history(g_zfs, argc + optind, argv - optind, argv[0],
		    B_FALSE, B_FALSE);
	}
	return (ret != 0);
}

/*
 * zfs send [-i <@snap>] <fs@snap>
 *
 * Send a backup stream to stdout.
 */
static int
zfs_do_send(int argc, char **argv)
{
	char *fromname = NULL;
	char *cp;
	zfs_handle_t *zhp;
	int c, err;

	/* check options */
	while ((c = getopt(argc, argv, ":i:")) != -1) {
		switch (c) {
		case 'i':
			if (fromname)
				usage(B_FALSE);
			fromname = optarg;
			break;
		case ':':
			(void) fprintf(stderr, gettext("missing argument for "
			    "'%c' option\n"), optopt);
			usage(B_FALSE);
			break;
		case '?':
			(void) fprintf(stderr, gettext("invalid option '%c'\n"),
			    optopt);
			usage(B_FALSE);
		}
	}

	argc -= optind;
	argv += optind;

	/* check number of arguments */
	if (argc < 1) {
		(void) fprintf(stderr, gettext("missing snapshot argument\n"));
		usage(B_FALSE);
	}
	if (argc > 1) {
		(void) fprintf(stderr, gettext("too many arguments\n"));
		usage(B_FALSE);
	}

	if (isatty(STDOUT_FILENO)) {
		(void) fprintf(stderr,
		    gettext("Error: Stream can not be written to a terminal.\n"
		    "You must redirect standard output.\n"));
		return (1);
	}

	if ((zhp = zfs_open(g_zfs, argv[0], ZFS_TYPE_SNAPSHOT)) == NULL)
		return (1);

	/*
	 * If they specified the full path to the snapshot, chop off
	 * everything except the short name of the snapshot.
	 */
	if (fromname && (cp = strchr(fromname, '@')) != NULL) {
		if (cp != fromname &&
		    strncmp(argv[0], fromname, cp - fromname + 1)) {
			(void) fprintf(stderr,
			    gettext("incremental source must be "
			    "in same filesystem\n"));
			usage(B_FALSE);
		}
		fromname = cp + 1;
		if (strchr(fromname, '@') || strchr(fromname, '/')) {
			(void) fprintf(stderr,
			    gettext("invalid incremental source\n"));
			usage(B_FALSE);
		}
	}

	err = zfs_send(zhp, fromname, STDOUT_FILENO);
	zfs_close(zhp);

	return (err != 0);
}

/*
 * zfs receive <fs@snap>
 *
 * Restore a backup stream from stdin.
 */
static int
zfs_do_receive(int argc, char **argv)
{
	int c, err;
	boolean_t isprefix = B_FALSE;
	boolean_t dryrun = B_FALSE;
	boolean_t verbose = B_FALSE;
	boolean_t force = B_FALSE;

	/* check options */
	while ((c = getopt(argc, argv, ":dnvF")) != -1) {
		switch (c) {
		case 'd':
			isprefix = B_TRUE;
			break;
		case 'n':
			dryrun = B_TRUE;
			break;
		case 'v':
			verbose = B_TRUE;
			break;
		case 'F':
			force = B_TRUE;
			break;
		case ':':
			(void) fprintf(stderr, gettext("missing argument for "
			    "'%c' option\n"), optopt);
			usage(B_FALSE);
			break;
		case '?':
			(void) fprintf(stderr, gettext("invalid option '%c'\n"),
			    optopt);
			usage(B_FALSE);
		}
	}

	argc -= optind;
	argv += optind;

	/* check number of arguments */
	if (argc < 1) {
		(void) fprintf(stderr, gettext("missing snapshot argument\n"));
		usage(B_FALSE);
	}
	if (argc > 1) {
		(void) fprintf(stderr, gettext("too many arguments\n"));
		usage(B_FALSE);
	}

	if (isatty(STDIN_FILENO)) {
		(void) fprintf(stderr,
		    gettext("Error: Backup stream can not be read "
		    "from a terminal.\n"
		    "You must redirect standard input.\n"));
		return (1);
	}

	err = zfs_receive(g_zfs, argv[0], isprefix, verbose, dryrun, force,
	    STDIN_FILENO);

	if (!err) {
		zpool_log_history(g_zfs, argc + optind, argv - optind, argv[0],
		    B_FALSE, B_FALSE);
	}

	return (err != 0);
}

typedef struct get_all_cbdata {
	zfs_handle_t	**cb_handles;
	size_t		cb_alloc;
	size_t		cb_used;
	uint_t		cb_types;
} get_all_cbdata_t;

static int
get_one_dataset(zfs_handle_t *zhp, void *data)
{
	get_all_cbdata_t *cbp = data;
	zfs_type_t type = zfs_get_type(zhp);

	/*
	 * Interate over any nested datasets.
	 */
	if (type == ZFS_TYPE_FILESYSTEM &&
	    zfs_iter_filesystems(zhp, get_one_dataset, data) != 0) {
		zfs_close(zhp);
		return (1);
	}

	/*
	 * Skip any datasets whose type does not match.
	 */
	if ((type & cbp->cb_types) == 0) {
		zfs_close(zhp);
		return (0);
	}

	if (cbp->cb_alloc == cbp->cb_used) {
		zfs_handle_t **handles;

		if (cbp->cb_alloc == 0)
			cbp->cb_alloc = 64;
		else
			cbp->cb_alloc *= 2;

		handles = safe_malloc(cbp->cb_alloc * sizeof (void *));

		if (cbp->cb_handles) {
			bcopy(cbp->cb_handles, handles,
			    cbp->cb_used * sizeof (void *));
			free(cbp->cb_handles);
		}

		cbp->cb_handles = handles;
	}

	cbp->cb_handles[cbp->cb_used++] = zhp;

	return (0);
}

static void
get_all_datasets(uint_t types, zfs_handle_t ***dslist, size_t *count)
{
	get_all_cbdata_t cb = { 0 };
	cb.cb_types = types;

	(void) zfs_iter_root(g_zfs, get_one_dataset, &cb);

	*dslist = cb.cb_handles;
	*count = cb.cb_used;
}

static int
dataset_cmp(const void *a, const void *b)
{
	zfs_handle_t **za = (zfs_handle_t **)a;
	zfs_handle_t **zb = (zfs_handle_t **)b;
	char mounta[MAXPATHLEN];
	char mountb[MAXPATHLEN];
	boolean_t gota, gotb;

	if ((gota = (zfs_get_type(*za) == ZFS_TYPE_FILESYSTEM)) != 0)
		verify(zfs_prop_get(*za, ZFS_PROP_MOUNTPOINT, mounta,
		    sizeof (mounta), NULL, NULL, 0, B_FALSE) == 0);
	if ((gotb = (zfs_get_type(*zb) == ZFS_TYPE_FILESYSTEM)) != 0)
		verify(zfs_prop_get(*zb, ZFS_PROP_MOUNTPOINT, mountb,
		    sizeof (mountb), NULL, NULL, 0, B_FALSE) == 0);

	if (gota && gotb)
		return (strcmp(mounta, mountb));

	if (gota)
		return (-1);
	if (gotb)
		return (1);

	return (strcmp(zfs_get_name(a), zfs_get_name(b)));
}

/*
 * Generic callback for sharing or mounting filesystems.  Because the code is so
 * similar, we have a common function with an extra parameter to determine which
 * mode we are using.
 */
#define	OP_SHARE	0x1
#define	OP_MOUNT	0x2

/*
 * Share or mount a dataset.
 */
static int
share_mount_one(zfs_handle_t *zhp, int op, int flags, boolean_t explicit,
    const char *options)
{
	char mountpoint[ZFS_MAXPROPLEN];
	char shareopts[ZFS_MAXPROPLEN];
	const char *cmdname = op == OP_SHARE ? "share" : "mount";
	struct mnttab mnt;
	uint64_t zoned, canmount;
	zfs_type_t type = zfs_get_type(zhp);

	assert(type & (ZFS_TYPE_FILESYSTEM | ZFS_TYPE_VOLUME));

	if (type == ZFS_TYPE_FILESYSTEM) {
		/*
		 * Check to make sure we can mount/share this dataset.  If we
		 * are in the global zone and the filesystem is exported to a
		 * local zone, or if we are in a local zone and the
		 * filesystem is not exported, then it is an error.
		 */
		zoned = zfs_prop_get_int(zhp, ZFS_PROP_ZONED);

		if (zoned && getzoneid() == GLOBAL_ZONEID) {
			if (!explicit)
				return (0);

			(void) fprintf(stderr, gettext("cannot %s '%s': "
			    "dataset is exported to a local zone\n"), cmdname,
			    zfs_get_name(zhp));
			return (1);

		} else if (!zoned && getzoneid() != GLOBAL_ZONEID) {
			if (!explicit)
				return (0);

			(void) fprintf(stderr, gettext("cannot %s '%s': "
			    "permission denied\n"), cmdname,
			    zfs_get_name(zhp));
			return (1);
		}

		/*
		 * Ignore any filesystems which don't apply to us. This
		 * includes those with a legacy mountpoint, or those with
		 * legacy share options.
		 */
		verify(zfs_prop_get(zhp, ZFS_PROP_MOUNTPOINT, mountpoint,
		    sizeof (mountpoint), NULL, NULL, 0, B_FALSE) == 0);
		verify(zfs_prop_get(zhp, ZFS_PROP_SHARENFS, shareopts,
		    sizeof (shareopts), NULL, NULL, 0, B_FALSE) == 0);
		canmount = zfs_prop_get_int(zhp, ZFS_PROP_CANMOUNT);

		if (op == OP_SHARE && strcmp(shareopts, "off") == 0) {
			if (!explicit)
				return (0);

			(void) fprintf(stderr, gettext("cannot share '%s': "
			    "legacy share\n"), zfs_get_name(zhp));
			(void) fprintf(stderr, gettext("use share(1M) to "
			    "share this filesystem\n"));
			return (1);
		}

		/*
		 * We cannot share or mount legacy filesystems. If the
		 * shareopts is non-legacy but the mountpoint is legacy, we
		 * treat it as a legacy share.
		 */
		if (strcmp(mountpoint, "legacy") == 0) {
			if (!explicit)
				return (0);

			(void) fprintf(stderr, gettext("cannot %s '%s': "
			    "legacy mountpoint\n"), cmdname, zfs_get_name(zhp));
			(void) fprintf(stderr, gettext("use %s to "
			    "%s this filesystem\n"), op == OP_SHARE ?
			    "share(1M)" : "mount(1M)", cmdname);
			return (1);
		}

		if (strcmp(mountpoint, "none") == 0) {
			if (!explicit)
				return (0);

			(void) fprintf(stderr, gettext("cannot %s '%s': no "
			    "mountpoint set\n"), cmdname, zfs_get_name(zhp));
			return (1);
		}

		if (!canmount) {
			if (!explicit)
				return (0);

			(void) fprintf(stderr, gettext("cannot %s '%s': "
			    "'canmount' property is set to 'off'\n"), cmdname,
			    zfs_get_name(zhp));
			return (1);
		}

		/*
		 * At this point, we have verified that the mountpoint and/or
		 * shareopts are appropriate for auto management. If the
		 * filesystem is already mounted or shared, return (failing
		 * for explicit requests); otherwise mount or share the
		 * filesystem.
		 */
		switch (op) {
		case OP_SHARE:
			if (zfs_is_shared_nfs(zhp, NULL)) {
				if (!explicit)
					return (0);

				(void) fprintf(stderr, gettext("cannot share "
				    "'%s': filesystem already shared\n"),
				    zfs_get_name(zhp));
				return (1);
			}

			if (!zfs_is_mounted(zhp, NULL) &&
			    zfs_mount(zhp, NULL, 0) != 0)
				return (1);

			if (zfs_share_nfs(zhp) != 0)
				return (1);
			break;

		case OP_MOUNT:
			if (options == NULL)
				mnt.mnt_mntopts = "";
			else
				mnt.mnt_mntopts = (char *)options;

			if (!hasmntopt(&mnt, MNTOPT_REMOUNT) &&
			    zfs_is_mounted(zhp, NULL)) {
				if (!explicit)
					return (0);

				(void) fprintf(stderr, gettext("cannot mount "
				    "'%s': filesystem already mounted\n"),
				    zfs_get_name(zhp));
				return (1);
			}

			if (zfs_mount(zhp, options, flags) != 0)
				return (1);
			break;
		}
	} else {
		assert(op == OP_SHARE);

		/*
		 * Ignore any volumes that aren't shared.
		 */
		verify(zfs_prop_get(zhp, ZFS_PROP_SHAREISCSI, shareopts,
		    sizeof (shareopts), NULL, NULL, 0, B_FALSE) == 0);

		if (strcmp(shareopts, "off") == 0) {
			if (!explicit)
				return (0);

			(void) fprintf(stderr, gettext("cannot share '%s': "
			    "'shareiscsi' property not set\n"),
			    zfs_get_name(zhp));
			(void) fprintf(stderr, gettext("set 'shareiscsi' "
			    "property or use iscsitadm(1M) to share this "
			    "volume\n"));
			return (1);
		}

		if (zfs_is_shared_iscsi(zhp)) {
			if (!explicit)
				return (0);

			(void) fprintf(stderr, gettext("cannot share "
			    "'%s': volume already shared\n"),
			    zfs_get_name(zhp));
			return (1);
		}

		if (zfs_share_iscsi(zhp) != 0)
			return (1);
	}

	return (0);
}

static int
share_mount(int op, int argc, char **argv)
{
	int do_all = 0;
	int c, ret = 0;
	const char *options = NULL;
	int types, flags = 0;

	/* check options */
	while ((c = getopt(argc, argv, op == OP_MOUNT ? ":ao:O" : "a"))
	    != -1) {
		switch (c) {
		case 'a':
			do_all = 1;
			break;
		case 'o':
			options = optarg;
			break;
		case 'O':
			warnx("no overlay mounts support on FreeBSD, ignoring");
			break;
		case ':':
			(void) fprintf(stderr, gettext("missing argument for "
			    "'%c' option\n"), optopt);
			usage(B_FALSE);
			break;
		case '?':
			(void) fprintf(stderr, gettext("invalid option '%c'\n"),
			    optopt);
			usage(B_FALSE);
		}
	}

	argc -= optind;
	argv += optind;

	/* check number of arguments */
	if (do_all) {
		zfs_handle_t **dslist = NULL;
		size_t i, count = 0;

		if (op == OP_MOUNT) {
			types = ZFS_TYPE_FILESYSTEM;
		} else if (argc > 0) {
			if (strcmp(argv[0], "nfs") == 0) {
				types = ZFS_TYPE_FILESYSTEM;
			} else if (strcmp(argv[0], "iscsi") == 0) {
				types = ZFS_TYPE_VOLUME;
			} else {
				(void) fprintf(stderr, gettext("share type "
				    "must be 'nfs' or 'iscsi'\n"));
				usage(B_FALSE);
			}

			argc--;
			argv++;
		} else {
			types = ZFS_TYPE_FILESYSTEM | ZFS_TYPE_VOLUME;
		}

		if (argc != 0) {
			(void) fprintf(stderr, gettext("too many arguments\n"));
			usage(B_FALSE);
		}

		get_all_datasets(types, &dslist, &count);

		if (count == 0)
			return (0);

		qsort(dslist, count, sizeof (void *), dataset_cmp);

		for (i = 0; i < count; i++) {
			if (share_mount_one(dslist[i], op, flags, B_FALSE,
			    options) != 0)
				ret = 1;
			zfs_close(dslist[i]);
		}

		free(dslist);
	} else if (argc == 0) {
		struct statfs *sfs;
		int i, n;

		if (op == OP_SHARE) {
			(void) fprintf(stderr, gettext("missing filesystem "
			    "argument\n"));
			usage(B_FALSE);
		}

		/*
		 * When mount is given no arguments, go through /etc/mnttab and
		 * display any active ZFS mounts.  We hide any snapshots, since
		 * they are controlled automatically.
		 */
		if ((n = getmntinfo(&sfs, MNT_WAIT)) == 0) {
			fprintf(stderr, "getmntinfo(): %s\n", strerror(errno));
			return (0);
		}
		for (i = 0; i < n; i++) {
			if (strcmp(sfs[i].f_fstypename, MNTTYPE_ZFS) != 0 ||
			    strchr(sfs[i].f_mntfromname, '@') != NULL)
				continue;

			(void) printf("%-30s  %s\n", sfs[i].f_mntfromname,
			    sfs[i].f_mntonname);
		}

	} else {
		zfs_handle_t *zhp;

		types = ZFS_TYPE_FILESYSTEM;
		if (op == OP_SHARE)
			types |= ZFS_TYPE_VOLUME;

		if (argc > 1) {
			(void) fprintf(stderr,
			    gettext("too many arguments\n"));
			usage(B_FALSE);
		}

		if ((zhp = zfs_open(g_zfs, argv[0], types)) == NULL) {
			ret = 1;
		} else {
			ret = share_mount_one(zhp, op, flags, B_TRUE,
			    options);
			zfs_close(zhp);
		}
	}

	return (ret);
}

/*
 * zfs mount -a [nfs | iscsi]
 * zfs mount filesystem
 *
 * Mount all filesystems, or mount the given filesystem.
 */
static int
zfs_do_mount(int argc, char **argv)
{
	return (share_mount(OP_MOUNT, argc, argv));
}

/*
 * zfs share -a [nfs | iscsi]
 * zfs share filesystem
 *
 * Share all filesystems, or share the given filesystem.
 */
static int
zfs_do_share(int argc, char **argv)
{
	return (share_mount(OP_SHARE, argc, argv));
}

typedef struct unshare_unmount_node {
	zfs_handle_t	*un_zhp;
	char		*un_mountp;
	uu_avl_node_t	un_avlnode;
} unshare_unmount_node_t;

/* ARGSUSED */
static int
unshare_unmount_compare(const void *larg, const void *rarg, void *unused)
{
	const unshare_unmount_node_t *l = larg;
	const unshare_unmount_node_t *r = rarg;

	return (strcmp(l->un_mountp, r->un_mountp));
}

/*
 * Convenience routine used by zfs_do_umount() and manual_unmount().  Given an
 * absolute path, find the entry /etc/mnttab, verify that its a ZFS filesystem,
 * and unmount it appropriately.
 */
static int
unshare_unmount_path(int op, char *path, int flags, boolean_t is_manual)
{
	zfs_handle_t *zhp;
	int ret;
	struct mnttab search = { 0 }, entry;
	const char *cmdname = (op == OP_SHARE) ? "unshare" : "unmount";
	char property[ZFS_MAXPROPLEN];

	/*
	 * Search for the given (major,minor) pair in the mount table.
	 */
	search.mnt_mountp = path;
	rewind(mnttab_file);
	if (getmntany(mnttab_file, &entry, &search) != 0) {
		(void) fprintf(stderr, gettext("cannot %s '%s': not "
		    "currently mounted\n"), cmdname, path);
		return (1);
	}

	if (strcmp(entry.mnt_fstype, MNTTYPE_ZFS) != 0) {
		(void) fprintf(stderr, gettext("cannot %s '%s': not a ZFS "
		    "filesystem\n"), cmdname, path);
		return (1);
	}

	if ((zhp = zfs_open(g_zfs, entry.mnt_special,
	    ZFS_TYPE_FILESYSTEM)) == NULL)
		return (1);

	verify(zfs_prop_get(zhp, op == OP_SHARE ?
	    ZFS_PROP_SHARENFS : ZFS_PROP_MOUNTPOINT, property,
	    sizeof (property), NULL, NULL, 0, B_FALSE) == 0);

	if (op == OP_SHARE) {
		if (strcmp(property, "off") == 0) {
			(void) fprintf(stderr, gettext("cannot unshare "
			    "'%s': legacy share\n"), path);
			(void) fprintf(stderr, gettext("use "
			    "unshare(1M) to unshare this filesystem\n"));
			ret = 1;
		} else if (!zfs_is_shared_nfs(zhp, NULL)) {
			(void) fprintf(stderr, gettext("cannot unshare '%s': "
			    "not currently shared\n"), path);
			ret = 1;
		} else {
			ret = zfs_unshareall_nfs(zhp);
		}
	} else {
		if (is_manual) {
			ret = zfs_unmount(zhp, NULL, flags);
		} else if (strcmp(property, "legacy") == 0) {
			(void) fprintf(stderr, gettext("cannot unmount "
			    "'%s': legacy mountpoint\n"),
			    zfs_get_name(zhp));
			(void) fprintf(stderr, gettext("use umount(1M) "
			    "to unmount this filesystem\n"));
			ret = 1;
		} else {
			ret = zfs_unmountall(zhp, flags);
		}
	}

	zfs_close(zhp);

	return (ret != 0);
}

/*
 * Generic callback for unsharing or unmounting a filesystem.
 */
static int
unshare_unmount(int op, int argc, char **argv)
{
	int do_all = 0;
	int flags = 0;
	int ret = 0;
	int types, c;
	zfs_handle_t *zhp;
	char property[ZFS_MAXPROPLEN];

	/* check options */
	while ((c = getopt(argc, argv, op == OP_SHARE ? "a" : "af")) != -1) {
		switch (c) {
		case 'a':
			do_all = 1;
			break;
		case 'f':
			flags = MS_FORCE;
			break;
		case '?':
			(void) fprintf(stderr, gettext("invalid option '%c'\n"),
			    optopt);
			usage(B_FALSE);
		}
	}

	argc -= optind;
	argv += optind;

	if (do_all) {
		/*
		 * We could make use of zfs_for_each() to walk all datasets in
		 * the system, but this would be very inefficient, especially
		 * since we would have to linearly search /etc/mnttab for each
		 * one.  Instead, do one pass through /etc/mnttab looking for
		 * zfs entries and call zfs_unmount() for each one.
		 *
		 * Things get a little tricky if the administrator has created
		 * mountpoints beneath other ZFS filesystems.  In this case, we
		 * have to unmount the deepest filesystems first.  To accomplish
		 * this, we place all the mountpoints in an AVL tree sorted by
		 * the special type (dataset name), and walk the result in
		 * reverse to make sure to get any snapshots first.
		 */
		uu_avl_pool_t *pool;
		uu_avl_t *tree;
		unshare_unmount_node_t *node;
		uu_avl_index_t idx;
		uu_avl_walk_t *walk;
		struct statfs *sfs;
		int i, n;

		if (argc != 0) {
			(void) fprintf(stderr, gettext("too many arguments\n"));
			usage(B_FALSE);
		}

		if ((pool = uu_avl_pool_create("unmount_pool",
		    sizeof (unshare_unmount_node_t),
		    offsetof(unshare_unmount_node_t, un_avlnode),
		    unshare_unmount_compare,
		    UU_DEFAULT)) == NULL) {
			(void) fprintf(stderr, gettext("internal error: "
			    "out of memory\n"));
			exit(1);
		}

		if ((tree = uu_avl_create(pool, NULL, UU_DEFAULT)) == NULL) {
			(void) fprintf(stderr, gettext("internal error: "
			    "out of memory\n"));
			exit(1);
		}

		if ((n = getmntinfo(&sfs, MNT_WAIT)) == 0) {
			(void) fprintf(stderr, gettext("internal error: "
			    "getmntinfo() failed\n"));
			exit(1);
		}
		for (i = 0; i < n; i++) {

			/* ignore non-ZFS entries */
			if (strcmp(sfs[i].f_fstypename, MNTTYPE_ZFS) != 0)
				continue;

			/* ignore snapshots */
			if (strchr(sfs[i].f_mntfromname, '@') != NULL)
				continue;

			if ((zhp = zfs_open(g_zfs, sfs[i].f_mntfromname,
			    ZFS_TYPE_FILESYSTEM)) == NULL) {
				ret = 1;
				continue;
			}

			verify(zfs_prop_get(zhp, op == OP_SHARE ?
			    ZFS_PROP_SHARENFS : ZFS_PROP_MOUNTPOINT,
			    property, sizeof (property), NULL, NULL,
			    0, B_FALSE) == 0);

			/* Ignore legacy mounts and shares */
			if ((op == OP_SHARE &&
			    strcmp(property, "off") == 0) ||
			    (op == OP_MOUNT &&
			    strcmp(property, "legacy") == 0)) {
				zfs_close(zhp);
				continue;
			}

			node = safe_malloc(sizeof (unshare_unmount_node_t));
			node->un_zhp = zhp;

			if ((node->un_mountp = strdup(sfs[i].f_mntonname)) ==
			    NULL) {
				(void) fprintf(stderr, gettext("internal error:"
				    " out of memory\n"));
				exit(1);
			}

			uu_avl_node_init(node, &node->un_avlnode, pool);

			if (uu_avl_find(tree, node, NULL, &idx) == NULL) {
				uu_avl_insert(tree, node, idx);
			} else {
				zfs_close(node->un_zhp);
				free(node->un_mountp);
				free(node);
			}
		}

		/*
		 * Walk the AVL tree in reverse, unmounting each filesystem and
		 * removing it from the AVL tree in the process.
		 */
		if ((walk = uu_avl_walk_start(tree,
		    UU_WALK_REVERSE | UU_WALK_ROBUST)) == NULL) {
			(void) fprintf(stderr,
			    gettext("internal error: out of memory"));
			exit(1);
		}

		while ((node = uu_avl_walk_next(walk)) != NULL) {
			uu_avl_remove(tree, node);

			switch (op) {
			case OP_SHARE:
				if (zfs_unshare_nfs(node->un_zhp,
				    node->un_mountp) != 0)
					ret = 1;
				break;

			case OP_MOUNT:
				if (zfs_unmount(node->un_zhp,
				    node->un_mountp, flags) != 0)
					ret = 1;
				break;
			}

			zfs_close(node->un_zhp);
			free(node->un_mountp);
			free(node);
		}

		uu_avl_walk_end(walk);
		uu_avl_destroy(tree);
		uu_avl_pool_destroy(pool);

		if (op == OP_SHARE) {
			/*
			 * Finally, unshare any volumes shared via iSCSI.
			 */
			zfs_handle_t **dslist = NULL;
			size_t i, count = 0;

			get_all_datasets(ZFS_TYPE_VOLUME, &dslist, &count);

			if (count != 0) {
				qsort(dslist, count, sizeof (void *),
				    dataset_cmp);

				for (i = 0; i < count; i++) {
					if (zfs_unshare_iscsi(dslist[i]) != 0)
						ret = 1;
					zfs_close(dslist[i]);
				}

				free(dslist);
			}
		}
	} else {
		if (argc != 1) {
			if (argc == 0)
				(void) fprintf(stderr,
				    gettext("missing filesystem argument\n"));
			else
				(void) fprintf(stderr,
				    gettext("too many arguments\n"));
			usage(B_FALSE);
		}

		/*
		 * We have an argument, but it may be a full path or a ZFS
		 * filesystem.  Pass full paths off to unmount_path() (shared by
		 * manual_unmount), otherwise open the filesystem and pass to
		 * zfs_unmount().
		 */
		if (argv[0][0] == '/')
			return (unshare_unmount_path(op, argv[0],
			    flags, B_FALSE));

		types = ZFS_TYPE_FILESYSTEM;
		if (op == OP_SHARE)
			types |= ZFS_TYPE_VOLUME;

		if ((zhp = zfs_open(g_zfs, argv[0], types)) == NULL)
			return (1);

		if (zfs_get_type(zhp) == ZFS_TYPE_FILESYSTEM) {
			verify(zfs_prop_get(zhp, op == OP_SHARE ?
			    ZFS_PROP_SHARENFS : ZFS_PROP_MOUNTPOINT, property,
			    sizeof (property), NULL, NULL, 0, B_FALSE) == 0);

			switch (op) {
			case OP_SHARE:
				if (strcmp(property, "off") == 0) {
					(void) fprintf(stderr, gettext("cannot "
					    "unshare '%s': legacy share\n"),
					    zfs_get_name(zhp));
					(void) fprintf(stderr, gettext("use "
					    "unshare(1M) to unshare this "
					    "filesystem\n"));
					ret = 1;
				} else if (!zfs_is_shared_nfs(zhp, NULL)) {
					(void) fprintf(stderr, gettext("cannot "
					    "unshare '%s': not currently "
					    "shared\n"), zfs_get_name(zhp));
					ret = 1;
				} else if (zfs_unshareall_nfs(zhp) != 0) {
					ret = 1;
				}
				break;

			case OP_MOUNT:
				if (strcmp(property, "legacy") == 0) {
					(void) fprintf(stderr, gettext("cannot "
					    "unmount '%s': legacy "
					    "mountpoint\n"), zfs_get_name(zhp));
					(void) fprintf(stderr, gettext("use "
					    "umount(1M) to unmount this "
					    "filesystem\n"));
					ret = 1;
				} else if (!zfs_is_mounted(zhp, NULL)) {
					(void) fprintf(stderr, gettext("cannot "
					    "unmount '%s': not currently "
					    "mounted\n"),
					    zfs_get_name(zhp));
					ret = 1;
				} else if (zfs_unmountall(zhp, flags) != 0) {
					ret = 1;
				}
				break;
			}
		} else {
			assert(op == OP_SHARE);

			verify(zfs_prop_get(zhp, ZFS_PROP_SHAREISCSI, property,
			    sizeof (property), NULL, NULL, 0, B_FALSE) == 0);

			if (strcmp(property, "off") == 0) {
				(void) fprintf(stderr, gettext("cannot unshare "
				    "'%s': 'shareiscsi' property not set\n"),
				    zfs_get_name(zhp));
				(void) fprintf(stderr, gettext("set "
				    "'shareiscsi' property or use "
				    "iscsitadm(1M) to share this volume\n"));
				ret = 1;
			} else if (!zfs_is_shared_iscsi(zhp)) {
				(void) fprintf(stderr, gettext("cannot "
				    "unshare '%s': not currently shared\n"),
				    zfs_get_name(zhp));
				ret = 1;
			} else if (zfs_unshare_iscsi(zhp) != 0) {
				ret = 1;
			}
		}

		zfs_close(zhp);
	}

	return (ret);
}

/*
 * zfs unmount -a
 * zfs unmount filesystem
 *
 * Unmount all filesystems, or a specific ZFS filesystem.
 */
static int
zfs_do_unmount(int argc, char **argv)
{
	return (unshare_unmount(OP_MOUNT, argc, argv));
}

/*
 * zfs unshare -a
 * zfs unshare filesystem
 *
 * Unshare all filesystems, or a specific ZFS filesystem.
 */
static int
zfs_do_unshare(int argc, char **argv)
{
	return (unshare_unmount(OP_SHARE, argc, argv));
}

/*
 * Attach/detach the given dataset to/from the given jail
 */
/* ARGSUSED */
static int
do_jail(int argc, char **argv, int attach)
{
	zfs_handle_t *zhp;
	int jailid, ret;

	/* check number of arguments */
	if (argc < 3) {
		(void) fprintf(stderr, gettext("missing argument(s)\n"));
		usage(B_FALSE);
	}
	if (argc > 3) {
		(void) fprintf(stderr, gettext("too many arguments\n"));
		usage(B_FALSE);
	}

	jailid = atoi(argv[1]);
	if (jailid == 0) {
		(void) fprintf(stderr, gettext("invalid jailid\n"));
		usage(B_FALSE);
	}

	zhp = zfs_open(g_zfs, argv[2], ZFS_TYPE_FILESYSTEM);
	if (zhp == NULL)
		return (1);

	ret = (zfs_jail(zhp, jailid, attach) != 0);

	if (!ret)
		zpool_log_history(g_zfs, argc, argv, argv[2], B_FALSE, B_FALSE);

	zfs_close(zhp);
	return (ret);
}

/*
 * zfs jail jailid filesystem
 *
 * Attach the given dataset to the given jail
 */
/* ARGSUSED */
static int
zfs_do_jail(int argc, char **argv)
{

	return (do_jail(argc, argv, 1));
}

/*
 * zfs unjail jailid filesystem
 *
 * Detach the given dataset from the given jail
 */
/* ARGSUSED */
static int
zfs_do_unjail(int argc, char **argv)
{

	return (do_jail(argc, argv, 0));
}

/*
 * Called when invoked as /etc/fs/zfs/mount.  Do the mount if the mountpoint is
 * 'legacy'.  Otherwise, complain that use should be using 'zfs mount'.
 */
static int
manual_mount(int argc, char **argv)
{
	zfs_handle_t *zhp;
	char mountpoint[ZFS_MAXPROPLEN];
	char mntopts[MNT_LINE_MAX] = { '\0' };
	int ret;
	int c;
	int flags = 0;
	char *dataset, *path;

	/* check options */
	while ((c = getopt(argc, argv, ":mo:O")) != -1) {
		switch (c) {
		case 'o':
			(void) strlcpy(mntopts, optarg, sizeof (mntopts));
			break;
		case 'O':
#if 0	/* FreeBSD: No support for MS_OVERLAY. */
			flags |= MS_OVERLAY;
#endif
			break;
		case 'm':
#if 0	/* FreeBSD: No support for MS_NOMNTTAB. */
			flags |= MS_NOMNTTAB;
#endif
			break;
		case ':':
			(void) fprintf(stderr, gettext("missing argument for "
			    "'%c' option\n"), optopt);
			usage(B_FALSE);
			break;
		case '?':
			(void) fprintf(stderr, gettext("invalid option '%c'\n"),
			    optopt);
			(void) fprintf(stderr, gettext("usage: mount [-o opts] "
			    "<path>\n"));
			return (2);
		}
	}

	argc -= optind;
	argv += optind;

	/* check that we only have two arguments */
	if (argc != 2) {
		if (argc == 0)
			(void) fprintf(stderr, gettext("missing dataset "
			    "argument\n"));
		else if (argc == 1)
			(void) fprintf(stderr,
			    gettext("missing mountpoint argument\n"));
		else
			(void) fprintf(stderr, gettext("too many arguments\n"));
		(void) fprintf(stderr, "usage: mount <dataset> <mountpoint>\n");
		return (2);
	}

	dataset = argv[0];
	path = argv[1];

	/* try to open the dataset */
	if ((zhp = zfs_open(g_zfs, dataset, ZFS_TYPE_FILESYSTEM)) == NULL)
		return (1);

	(void) zfs_prop_get(zhp, ZFS_PROP_MOUNTPOINT, mountpoint,
	    sizeof (mountpoint), NULL, NULL, 0, B_FALSE);

	/* check for legacy mountpoint and complain appropriately */
	ret = 0;
	if (strcmp(mountpoint, ZFS_MOUNTPOINT_LEGACY) == 0) {
		if (zmount(dataset, path, flags, MNTTYPE_ZFS,
		    NULL, 0, mntopts, sizeof (mntopts)) != 0) {
			(void) fprintf(stderr, gettext("mount failed: %s\n"),
			    strerror(errno));
			ret = 1;
		}
	} else {
		(void) fprintf(stderr, gettext("filesystem '%s' cannot be "
		    "mounted using 'mount -F zfs'\n"), dataset);
		(void) fprintf(stderr, gettext("Use 'zfs set mountpoint=%s' "
		    "instead.\n"), path);
		(void) fprintf(stderr, gettext("If you must use 'mount -F zfs' "
		    "or /etc/vfstab, use 'zfs set mountpoint=legacy'.\n"));
		(void) fprintf(stderr, gettext("See zfs(1M) for more "
		    "information.\n"));
		ret = 1;
	}

	return (ret);
}

/*
 * Called when invoked as /etc/fs/zfs/umount.  Unlike a manual mount, we allow
 * unmounts of non-legacy filesystems, as this is the dominant administrative
 * interface.
 */
static int
manual_unmount(int argc, char **argv)
{
	int flags = 0;
	int c;

	/* check options */
	while ((c = getopt(argc, argv, "f")) != -1) {
		switch (c) {
		case 'f':
			flags = MS_FORCE;
			break;
		case '?':
			(void) fprintf(stderr, gettext("invalid option '%c'\n"),
			    optopt);
			(void) fprintf(stderr, gettext("usage: unmount [-f] "
			    "<path>\n"));
			return (2);
		}
	}

	argc -= optind;
	argv += optind;

	/* check arguments */
	if (argc != 1) {
		if (argc == 0)
			(void) fprintf(stderr, gettext("missing path "
			    "argument\n"));
		else
			(void) fprintf(stderr, gettext("too many arguments\n"));
		(void) fprintf(stderr, gettext("usage: unmount [-f] <path>\n"));
		return (2);
	}

	return (unshare_unmount_path(OP_MOUNT, argv[0], flags, B_TRUE));
}

static int
volcheck(zpool_handle_t *zhp, void *data)
{
	boolean_t isinit = *((boolean_t *)data);

	if (isinit)
		return (zpool_create_zvol_links(zhp));
	else
		return (zpool_remove_zvol_links(zhp));
}

/*
 * Iterate over all pools in the system and either create or destroy /dev/zvol
 * links, depending on the value of 'isinit'.
 */
static int
do_volcheck(boolean_t isinit)
{
	return (zpool_iter(g_zfs, volcheck, &isinit) ? 1 : 0);
}

int
main(int argc, char **argv)
{
	int ret;
	int i;
	char *progname;
	char *cmdname;

	(void) setlocale(LC_ALL, "");
	(void) textdomain(TEXT_DOMAIN);

	opterr = 0;

	if ((g_zfs = libzfs_init()) == NULL) {
		(void) fprintf(stderr, gettext("internal error: failed to "
		    "initialize ZFS library\n"));
		return (1);
	}

	libzfs_print_on_error(g_zfs, B_TRUE);

	if ((mnttab_file = fopen(MNTTAB, "r")) == NULL) {
		(void) fprintf(stderr, gettext("internal error: unable to "
		    "open %s\n"), MNTTAB);
		return (1);
	}

	/*
	 * This command also doubles as the /etc/fs mount and unmount program.
	 * Determine if we should take this behavior based on argv[0].
	 */
	progname = basename(argv[0]);
	if (strcmp(progname, "mount") == 0) {
		ret = manual_mount(argc, argv);
	} else if (strcmp(progname, "umount") == 0) {
		ret = manual_unmount(argc, argv);
	} else {
		/*
		 * Make sure the user has specified some command.
		 */
		if (argc < 2) {
			(void) fprintf(stderr, gettext("missing command\n"));
			usage(B_FALSE);
		}

		cmdname = argv[1];

		/*
		 * The 'umount' command is an alias for 'unmount'
		 */
		if (strcmp(cmdname, "umount") == 0)
			cmdname = "unmount";

		/*
		 * The 'recv' command is an alias for 'receive'
		 */
		if (strcmp(cmdname, "recv") == 0)
			cmdname = "receive";

		/*
		 * Special case '-?'
		 */
		if (strcmp(cmdname, "-?") == 0)
			usage(B_TRUE);

		/*
		 * 'volinit' and 'volfini' do not appear in the usage message,
		 * so we have to special case them here.
		 */
		if (strcmp(cmdname, "volinit") == 0)
			return (do_volcheck(B_TRUE));
		else if (strcmp(cmdname, "volfini") == 0)
			return (do_volcheck(B_FALSE));

		/*
		 * Run the appropriate command.
		 */
		for (i = 0; i < NCOMMAND; i++) {
			if (command_table[i].name == NULL)
				continue;

			if (strcmp(cmdname, command_table[i].name) == 0) {
				current_command = &command_table[i];
				ret = command_table[i].func(argc - 1, argv + 1);
				break;
			}
		}

		if (i == NCOMMAND) {
			(void) fprintf(stderr, gettext("unrecognized "
			    "command '%s'\n"), cmdname);
			usage(B_FALSE);
		}
	}

	(void) fclose(mnttab_file);

	libzfs_fini(g_zfs);

	/*
	 * The 'ZFS_ABORT' environment variable causes us to dump core on exit
	 * for the purposes of running ::findleaks.
	 */
	if (getenv("ZFS_ABORT") != NULL) {
		(void) printf("dumping core by request\n");
		abort();
	}

	return (ret);
}
