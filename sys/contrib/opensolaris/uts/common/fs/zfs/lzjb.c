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
 * We keep our own copy of this algorithm for 2 main reasons:
 * 	1. If we didn't, anyone modifying common/os/compress.c would
 *         directly break our on disk format
 * 	2. Our version of lzjb does not have a number of checks that the
 *         common/os version needs and uses
 * In particular, we are adding the "feature" that compress() can
 * take a destination buffer size and return -1 if the data will not
 * compress to d_len or less.
 */

#include <sys/zfs_context.h>
#include <sys/types.h>

#define	MATCH_BITS	6
#define	MATCH_MIN	3
#define	MATCH_MAX	((1 << MATCH_BITS) + (MATCH_MIN - 1))
#define	OFFSET_MASK	((1 << (16 - MATCH_BITS)) - 1)
#define	LEMPEL_SIZE	256

/*ARGSUSED*/
size_t
lzjb_compress(void *s_start, void *d_start, size_t s_len, size_t d_len, int n)
{
	uchar_t *src = s_start;
	uchar_t *dst = d_start;
	uchar_t *cpy, *copymap;
	int copymask = 1 << (NBBY - 1);
	int mlen, offset;
	uint16_t *hp;
	uint16_t lempel[LEMPEL_SIZE];	/* uninitialized; see above */

	while (src < (uchar_t *)s_start + s_len) {
		if ((copymask <<= 1) == (1 << NBBY)) {
			if (dst >= (uchar_t *)d_start + d_len - 1 - 2 * NBBY) {
				if (d_len != s_len)
					return (s_len);
				mlen = s_len;
				for (src = s_start, dst = d_start; mlen; mlen--)
					*dst++ = *src++;
				return (s_len);
			}
			copymask = 1;
			copymap = dst;
			*dst++ = 0;
		}
		if (src > (uchar_t *)s_start + s_len - MATCH_MAX) {
			*dst++ = *src++;
			continue;
		}
		hp = &lempel[((src[0] + 13) ^ (src[1] - 13) ^ src[2]) &
		    (LEMPEL_SIZE - 1)];
		offset = (intptr_t)(src - *hp) & OFFSET_MASK;
		*hp = (uint16_t)(uintptr_t)src;
		cpy = src - offset;
		if (cpy >= (uchar_t *)s_start && cpy != src &&
		    src[0] == cpy[0] && src[1] == cpy[1] && src[2] == cpy[2]) {
			*copymap |= copymask;
			for (mlen = MATCH_MIN; mlen < MATCH_MAX; mlen++)
				if (src[mlen] != cpy[mlen])
					break;
			*dst++ = ((mlen - MATCH_MIN) << (NBBY - MATCH_BITS)) |
			    (offset >> NBBY);
			*dst++ = (uchar_t)offset;
			src += mlen;
		} else {
			*dst++ = *src++;
		}
	}
	return (dst - (uchar_t *)d_start);
}

/*ARGSUSED*/
int
lzjb_decompress(void *s_start, void *d_start, size_t s_len, size_t d_len, int n)
{
	uchar_t *src = s_start;
	uchar_t *dst = d_start;
	uchar_t *d_end = (uchar_t *)d_start + d_len;
	uchar_t *cpy, copymap;
	int copymask = 1 << (NBBY - 1);

	while (dst < d_end) {
		if ((copymask <<= 1) == (1 << NBBY)) {
			copymask = 1;
			copymap = *src++;
		}
		if (copymap & copymask) {
			int mlen = (src[0] >> (NBBY - MATCH_BITS)) + MATCH_MIN;
			int offset = ((src[0] << NBBY) | src[1]) & OFFSET_MASK;
			src += 2;
			if ((cpy = dst - offset) < (uchar_t *)d_start)
				return (-1);
			while (--mlen >= 0 && dst < d_end)
				*dst++ = *cpy++;
		} else {
			*dst++ = *src++;
		}
	}
	return (0);
}
