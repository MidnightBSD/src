/*	$FreeBSD: release/7.0.0/contrib/ipfilter/lib/printifname.c 170268 2007-06-04 02:54:36Z darrenr $	*/

/*
 * Copyright (C) 2002 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id: printifname.c,v 1.2.4.1 2006/06/16 17:21:12 darrenr Exp $
 */

#include "ipf.h"

void printifname(format, name, ifp)
char *format, *name;
void *ifp;
{
	printf("%s%s", format, name);
	if ((ifp == NULL) && strcmp(name, "-") && strcmp(name, "*"))
		printf("(!)");
}
