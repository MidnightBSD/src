/*	$FreeBSD: src/contrib/ipfilter/lib/count6bits.c,v 1.4 2007/06/04 02:54:32 darrenr Exp $	*/

/*
 * Copyright (C) 2000-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id: count6bits.c,v 1.1.1.2 2008-11-22 14:33:09 laffer1 Exp $
 */

#include "ipf.h"


int count6bits(msk)
u_32_t *msk;
{
	int i = 0, k;
	u_32_t j;

	for (k = 3; k >= 0; k--)
		if (msk[k] == 0xffffffff)
			i += 32;
		else {
			for (j = msk[k]; j; j <<= 1)
				if (j & 0x80000000)
					i++;
		}
	return i;
}
