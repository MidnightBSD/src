/*	$FreeBSD: src/contrib/ipfilter/lib/icmpcode.c,v 1.5 2007/06/04 02:54:32 darrenr Exp $	*/

/*
 * Copyright (C) 2000-2006 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id: icmpcode.c,v 1.1.1.2 2008-11-22 14:33:09 laffer1 Exp $
 */

#include <ctype.h>

#include "ipf.h"

#ifndef	MIN
# define	MIN(a,b)	((a) > (b) ? (b) : (a))
#endif


char	*icmpcodes[MAX_ICMPCODE + 1] = {
	"net-unr", "host-unr", "proto-unr", "port-unr", "needfrag", "srcfail",
	"net-unk", "host-unk", "isolate", "net-prohib", "host-prohib",
	"net-tos", "host-tos", "filter-prohib", "host-preced", "preced-cutoff",
	NULL };
