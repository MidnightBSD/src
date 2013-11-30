/*	$FreeBSD: src/contrib/ipfilter/lib/kmemcpywrap.c,v 1.3 2007/06/04 02:54:32 darrenr Exp $	*/

/*
 * Copyright (C) 2002 by Darren Reed.
 * 
 * See the IPFILTER.LICENCE file for details on licencing.  
 *   
 * $Id: kmemcpywrap.c,v 1.1.1.2 2008-11-22 14:33:09 laffer1 Exp $ 
 */     

#include "ipf.h"
#include "kmem.h"

int kmemcpywrap(from, to, size)
void *from, *to;
size_t size;
{
	int ret;

	ret = kmemcpy((caddr_t)to, (u_long)from, size);
	return ret;
}

