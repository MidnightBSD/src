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

/* $Id: lib.c,v 1.11.18.3 2005/08/15 01:46:50 marka Exp $ */

/*! \file */

#include <config.h>

#include <stddef.h>

#include <isc/once.h>
#include <isc/msgcat.h>
#include <isc/util.h>

#include <dns/lib.h>

/***
 *** Globals
 ***/

LIBDNS_EXTERNAL_DATA unsigned int			dns_pps = 0U;
LIBDNS_EXTERNAL_DATA isc_msgcat_t *			dns_msgcat = NULL;


/***
 *** Private
 ***/

static isc_once_t		msgcat_once = ISC_ONCE_INIT;


/***
 *** Functions
 ***/

static void
open_msgcat(void) {
	isc_msgcat_open("libdns.cat", &dns_msgcat);
}

void
dns_lib_initmsgcat(void) {

	/*
	 * Initialize the DNS library's message catalog, dns_msgcat, if it
	 * has not already been initialized.
	 */

	RUNTIME_CHECK(isc_once_do(&msgcat_once, open_msgcat) == ISC_R_SUCCESS);
}
