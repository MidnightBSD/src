/*	$FreeBSD: stable/10/contrib/ipfilter/lib/printifname.c 255332 2013-09-06 23:11:19Z cy $	*/

/*
 * Copyright (C) 2012 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id$
 */

#include "ipf.h"


void
printifname(format, name, ifp)
	char *format, *name;
	void *ifp;
{
	PRINTF("%s%s", format, name);
	if ((ifp == NULL) && strcmp(name, "-") && strcmp(name, "*"))
		PRINTF("(!)");
}
