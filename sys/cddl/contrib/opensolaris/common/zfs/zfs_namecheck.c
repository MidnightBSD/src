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
 * Common name validation routines for ZFS.  These routines are shared by the
 * userland code as well as the ioctl() layer to ensure that we don't
 * inadvertently expose a hole through direct ioctl()s that never gets tested.
 * In userland, however, we want significantly more information about _why_ the
 * name is invalid.  In the kernel, we only care whether it's valid or not.
 * Each routine therefore takes a 'namecheck_err_t' which describes exactly why
 * the name failed to validate.
 *
 * Each function returns 0 on success, -1 on error.
 */

#if defined(_KERNEL)
#include <sys/systm.h>
#else
#include <string.h>
#endif

#include <sys/param.h>
#include "zfs_namecheck.h"

static int
valid_char(char c)
{
	return ((c >= 'a' && c <= 'z') ||
	    (c >= 'A' && c <= 'Z') ||
	    (c >= '0' && c <= '9') ||
	    c == '-' || c == '_' || c == '.' || c == ':');
}

/*
 * Snapshot names must be made up of alphanumeric characters plus the following
 * characters:
 *
 * 	[-_.:]
 */
int
snapshot_namecheck(const char *path, namecheck_err_t *why, char *what)
{
	const char *loc;

	if (strlen(path) >= MAXNAMELEN) {
		if (why)
			*why = NAME_ERR_TOOLONG;
		return (-1);
	}

	if (path[0] == '\0') {
		if (why)
			*why = NAME_ERR_EMPTY_COMPONENT;
		return (-1);
	}

	for (loc = path; *loc; loc++) {
		if (!valid_char(*loc)) {
			if (why) {
				*why = NAME_ERR_INVALCHAR;
				*what = *loc;
			}
			return (-1);
		}
	}
	return (0);
}

/*
 * Dataset names must be of the following form:
 *
 * 	[component][/]*[component][@component]
 *
 * Where each component is made up of alphanumeric characters plus the following
 * characters:
 *
 * 	[-_.:]
 */
int
dataset_namecheck(const char *path, namecheck_err_t *why, char *what)
{
	const char *loc, *end;
	int found_snapshot;

	/*
	 * Make sure the name is not too long.
	 *
	 * ZFS_MAXNAMELEN is the maximum dataset length used in the userland
	 * which is the same as MAXNAMELEN used in the kernel.
	 * If ZFS_MAXNAMELEN value is changed, make sure to cleanup all
	 * places using MAXNAMELEN.
	 */
	if (strlen(path) >= MAXNAMELEN) {
		if (why)
			*why = NAME_ERR_TOOLONG;
		return (-1);
	}

	/* Explicitly check for a leading slash.  */
	if (path[0] == '/') {
		if (why)
			*why = NAME_ERR_LEADING_SLASH;
		return (-1);
	}

	if (path[0] == '\0') {
		if (why)
			*why = NAME_ERR_EMPTY_COMPONENT;
		return (-1);
	}

	loc = path;
	found_snapshot = 0;
	for (;;) {
		/* Find the end of this component */
		end = loc;
		while (*end != '/' && *end != '@' && *end != '\0')
			end++;

		if (*end == '\0' && end[-1] == '/') {
			/* trailing slashes are not allowed */
			if (why)
				*why = NAME_ERR_TRAILING_SLASH;
			return (-1);
		}

		/* Zero-length components are not allowed */
		if (loc == end) {
			if (why) {
				/*
				 * Make sure this is really a zero-length
				 * component and not a '@@'.
				 */
				if (*end == '@' && found_snapshot) {
					*why = NAME_ERR_MULTIPLE_AT;
				} else {
					*why = NAME_ERR_EMPTY_COMPONENT;
				}
			}

			return (-1);
		}

		/* Validate the contents of this component */
		while (loc != end) {
			if (!valid_char(*loc)) {
				if (why) {
					*why = NAME_ERR_INVALCHAR;
					*what = *loc;
				}
				return (-1);
			}
			loc++;
		}

		/* If we've reached the end of the string, we're OK */
		if (*end == '\0')
			return (0);

		if (*end == '@') {
			/*
			 * If we've found an @ symbol, indicate that we're in
			 * the snapshot component, and report a second '@'
			 * character as an error.
			 */
			if (found_snapshot) {
				if (why)
					*why = NAME_ERR_MULTIPLE_AT;
				return (-1);
			}

			found_snapshot = 1;
		}

		/*
		 * If there is a '/' in a snapshot name
		 * then report an error
		 */
		if (*end == '/' && found_snapshot) {
			if (why)
				*why = NAME_ERR_TRAILING_SLASH;
			return (-1);
		}

		/* Update to the next component */
		loc = end + 1;
	}
}

/*
 * For pool names, we have the same set of valid characters as described in
 * dataset names, with the additional restriction that the pool name must begin
 * with a letter.  The pool names 'raidz' and 'mirror' are also reserved names
 * that cannot be used.
 */
int
pool_namecheck(const char *pool, namecheck_err_t *why, char *what)
{
	const char *c;

	/*
	 * Make sure the name is not too long.
	 *
	 * ZPOOL_MAXNAMELEN is the maximum pool length used in the userland
	 * which is the same as MAXNAMELEN used in the kernel.
	 * If ZPOOL_MAXNAMELEN value is changed, make sure to cleanup all
	 * places using MAXNAMELEN.
	 */
	if (strlen(pool) >= MAXNAMELEN) {
		if (why)
			*why = NAME_ERR_TOOLONG;
		return (-1);
	}

	c = pool;
	while (*c != '\0') {
		if (!valid_char(*c)) {
			if (why) {
				*why = NAME_ERR_INVALCHAR;
				*what = *c;
			}
			return (-1);
		}
		c++;
	}

	if (!(*pool >= 'a' && *pool <= 'z') &&
	    !(*pool >= 'A' && *pool <= 'Z')) {
		if (why)
			*why = NAME_ERR_NOLETTER;
		return (-1);
	}

	if (strcmp(pool, "mirror") == 0 || strcmp(pool, "raidz") == 0) {
		if (why)
			*why = NAME_ERR_RESERVED;
		return (-1);
	}

	if (pool[0] == 'c' && (pool[1] >= '0' && pool[1] <= '9')) {
		if (why)
			*why = NAME_ERR_DISKLIKE;
		return (-1);
	}

	return (0);
}

/*
 * Check if the dataset name is private for internal usage.
 * '$' is reserved for internal dataset names. e.g. "$MOS"
 *
 * Return 1 if the given name is used internally.
 * Return 0 if it is not.
 */
int
dataset_name_hidden(const char *name)
{
	if (strchr(name, '$') != NULL)
		return (1);

	return (0);
}
