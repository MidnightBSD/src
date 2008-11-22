/*	$FreeBSD: src/contrib/ipfilter/lib/bcopywrap.c,v 1.3 2007/06/04 02:54:32 darrenr Exp $	*/

/*
 * Copyright (C) 2002 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *   
 * $Id: bcopywrap.c,v 1.1.1.2 2008-11-22 14:33:09 laffer1 Exp $
 */  

#include "ipf.h"

int bcopywrap(from, to, size)
void *from, *to;
size_t size;
{
	bcopy((caddr_t)from, (caddr_t)to, size);
	return 0;
}

