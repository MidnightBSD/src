/*
 * Copyright (C) 1998-2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: boolean.h,v 1.12 2001/01/09 21:56:45 bwelling Exp $ */

#ifndef ISC_BOOLEAN_H
#define ISC_BOOLEAN_H 1

typedef enum { isc_boolean_false = 0, isc_boolean_true = 1 } isc_boolean_t;

#define ISC_FALSE isc_boolean_false
#define ISC_TRUE isc_boolean_true
#define ISC_TF(x) ((x) ? ISC_TRUE : ISC_FALSE)

#endif /* ISC_BOOLEAN_H */
