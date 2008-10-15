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

#ifndef	_ZFS_NAMECHECK_H
#define	_ZFS_NAMECHECK_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#ifdef	__cplusplus
extern "C" {
#endif

typedef enum {
	NAME_ERR_LEADING_SLASH,		/* name begins with leading slash */
	NAME_ERR_EMPTY_COMPONENT,	/* name contains an empty component */
	NAME_ERR_TRAILING_SLASH,	/* name ends with a slash */
	NAME_ERR_INVALCHAR,		/* invalid character found */
	NAME_ERR_MULTIPLE_AT,		/* multiple '@' characters found */
	NAME_ERR_NOLETTER,		/* pool doesn't begin with a letter */
	NAME_ERR_RESERVED,		/* entire name is reserved */
	NAME_ERR_DISKLIKE,		/* reserved disk name (c[0-9].*) */
	NAME_ERR_TOOLONG,		/* name is too long */
} namecheck_err_t;

int pool_namecheck(const char *, namecheck_err_t *, char *);
int dataset_namecheck(const char *, namecheck_err_t *, char *);
int dataset_name_hidden(const char *);
int snapshot_namecheck(const char *, namecheck_err_t *, char *);

#ifdef	__cplusplus
}
#endif

#endif	/* _ZFS_NAMECHECK_H */
