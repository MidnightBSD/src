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
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	Copyright (c) 1983, 1984, 1985, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*
 * Portions of this source code were derived from Berkeley 4.3 BSD
 * under license from the Regents of the University of California.
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

/*
 * xdr_array.c, Generic XDR routines impelmentation.
 * These are the "non-trivial" xdr primitives used to serialize and de-serialize
 * arrays.  See xdr.h for more info on the interface to xdr.
 */

#include <sys/param.h>
#include <sys/cmn_err.h>
#include <sys/types.h>
#include <sys/systm.h>

#include <rpc/types.h>
#include <rpc/xdr.h>

#define	LASTUNSIGNED	((uint_t)0-1)

/*
 * XDR an array of arbitrary elements
 * *addrp is a pointer to the array, *sizep is the number of elements.
 * If addrp is NULL (*sizep * elsize) bytes are allocated.
 * elsize is the size (in bytes) of each element, and elproc is the
 * xdr procedure to call to handle each element of the array.
 */
bool_t
xdr_array(XDR *xdrs, caddr_t *addrp, uint_t *sizep, const uint_t maxsize,
	const uint_t elsize, const xdrproc_t elproc)
{
	uint_t i;
	caddr_t target = *addrp;
	uint_t c;  /* the actual element count */
	bool_t stat = TRUE;
	uint_t nodesize;

	/* like strings, arrays are really counted arrays */
	if (!xdr_u_int(xdrs, sizep)) {
#ifdef DEBUG
		printf("xdr_array: size FAILED\n");
#endif
		return (FALSE);
	}
	c = *sizep;
	if ((c > maxsize || LASTUNSIGNED / elsize < c) &&
	    xdrs->x_op != XDR_FREE) {
#ifdef DEBUG
		printf("xdr_array: bad size FAILED\n");
#endif
		return (FALSE);
	}
	nodesize = c * elsize;

	/*
	 * if we are deserializing, we may need to allocate an array.
	 * We also save time by checking for a null array if we are freeing.
	 */
	if (target == NULL)
		switch (xdrs->x_op) {
		case XDR_DECODE:
			if (c == 0)
				return (TRUE);
			*addrp = target = (char *)mem_alloc(nodesize);
			bzero(target, nodesize);
			break;

		case XDR_FREE:
			return (TRUE);

		case XDR_ENCODE:
			break;
		}

	/*
	 * now we xdr each element of array
	 */
	for (i = 0; (i < c) && stat; i++) {
		stat = (*elproc)(xdrs, target, LASTUNSIGNED);
		target += elsize;
	}

	/*
	 * the array may need freeing
	 */
	if (xdrs->x_op == XDR_FREE) {
		mem_free(*addrp, nodesize);
		*addrp = NULL;
	}
	return (stat);
}
