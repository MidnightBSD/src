/*	$FreeBSD: src/contrib/ipfilter/lib/printfraginfo.c,v 1.4 2007/06/04 02:54:32 darrenr Exp $	*/

/*
 * Copyright (C) 2004-2005 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id: printfraginfo.c,v 1.1.1.2 2008-11-22 14:33:10 laffer1 Exp $
 */
#include "ipf.h"
#include "kmem.h"

void printfraginfo(prefix, ifr)
char *prefix;
struct ipfr *ifr;
{
	frentry_t fr;

	fr.fr_flags = 0xffffffff;

	printf("%s%s -> ", prefix, hostname(4, &ifr->ipfr_src));
/*
	if (kmemcpy((char *)&fr, (u_long)ifr->ipfr_rule,
		    sizeof(fr)) == -1)
		return;
*/
	printf("%s id %d ttl %ld pr %d seen0 %d ref %d tos %#02x\n",
		hostname(4, &ifr->ipfr_dst), ifr->ipfr_id, ifr->ipfr_ttl,
		ifr->ipfr_p, ifr->ipfr_seen0, ifr->ipfr_ref, ifr->ipfr_tos);
}
