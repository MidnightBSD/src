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

/*	Copyright (c) 1983, 1984, 1985, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*
 * Portions of this source code were derived from Berkeley 4.3 BSD
 * under license from the Regents of the University of California.
 */

#ifndef	_OPENSOLARIS_RPC_XDR_H_
#define	_OPENSOLARIS_RPC_XDR_H_

#include_next <rpc/xdr.h>

#ifndef _KERNEL
#include_next <rpc/xdr.h>

/*
 * Strangely, my glibc version (2.3.6) doesn't have xdr_control(), so
 * we have to hack it in here (source taken from OpenSolaris).
 * By the way, it is assumed the xdrmem implementation is used.
 */

#undef xdr_control
#define xdr_control(a,b,c) xdrmem_control(a,b,c)

/*
 * These are XDR control operators
 */

#define	XDR_GET_BYTES_AVAIL 1

struct xdr_bytesrec {
	bool_t xc_is_last_record;
	size_t xc_num_avail;
};

typedef struct xdr_bytesrec xdr_bytesrec;

/*
 * These are the request arguments to XDR_CONTROL.
 *
 * XDR_PEEK - returns the contents of the next XDR unit on the XDR stream.
 * XDR_SKIPBYTES - skips the next N bytes in the XDR stream.
 * XDR_RDMAGET - for xdr implementation over RDMA, gets private flags from
 *		 the XDR stream being moved over RDMA
 * XDR_RDMANOCHUNK - for xdr implementaion over RDMA, sets private flags in
 *                   the XDR stream moving over RDMA.
 */
#define XDR_PEEK      2
#define XDR_SKIPBYTES 3
#define XDR_RDMAGET   4
#define XDR_RDMASET   5

/* FIXME: probably doesn't work */
static __inline bool_t
xdrmem_control(XDR *xdrs, int request, void *info)
{
	xdr_bytesrec *xptr;
	int32_t *int32p;
	int len;

	switch (request) {

	case XDR_GET_BYTES_AVAIL:
		xptr = (xdr_bytesrec *)info;
		xptr->xc_is_last_record = TRUE;
		xptr->xc_num_avail = xdrs->x_handy;
		return (TRUE);

	case XDR_PEEK:
		/*
		 * Return the next 4 byte unit in the XDR stream.
		 */
		if (xdrs->x_handy < sizeof (int32_t))
			return (FALSE);
		int32p = (int32_t *)info;
		*int32p = (int32_t)ntohl((uint32_t)
		    (*((int32_t *)(xdrs->x_private))));
		return (TRUE);

	case XDR_SKIPBYTES:
		/*
		 * Skip the next N bytes in the XDR stream.
		 */
		int32p = (int32_t *)info;
		len = RNDUP((int)(*int32p));
		if ((xdrs->x_handy -= len) < 0)
			return (FALSE);
		xdrs->x_private += len;
		return (TRUE);

	}
	return (FALSE);
}
#endif	/* !_KERNEL */

#endif	/* !_OPENSOLARIS_RPC_XDR_H_ */
