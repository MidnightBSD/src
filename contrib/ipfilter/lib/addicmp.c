/*	$FreeBSD: src/contrib/ipfilter/lib/addicmp.c,v 1.5 2007/06/04 02:54:32 darrenr Exp $	*/

/*
 * Copyright (C) 2000-2006 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id: addicmp.c,v 1.1.1.2 2008-11-22 14:33:09 laffer1 Exp $
 */

#include <ctype.h>

#include "ipf.h"


char	*icmptypes[MAX_ICMPTYPE + 1] = {
	"echorep", (char *)NULL, (char *)NULL, "unreach", "squench",
	"redir", (char *)NULL, (char *)NULL, "echo", "routerad",
	"routersol", "timex", "paramprob", "timest", "timestrep",
	"inforeq", "inforep", "maskreq", "maskrep", "END"
};
