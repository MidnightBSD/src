/* $OpenBSD: ohash_enum.c,v 1.3 2004/06/22 20:00:16 espie Exp $ */
/* ex:ts=8 sw=4: 
 */

/* Copyright (c) 1999, 2004 Marc Espie <espie@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "ohash_int.h"

void *
ohash_first(struct ohash *h, unsigned int *pos)
{
	*pos = 0;
	return ohash_next(h, pos);
}
	
void *
ohash_next(struct ohash *h, unsigned int *pos)
{
	for (; *pos < h->size; (*pos)++) 
		if (h->t[*pos].p != DELETED && h->t[*pos].p != NULL) 
			return (void *)h->t[(*pos)++].p;
	return NULL;
}
