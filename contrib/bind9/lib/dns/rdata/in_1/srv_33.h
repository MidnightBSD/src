/*
 * Copyright (C) 2004, 2005  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef IN_1_SRV_33_H
#define IN_1_SRV_33_H 1

/* $Id: srv_33.h,v 1.15.18.2 2005/04/29 00:16:43 marka Exp $ */

/* Reviewed: Fri Mar 17 13:01:00 PST 2000 by bwelling */

/*! 
 *  \brief Per RFC2782 */

typedef struct dns_rdata_in_srv {
	dns_rdatacommon_t	common;
	isc_mem_t		*mctx;
	isc_uint16_t		priority;
	isc_uint16_t		weight;
	isc_uint16_t		port;
	dns_name_t		target;
} dns_rdata_in_srv_t;

#endif /* IN_1_SRV_33_H */
