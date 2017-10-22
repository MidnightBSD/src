/*	$FreeBSD: release/10.0.0/contrib/ipfilter/lib/printhostmask.c 255332 2013-09-06 23:11:19Z cy $	*/

/*
 * Copyright (C) 2012 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id$
 */

#include "ipf.h"


void
printhostmask(family, addr, mask)
	int	family;
	u_32_t	*addr, *mask;
{
#ifdef  USE_INET6
	char ipbuf[64];
#else
	struct in_addr ipa;
#endif

	if ((family == -1) || ((!addr || !*addr) && (!mask || !*mask)))
		PRINTF("any");
	else {
		void *ptr = addr;

#ifdef  USE_INET6
		PRINTF("%s", inet_ntop(family, ptr, ipbuf, sizeof(ipbuf)));
#else
		ipa.s_addr = *addr;
		PRINTF("%s", inet_ntoa(ipa));
#endif
		if (mask != NULL)
			printmask(family, mask);
	}
}
