/*	$FreeBSD: release/10.0.0/contrib/ipfilter/lib/printportcmp.c 255332 2013-09-06 23:11:19Z cy $	*/

/*
 * Copyright (C) 2012 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id$
 */

#include "ipf.h"


void
printportcmp(pr, frp)
	int	pr;
	frpcmp_t	*frp;
{
	static char *pcmp1[] = { "*", "=", "!=", "<", ">", "<=", ">=",
				 "<>", "><", ":" };

	if (frp->frp_cmp == FR_INRANGE || frp->frp_cmp == FR_OUTRANGE)
		PRINTF(" port %d %s %d", frp->frp_port,
			     pcmp1[frp->frp_cmp], frp->frp_top);
	else if (frp->frp_cmp == FR_INCRANGE)
		PRINTF(" port %d:%d", frp->frp_port, frp->frp_top);
	else
		PRINTF(" port %s %s", pcmp1[frp->frp_cmp],
			     portname(pr, frp->frp_port));
}
