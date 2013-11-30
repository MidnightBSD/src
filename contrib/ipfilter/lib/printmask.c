/*	$FreeBSD: src/contrib/ipfilter/lib/printmask.c,v 1.4 2007/06/04 02:54:32 darrenr Exp $	*/

/*
 * Copyright (C) 2000-2005 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id: printmask.c,v 1.1.1.2 2008-11-22 14:33:10 laffer1 Exp $
 */

#include "ipf.h"


void	printmask(mask)
u_32_t	*mask;
{
	struct in_addr ipa;
	int ones;

#ifdef  USE_INET6
	if (use_inet6)
		printf("/%d", count6bits(mask));
	else
#endif
	if ((ones = count4bits(*mask)) == -1) {
		ipa.s_addr = *mask;
		printf("/%s", inet_ntoa(ipa));
	} else
		printf("/%d", ones);
}
