/*	$FreeBSD: src/contrib/ipfilter/lib/printifname.c,v 1.4 2007/06/04 02:54:32 darrenr Exp $	*/

/*
 * Copyright (C) 2002 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id: printifname.c,v 1.1.1.2 2008-11-22 14:33:10 laffer1 Exp $
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
